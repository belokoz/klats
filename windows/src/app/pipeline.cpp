#include "pipeline.h"

#include "foreground.h"
#include "hook_thread.h"
#include "input_sources.h"
#include "log.h"
#include "synthetic_keys.h"

#include "core/case_converter.h"
#include "core/layout_converter.h"
#include "core/unicode.h"

#include <timeapi.h>

#include <algorithm>
#include <vector>

namespace klats::app {

namespace {

// All in milliseconds.
constexpr ULONGLONG kModifierReleaseMs = 600;  // how long the user gets to let go of the hotkey
constexpr ULONGLONG kCopyMs = 500;             // how long the program gets to answer the copy keys
constexpr ULONGLONG kLateCopyMs = 5000;        // how late an answer to the copy keys is still undone
constexpr DWORD kMenuCloseMs = 150;            // how long the tray menu needs to give the focus back
constexpr ULONGLONG kLayoutCheckMs = 150;      // how long a window gets to switch its layout
constexpr DWORD kFinalRestoreWaitMs = 2000;    // how long the clipboard may stay busy before the last restore
constexpr DWORD kPasteLandingMs = 100;         // what a quitting Klats still gives the paste

constexpr UINT_PTR kLateCopyTimer = 1;

const wchar_t* kWindowClass = L"KlatsPipeline";

// The program reads the clipboard some time after the paste keys; a program that was slow to answer
// the copy keys is slow to paste too. Restoring too early would paste the old clipboard.
DWORD restoreDelay(ULONGLONG copyLatency) {
    return static_cast<DWORD>(std::clamp<ULONGLONG>(200 + 3 * copyLatency, 300, 2000));
}

bool screenReaderRunning() {
    BOOL running = FALSE;
    return SystemParametersInfoW(SPI_GETSCREENREADER, 0, &running, 0) && running;
}

// VS Code and its relatives: their terminal cannot be told apart from the editor by window class.
bool isCodeEditor(const std::wstring& exe) {
    return exe == L"code.exe" || exe == L"code - insiders.exe" || exe == L"cursor.exe" || exe == L"vscodium.exe" ||
           exe == L"windsurf.exe";
}

// Sublime Text copies the whole line, line break included, when nothing is selected, and leaves no
// mark to tell. The same text from a real selection is rare: one whole line taken with its break.
bool looksLikeLineCopy(const std::wstring& exe, const std::wstring& text) {
    return exe == L"sublime_text.exe" && !text.empty() && text.back() == L'\n' && std::count(text.begin(), text.end(), L'\n') == 1;
}

size_t lineBreaks(const std::wstring& text) {
    return static_cast<size_t>(std::count_if(text.begin(), text.end(), [](wchar_t c) { return c == L'\r' || c == L'\n'; }));
}

bool sameLayout(HKL a, HKL b) {
    return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(a)) == static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(b));
}

bool waitForLayout(DWORD thread, HKL target, HANDLE cancel) {
    ULONGLONG start = nowMs();
    while (!sameLayout(GetKeyboardLayout(thread), target)) {
        if (nowMs() - start >= kLayoutCheckMs || !pumpingWait(10, cancel)) return false;
    }
    return true;
}

// One log line per run, written whatever way the run ends. Never the text itself.
struct Report {
    std::wstring action;
    std::wstring trigger;
    std::wstring program = L"unknown";
    std::wstring outcome = L"unknown";
    std::vector<std::wstring> notes;
    ULONGLONG start = nowMs();

    ~Report() {
        std::wstring details;
        for (size_t i = 0; i < notes.size(); ++i) details += (i ? L", " : L" (") + notes[i];
        if (!notes.empty()) details += L")";
        log::write(action + L" via " + trigger + L" in " + program + L": " + outcome + details + L" [" +
                   std::to_wstring(nowMs() - start) + L" ms]");
    }
};

}  // namespace

bool Pipeline::start() {
    if (thread_) return true;
    ready_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    cancel_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ready_ || !cancel_) return false;
    thread_ = CreateThread(nullptr, 0, &Pipeline::threadMain, this, 0, nullptr);
    if (!thread_) return false;
    WaitForSingleObject(ready_, INFINITE);
    return window_ != nullptr;
}

void Pipeline::stop() {
    if (thread_) {
        SetEvent(cancel_);
        PostThreadMessageW(GetThreadId(thread_), WM_QUIT, 0, 0);
        WaitForSingleObject(thread_, 5000);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    if (ready_) CloseHandle(ready_);
    if (cancel_) CloseHandle(cancel_);
    ready_ = cancel_ = nullptr;
    window_ = nullptr;
}

void Pipeline::request(Action action, bool fromMenu) {
    if (window_) PostMessageW(window_, WM_KLATS_ACTION, static_cast<WPARAM>(action), fromMenu ? 1 : 0);
}

LRESULT CALLBACK Pipeline::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<Pipeline*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (self && (message == WM_CLIPBOARDUPDATE || (message == WM_TIMER && wParam == kLateCopyTimer))) {
        self->settleLateCopy();
        return 0;
    }
    // Klats never delays rendering, so WM_RENDERFORMAT and WM_RENDERALLFORMATS have nothing to
    // render: the default handling is exactly right.
    return DefWindowProcW(window, message, wParam, lParam);
}

DWORD WINAPI Pipeline::threadMain(void* parameter) {
    auto* self = static_cast<Pipeline*>(parameter);
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClass;
    RegisterClassW(&windowClass);
    self->window_ = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
    if (self->window_) SetWindowLongPtrW(self->window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    MSG message;
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);  // creates the message queue
    SetEvent(self->ready_);
    if (!self->window_) return 1;

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_KLATS_ACTION && message.hwnd == self->window_) {
            self->run(static_cast<Action>(message.wParam), message.lParam != 0, message.time);
            self->lastRunEnd_ = GetTickCount();
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    self->settleLateCopy();
    DestroyWindow(self->window_);
    return 0;
}

void Pipeline::watchLateCopy(LateCopy late) {
    lateCopy_ = std::move(late);
    AddClipboardFormatListener(window_);
    SetTimer(window_, kLateCopyTimer, static_cast<UINT>(kLateCopyMs), nullptr);
    // The copy may have landed just now, before the listener was in place.
    if (GetClipboardSequenceNumber() != lateCopy_->sequenceBefore) settleLateCopy();
}

// The first clipboard change after the copy keys went unanswered decides. The user's clipboard
// comes back only for a copy that is surely the late answer: made by the same program, within 5 s,
// and with no key typed since, which could have been the user's own Ctrl+C. A copy the user makes
// with the mouse in that time is not told apart.
void Pipeline::settleLateCopy() {
    if (!lateCopy_) return;
    LateCopy late = std::move(*lateCopy_);
    lateCopy_.reset();
    RemoveClipboardFormatListener(window_);
    KillTimer(window_, kLateCopyTimer);

    DWORD sequence = GetClipboardSequenceNumber();
    if (sequence == late.sequenceBefore) return;
    DWORD owner = 0;
    if (HWND window = GetClipboardOwner()) GetWindowThreadProcessId(window, &owner);
    if (owner != late.process || nowMs() - late.sentAt > kLateCopyMs || HookThread::lastTypedAt() > late.sentAt) return;
    WriteResult result = late.snapshot.restore(window_, sequence);
    log::write(L"late copy in " + late.exe + L" after " + std::to_wstring(nowMs() - late.sentAt) + L" ms: user's clipboard " +
               (result == WriteResult::Written ? std::wstring(L"put back") : describe(result)));
}

void Pipeline::run(Action action, bool fromMenu, DWORD postedAt) {
    // Waits are counted in milliseconds; at the default 15.6 ms timer tick every wait would
    // overshoot, and short clipboard locks by other programs would go unnoticed. Only for the
    // half second a conversion takes.
    struct FineTimer {
        FineTimer() { timeBeginPeriod(1); }
        ~FineTimer() { timeEndPeriod(1); }
    } fineTimer;
    Report report;
    report.action = actionName(action);
    report.trigger = fromMenu ? L"menu" : L"hotkey";

    // A hotkey pressed while the previous run was still working waited in the queue: by now it
    // would land somewhere the user no longer expects.
    if (lastRunEnd_ && static_cast<LONG>(postedAt - lastRunEnd_) < 0) {
        report.outcome = L"skipped, previous run still in progress";
        return;
    }
    const std::wstring quitting = L"cancelled: Klats is quitting";
    // The tray menu has to finish closing and give the focus back before the copy keys go out.
    if (fromMenu && !pumpingWait(kMenuCloseMs, cancel_)) {
        report.outcome = quitting;
        return;
    }

    Foreground foreground = inspectForeground();
    report.program = foreground.exe.empty() ? L"unknown" : foreground.exe;
    if (auto reason = skipReason(foreground)) {
        report.outcome = L"nothing done: " + *reason;
        return;
    }
    Settings settings = settings_.snapshot();
    CopyKeys keys = screenReaderRunning() ? CopyKeys::Ctrl : settings.copyKeys;

    // The layouts are settled before anything touches the clipboard.
    LayoutSet layouts;
    if (action == Action::Layout) {
        layouts = readLayouts(settings.layoutPair);
        if (layouts.pair.problem == PairChoice::Problem::NeedTwo) {
            report.outcome = L"nothing done: two different keyboard layouts are needed";
            return;
        }
        if (layouts.pair.problem == PairChoice::Problem::Ambiguous) {
            report.outcome = L"nothing done: the layout pair has to be chosen in the settings";
            return;
        }
    }

    ULONGLONG releaseStart = nowMs();
    while (synthetic::modifiersAreDown()) {
        if (nowMs() - releaseStart >= kModifierReleaseMs) {
            report.outcome = L"the hotkey keys were still held";
            return;
        }
        if (!pumpingWait(10, cancel_)) {
            report.outcome = quitting;
            return;
        }
    }
    report.notes.push_back(L"modifiers up after " + std::to_wstring(nowMs() - releaseStart) + L" ms");

    settleLateCopy();
    ClipboardSnapshot::Failure failure;
    auto snapshot = ClipboardSnapshot::take(window_, cancel_, &failure);
    if (!snapshot) {
        report.outcome = std::wstring(L"nothing done: ") + describe(failure);
        return;
    }
    report.notes.push_back(L"clipboard snapshot " + std::to_wstring(snapshot->byteCount()) + L" bytes in " +
                           std::to_wstring(snapshot->formatCount()) + L" formats" +
                           (snapshot->skippedCount() ? L", " + std::to_wstring(snapshot->skippedCount()) + L" skipped" : L""));
    // Puts the user's clipboard back, but never over anything copied after `expected`.
    auto restore = [&](std::optional<DWORD> expected, DWORD waitMs = 500) {
        WriteResult result = snapshot->restore(window_, expected, waitMs);
        if (result != WriteResult::Written) report.notes.push_back(std::wstring(L"user's clipboard ") + describe(result));
    };

    if (!focusUnchanged(foreground)) {
        report.outcome = L"the focus moved before the copy";
        return;
    }
    DWORD sequenceBefore = GetClipboardSequenceNumber();
    synthetic::copy(keys, foreground.layout);
    ULONGLONG copyStart = nowMs();
    while (GetClipboardSequenceNumber() == sequenceBefore) {
        bool timedOut = nowMs() - copyStart >= kCopyMs;
        bool cancelled = !timedOut && !pumpingWait(5, cancel_);
        if (!timedOut && !cancelled) continue;
        // A busy program may still answer the copy keys and replace the user's clipboard.
        watchLateCopy({std::move(*snapshot), sequenceBefore, foreground.process, foreground.exe, copyStart});
        report.outcome = cancelled ? quitting : L"nothing selected, or the program ignored the copy keys";
        return;
    }
    ULONGLONG copyLatency = nowMs() - copyStart;
    report.notes.push_back(L"copy answered in " + std::to_wstring(copyLatency) + L" ms");

    auto copied = readCopiedText(window_, cancel_, foreground.layout);
    if (!copied) {
        restore(std::nullopt);
        report.outcome = L"could not read the clipboard";
        return;
    }
    if (copied->tooLarge) {
        restore(copied->sequence);
        report.outcome = L"nothing done: the selection is too large";
        return;
    }
    if (copied->text.empty()) {
        restore(copied->sequence);
        report.outcome = L"the selection is not text";
        return;
    }
    if (copied->lineCopy || looksLikeLineCopy(foreground.exe, copied->text)) {
        restore(copied->sequence);
        report.outcome = L"nothing selected: the editor copied the whole line";
        return;
    }
    const std::wstring& text = copied->text;
    report.notes.push_back(std::to_wstring(unicode::graphemeCount(text)) + L" characters" + (copied->columnCopy ? L" in a column" : L""));

    std::wstring converted;
    const KeyboardLayout* targetLayout = nullptr;
    if (action == Action::Case) {
        converted = convertCase(text, settings.caseMode);
    } else {
        const KeyboardLayout& first = layouts.layouts[layouts.pair.first];
        const KeyboardLayout& second = layouts.layouts[layouts.pair.second];
        Direction tieBreak = sameLayout(foreground.layout, second.hkl) ? Direction::BToA : Direction::AToB;
        Conversion result = convertLayout(text, layouts.tables[layouts.pair.first], layouts.tables[layouts.pair.second], tieBreak);
        converted = std::move(result.text);
        bool forward = result.direction == Direction::AToB;
        targetLayout = forward ? &second : &first;
        report.notes.push_back((forward ? first.id : second.id) + L" → " + targetLayout->id);
    }

    if (converted == text) {
        restore(copied->sequence);
        report.outcome = L"nothing to change";
        return;
    }
    // Only characters on keys change, never line breaks; anything else would be a bug, and a column
    // pasted back with other breaks would come out skewed.
    if (lineBreaks(converted) != lineBreaks(text)) {
        restore(copied->sequence);
        report.outcome = L"nothing done: the line breaks would change";
        return;
    }
    bool multiline = converted.find_first_of(L"\r\n") != std::wstring::npos;
    if (multiline && isCodeEditor(foreground.exe) && !copied->fromCodeEditor) {
        // Probably VS Code's terminal: pasting there runs every line as a command.
        restore(copied->sequence);
        report.outcome = L"nothing done: several lines from a terminal inside the editor";
        return;
    }
    // Nothing is published for a window that is already gone.
    if (!focusUnchanged(foreground) || skipReason(inspectForeground())) {
        restore(copied->sequence);
        report.outcome = L"the focus moved before the paste";
        return;
    }

    HKL resultLayout = targetLayout ? targetLayout->hkl : foreground.layout;
    DWORD sequenceAfterWrite = 0;
    WriteResult written = publishText(window_, converted, clipboardLocale(converted, resultLayout), copied->columnCopy, copied->sequence,
                                      cancel_, &sequenceAfterWrite);
    if (written != WriteResult::Written) {
        // Something copied meanwhile is the user's now: it stays, and the old clipboard with it.
        if (written == WriteResult::Changed) {
            report.outcome = L"nothing done: the clipboard changed after the copy";
        } else {
            restore(written == WriteResult::Partial ? std::optional<DWORD>(sequenceAfterWrite) : std::optional<DWORD>(copied->sequence));
            report.outcome = written == WriteResult::Cancelled ? quitting : L"could not write the clipboard";
        }
        return;
    }
    ULONGLONG quietStart = nowMs();
    auto readers = waitForQuietClipboard(cancel_);
    if (!readers.empty()) {
        std::wstring names;
        for (const auto& name : readers) names += (names.empty() ? L"" : L", ") + (name.empty() ? L"unknown" : name);
        report.notes.push_back(L"waited " + std::to_wstring(nowMs() - quietStart) + L" ms for clipboard readers (" + names + L")");
    }
    // The last look before the keys go, with nothing in between: the focus may have moved during
    // the write and the wait, and a quitting Klats puts the clipboard back instead of pasting.
    if (WaitForSingleObject(cancel_, 0) == WAIT_OBJECT_0) {
        restore(sequenceAfterWrite);
        report.outcome = quitting;
        return;
    }
    if (skipReason(inspectForeground()) || !focusUnchanged(foreground)) {
        restore(sequenceAfterWrite);
        report.outcome = L"the focus moved before the paste";
        return;
    }
    synthetic::paste(keys, foreground.layout);

    if (targetLayout && settings.switchLayoutAfterConversion) {
        if (sameLayout(GetKeyboardLayout(foreground.thread), targetLayout->hkl)) {
            report.notes.push_back(L"layout already " + targetLayout->id);
        } else {
            // The focused window first; a few programs only listen at their top-level window.
            requestLayout(foreground.focus, targetLayout->hkl);
            bool switched = waitForLayout(foreground.thread, targetLayout->hkl, cancel_);
            if (!switched && foreground.top != foreground.focus) {
                requestLayout(foreground.top, targetLayout->hkl);
                switched = waitForLayout(foreground.thread, targetLayout->hkl, cancel_);
            }
            report.notes.push_back(switched ? L"layout switched" : L"layout switch failed");
        }
    }

    // Klats quitting (or Windows shutting down) shortens the wait, but the paste still gets a moment.
    if (!pumpingWait(restoreDelay(copyLatency), cancel_)) pumpingWait(kPasteLandingMs, nullptr);
    restore(sequenceAfterWrite, kFinalRestoreWaitMs);
    report.outcome = L"replaced";
}

}  // namespace klats::app
