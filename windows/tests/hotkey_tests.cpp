#include "test.h"

#include "core/hotkey.h"

using klats::Hotkey;
using klats::Modifier;
using klats::Modifiers;

TEST("Сочетание: сохраняется и читается без потерь") {
    const Hotkey hotkeys[] = {
        klats::defaults::layoutHotkey,
        klats::defaults::caseHotkey,
        Hotkey{0x20 /* Space */, {Modifier::Ctrl}},
        Hotkey{0, {Modifier::Ctrl, Modifier::Shift, Modifier::Alt, Modifier::Win}},
    };
    for (const Hotkey& hotkey : hotkeys) {
        auto decoded = klats::decodeHotkey(klats::encodeHotkey(hotkey), Hotkey{});
        CHECK(decoded.has_value());
        if (decoded) CHECK(*decoded == hotkey);
    }
}

TEST("Сочетание: без клавиши — это сочетание из одних модификаторов") {
    CHECK(klats::defaults::layoutHotkey.isChord());
    CHECK(!klats::defaults::caseHotkey.isChord());
}

TEST("Сочетание: по умолчанию Win+Alt и Win+Alt+Z") {
    CHECK(klats::defaults::layoutHotkey.modifiers == (Modifiers{Modifier::Win, Modifier::Alt}));
    CHECK_EQ(klats::defaults::caseHotkey.vk, 0x5A);
    CHECK(klats::defaults::caseHotkey.modifiers == (Modifiers{Modifier::Alt, Modifier::Win}));
}

TEST("Сочетание: убранное хранится нулём и не путается с «не задано»") {
    CHECK_EQ(klats::encodeHotkey(std::nullopt), 0u);
    CHECK(!klats::decodeHotkey(0, klats::defaults::layoutHotkey).has_value());
}

TEST("Сочетание: негодное значение заменяется значением по умолчанию") {
    const Hotkey fallback = klats::defaults::caseHotkey;
    const uint32_t broken[] = {
        0x0100,          // one modifier and no key is not a chord
        0x005A,          // a bare key
        0x0410,          // Alt plus Shift as the key
        0x045B,          // Alt plus the Win key as the key
        0x10000,         // bits above the modifiers
        0x04FF,          // 0xFF is not a key
    };
    for (uint32_t value : broken) {
        auto decoded = klats::decodeHotkey(value, fallback);
        CHECK(decoded.has_value());
        if (decoded) CHECK(*decoded == fallback);
    }
}

TEST("Сочетание: что годится, а что нет") {
    CHECK((Hotkey{0, {Modifier::Win, Modifier::Alt}}.isValid()));
    CHECK(!(Hotkey{0, {Modifier::Alt}}.isValid()));
    CHECK((Hotkey{0x5A, {Modifier::Ctrl}}.isValid()));
    CHECK(!(Hotkey{0x5A, {}}.isValid()));
    CHECK(!(Hotkey{0xA4 /* left Alt */, {Modifier::Ctrl}}.isValid()));
}
