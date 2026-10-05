#pragma once
#include <windows.h>

#include <string>
#include <string_view>

namespace klats::app {

// What a hotkey or a menu item asks for.
enum class Action : WPARAM {
    Layout = 1,
    Case = 2,
};

// Stamped on every key Klats sends (KEYBDINPUT::dwExtraInfo), so its own hook lets them through.
// Bit 0 stays clear: PowerToys treats events with bit 0 set as its own.
inline constexpr ULONG_PTR kInjectedMarker = 0x4B4C4154;  // «KLAT»

// Private messages between Klats's threads and windows.
enum : UINT {
    WM_KLATS_ACTION = WM_APP + 1,       // to the pipeline window; wParam: Action, lParam: 1 when from the menu
    WM_KLATS_TRAY = WM_APP + 2,         // tray icon callback
    WM_KLATS_HOOK_STATUS = WM_APP + 3,  // to the tray owner; wParam: HookStatus, lParam: error code
    WM_KLATS_UPDATE = WM_APP + 4,       // to the tray owner; lParam: WindowsRelease*, the receiver deletes it
    WM_KLATS_RECORD_KEY = WM_APP + 5,   // to the settings window; wParam: vk, lParam: 1 if pressed | modifiers << 8
};

// What the hook thread reports. The UI thread writes it to the log: a slow disk on the hook thread
// would hold up every keystroke in the system.
enum class HookStatus : WPARAM {
    Failed,
    Reinstalled,
    ReinstalledAfterRemoval,  // Windows had dropped the hook without a word
};

inline std::wstring actionName(Action action) { return action == Action::Layout ? L"layout" : L"case"; }

// Milliseconds on a monotonic clock with a 1 ms resolution: GetTickCount64 moves in 15.6 ms steps,
// which would turn a 25 ms wait into anything from 16 to 31 ms.
inline ULONGLONG nowMs() {
    static const LONGLONG frequency = [] {
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        return value.QuadPart;
    }();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<ULONGLONG>(counter.QuadPart / frequency * 1000 + counter.QuadPart % frequency * 1000 / frequency);
}

std::wstring utf16(std::string_view utf8);
std::string utf8(std::wstring_view utf16);

// The file name of a process (e.g. «chrome.exe»), lowercase; empty when it cannot be read.
std::wstring processName(DWORD processId);

std::wstring windowClass(HWND window);

}  // namespace klats::app
