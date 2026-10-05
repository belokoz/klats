#include "app.h"

#include "common.h"
#include "foreground.h"
#include "hook_thread.h"
#include "hotkey_display.h"
#include "input_sources.h"
#include "log.h"
#include "pipeline.h"
#include "settings_store.h"
#include "strings.h"
#include "tray_icon.h"
#include "version.h"

#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <wtsapi32.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cwchar>

namespace klats::app {

namespace {

const wchar_t* kOwnerClass = L"KlatsTrayOwner";
const wchar_t* kMutexName = L"Local\\io.github.belokoz.klats";
const wchar_t* kShowSettingsMessage = L"Klats.ShowSettings";
const wchar_t* kRepository = L"https://github.com/belokoz/klats";

constexpr UINT_PTR kTrayRetryTimer = 1;
constexpr int kTrayRetries = 30;  // at logon the taskbar may take a while to appear

enum MenuId : UINT {
    kMenuLayout = 1,
    kMenuCase,
    kMenuPause,
    kMenuSettings,
    kMenuAbout,
    kMenuLog,
    kMenuExit,
};

std::wstring windowsVersion() {
    wchar_t display[64] = L"", build[32] = L"";
    DWORD revision = 0;
    DWORD size = sizeof display;
    const wchar_t* key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    RegGetValueW(HKEY_LOCAL_MACHINE, key, L"DisplayVersion", RRF_RT_REG_SZ, nullptr, display, &size);
    size = sizeof build;
    RegGetValueW(HKEY_LOCAL_MACHINE, key, L"CurrentBuildNumber", RRF_RT_REG_SZ, nullptr, build, &size);
    size = sizeof revision;
    RegGetValueW(HKEY_LOCAL_MACHINE, key, L"UBR", RRF_RT_REG_DWORD, nullptr, &revision, &size);
    return std::wstring(display) + L" (build " + build + L"." + std::to_wstring(revision) + L")";
}

std::wstring exePath() {
    wchar_t path[MAX_PATH * 2];
    DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    return std::wstring(path, length);
}

bool isShellWindow(HWND window) {
    std::wstring name = windowClass(window);
    return name == L"Shell_TrayWnd" || name == L"Shell_SecondaryTrayWnd" || name == L"NotifyIconOverflowWindow" ||
           name == L"TopLevelWindowForOverflowXamlIsland";
}

HRESULT CALLBACK aboutCallback(HWND, UINT notification, WPARAM, LPARAM lParam, LONG_PTR) {
    if (notification == TDN_HYPERLINK_CLICKED) {
        ShellExecuteW(nullptr, L"open", reinterpret_cast<const wchar_t*>(lParam), nullptr, nullptr, SW_SHOWNORMAL);
    }
    return S_OK;
}

class App {
public:
    App(HINSTANCE instance, bool installHook) : instance_(instance), installHook_(installHook), pipeline_(settings_) {}
    int run();

private:
    static LRESULT CALLBACK ownerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static void CALLBACK foregroundChanged(HWINEVENTHOOK, DWORD, HWND window, LONG, LONG, DWORD, DWORD);
    LRESULT handle(UINT message, WPARAM wParam, LPARAM lParam);

    void configureHotkeys();
    void showMenu(POINT anchor, bool extended);
    void command(UINT id, HWND target);
    void setPaused(bool paused);
    void refreshIcon();
    void showAbout();
    void showMessage(const wchar_t* title, const wchar_t* text);
    void shutdown();

    HINSTANCE instance_;
    bool installHook_;
    SettingsStore settings_;
    Pipeline pipeline_;
    HookThread hooks_;
    TrayIcon tray_;
    HWND owner_ = nullptr;
    UINT taskbarCreated_ = 0;
    UINT showSettings_ = 0;
    HWINEVENTHOOK foregroundHook_ = nullptr;
    HPOWERNOTIFY displayNotification_ = nullptr;
    HWND lastForeground_ = nullptr;  // the user's window before the tray menu took the focus
    int displayState_ = -1;          // 0 off, 1 on, 2 dimmed; -1 until Windows first reports it
    int trayRetries_ = 0;
    bool shutDown_ = false;
};

App* g_app = nullptr;

int App::run() {
    log::open();
    std::wstring layouts;
    for (const auto& layout : enabledLayouts()) layouts += (layouts.empty() ? L"" : L", ") + layout.id;
    log::write(L"---- Klats " KLATS_VERSION_WSTRING L" started from " + exePath() + L" on Windows " + windowsVersion() +
               L"; layouts: " + layouts + L"; rights: " + (runningElevated() ? L"administrator" : L"standard"));

    if (!pipeline_.start()) {
        log::write(L"pipeline thread could not start");
        return 1;
    }

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = ownerProc;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = kOwnerClass;
    windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(1));
    RegisterClassW(&windowClass);
    // A hidden top-level window, not a message-only one: only top-level windows hear TaskbarCreated
    // and WM_SETTINGCHANGE, and the installer's Restart Manager can only close those politely.
    owner_ = CreateWindowExW(WS_EX_TOOLWINDOW, kOwnerClass, L"Klats", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance_, nullptr);
    if (!owner_) return 1;
    taskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");
    showSettings_ = RegisterWindowMessageW(kShowSettingsMessage);
    ChangeWindowMessageFilterEx(owner_, taskbarCreated_, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(owner_, showSettings_, MSGFLT_ALLOW, nullptr);

    if (!hooks_.start(pipeline_.window(), owner_, installHook_)) log::write(L"hotkeys are off: the keyboard hook is not installed");
    else if (installHook_) log::write(L"keyboard hook is up, hotkeys are live");
    configureHotkeys();

    refreshIcon();
    if (!tray_.shown()) SetTimer(owner_, kTrayRetryTimer, 2000, nullptr);
    foregroundHook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, foregroundChanged, 0, 0,
                                      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    WTSRegisterSessionNotification(owner_, NOTIFY_FOR_THIS_SESSION);
    displayNotification_ = RegisterPowerSettingNotification(owner_, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    shutdown();
    return static_cast<int>(message.wParam);
}

void App::configureHotkeys() {
    Settings settings = settings_.snapshot();
    std::vector<Binding> bindings;
    if (settings.layoutHotkey) bindings.push_back({Action::Layout, *settings.layoutHotkey});
    if (settings.caseHotkey) bindings.push_back({Action::Case, *settings.caseHotkey});
    hooks_.configure(bindings);
    hooks_.setPaused(settings.paused);
}

void App::refreshIcon() {
    bool paused = settings_.snapshot().paused;
    tray_.add(owner_, WM_KLATS_TRAY, paused, paused ? tr(L"Клац — приостановлен") : tr(L"Клац"));
}

void App::setPaused(bool paused) {
    settings_.update([&](Settings& settings) { settings.paused = paused; });
    hooks_.setPaused(paused);
    refreshIcon();
    log::write(paused ? L"paused" : L"resumed");
}

void App::showMenu(POINT anchor, bool extended) {
    Settings settings = settings_.snapshot();
    HWND target = lastForeground_ && IsWindow(lastForeground_) ? lastForeground_ : nullptr;
    HMENU menu = CreatePopupMenu();

    const wchar_t* header = nullptr;
    if (installHook_ && !hooks_.hookInstalled()) {
        header = tr(L"Клац не может следить за клавиатурой");
    } else if (!settings.paused && target) {
        DWORD process = 0;
        GetWindowThreadProcessId(target, &process);
        if (isElevatedAboveUs(process)) header = tr(L"Окно администратора: здесь Клац не работает");
    }
    if (header) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, header);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    if (!settings.paused) {
        auto item = [&](UINT id, const wchar_t* title, const std::optional<Hotkey>& hotkey) {
            std::wstring text = title;
            if (hotkey) text += L"\t" + displayString(*hotkey);
            AppendMenuW(menu, MF_STRING, id, text.c_str());
        };
        item(kMenuLayout, tr(L"Сменить раскладку выделенного"), settings.layoutHotkey);
        item(kMenuCase, tr(L"Сменить регистр выделенного"), settings.caseHotkey);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuW(menu, MF_STRING | (settings.paused ? MF_CHECKED : MF_UNCHECKED), kMenuPause, tr(L"Приостановить"));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuSettings, tr(L"Параметры…"));
    SetMenuDefaultItem(menu, kMenuSettings, FALSE);
    AppendMenuW(menu, MF_STRING, kMenuAbout, tr(L"О программе"));
    if (extended) AppendMenuW(menu, MF_STRING, kMenuLog, tr(L"Открыть журнал"));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, tr(L"Выход"));

    // Without the owner in front the menu would not close on a click elsewhere; the WM_NULL
    // afterwards keeps the next opening from flickering shut.
    SetForegroundWindow(owner_);
    UINT alignment = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    UINT chosen = static_cast<UINT>(TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | alignment,
                                                     anchor.x, anchor.y, owner_, nullptr));
    PostMessageW(owner_, WM_NULL, 0, 0);
    DestroyMenu(menu);
    if (chosen) command(chosen, target);
}

void App::command(UINT id, HWND target) {
    switch (id) {
    case kMenuLayout:
    case kMenuCase:
        // The menu took the focus from the user's window: give it back before the copy keys go.
        if (target) SetForegroundWindow(target);
        pipeline_.request(id == kMenuLayout ? Action::Layout : Action::Case, true);
        break;
    case kMenuPause:
        setPaused(!settings_.snapshot().paused);
        break;
    case kMenuSettings:
        showMessage(tr(L"Клац"), tr(L"Окно параметров появится в следующей сборке."));
        break;
    case kMenuAbout:
        showAbout();
        break;
    case kMenuLog:
        ShellExecuteW(nullptr, L"open", log::filePath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case kMenuExit:
        DestroyWindow(owner_);
        break;
    default:
        break;
    }
}

void App::showAbout() {
    std::wstring content = std::wstring(tr(L"Версия")) + L" " KLATS_VERSION_WSTRING L"\n" +
                           tr(L"Исправляет раскладку выделенного текста одним нажатием.") + L"\n\n<a href=\"" + kRepository +
                           L"\">github.com/belokoz/klats</a>\n<a href=\"https://diktuy.ru/?utm_source=klats&utm_medium=app&utm_campaign=about\">" +
                           (interfaceIsRussian() ? L"Диктуй: голос в текст" : L"Diktuy: voice to text") + L"</a>\n\nMIT License";
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof config;
    config.hInstance = instance_;
    config.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    config.pszWindowTitle = tr(L"О программе");
    config.pszMainIcon = MAKEINTRESOURCEW(1);
    config.pszMainInstruction = tr(L"Клац");
    config.pszContent = content.c_str();
    config.dwCommonButtons = TDCBF_OK_BUTTON;
    config.pfCallback = aboutCallback;
    SetForegroundWindow(owner_);
    TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
}

void App::showMessage(const wchar_t* title, const wchar_t* text) {
    SetForegroundWindow(owner_);
    TaskDialog(nullptr, instance_, title, title, text, TDCBF_OK_BUTTON, MAKEINTRESOURCEW(1), nullptr);
}

void App::shutdown() {
    if (shutDown_) return;
    shutDown_ = true;
    // The pipeline first: a conversion in flight puts the user's clipboard back before anything else.
    hooks_.stop();
    pipeline_.stop();
    tray_.remove();
    if (foregroundHook_) UnhookWinEvent(foregroundHook_);
    if (displayNotification_) UnregisterPowerSettingNotification(displayNotification_);
    if (owner_) WTSUnRegisterSessionNotification(owner_);
    log::write(L"exited");
}

void CALLBACK App::foregroundChanged(HWINEVENTHOOK, DWORD, HWND window, LONG, LONG, DWORD, DWORD) {
    if (g_app && window && !isShellWindow(window)) g_app->lastForeground_ = window;
}

LRESULT CALLBACK App::ownerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (g_app && (g_app->owner_ == window || !g_app->owner_)) {
        if (!g_app->owner_) g_app->owner_ = window;
        return g_app->handle(message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT App::handle(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == taskbarCreated_ && taskbarCreated_) {
        // Explorer restarted, or the primary display changed its DPI: the icon has to be added again.
        refreshIcon();
        return 0;
    }
    if (message == showSettings_ && showSettings_) {
        showMessage(tr(L"Клац уже работает"), tr(L"Его значок — в области уведомлений, рядом с часами. Если его не видно, нажмите стрелку ^."));
        return 0;
    }
    switch (message) {
    case WM_KLATS_TRAY:
        switch (LOWORD(lParam)) {
        case NIN_SELECT:
        case NIN_KEYSELECT:
        case WM_CONTEXTMENU: {
            POINT anchor{GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)};
            showMenu(anchor, LOWORD(lParam) != NIN_KEYSELECT && (GetKeyState(VK_SHIFT) & 0x8000));
            break;
        }
        default:
            break;
        }
        return 0;
    case WM_KLATS_HOOK_FAILED:
        refreshIcon();
        return 0;
    case WM_TIMER:
        if (wParam == kTrayRetryTimer) {
            refreshIcon();
            if (tray_.shown() || ++trayRetries_ >= kTrayRetries) KillTimer(owner_, kTrayRetryTimer);
        }
        return 0;
    case WM_SETTINGCHANGE:
        if (lParam && std::wcscmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0) refreshIcon();
        return 0;
    case WM_DISPLAYCHANGE:
        refreshIcon();
        return 0;
    case WM_WTSSESSION_CHANGE:
        if (wParam == WTS_SESSION_LOCK) hooks_.reset();
        if (wParam == WTS_SESSION_UNLOCK) hooks_.reinstall();
        return 0;
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC) hooks_.reinstall();
        if (wParam == PBT_POWERSETTINGCHANGE) {
            // Windows reports the current state right after registering, so only a change from «off»
            // to «on» (Modern Standby wake-up) counts.
            auto* setting = reinterpret_cast<const POWERBROADCAST_SETTING*>(lParam);
            if (setting && setting->PowerSetting == GUID_CONSOLE_DISPLAY_STATE && setting->DataLength >= sizeof(DWORD)) {
                int state = static_cast<int>(*reinterpret_cast<const DWORD*>(setting->Data));
                if (displayState_ == 0 && state == 1) hooks_.reinstall();
                displayState_ = state;
            }
        }
        return TRUE;
    case WM_QUERYENDSESSION:
        return TRUE;
    case WM_ENDSESSION:
        if (wParam) shutdown();
        return 0;
    case WM_CLOSE:
        DestroyWindow(owner_);
        return 0;
    case WM_DESTROY:
        shutdown();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(owner_, message, wParam, lParam);
    }
}

}  // namespace

int run(HINSTANCE instance, const std::wstring& commandLine) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Klats is already running: let it show itself, then step aside.
        if (HWND other = FindWindowW(kOwnerClass, nullptr)) {
            DWORD process = 0;
            GetWindowThreadProcessId(other, &process);
            AllowSetForegroundWindow(process);
            PostMessageW(other, RegisterWindowMessageW(kShowSettingsMessage), 0, 0);
        }
        if (mutex) CloseHandle(mutex);
        return 0;
    }

    INITCOMMONCONTROLSEX controls{sizeof controls, ICC_STANDARD_CLASSES | ICC_LINK_CLASS};
    InitCommonControlsEx(&controls);
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplus = 0;
    Gdiplus::GdiplusStartup(&gdiplus, &gdiplusInput, nullptr);

    bool installHook = commandLine.find(L"--no-hook") == std::wstring::npos;
    int result = 0;
    {
        App app(instance, installHook);
        g_app = &app;
        result = app.run();
        g_app = nullptr;
    }
    Gdiplus::GdiplusShutdown(gdiplus);
    if (mutex) CloseHandle(mutex);
    return result;
}

}  // namespace klats::app
