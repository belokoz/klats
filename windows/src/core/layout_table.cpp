#include "layout_table.h"

#include "unicode.h"

#include <algorithm>

namespace klats {

LayoutTable::LayoutTable(std::wstring id, const std::vector<Row>& rows) : id_(std::move(id)) {
    for (const auto& [stroke, character] : rows) forward_[stroke] = character;

    std::vector<Row> ordered(forward_.begin(), forward_.end());
    std::stable_sort(ordered.begin(), ordered.end(), [](const Row& lhs, const Row& rhs) {
        if (lhs.first.shift != rhs.first.shift) return !lhs.first.shift;
        return lhs.first.scanCode < rhs.first.scanCode;
    });
    for (const auto& [stroke, character] : ordered) reverse_.try_emplace(unicode::nfc(character), stroke);
}

std::optional<std::wstring_view> LayoutTable::character(KeyStroke stroke) const {
    auto found = forward_.find(stroke);
    if (found == forward_.end()) return std::nullopt;
    return std::wstring_view(found->second);
}

std::optional<KeyStroke> LayoutTable::stroke(std::wstring_view character) const {
    // A key types at most a few UTF-16 units. A longer cluster can match nothing, and normalising
    // a cluster of thousands of combining marks costs quadratic time: skip it.
    if (character.size() > kLongestKey) return std::nullopt;
    auto found = reverse_.find(unicode::nfc(character));
    if (found == reverse_.end()) return std::nullopt;
    return found->second;
}

}  // namespace klats
