#include "foreground.h"

#include "common.h"

#include <imm.h>
#include <shellapi.h>

#include <string_view>

namespace klats::app {

namespace {

// Window classes where Klats must do nothing. In a console Ctrl+Insert is harmless, but pasting the
// result runs it as commands; remote and virtual machines get the keys and the clipboard on their
// own schedule.
const std::wstring_view kSkipClasses[] = {
    L"ConsoleWindowClass",             // cmd and PowerShell in conhost
    L"CASCADIA_HOSTING_WINDOW_CLASS",  // Windows Terminal
    L"PseudoConsoleWindow",
    L"VirtualConsoleClass",            // ConEmu
    L"PuTTY",
    L"IHWindowClass",                  // the input window of Remote Desktop, embedded in any program
    L"TscShellContainerClass",         // Remote Desktop Connection
};

const std::wstring_view kSkipProcesses[] = {
    // Terminals
    L"windowsterminal.exe", L"openconsole.exe", L"mintty.exe", L"putty.exe", L"kitty.exe", L"conemu.exe",
    L"conemu64.exe", L"alacritty.exe", L"wezterm-gui.exe", L"tabby.exe", L"hyper.exe", L"mobaxterm.exe",
    L"termius.exe", L"securecrt.exe", L"xshell.exe",
    // Remote desktops and virtual machines
    L"mstsc.exe", L"msrdc.exe", L"vmconnect.exe", L"vmware.exe", L"vmplayer.exe", L"vmware-view.exe",
    L"virtualboxvm.exe", L"wfica32.exe", L"cdviewer.exe", L"rustdesk.exe", L"anydesk.exe", L"teamviewer.exe",
    L"parsecd.exe", L"vncviewer.exe", L"tvnviewer.exe",
};

// The mandatory integrity level of a token: 0x2000 medium, 0x3000 high, 0x4000 system; 0 if unknown.
DWORD integrityLevel(HANDLE token) {
    DWORD size = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &size);
    if (!size) return 0;
    std::wstring buffer(size / sizeof(wchar_t) + 1, L'\0');
    auto* label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer.data());
    if (!GetTokenInformation(token, TokenIntegrityLevel, label, size, &size)) return 0;
    PSID sid = label->Label.Sid;
    return *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1);
}

DWORD ownIntegrity() {
    static const DWORD level = [] {
        HANDLE token = nullptr;
        DWORD result = 0;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            result = integrityLevel(token);
            CloseHandle(token);
        }
        return result;
    }();
    return level;
}

HWND currentFocus() {
    GUITHREADINFO info{};
    info.cbSize = sizeof info;
    if (GetGUIThreadInfo(0, &info)) {
        if (info.hwndFocus) return info.hwndFocus;
        if (info.hwndActive) return info.hwndActive;
    }
    return GetForegroundWindow();
}

}  // namespace

Foreground inspectForeground() {
    Foreground f;
    f.top = GetForegroundWindow();
    f.focus = currentFocus();
    if (f.focus) f.thread = GetWindowThreadProcessId(f.focus, &f.process);
    f.exe = processName(f.process);
    f.topClass = windowClass(f.top);
    f.focusClass = windowClass(f.focus);
    f.layout = f.thread ? GetKeyboardLayout(f.thread) : nullptr;
    if (!f.layout && f.top) {
        // Console windows report the client's thread, whose layout is 0; the IME window of the
        // console belongs to conhost and knows the real one.
        if (HWND ime = ImmGetDefaultIMEWnd(f.top)) f.layout = GetKeyboardLayout(GetWindowThreadProcessId(ime, nullptr));
    }
    return f;
}

std::optional<std::wstring> skipReason(const Foreground& f) {
    if (!f.top || !f.focus) return L"no foreground window";
    for (std::wstring_view name : kSkipClasses) {
        if (f.topClass == name || f.focusClass == name) return L"terminal or remote window (" + std::wstring(name) + L")";
    }
    for (std::wstring_view name : kSkipProcesses) {
        if (f.exe == name) return L"terminal, remote desktop or virtual machine";
    }
    if (IsHungAppWindow(f.top)) return L"the window is not responding";
    QUERY_USER_NOTIFICATION_STATE state;
    if (SUCCEEDED(SHQueryUserNotificationState(&state)) && state == QUNS_RUNNING_D3D_FULL_SCREEN) return L"full-screen game";
    if (isElevatedAboveUs(f.process)) return L"administrator window: Windows does not let Klats send keys there";
    return std::nullopt;
}

bool focusUnchanged(const Foreground& f) { return GetForegroundWindow() == f.top && currentFocus() == f.focus; }

bool runningElevated() { return ownIntegrity() >= SECURITY_MANDATORY_HIGH_RID; }

bool isElevatedAboveUs(DWORD processId) {
    if (processId == GetCurrentProcessId()) return false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return true;
    bool above = true;
    HANDLE token = nullptr;
    if (OpenProcessToken(process, TOKEN_QUERY, &token)) {
        DWORD level = integrityLevel(token);
        above = level == 0 || level > ownIntegrity();
        CloseHandle(token);
    }
    CloseHandle(process);
    return above;
}

}  // namespace klats::app
