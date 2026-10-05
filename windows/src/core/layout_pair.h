#pragma once
#include "layout_table.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace klats {

// Which two layouts to convert between.
struct PairChoice {
    enum class Problem {
        None,
        NeedTwo,    // fewer than two distinct layouts are turned on
        Ambiguous,  // three or more, and the user has not chosen two
    };
    Problem problem = Problem::None;
    size_t first = 0;   // indices into the list of layouts
    size_t second = 0;
};

// The pair the user chose, when both of its layouts are still turned on. Otherwise the automatic
// pair, which exists only when exactly two layouts with different tables remain: with three or
// more, the first two would be a guess. Layouts with empty tables and duplicates of an earlier
// layout's table (the same keyboard under two languages) do not count.
PairChoice choosePair(const std::vector<std::wstring>& chosen, const std::vector<const LayoutTable*>& layouts);

}  // namespace klats
