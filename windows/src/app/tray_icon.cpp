#include "tray_icon.h"

#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <cwchar>

namespace klats::app {

namespace {

constexpr UINT kIconId = 1;

// The menu bar glyph of the Mac version, scaled from its 18-point canvas: a rounded keycap
// outline and a K made of three strokes.
HICON drawGlyph(int size, Gdiplus::Color ink) {
    using namespace Gdiplus;
    Bitmap bitmap(size, size, PixelFormat32bppARGB);
    {
        Graphics graphics(&bitmap);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
        graphics.Clear(Color(0, 0, 0, 0));

        float scale = size / 18.0f;
        // Thin strokes vanish at 16 px, so they never go below a pixel and a half.
        float frameWidth = std::max(1.5f * scale, 1.5f);
        float letterWidth = std::max(1.6f * scale, 1.6f);

        Pen frame(ink, frameWidth);
        float inset = 1.6f * scale;
        float radius = 4.2f * scale;
        float left = inset, top = inset, right = size - inset, bottom = size - inset;
        GraphicsPath cap;
        cap.AddArc(left, top, 2 * radius, 2 * radius, 180, 90);
        cap.AddArc(right - 2 * radius, top, 2 * radius, 2 * radius, 270, 90);
        cap.AddArc(right - 2 * radius, bottom - 2 * radius, 2 * radius, 2 * radius, 0, 90);
        cap.AddArc(left, bottom - 2 * radius, 2 * radius, 2 * radius, 90, 90);
        cap.CloseFigure();
        graphics.DrawPath(&frame, &cap);

        // The K's box (5.1, 5.0, 6.6 × 8.0 on the Mac's bottom-up canvas), flipped to top-down.
        float boxLeft = 5.1f * scale, boxWidth = 6.6f * scale;
        float boxTop = (18.0f - 5.0f - 8.0f) * scale, boxHeight = 8.0f * scale;
        float boxRight = boxLeft + boxWidth, boxBottom = boxTop + boxHeight;
        float spine = boxLeft + boxWidth * 0.18f;
        float middle = boxTop + boxHeight / 2;
        Pen letter(ink, letterWidth);
        letter.SetStartCap(LineCapRound);
        letter.SetEndCap(LineCapRound);
        letter.SetLineJoin(LineJoinRound);
        graphics.DrawLine(&letter, spine, boxTop, spine, boxBottom);
        graphics.DrawLine(&letter, spine, middle + boxHeight * 0.04f, boxRight, boxTop);
        graphics.DrawLine(&letter, spine + boxWidth * 0.2f, middle - boxHeight * 0.06f, boxRight, boxBottom);
    }
    HICON icon = nullptr;
    bitmap.GetHICON(&icon);
    return icon;
}

int trayIconSize() {
    // The taskbar draws notification icons at the small-icon size of its own monitor. Its window
    // knows the current scale; the system DPI stays as it was at sign-in.
    UINT dpi = 0;
    if (HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr)) dpi = GetDpiForWindow(taskbar);
    if (!dpi) dpi = GetDpiForSystem();
    return GetSystemMetricsForDpi(SM_CXSMICON, dpi);
}

bool highContrast() {
    HIGHCONTRASTW contrast{sizeof contrast};
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof contrast, &contrast, 0) && (contrast.dwFlags & HCF_HIGHCONTRASTON);
}

// White on a dark taskbar, dark on a light one, faded while paused. A contrast theme paints the
// taskbar in its own colours and leaves the light/dark setting as it was: the theme's text colour,
// never faded.
Gdiplus::Color glyphInk(bool paused) {
    if (highContrast()) {
        COLORREF color = GetSysColor(paused ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT);
        return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
    }
    BYTE alpha = paused ? 110 : 255;
    return taskbarIsLight() ? Gdiplus::Color(alpha, 0x1A, 0x1A, 0x1A) : Gdiplus::Color(alpha, 0xFF, 0xFF, 0xFF);
}

}  // namespace

bool taskbarIsLight() {
    DWORD value = 0;
    DWORD size = sizeof value;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return false;  // Windows 10 before 1903 had a dark taskbar only
    }
    return value != 0;
}

bool TrayIcon::push(DWORD message) {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof data;
    data.hWnd = owner_;
    data.uID = kIconId;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = callbackMessage_;
    data.hIcon = icon_;
    wcsncpy_s(data.szTip, tooltip_, _TRUNCATE);
    if (!Shell_NotifyIconW(message, &data)) return false;
    if (message == NIM_ADD) {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
    return true;
}

bool TrayIcon::add(HWND owner, UINT callbackMessage, bool paused, const wchar_t* tooltip) {
    owner_ = owner;
    callbackMessage_ = callbackMessage;
    refresh(paused, tooltip);
    return shown_;
}

void TrayIcon::refresh(bool paused, const wchar_t* tooltip) {
    if (!owner_) return;
    wcsncpy_s(tooltip_, tooltip, _TRUNCATE);
    HICON previous = icon_;
    icon_ = drawGlyph(trayIconSize(), glyphInk(paused));
    if (shown_ && push(NIM_MODIFY)) {
        // Updated in place.
    } else if (push(NIM_ADD)) {
        shown_ = true;
    } else if (push(NIM_MODIFY)) {
        // A quick restart can leave the old entry behind: NIM_ADD then fails, but the entry is
        // there and takes a modification.
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof data;
        data.hWnd = owner_;
        data.uID = kIconId;
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
        shown_ = true;
    } else {
        shown_ = false;  // the taskbar is not there yet; TaskbarCreated brings Klats back
    }
    if (previous) DestroyIcon(previous);
}

void TrayIcon::focus() {
    if (!owner_) return;
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof data;
    data.hWnd = owner_;
    data.uID = kIconId;
    Shell_NotifyIconW(NIM_SETFOCUS, &data);
}

void TrayIcon::remove() {
    if (owner_) {
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof data;
        data.hWnd = owner_;
        data.uID = kIconId;
        Shell_NotifyIconW(NIM_DELETE, &data);
    }
    shown_ = false;
    if (icon_) {
        DestroyIcon(icon_);
        icon_ = nullptr;
    }
}

}  // namespace klats::app
