#include "app.h"

#include "about_window.h"
#include "common.h"
#include "foreground.h"
#include "hook_thread.h"
#include "hotkey_display.h"
#include "input_sources.h"
#include "log.h"
#include "login_item.h"
#include "onboarding_window.h"
#include "pipeline.h"
#include "settings_store.h"
#include "settings_window.h"
#include "strings.h"
#include "tray_icon.h"
#include "update_checker.h"
#include "version.h"

#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <wtsapi32.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cwchar>
#include <memory>

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
    kMenuChoosePair,
    kMenuPause,
    kMenuSettings,
    kMenuAbout,
    kMenuLog,
    kMenuExit,
};

struct Options {
    bool installHook = true;               // --no-hook: for a debugger, which would freeze all input
    bool startedAtLogin = false;           // --autostart: started from the Run key
    std::vector<std::wstring> debugShow;   // --debug-show settings,onboarding,about,menu,update
    std::optional<AppVersion> pretendVersion;  // --debug-pretend-version 0.0.1
};

Options parseOptions(const std::wstring& commandLine) {
    Options options;
    int count = 0;
    // The first argument is parsed as a program path: give it one, so the real ones parse normally.
    std::wstring line = L"Klats.exe " + commandLine;
    LPWSTR* arguments = CommandLineToArgvW(line.c_str(), &count);
    if (!arguments) return options;
    for (int i = 1; i < count; ++i) {
        std::wstring argument = arguments[i];
        if (argument == L"--no-hook") {
            options.installHook = false;
        } else if (argument == L"--autostart") {
            options.startedAtLogin = true;
        } else if (argument == L"--debug-show" && i + 1 < count) {
            std::wstring list = arguments[++i];
            for (size_t start = 0; start <= list.size();) {
                size_t comma = list.find(L',', start);
                if (comma == std::wstring::npos) comma = list.size();
                if (comma > start) options.debugShow.push_back(list.substr(start, comma - start));
                start = comma + 1;
            }
        } else if (argument == L"--debug-pretend-version" && i + 1 < count) {
            options.pretendVersion = AppVersion::parse(arguments[++i]);
        }
    }
    LocalFree(arguments);
    return options;
}

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

// The tray window of a Klats that is already running. The mutex alone proves nothing: any program
// in the session may create that name first, or hold it open after Klats exits. A Klats started a
// moment earlier may not have its window yet, so the search waits a little.
HWND runningKlats() {
    std::wstring ownName = processName(GetCurrentProcessId());
    for (int attempt = 0; attempt < 30; ++attempt) {
        if (HWND window = FindWindowW(kOwnerClass, nullptr)) {
            DWORD process = 0;
            GetWindowThreadProcessId(window, &process);
            return processName(process) == ownName ? window : nullptr;
        }
        Sleep(100);
    }
    return nullptr;
}

class App {
public:
    App(HINSTANCE instance, Options options)
        : instance_(instance),
          options_(std::move(options)),
          pipeline_(settings_),
          settingsWindow_(instance, settings_, hooks_, [this] { configureHotkeys(); }),
          onboarding_(instance, settings_) {}
    int run();

private:
    static LRESULT CALLBACK ownerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static void CALLBACK foregroundChanged(HWINEVENTHOOK, DWORD, HWND window, LONG, LONG, DWORD, DWORD);
    LRESULT handle(UINT message, WPARAM wParam, LPARAM lParam);

    void configureHotkeys();
    void showMenu(POINT anchor, bool extended);
    POINT trayAnchor() const;
    void command(UINT id, HWND target);
    void setPaused(bool paused);
    void refreshIcon();
    void showDebugWindows();
    AppVersion currentVersion() const;
    void shutdown();

    HINSTANCE instance_;
    Options options_;
    SettingsStore settings_;
    Pipeline pipeline_;
    HookThread hooks_;
    TrayIcon tray_;
    SettingsWindow settingsWindow_;
    OnboardingWindow onboarding_;
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
    std::wstring layouts;
    for (const auto& layout : enabledLayouts()) layouts += (layouts.empty() ? L"" : L", ") + layout.id;
    log::write(L"---- Klats " KLATS_VERSION_WSTRING L" started" + std::wstring(options_.startedAtLogin ? L" at login" : L"") +
               L" from " + exePath() + L" on Windows " + windowsVersion() + L"; layouts: " + layouts + L"; rights: " +
               (runningElevated() ? L"administrator" : L"standard"));
    login_item::refreshLocation(options_.startedAtLogin);

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

    if (!hooks_.start(pipeline_.window(), owner_, options_.installHook)) log::write(L"hotkeys are off: the keyboard hook is not installed");
    else if (options_.installHook) log::write(L"keyboard hook is up, hotkeys are live");
    configureHotkeys();

    refreshIcon();
    if (!tray_.shown()) SetTimer(owner_, kTrayRetryTimer, 2000, nullptr);
    foregroundHook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, foregroundChanged, 0, 0,
                                      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    WTSRegisterSessionNotification(owner_, NOTIFY_FOR_THIS_SESSION);
    displayNotification_ = RegisterPowerSettingNotification(owner_, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);

    Settings settings = settings_.snapshot();
    if (!settings.onboardingCompleted) onboarding_.show();
    if (!options_.debugShow.empty()) {
        showDebugWindows();
    } else if (settings.checkForUpdates && settings.onboardingCompleted) {
        updates::check(owner_, currentVersion());
    }

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        // Tab, arrows, Enter and Esc in the modeless windows.
        if (settingsWindow_.window() && IsDialogMessageW(settingsWindow_.window(), &message)) continue;
        if (onboarding_.window() && IsDialogMessageW(onboarding_.window(), &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    shutdown();
    return static_cast<int>(message.wParam);
}

AppVersion currentVersionOf(const std::optional<AppVersion>& pretended) {
    if (pretended) return *pretended;
    return AppVersion::parse(KLATS_VERSION_WSTRING).value_or(AppVersion());
}

AppVersion App::currentVersion() const { return currentVersionOf(options_.pretendVersion); }

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
    if (options_.installHook && !hooks_.hookInstalled()) {
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
        // With three or more layouts and no pair chosen, the layout hotkey does nothing: say so and
        // lead to the choice. Fewer than three loaded layouts cannot need it, and the full list
        // takes Windows tens of milliseconds to read.
        if (GetKeyboardLayoutList(0, nullptr) > 2 && readLayouts(settings.layoutPair).pair.problem == PairChoice::Problem::Ambiguous) {
            AppendMenuW(menu, MF_STRING, kMenuChoosePair, tr(L"Выберите пару раскладок…"));
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        }
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
    // A menu closed with Esc, or one that opened no window, would leave the keyboard on Klats's
    // invisible window. Not after the actions: they give the focus back to the user's window.
    if ((chosen == 0 || chosen == kMenuPause) && GetForegroundWindow() == owner_) tray_.focus();
}

// Where the menu opens without a click: at the icon, or at the mouse when the icon is hidden.
POINT App::trayAnchor() const {
    NOTIFYICONIDENTIFIER icon{sizeof icon};
    icon.hWnd = owner_;
    icon.uID = 1;
    RECT bounds;
    if (SUCCEEDED(Shell_NotifyIconGetRect(&icon, &bounds))) return {(bounds.left + bounds.right) / 2, bounds.top};
    POINT cursor{};
    GetCursorPos(&cursor);
    return cursor;
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
    case kMenuChoosePair:
    case kMenuSettings:
        settingsWindow_.show();
        break;
    case kMenuAbout:
        showAbout(instance_);
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

// `--debug-show settings,onboarding,about,menu,update` opens windows straight away, for screenshots.
void App::showDebugWindows() {
    for (const std::wstring& name : options_.debugShow) {
        if (name == L"settings") {
            settingsWindow_.show();
        } else if (name == L"onboarding") {
            onboarding_.show();
        } else if (name == L"about") {
            showAbout(instance_);
        } else if (name == L"menu") {
            showMenu(trayAnchor(), false);
        } else if (name == L"update") {
            std::wstring next = std::to_wstring(KLATS_VERSION_MAJOR) + L"." + std::to_wstring(KLATS_VERSION_MINOR) + L"." +
                                std::to_wstring(KLATS_VERSION_PATCH + 1);
            WindowsRelease release{AppVersion::parse(next).value_or(AppVersion()), std::wstring(kRepository) + L"/releases"};
            updates::offer(owner_, instance_, release, currentVersion());
        }
    }
}

void App::shutdown() {
    if (shutDown_) return;
    shutDown_ = true;
    // The pipeline first: a conversion in flight puts the user's clipboard back before anything else.
    pipeline_.stop();
    hooks_.stop();
    if (settingsWindow_.window()) DestroyWindow(settingsWindow_.window());
    if (onboarding_.window()) DestroyWindow(onboarding_.window());
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
        // Klats was started a second time: the user is looking for it.
        settingsWindow_.show();
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
    case WM_KLATS_HOOK_STATUS:
        switch (static_cast<HookStatus>(wParam)) {
        case HookStatus::Failed:
            log::write(L"keyboard hook could not be installed, error " + std::to_wstring(lParam));
            break;
        case HookStatus::Reinstalled:
            log::write(L"keyboard hook reinstalled");
            break;
        case HookStatus::ReinstalledAfterRemoval:
            log::write(L"Windows had removed the keyboard hook; reinstalled");
            break;
        }
        return 0;
    case WM_KLATS_UPDATE: {
        std::unique_ptr<WindowsRelease> release(reinterpret_cast<WindowsRelease*>(lParam));
        if (release) updates::offer(owner_, instance_, *release, currentVersion());
        return 0;
    }
    case WM_TIMER:
        if (wParam == kTrayRetryTimer) {
            refreshIcon();
            if (tray_.shown() || ++trayRetries_ >= kTrayRetries) KillTimer(owner_, kTrayRetryTimer);
        }
        return 0;
    case WM_SETTINGCHANGE:
        if (wParam == SPI_SETHIGHCONTRAST ||
            (lParam && std::wcscmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0)) {
            refreshIcon();
        }
        return 0;
    case WM_SYSCOLORCHANGE:  // a contrast theme turned on, off, or changed its colours
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
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
        // Windows signs out, or an installer's Restart Manager asks Klats to make way.
        if (wParam) {
            shutdown();
            PostQuitMessage(0);
        }
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
    log::open();
    Options options = parseOptions(commandLine);
    HANDLE mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND other = runningKlats()) {
            // Klats is already running: let it show itself, then step aside.
            DWORD process = 0;
            GetWindowThreadProcessId(other, &process);
            AllowSetForegroundWindow(process);
            PostMessageW(other, RegisterWindowMessageW(kShowSettingsMessage), 0, 0);
            if (mutex) CloseHandle(mutex);
            return 0;
        }
        log::write(L"another program holds the name Klats uses to run once; starting anyway");
    }

    INITCOMMONCONTROLSEX controls{sizeof controls, ICC_STANDARD_CLASSES | ICC_LINK_CLASS};
    InitCommonControlsEx(&controls);
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplus = 0;
    Gdiplus::GdiplusStartup(&gdiplus, &gdiplusInput, nullptr);

    int result = 0;
    {
        App app(instance, std::move(options));
        g_app = &app;
        result = app.run();
        g_app = nullptr;
    }
    Gdiplus::GdiplusShutdown(gdiplus);
    if (mutex) CloseHandle(mutex);
    return result;
}

}  // namespace klats::app
