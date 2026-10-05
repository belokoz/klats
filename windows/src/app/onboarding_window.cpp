#include "onboarding_window.h"

#include "hotkey_display.h"
#include "log.h"
#include "login_item.h"
#include "strings.h"

#include "../../res/resource.h"

#include <shellapi.h>

#include <string>

namespace klats::app {

namespace {

OnboardingWindow* g_window = nullptr;

void setText(HWND dialog, int id, const std::wstring& text) { SetDlgItemTextW(dialog, id, text.c_str()); }

HFONT derivedFont(HWND dialog, int weight, int percent) {
    LOGFONTW font{};
    GetObjectW(reinterpret_cast<HFONT>(SendMessageW(dialog, WM_GETFONT, 0, 0)), sizeof font, &font);
    font.lfWeight = weight;
    font.lfHeight = MulDiv(font.lfHeight, percent, 100);
    return CreateFontIndirectW(&font);
}

}  // namespace

void OnboardingWindow::show() {
    if (!window_) {
        g_window = this;
        window_ = CreateDialogParamW(instance_, MAKEINTRESOURCEW(IDD_ONBOARDING), nullptr, dialogProc, 0);
        if (!window_) {
            log::write(L"first-run window could not be created, error " + std::to_wstring(GetLastError()));
            return;
        }
    }
    ShowWindow(window_, SW_SHOWNORMAL);
    SetForegroundWindow(window_);
}

INT_PTR CALLBACK OnboardingWindow::dialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!g_window) return FALSE;
    if (message == WM_INITDIALOG) g_window->window_ = window;
    if (g_window->window_ != window) return FALSE;
    return g_window->handle(message, wParam, lParam);
}

void OnboardingWindow::initialize() {
    HWND dialog = window_;
    SetWindowTextW(dialog, tr(L"Добро пожаловать в Клац"));
    UINT dpi = GetDpiForWindow(dialog);
    int iconSize = MulDiv(48, static_cast<int>(dpi), 96);
    icon_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, iconSize, iconSize, 0));
    SendDlgItemMessageW(dialog, IDC_OB_ICON, STM_SETICON, reinterpret_cast<WPARAM>(icon_), 0);
    SendMessageW(dialog, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon_));

    titleFont_ = derivedFont(dialog, FW_BOLD, 160);
    headerFont_ = derivedFont(dialog, FW_SEMIBOLD, 100);
    SendDlgItemMessageW(dialog, IDC_OB_TITLE, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont_), FALSE);
    SendDlgItemMessageW(dialog, IDC_OB_TRY_HEADER, WM_SETFONT, reinterpret_cast<WPARAM>(headerFont_), FALSE);
    SendDlgItemMessageW(dialog, IDC_OB_WHERE_HEADER, WM_SETFONT, reinterpret_cast<WPARAM>(headerFont_), FALSE);

    Settings settings = settings_.snapshot();
    std::wstring chord = settings.layoutHotkey ? displayString(*settings.layoutHotkey) : displayString(defaults::layoutHotkey);
    setText(dialog, IDC_OB_TITLE, tr(L"Клац"));
    setText(dialog, IDC_OB_SUBTITLE, tr(L"Исправляет раскладку выделенного текста одним нажатием."));
    setText(dialog, IDC_OB_TRY_HEADER, tr(L"Попробуйте"));
    setText(dialog, IDC_OB_TRY_TEXT, tr(L"Выделите текст ниже, нажмите {} и отпустите.", chord));
    setText(dialog, IDC_OB_PROBE, L"ghbdtn vbh");
    setText(dialog, IDC_OB_TRY_RESULT, tr(L"Должно получиться «привет мир», а раскладка переключится на русскую."));
    setText(dialog, IDC_OB_WHERE_HEADER, tr(L"Где живёт Клац"));
    setText(dialog, IDC_OB_WHERE_TEXT,
            tr(L"Значок Клаца — в области уведомлений, рядом с часами. Если его не видно, нажмите стрелку ^ и перетащите "
               L"значок на панель задач: так он всегда будет под рукой."));
    setText(dialog, IDC_OB_TASKBAR, tr(L"Параметры панели задач"));
    setText(dialog, IDC_OB_AUTOSTART, tr(L"Запускать Клац при входе в Windows"));
    setText(dialog, IDC_OB_ADMIN_NOTE, tr(L"В окнах программ, запущенных от имени администратора, Клац не работает: так Windows их защищает."));
    setText(dialog, IDOK, tr(L"Готово"));
    // On by default, as the design asks; it takes effect when the window closes.
    CheckDlgButton(dialog, IDC_OB_AUTOSTART, BST_CHECKED);

    HWND probe = GetDlgItem(dialog, IDC_OB_PROBE);
    SetFocus(probe);
    SendMessageW(probe, EM_SETSEL, 0, -1);
}

void OnboardingWindow::finish() {
    bool autostart = IsDlgButtonChecked(window_, IDC_OB_AUTOSTART) == BST_CHECKED;
    bool now = login_item::state() == login_item::State::On;
    if (autostart != now) login_item::setEnabled(autostart);
    settings_.update([](Settings& settings) { settings.onboardingCompleted = true; });
    log::write(L"first run finished");
    DestroyWindow(window_);
}

INT_PTR OnboardingWindow::handle(UINT message, WPARAM wParam, LPARAM) {
    switch (message) {
    case WM_INITDIALOG:
        initialize();
        return FALSE;  // the focus was set by hand, on the probe field
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_OB_TASKBAR:
            ShellExecuteW(nullptr, L"open", L"ms-settings:taskbar", nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        case IDOK:
        case IDCANCEL:
            finish();
            return TRUE;
        default:
            return FALSE;
        }
    case WM_DESTROY:
        if (titleFont_) DeleteObject(titleFont_);
        if (headerFont_) DeleteObject(headerFont_);
        if (icon_) DestroyIcon(icon_);
        titleFont_ = headerFont_ = nullptr;
        icon_ = nullptr;
        window_ = nullptr;
        return FALSE;
    default:
        return FALSE;
    }
}

}  // namespace klats::app
