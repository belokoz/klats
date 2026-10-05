#include "layout_converter.h"

#include "unicode.h"

namespace klats {

Direction detectDirection(std::wstring_view text, const LayoutTable& a, const LayoutTable& b, Direction tieBreak) {
    int onlyA = 0;
    int onlyB = 0;
    unicode::forEachGrapheme(text, [&](std::wstring_view character) {
        bool inA = a.contains(character);
        bool inB = b.contains(character);
        if (inA && !inB) ++onlyA;
        if (inB && !inA) ++onlyB;
    });
    if (onlyA == onlyB) return tieBreak;
    return onlyA > onlyB ? Direction::AToB : Direction::BToA;
}

Conversion convertLayout(std::wstring_view text, const LayoutTable& a, const LayoutTable& b, Direction tieBreak) {
    Direction direction = detectDirection(text, a, b, tieBreak);
    const LayoutTable& source = direction == Direction::AToB ? a : b;
    const LayoutTable& target = direction == Direction::AToB ? b : a;
    std::wstring result;
    result.reserve(text.size());
    unicode::forEachGrapheme(text, [&](std::wstring_view character) {
        if (auto stroke = source.stroke(character)) {
            if (auto mapped = target.character(*stroke)) {
                result += *mapped;
                return;
            }
        }
        result += character;
    });
    return {std::move(result), direction};
}

}  // namespace klats
