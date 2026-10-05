#pragma once
#include "layout_table.h"

#include <string>
#include <string_view>

namespace klats {

// Re-types text as if the same keys had been pressed in the other layout.
enum class Direction { AToB, BToA };

struct Conversion {
    std::wstring text;
    Direction direction;
};

// The source layout is the one that owns more of the text's characters. Characters present in both
// layouts (digits, most punctuation) do not vote. A tie falls back to `tieBreak`, which the app
// derives from the layout of the focused window.
Direction detectDirection(std::wstring_view text, const LayoutTable& a, const LayoutTable& b,
                          Direction tieBreak = Direction::AToB);

// One direction per call: every character found in the source layout is replaced by the character
// on the same key in the target layout. Everything else passes through untouched.
Conversion convertLayout(std::wstring_view text, const LayoutTable& a, const LayoutTable& b,
                         Direction tieBreak = Direction::AToB);

}  // namespace klats
