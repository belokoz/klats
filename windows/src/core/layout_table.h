#pragma once
#include <cstdint>
#include <compare>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace klats {

// One physical key press that types a character: the set-1 scan code of the key plus the Shift
// state. The scan code is the physical key, like the Mac's key code; virtual keys move with the
// layout (AZERTY, QWERTZ, Dvorak) and would pair the wrong characters.
struct KeyStroke {
    uint16_t scanCode = 0;
    bool shift = false;

    friend constexpr auto operator<=>(const KeyStroke&, const KeyStroke&) = default;
};

// A keyboard layout as two lookup tables: key stroke → character and character → key stroke.
// The app builds them from the real Windows layouts; tests build them from fixtures.
class LayoutTable {
public:
    using Row = std::pair<KeyStroke, std::wstring>;

    LayoutTable(std::wstring id, const std::vector<Row>& rows);

    const std::wstring& id() const { return id_; }
    const std::map<KeyStroke, std::wstring>& forward() const { return forward_; }
    bool empty() const { return forward_.empty(); }

    std::optional<std::wstring_view> character(KeyStroke stroke) const;

    // The key that types `character`, written in any canonically equivalent way. A character can
    // sit on several keys: the unshifted stroke wins, then the lowest scan code.
    std::optional<KeyStroke> stroke(std::wstring_view character) const;
    bool contains(std::wstring_view character) const { return stroke(character).has_value(); }

    // ToUnicodeEx writes at most 16 units per key; anything far longer cannot be a key's character.
    static constexpr size_t kLongestKey = 64;

private:
    std::wstring id_;
    std::map<KeyStroke, std::wstring> forward_;
    std::unordered_map<std::wstring, KeyStroke> reverse_;  // keyed by NFC
};

}  // namespace klats
