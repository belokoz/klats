// End-to-end check of a running Klats on the real desktop: opens Notepad and WordPad, types a
// selection into them, presses the hotkeys with SendInput and reads what came back.
//
// It takes over the keyboard focus for a few seconds per program, so it waits first: hands off
// the keyboard and mouse once it starts. Nothing it reads from the clipboard is printed or saved;
// the clipboard is only compared, before and after, by format list and hash.
//
//   klats_e2e.exe [seconds to wait before starting]
#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

struct Fingerprint {
    std::vector<UINT> formats;
    std::vector<unsigned long long> hashes;
    bool operator==(const Fingerprint&) const = default;
};

unsigned long long fnv(const BYTE* data, size_t size) {
    unsigned long long hash = 1469598103934665603ull;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ data[i]) * 1099511628211ull;
    return hash;
}

// What is on the clipboard, without keeping any of it: format ids and a hash of each memory block.
Fingerprint clipboardFingerprint() {
    Fingerprint print;
    for (int attempt = 0; attempt < 50 && !OpenClipboard(nullptr); ++attempt) Sleep(10);
    for (UINT format = EnumClipboardFormats(0); format; format = EnumClipboardFormats(format)) {
        if (format == CF_BITMAP || format == CF_PALETTE || format == CF_ENHMETAFILE || format == CF_METAFILEPICT ||
            format == CF_OWNERDISPLAY || (format >= 0x80 && format <= 0x8E) || (format >= 0x200 && format <= 0x3FF)) {
            continue;
        }
        unsigned long long hash = 0;
        if (HANDLE handle = GetClipboardData(format)) {
            if (const void* data = GlobalLock(handle)) {
                hash = fnv(static_cast<const BYTE*>(data), GlobalSize(handle));
                GlobalUnlock(handle);
            }
        }
        print.formats.push_back(format);
        print.hashes.push_back(hash);
    }
    CloseClipboard();
    return print;
}

void bringToFront(HWND window) {
    DWORD foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD ownThread = GetCurrentThreadId();
    AttachThreadInput(ownThread, foregroundThread, TRUE);
    ShowWindow(window, SW_RESTORE);
    SetForegroundWindow(window);
    BringWindowToTop(window);
    AttachThreadInput(ownThread, foregroundThread, FALSE);
}

INPUT key(WORD vk, bool down) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    input.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP) | (vk == VK_LWIN ? KEYEVENTF_EXTENDEDKEY : 0);
    return input;
}

// Like a person: each key on its own, a little apart.
void press(const std::vector<std::pair<WORD, bool>>& keys) {
    for (auto [vk, down] : keys) {
        INPUT input = key(vk, down);
        SendInput(1, &input, sizeof input);
        Sleep(40);
    }
}

std::wstring text(HWND edit) {
    int length = static_cast<int>(SendMessageW(edit, WM_GETTEXTLENGTH, 0, 0));
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    SendMessageW(edit, WM_GETTEXT, value.size(), reinterpret_cast<LPARAM>(value.data()));
    value.resize(static_cast<size_t>(length));
    return value;
}

struct Context {
    DWORD process = 0;
    HWND top = nullptr;
};

BOOL CALLBACK findTop(HWND window, LPARAM parameter) {
    auto* context = reinterpret_cast<Context*>(parameter);
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process == context->process && IsWindowVisible(window) && !GetWindow(window, GW_OWNER)) {
        context->top = window;
        return FALSE;
    }
    return TRUE;
}

std::string utf8(const std::wstring& value) {
    int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}

int failures = 0;

void report(bool ok, const char* what, const std::wstring& detail = {}) {
    if (!ok) ++failures;
    std::printf("%s %s%s%s\n", ok ? "ok  " : "FAIL", what, detail.empty() ? "" : ": ", utf8(detail).c_str());
}

struct Scenario {
    const char* name;
    const wchar_t* program;
    const wchar_t* editClass;
    const wchar_t* input;
    bool selectAll;
    std::vector<std::pair<WORD, bool>> keys;
    const wchar_t* expected;
    HKL expectedLayout;  // nullptr: no check
};

void run(const Scenario& scenario) {
    STARTUPINFOW startup{sizeof startup};
    PROCESS_INFORMATION process{};
    std::wstring commandLine = scenario.program;
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        report(false, scenario.name, L"could not start the program");
        return;
    }
    WaitForInputIdle(process.hProcess, 5000);
    Context context{process.dwProcessId};
    for (int i = 0; i < 50 && !context.top; ++i) {
        EnumWindows(findTop, reinterpret_cast<LPARAM>(&context));
        if (!context.top) Sleep(100);
    }
    HWND edit = context.top ? FindWindowExW(context.top, nullptr, scenario.editClass, nullptr) : nullptr;
    if (!edit) {
        report(false, scenario.name, L"no edit control");
    } else {
        SendMessageW(edit, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(scenario.input));
        bringToFront(context.top);
        SetFocus(edit);
        Sleep(300);
        if (scenario.selectAll) SendMessageW(edit, EM_SETSEL, 0, -1);
        else SendMessageW(edit, EM_SETSEL, static_cast<WPARAM>(-1), -1);
        Sleep(200);
        bool front = GetForegroundWindow() == context.top;
        if (!front) {
            report(false, scenario.name, L"could not bring the program to the front");
        } else {
            ULONGLONG start = GetTickCount64();
            press(scenario.keys);
            // Every distinct text the field shows over 2.5 s, with when it appeared.
            std::wstring timeline;
            std::wstring last = scenario.input;
            while (GetTickCount64() - start < 2500) {
                Sleep(20);
                std::wstring now = text(edit);
                if (now != last) {
                    timeline += L" → «" + now + L"» at " + std::to_wstring(GetTickCount64() - start) + L" ms";
                    last = now;
                }
            }
            std::wstring now = text(edit);
            report(now == scenario.expected, scenario.name, L"«" + now + L"»" + timeline);
            report(GetForegroundWindow() == context.top, (std::string(scenario.name) + ": still in front (no Start menu)").c_str());
            if (scenario.expectedLayout) {
                HKL layout = GetKeyboardLayout(GetWindowThreadProcessId(context.top, nullptr));
                wchar_t hex[16];
                swprintf_s(hex, L"%08X", static_cast<unsigned>(reinterpret_cast<ULONG_PTR>(layout)));
                report(layout == scenario.expectedLayout, (std::string(scenario.name) + ": layout switched").c_str(), hex);
            }
        }
    }
    TerminateProcess(process.hProcess, 0);
    WaitForSingleObject(process.hProcess, 3000);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    Sleep(300);
}

HKL findLayout(WORD language) {
    HKL layouts[32];
    int count = GetKeyboardLayoutList(32, layouts);
    for (int i = 0; i < count; ++i) {
        if (LOWORD(reinterpret_cast<ULONG_PTR>(layouts[i])) == language) return layouts[i];
    }
    return nullptr;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    int delay = argc > 1 ? _wtoi(argv[1]) : 10;
    if (!FindWindowW(L"KlatsTrayOwner", nullptr)) {
        std::printf("Klats is not running\n");
        return 2;
    }
    std::printf("starting in %d s: hands off the keyboard and mouse\n", delay);
    std::fflush(stdout);
    Sleep(static_cast<DWORD>(delay) * 1000);

    HWND userWindow = GetForegroundWindow();
    HKL userLayout = GetKeyboardLayout(GetWindowThreadProcessId(userWindow, nullptr));
    Fingerprint before = clipboardFingerprint();
    HKL russian = findLayout(0x0419);
    HKL english = findLayout(0x0409);

    constexpr WORD win = VK_LWIN, alt = VK_LMENU, z = 'Z';
    const std::vector<std::pair<WORD, bool>> chord = {{win, true}, {alt, true}, {alt, false}, {win, false}};
    const std::vector<std::pair<WORD, bool>> chordOtherOrder = {{alt, true}, {win, true}, {alt, false}, {win, false}};
    const std::vector<std::pair<WORD, bool>> caseCombo = {{win, true}, {alt, true}, {z, true}, {z, false}, {alt, false}, {win, false}};

    const Scenario scenarios[] = {
        {"Notepad: ghbdtn vbh -> привет мир", L"notepad.exe", L"Edit", L"ghbdtn vbh", true, chord, L"привет мир", russian},
        {"Notepad: руддщ цщкдв -> hello world (Alt first)", L"notepad.exe", L"Edit", L"руддщ цщкдв", true, chordOtherOrder, L"hello world", english},
        {"Notepad: case пРИВЕТ -> Привет", L"notepad.exe", L"Edit", L"пРИВЕТ", true, caseCombo, L"Привет", nullptr},
        {"Notepad: nothing selected, nothing changes", L"notepad.exe", L"Edit", L"ghbdtn", false, chord, L"ghbdtn", nullptr},
        {"WordPad: ghbdtn vbh -> привет мир", L"C:\\Program Files\\Windows NT\\Accessories\\wordpad.exe", L"RICHEDIT50W", L"ghbdtn vbh", true, chord, L"привет мир", russian},
        {"Notepad again: ghbdtn vbh -> привет мир", L"notepad.exe", L"Edit", L"ghbdtn vbh", true, chord, L"привет мир", russian},
    };
    for (const Scenario& scenario : scenarios) run(scenario);

    Sleep(500);
    report(clipboardFingerprint() == before, "clipboard is back exactly as it was");

    // Put the user back where they were.
    if (userWindow && IsWindow(userWindow)) {
        bringToFront(userWindow);
        PostMessageW(userWindow, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(userLayout));
    }
    std::printf("%d failed\n", failures);
    return failures ? 1 : 0;
}
