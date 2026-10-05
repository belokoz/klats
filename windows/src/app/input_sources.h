#pragma once
#include "core/layout_table.h"

#include <windows.h>

#include <optional>
#include <string>
#include <vector>

namespace klats::app {

struct KeyboardLayout {
    std::wstring id;    // «LANGID:KLID», as Windows itself names input methods: «0419:00000419»
    HKL hkl = nullptr;
    std::wstring name;  // «Русская», «США»
};

// The keyboard layouts the user turned on in Windows settings, in their order. Input method
// editors (Japanese, Chinese, Korean and the like) are left out.
std::vector<KeyboardLayout> enabledLayouts();

// What every printable key of the main block types without and with Shift. Built anew on every
// use: it takes a fraction of a millisecond, and a pending dead key cannot freeze into a cache.
std::vector<LayoutTable::Row> layoutRows(HKL layout);
std::optional<LayoutTable> buildTable(const KeyboardLayout& layout);

// The pair to convert between: the chosen one, or the first two enabled layouts on «automatic».
std::optional<std::pair<KeyboardLayout, KeyboardLayout>> resolvePair(const std::vector<std::wstring>& chosen);

// Asks the window with the focus to switch to `layout`. The caller checks the result.
void requestLayout(HWND window, HKL layout);

}  // namespace klats::app
