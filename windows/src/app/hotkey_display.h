#pragma once
#include "core/hotkey.h"

#include <string>
#include <vector>

namespace klats::app {

// One label per key, for keycaps: {"Win", "Alt", "Z"}. Modifiers in the order Windows writes them:
// Win, Ctrl, Alt, Shift.
std::vector<std::wstring> keycaps(const Hotkey& hotkey);

// One string, for menus and messages: «Win+Alt+Z».
std::wstring displayString(const Hotkey& hotkey);

}  // namespace klats::app
