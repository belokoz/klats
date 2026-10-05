#include "links.h"

#include <commctrl.h>
#include <shellapi.h>

namespace klats::app {

bool drawPlainLink(HWND dialog, LPARAM lParam) {
    auto* header = reinterpret_cast<const NMHDR*>(lParam);
    if (header->code != NM_CUSTOMTEXT) return false;
    auto* text = reinterpret_cast<const NMCUSTOMTEXT*>(lParam);
    if (text->fLink) {
        SelectObject(text->hDC, reinterpret_cast<HFONT>(SendMessageW(dialog, WM_GETFONT, 0, 0)));
        SetTextColor(text->hDC, GetSysColor(COLOR_HOTLIGHT));
    }
    return true;
}

void openLink(const wchar_t* url) { ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL); }

}  // namespace klats::app
