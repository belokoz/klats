#pragma once
#include <cstdint>
#include <initializer_list>

namespace klats {

// The four modifier keys Klats cares about. Left and right count as one key; Caps Lock, Num Lock
// and Fn are ignored. Same bits as on the Mac: Shift, Control, Option there is Alt, Command is Win.
enum class Modifier : uint8_t {
    Shift = 1 << 0,
    Ctrl = 1 << 1,
    Alt = 1 << 2,
    Win = 1 << 3,
};

class Modifiers {
public:
    constexpr Modifiers() = default;
    constexpr Modifiers(std::initializer_list<Modifier> list) {
        for (Modifier m : list) bits_ |= static_cast<uint8_t>(m);
    }

    static constexpr Modifiers fromBits(uint32_t bits) {
        Modifiers m;
        m.bits_ = static_cast<uint8_t>(bits & 0x0F);
        return m;
    }

    constexpr uint8_t bits() const { return bits_; }
    constexpr bool empty() const { return bits_ == 0; }
    constexpr bool contains(Modifier m) const { return (bits_ & static_cast<uint8_t>(m)) != 0; }
    constexpr void insert(Modifier m) { bits_ |= static_cast<uint8_t>(m); }
    constexpr void remove(Modifier m) { bits_ &= static_cast<uint8_t>(~static_cast<uint8_t>(m)); }
    constexpr bool isSubsetOf(Modifiers other) const { return (bits_ & ~other.bits_) == 0; }
    constexpr bool isStrictSubsetOf(Modifiers other) const { return isSubsetOf(other) && bits_ != other.bits_; }

    constexpr int count() const {
        int n = 0;
        for (uint8_t b = bits_; b; b &= b - 1) ++n;
        return n;
    }

    friend constexpr bool operator==(Modifiers, Modifiers) = default;

private:
    uint8_t bits_ = 0;
};

}  // namespace klats
