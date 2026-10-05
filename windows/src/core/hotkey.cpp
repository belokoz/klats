#include "hotkey.h"

namespace klats {

namespace {

// Virtual keys of the modifiers themselves (VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN and the
// left/right variants). The core stays free of <windows.h>, so the codes are spelled out.
bool isModifierKey(uint8_t vk) {
    switch (vk) {
    case 0x10: case 0x11: case 0x12:
    case 0x5B: case 0x5C:
    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5:
        return true;
    default:
        return false;
    }
}

}  // namespace

bool Hotkey::isValid() const {
    if (isChord()) return modifiers.count() >= 2;
    return !modifiers.empty() && vk != 0xFF && !isModifierKey(vk);
}

uint32_t encodeHotkey(const std::optional<Hotkey>& hotkey) {
    if (!hotkey) return 0;
    return static_cast<uint32_t>(hotkey->vk) | (static_cast<uint32_t>(hotkey->modifiers.bits()) << 8);
}

std::optional<Hotkey> decodeHotkey(uint32_t value, const Hotkey& fallback) {
    if (value == 0) return std::nullopt;
    if (value & ~0x0FFFu) return fallback;
    Hotkey hotkey{static_cast<uint8_t>(value & 0xFF), Modifiers::fromBits(value >> 8)};
    return hotkey.isValid() ? hotkey : fallback;
}

}  // namespace klats
