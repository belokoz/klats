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
    WM_KLATS_ACTION = WM_APP + 1,  // to the pipeline window; wParam: Action, lParam: 1 when from the menu
    WM_KLATS_TRAY = WM_APP + 2,    // tray icon callback
    WM_KLATS_HOOK_FAILED = WM_APP + 3,  // to the tray owner: the hook could not be installed
};

inline std::wstring actionName(Action action) { return action == Action::Layout ? L"layout" : L"case"; }

// Milliseconds on a monotonic clock.
inline ULONGLONG nowMs() { return GetTickCount64(); }

std::wstring utf16(std::string_view utf8);
std::string utf8(std::wstring_view utf16);

// The file name of a process (e.g. «chrome.exe»), lowercase; empty when it cannot be read.
std::wstring processName(DWORD processId);

std::wstring windowClass(HWND window);

}  // namespace klats::app
