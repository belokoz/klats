#include "hotkey_display.h"

#include "strings.h"

#include <windows.h>

namespace klats::app {

namespace {

// Key names as the keys are labelled. Punctuation follows the US keycaps: GetKeyNameText would
// answer «б» or «ж» for them while the Russian layout is active, and nothing for some keys.
std::wstring keyName(uint8_t vk) {
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return L"Num " + std::to_wstring(vk - VK_NUMPAD0);
    switch (vk) {
    case VK_SPACE: return tr(L"Пробел");
    case VK_RETURN: return L"Enter";
    case VK_TAB: return L"Tab";
    case VK_ESCAPE: return L"Esc";
    case VK_BACK: return L"Backspace";
    case VK_INSERT: return L"Insert";
    case VK_DELETE: return L"Delete";
    case VK_HOME: return L"Home";
    case VK_END: return L"End";
    case VK_PRIOR: return L"PgUp";
    case VK_NEXT: return L"PgDn";
    case VK_LEFT: return L"←";
    case VK_UP: return L"↑";
    case VK_RIGHT: return L"→";
    case VK_DOWN: return L"↓";
    case VK_PAUSE: return L"Pause";
    case VK_SCROLL: return L"Scroll Lock";
    case VK_SNAPSHOT: return L"Print Screen";
    case VK_CAPITAL: return L"Caps Lock";
    case VK_OEM_1: return L";";
    case VK_OEM_PLUS: return L"=";
    case VK_OEM_COMMA: return L",";
    case VK_OEM_MINUS: return L"-";
    case VK_OEM_PERIOD: return L".";
    case VK_OEM_2: return L"/";
    case VK_OEM_3: return L"`";
    case VK_OEM_4: return L"[";
    case VK_OEM_5: return L"\\";
    case VK_OEM_6: return L"]";
    case VK_OEM_7: return L"'";
    case VK_OEM_102: return L"\\";
    case VK_MULTIPLY: return L"Num *";
    case VK_ADD: return L"Num +";
    case VK_SUBTRACT: return L"Num -";
    case VK_DECIMAL: return L"Num .";
    case VK_DIVIDE: return L"Num /";
    default: {
        wchar_t code[8];
        swprintf_s(code, L"#%02X", vk);
        return code;
    }
    }
}

}  // namespace

std::vector<std::wstring> keycaps(const Hotkey& hotkey) {
    std::vector<std::wstring> caps;
    if (hotkey.modifiers.contains(Modifier::Win)) caps.push_back(L"Win");
    if (hotkey.modifiers.contains(Modifier::Ctrl)) caps.push_back(L"Ctrl");
    if (hotkey.modifiers.contains(Modifier::Alt)) caps.push_back(L"Alt");
    if (hotkey.modifiers.contains(Modifier::Shift)) caps.push_back(L"Shift");
    if (!hotkey.isChord()) caps.push_back(keyName(hotkey.vk));
    return caps;
}

std::wstring displayString(const Hotkey& hotkey) {
    std::wstring text;
    for (const auto& cap : keycaps(hotkey)) text += (text.empty() ? L"" : L"+") + cap;
    return text;
}

}  // namespace klats::app
