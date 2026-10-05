#include "hotkey_recorder.h"

#include "hotkey_display.h"
#include "strings.h"

#include <vector>

namespace klats::app {

namespace {

bool isModifierKey(UINT vk) {
    switch (vk) {
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    case VK_MENU: case VK_LMENU: case VK_RMENU:
    case VK_LWIN: case VK_RWIN:
        return true;
    default:
        return false;
    }
}

COLORREF blend(COLORREF a, COLORREF b, int percentOfB) {
    auto mix = [&](int x, int y) { return static_cast<BYTE>((x * (100 - percentOfB) + y * percentOfB) / 100); };
    return RGB(mix(GetRValue(a), GetRValue(b)), mix(GetGValue(a), GetGValue(b)), mix(GetBValue(a), GetBValue(b)));
}

}  // namespace

void HotkeyRecorder::attach(HWND button, std::wstring label, std::optional<Hotkey> value) {
    button_ = button;
    label_ = std::move(label);
    value_ = value;
    refresh();
}

void HotkeyRecorder::setValue(std::optional<Hotkey> value) {
    value_ = value;
    refresh();
}

void HotkeyRecorder::startRecording() {
    recording_ = true;
    held_ = {};
    keyPressed_ = false;
    refresh();
}

void HotkeyRecorder::stopRecording() {
    recording_ = false;
    held_ = {};
    keyPressed_ = false;
    refresh();
}

HotkeyRecorder::Outcome HotkeyRecorder::key(UINT vk, bool pressed, Modifiers held, Hotkey* candidate) {
    if (isModifierKey(vk)) {
        if (held.empty()) {
            // Everything released. A chord needs at least two modifiers and no key in between.
            Outcome outcome = Outcome::None;
            if (!keyPressed_ && held_.count() >= 2) {
                *candidate = Hotkey{0, held_};
                outcome = Outcome::Candidate;
            }
            held_ = {};
            keyPressed_ = false;
            refresh();
            return outcome;
        }
        if (pressed) held_ = Modifiers::fromBits(held_.bits() | held.bits());
        refresh();
        return Outcome::None;
    }
    if (!pressed) return Outcome::None;
    keyPressed_ = true;
    if (vk == VK_ESCAPE) return Outcome::Cancelled;
    if (vk == VK_BACK || vk == VK_DELETE) return Outcome::Cleared;
    if (held.empty() || vk > 0xFE) return Outcome::None;  // a bare key would fire in the middle of typing
    *candidate = Hotkey{static_cast<uint8_t>(vk), held};
    return Outcome::Candidate;
}

void HotkeyRecorder::refresh() {
    if (!button_) return;
    // The window text is what screen readers announce for the button.
    std::wstring state = recording_ ? tr(L"запись сочетания") : value_ ? displayString(*value_) : std::wstring(tr(L"не задано"));
    SetWindowTextW(button_, (label_ + L": " + state).c_str());
    InvalidateRect(button_, nullptr, TRUE);
}

void HotkeyRecorder::draw(const DRAWITEMSTRUCT& item) const {
    HDC dc = item.hDC;
    RECT bounds = item.rcItem;
    UINT dpi = GetDpiForWindow(item.hwndItem);
    auto scaled = [&](int value) { return MulDiv(value, static_cast<int>(dpi), 96); };

    COLORREF window = GetSysColor(COLOR_WINDOW);
    COLORREF text = GetSysColor(COLOR_WINDOWTEXT);
    COLORREF gray = GetSysColor(COLOR_GRAYTEXT);
    HBRUSH background = CreateSolidBrush(window);
    FillRect(dc, &bounds, background);
    DeleteObject(background);

    int radius = scaled(7);
    HPEN pen = CreatePen(PS_SOLID, recording_ ? scaled(2) : 1, recording_ ? GetSysColor(COLOR_HIGHLIGHT) : blend(window, gray, 60));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom, radius, radius);

    HFONT font = reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0));
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);

    std::vector<std::wstring> caps;
    std::wstring placeholder;
    if (recording_) {
        if (held_.empty()) placeholder = tr(L"Введите сочетание…");
        else caps = keycaps(Hotkey{0, held_});
    } else if (value_) {
        caps = keycaps(*value_);
    } else {
        placeholder = tr(L"Не задано");
    }

    if (!placeholder.empty()) {
        SetTextColor(dc, gray);
        DrawTextW(dc, placeholder.c_str(), -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        // Keycaps: each label in a light rounded box, the row centred.
        int padding = scaled(5), gap = scaled(3);
        std::vector<SIZE> sizes;
        int total = 0;
        for (const auto& cap : caps) {
            SIZE size{};
            GetTextExtentPoint32W(dc, cap.c_str(), static_cast<int>(cap.size()), &size);
            sizes.push_back(size);
            total += size.cx + 2 * padding;
        }
        total += gap * static_cast<int>(caps.empty() ? 0 : caps.size() - 1);
        int x = bounds.left + (bounds.right - bounds.left - total) / 2;
        int height = bounds.bottom - bounds.top;
        int capHeight = height - scaled(8);
        int top = bounds.top + (height - capHeight) / 2;
        HBRUSH capFill = CreateSolidBrush(blend(window, gray, 18));
        HPEN noPen = static_cast<HPEN>(GetStockObject(NULL_PEN));
        SelectObject(dc, noPen);
        SelectObject(dc, capFill);
        SetTextColor(dc, text);
        for (size_t i = 0; i < caps.size(); ++i) {
            RECT cap{x, top, x + sizes[i].cx + 2 * padding, top + capHeight};
            RoundRect(dc, cap.left, cap.top, cap.right, cap.bottom, scaled(5), scaled(5));
            DrawTextW(dc, caps[i].c_str(), -1, &cap, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            x = cap.right + gap;
        }
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        DeleteObject(capFill);
    }

    if ((item.itemState & ODS_FOCUS) && !recording_) {
        RECT focus = bounds;
        InflateRect(&focus, -scaled(3), -scaled(3));
        DrawFocusRect(dc, &focus);
    }
    SelectObject(dc, oldFont);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

}  // namespace klats::app
