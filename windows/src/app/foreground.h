#pragma once
#include <windows.h>

#include <optional>
#include <string>

namespace klats::app {

// The window the user is typing in, as Klats sees it at the moment of the hotkey.
struct Foreground {
    HWND top = nullptr;    // the foreground window
    HWND focus = nullptr;  // the window with the keyboard focus inside it
    DWORD thread = 0;      // the thread that owns the focus
    DWORD process = 0;
    std::wstring exe;      // lowercase file name, e.g. «chrome.exe»
    std::wstring topClass;
    std::wstring focusClass;
    HKL layout = nullptr;  // the keyboard layout of the focus thread
};

Foreground inspectForeground();

// Nothing to do in terminals, remote desktops, virtual machines, full-screen games, hung windows and
// windows of programs running with higher rights than Klats. The reason goes to the log.
std::optional<std::wstring> skipReason(const Foreground& foreground);

// Whether the focus is still where it was when the hotkey fired.
bool focusUnchanged(const Foreground& foreground);

// Whether this process runs with higher rights than Klats (or its rights cannot be read).
bool isElevatedAboveUs(DWORD processId);

// Whether Klats itself runs with administrator rights (always so when UAC is off).
bool runningElevated();

}  // namespace klats::app
