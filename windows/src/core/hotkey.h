#pragma once
#include "modifiers.h"

#include <cstdint>
#include <optional>

namespace klats {

// A user-chosen shortcut. Without a key it is a chord of modifiers only, such as Win+Alt.
struct Hotkey {
    uint8_t vk = 0;  // Windows virtual-key code; 0 for a chord
    Modifiers modifiers;

    constexpr bool isChord() const { return vk == 0; }

    // A chord needs at least two modifiers; a key needs at least one and must not be a modifier
    // itself, because a bare key would fire in the middle of typing.
    bool isValid() const;

    friend constexpr bool operator==(const Hotkey&, const Hotkey&) = default;
};

namespace defaults {
inline constexpr Hotkey layoutHotkey{0, {Modifier::Win, Modifier::Alt}};
inline constexpr Hotkey caseHotkey{0x5A /* Z */, {Modifier::Win, Modifier::Alt}};
}  // namespace defaults

// The settings value of a hotkey: bits 0-7 hold the virtual key, bits 8-11 the modifiers. A
// removed hotkey is stored as 0, so it is not confused with «never set» (no value at all).
uint32_t encodeHotkey(const std::optional<Hotkey>& hotkey);

// nullopt when the user removed the hotkey, the fallback when the value is not a valid hotkey.
std::optional<Hotkey> decodeHotkey(uint32_t value, const Hotkey& fallback);

}  // namespace klats
