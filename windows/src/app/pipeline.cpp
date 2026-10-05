#include "pipeline.h"

#include "clipboard.h"
#include "foreground.h"
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
constexpr DWORD kMenuCloseMs = 150;            // how long the tray menu needs to give the focus back
constexpr ULONGLONG kLayoutCheckMs = 150;      // how long a window gets to switch its layout

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

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    // Klats never delays rendering, so WM_RENDERFORMAT and WM_RENDERALLFORMATS have nothing to
    // render: the default handling is exactly right.
    return DefWindowProcW(window, message, wParam, lParam);
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

DWORD WINAPI Pipeline::threadMain(void* parameter) {
    auto* self = static_cast<Pipeline*>(parameter);
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClass;
    RegisterClassW(&windowClass);
    self->window_ = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
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
    DestroyWindow(self->window_);
    return 0;
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

    ClipboardSnapshot::Failure failure;
    auto snapshot = ClipboardSnapshot::take(window_, cancel_, &failure);
    if (!snapshot) {
        report.outcome = std::wstring(L"nothing done: ") + describe(failure);
        return;
    }
    report.notes.push_back(L"clipboard snapshot " + std::to_wstring(snapshot->byteCount()) + L" bytes in " +
                           std::to_wstring(snapshot->formatCount()) + L" formats" +
                           (snapshot->skippedCount() ? L", " + std::to_wstring(snapshot->skippedCount()) + L" skipped" : L""));

    if (!focusUnchanged(foreground)) {
        report.outcome = L"the focus moved before the copy";
        return;
    }
    DWORD sequenceBefore = GetClipboardSequenceNumber();
    synthetic::copy(keys, foreground.layout);
    ULONGLONG copyStart = nowMs();
    while (GetClipboardSequenceNumber() == sequenceBefore) {
        if (nowMs() - copyStart >= kCopyMs) {
            report.outcome = L"nothing selected, or the program ignored the copy keys";
            return;
        }
        if (!pumpingWait(5, cancel_)) {
            // The copy may still land later; the clipboard was not touched by Klats itself.
            report.outcome = quitting;
            return;
        }
    }
    ULONGLONG copyLatency = nowMs() - copyStart;
    report.notes.push_back(L"copy answered in " + std::to_wstring(copyLatency) + L" ms");

    auto copied = readCopiedText(window_, cancel_);
    if (!copied || copied->text.empty()) {
        snapshot->restore(window_);
        report.outcome = L"the selection is not text";
        return;
    }
    if (copied->lineCopy) {
        snapshot->restore(window_);
        report.outcome = L"nothing selected: the editor copied the whole line";
        return;
    }
    const std::wstring& text = copied->text;
    report.notes.push_back(std::to_wstring(unicode::graphemeCount(text)) + L" characters");

    std::wstring converted;
    std::optional<KeyboardLayout> targetLayout;
    if (action == Action::Case) {
        converted = convertCase(text, settings.caseMode);
    } else {
        auto pair = resolvePair(settings.layoutPair);
        if (!pair) {
            snapshot->restore(window_);
            report.outcome = L"two keyboard layouts are needed";
            return;
        }
        auto tableA = buildTable(pair->first);
        auto tableB = buildTable(pair->second);
        if (!tableA || !tableB) {
            snapshot->restore(window_);
            report.outcome = L"could not read a keyboard layout";
            return;
        }
        Direction tieBreak = sameLayout(foreground.layout, pair->second.hkl) ? Direction::BToA : Direction::AToB;
        Conversion result = convertLayout(text, *tableA, *tableB, tieBreak);
        converted = std::move(result.text);
        bool forward = result.direction == Direction::AToB;
        targetLayout = forward ? pair->second : pair->first;
        report.notes.push_back((forward ? pair->first.id : pair->second.id) + L" → " + targetLayout->id);
    }

    if (converted == text) {
        snapshot->restore(window_);
        report.outcome = L"nothing to change";
        return;
    }
    bool multiline = converted.find_first_of(L"\r\n") != std::wstring::npos;
    if (multiline && isCodeEditor(foreground.exe) && !copied->fromCodeEditor) {
        // Probably VS Code's terminal: pasting there runs every line as a command.
        snapshot->restore(window_);
        report.outcome = L"nothing done: several lines from a terminal inside the editor";
        return;
    }
    if (!focusUnchanged(foreground) || skipReason(inspectForeground())) {
        snapshot->restore(window_);
        report.outcome = L"the focus moved before the paste";
        return;
    }

    HKL resultLayout = targetLayout ? targetLayout->hkl : foreground.layout;
    LCID locale = MAKELCID(LOWORD(reinterpret_cast<ULONG_PTR>(resultLayout)), SORT_DEFAULT);
    auto sequenceAfterWrite = publishText(window_, converted, locale, cancel_);
    if (!sequenceAfterWrite) {
        snapshot->restore(window_);
        report.outcome = L"could not write the clipboard";
        return;
    }
    ULONGLONG quietStart = nowMs();
    auto readers = waitForQuietClipboard(cancel_);
    if (!readers.empty()) {
        std::wstring names;
        for (const auto& name : readers) names += (names.empty() ? L"" : L", ") + (name.empty() ? L"unknown" : name);
        report.notes.push_back(L"waited " + std::to_wstring(nowMs() - quietStart) + L" ms for clipboard readers (" + names + L")");
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

    // A cancelled wait (Klats is quitting, Windows shuts down) restores right away.
    pumpingWait(restoreDelay(copyLatency), cancel_);
    if (GetClipboardSequenceNumber() == *sequenceAfterWrite) {
        if (!snapshot->restore(window_)) report.notes.push_back(L"clipboard restored only partly");
    } else {
        report.notes.push_back(L"clipboard changed meanwhile, not restored");
    }
    report.outcome = L"replaced";
}

}  // namespace klats::app
