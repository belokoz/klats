#include "synthetic_keys.h"

#include "common.h"

#include <array>

namespace klats::app::synthetic {

namespace {

INPUT key(WORD vk, WORD scan, bool down, bool extended = false) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.wScan = scan;
    input.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP) | (extended ? KEYEVENTF_EXTENDEDKEY : 0);
    input.ki.dwExtraInfo = kInjectedMarker;
    return input;
}

void send(WORD modifierVk, WORD modifierScan, WORD vk, WORD scan, bool extended) {
    std::array<INPUT, 4> inputs = {
        key(modifierVk, modifierScan, true),
        key(vk, scan, true, extended),
        key(vk, scan, false, extended),
        key(modifierVk, modifierScan, false),
    };
    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

// The gray Insert key. MapVirtualKeyEx answers with the keypad's 0x52 and no extended flag, and a
// non-extended Insert reaches Qt and Chromium as keypad 0, so the code is written out here.
constexpr WORD kInsertScan = 0x52;
constexpr WORD kLeftCtrlScan = 0x1D;
constexpr WORD kLeftShiftScan = 0x2A;

WORD scanFor(WORD vk, HKL layout) { return static_cast<WORD>(MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC, layout)); }

}  // namespace

void copy(CopyKeys keys, HKL layout) {
    if (keys == CopyKeys::Insert) {
        send(VK_LCONTROL, kLeftCtrlScan, VK_INSERT, kInsertScan, true);
    } else {
        // Shortcuts match the virtual key, so 'C' is right in any layout, Dvorak included.
        send(VK_LCONTROL, kLeftCtrlScan, 'C', scanFor('C', layout), false);
    }
}

void paste(CopyKeys keys, HKL layout) {
    if (keys == CopyKeys::Insert) {
        send(VK_LSHIFT, kLeftShiftScan, VK_INSERT, kInsertScan, true);
    } else {
        send(VK_LCONTROL, kLeftCtrlScan, 'V', scanFor('V', layout), false);
    }
}

void mask() {
    std::array<INPUT, 2> inputs = {key(0xE8, 0, true), key(0xE8, 0, false)};
    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

bool modifiersAreDown() {
    for (int vk : {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN}) {
        if (GetAsyncKeyState(vk) & 0x8000) return true;
    }
    return false;
}

}  // namespace klats::app::synthetic
