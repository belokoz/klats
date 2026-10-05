#pragma once
#include "core/layout_pair.h"
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

// The keyboard layouts the user turned on in Windows settings. Windows lists them by language, not
// in the user's order, so the order means nothing. Input method editors (Japanese, Chinese, Korean
// and the like) are left out.
std::vector<KeyboardLayout> enabledLayouts();

// What every printable key of the main block types without and with Shift. Built anew on every
// use: it takes a fraction of a millisecond, and a pending dead key cannot freeze into a cache.
std::vector<LayoutTable::Row> layoutRows(HKL layout);

// Every enabled layout with its table, and the pair to convert between: the chosen one, or the
// automatic one when exactly two different layouts are on (see choosePair).
struct LayoutSet {
    std::vector<KeyboardLayout> layouts;
    std::vector<LayoutTable> tables;  // one per layout, empty when Windows would not say what it types
    PairChoice pair;                  // indices into both lists
};
LayoutSet readLayouts(const std::vector<std::wstring>& chosen);

// Asks the window with the focus to switch to `layout`. The caller checks the result.
void requestLayout(HWND window, HKL layout);

}  // namespace klats::app
