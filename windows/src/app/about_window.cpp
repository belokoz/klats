#include "about_window.h"

#include "links.h"
#include "strings.h"
#include "version.h"

#include "../../res/resource.h"

#include <commctrl.h>

#include <string>

namespace klats::app {

namespace {

const wchar_t* kRepositoryLink = L"https://github.com/belokoz/klats";
const wchar_t* kDiktuyLink = L"https://diktuy.ru/?utm_source=klats&utm_medium=app&utm_campaign=about";

HWND g_about = nullptr;

struct AboutState {
    HINSTANCE instance = nullptr;
    HICON icon = nullptr;
    HFONT titleFont = nullptr;
};

void setText(HWND dialog, int id, const std::wstring& text) { SetDlgItemTextW(dialog, id, text.c_str()); }

void initialize(HWND dialog, AboutState& state) {
    SetWindowTextW(dialog, tr(L"О программе"));
    int iconSize = MulDiv(48, static_cast<int>(GetDpiForWindow(dialog)), 96);
    state.icon = static_cast<HICON>(LoadImageW(state.instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, iconSize, iconSize, 0));
    SendDlgItemMessageW(dialog, IDC_ABOUT_ICON, STM_SETICON, reinterpret_cast<WPARAM>(state.icon), 0);

    LOGFONTW font{};
    GetObjectW(reinterpret_cast<HFONT>(SendMessageW(dialog, WM_GETFONT, 0, 0)), sizeof font, &font);
    font.lfWeight = FW_BOLD;
    font.lfHeight = MulDiv(font.lfHeight, 160, 100);
    state.titleFont = CreateFontIndirectW(&font);
    SendDlgItemMessageW(dialog, IDC_ABOUT_TITLE, WM_SETFONT, reinterpret_cast<WPARAM>(state.titleFont), FALSE);

    setText(dialog, IDC_ABOUT_TITLE, tr(L"Клац"));
    setText(dialog, IDC_ABOUT_VERSION, std::wstring(tr(L"Версия")) + L" " KLATS_VERSION_WSTRING);
    setText(dialog, IDC_ABOUT_DESCRIPTION, tr(L"Исправляет раскладку выделенного текста одним нажатием."));
    setText(dialog, IDC_ABOUT_REPO, L"<a>github.com/belokoz/klats</a>");
    setText(dialog, IDC_ABOUT_DIKTUY, std::wstring(L"<a>") + tr(L"Диктуй: голос в текст") + L"</a>");
    setText(dialog, IDC_ABOUT_LICENSE, L"MIT License");
    setText(dialog, IDOK, tr(L"ОК"));
}

INT_PTR CALLBACK aboutProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<AboutState*>(GetWindowLongPtrW(dialog, DWLP_USER));
    switch (message) {
    case WM_INITDIALOG:
        SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        g_about = dialog;
        initialize(dialog, *reinterpret_cast<AboutState*>(lParam));
        // The focus goes to «ОК», not to the first link: Enter and Esc close the window.
        SetFocus(GetDlgItem(dialog, IDOK));
        return FALSE;
    case WM_NOTIFY: {
        if (drawPlainLink(dialog, lParam)) return FALSE;  // the link draws itself, with the plain font set
        auto* header = reinterpret_cast<const NMHDR*>(lParam);
        if ((header->code == NM_CLICK || header->code == NM_RETURN) &&
            (header->idFrom == IDC_ABOUT_REPO || header->idFrom == IDC_ABOUT_DIKTUY)) {
            openLink(header->idFrom == IDC_ABOUT_REPO ? kRepositoryLink : kDiktuyLink);
            return TRUE;
        }
        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) EndDialog(dialog, 0);
        return TRUE;
    case WM_DESTROY:
        if (state) {
            if (state->icon) DestroyIcon(state->icon);
            if (state->titleFont) DeleteObject(state->titleFont);
        }
        g_about = nullptr;
        return FALSE;
    default:
        return FALSE;
    }
}

}  // namespace

void showAbout(HINSTANCE instance) {
    if (g_about) {
        SetForegroundWindow(g_about);
        return;
    }
    AboutState state;
    state.instance = instance;
    DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_ABOUT), nullptr, aboutProc, reinterpret_cast<LPARAM>(&state));
}

}  // namespace klats::app
