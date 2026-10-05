#pragma once
#include <windows.h>

namespace klats::app {

// The icon in the notification area: a keycap outline with a K, white on a dark taskbar and dark on
// a light one, drawn for the exact size the taskbar uses.
//
// Identified by window and id, not by GUID: Windows binds an icon GUID to the path of an unsigned
// exe, and the icon would vanish once Klats moves.
class TrayIcon {
public:
    ~TrayIcon() { remove(); }

    bool add(HWND owner, UINT callbackMessage, bool paused, const wchar_t* tooltip);
    // Redraws after a theme, DPI or pause change. Also re-adds the icon after Explorer restarted.
    void refresh(bool paused, const wchar_t* tooltip);
    void remove();
    bool shown() const { return shown_; }

private:
    bool push(DWORD message);

    HWND owner_ = nullptr;
    UINT callbackMessage_ = 0;
    HICON icon_ = nullptr;
    bool shown_ = false;
    wchar_t tooltip_[128] = {};
};

// Whether the taskbar uses the light theme (Settings → Personalisation → Colours → Windows mode).
bool taskbarIsLight();

}  // namespace klats::app
