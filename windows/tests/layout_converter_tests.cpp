#include "test.h"

#include "core/layout_converter.h"
#include "fixtures/layouts.h"

using klats::Direction;

namespace {

std::wstring convert(std::wstring_view text, const klats::LayoutTable& a, const klats::LayoutTable& b,
                     Direction tieBreak = Direction::AToB) {
    return klats::convertLayout(text, a, b, tieBreak).text;
}

}  // namespace

TEST("Конвертация: латиница вместо кириллицы") {
    const auto& us = fixtures::us();
    const auto& ru = fixtures::russian();
    const std::pair<const wchar_t*, const wchar_t*> cases[] = {
        {L"ghbdtn", L"привет"},
        {L"Ghbdtn? rfr ltkf&", L"Привет, как дела?"},
        {L"GHBDTN", L"ПРИВЕТ"},
        {L"z nt,z k.,k.", L"я тебя люблю"},
        // On a PC keyboard «ё» lives on the top-left key, which types «`» in the US layout.
        {L"`=]", L"ё=ъ"},
    };
    for (auto [input, expected] : cases) {
        auto result = klats::convertLayout(input, us, ru);
        CHECK_EQ(result.text, std::wstring(expected));
        CHECK(result.direction == Direction::AToB);
    }
}

TEST("Конвертация: кириллица вместо латиницы") {
    const auto& us = fixtures::us();
    const auto& ru = fixtures::russian();
    const std::pair<const wchar_t*, const wchar_t*> cases[] = {
        {L"руддщ", L"hello"},
        {L"Привет, как дела?", L"Ghbdtn? rfr ltkf&"},
        {L"ЦЩКВ", L"WORD"},
    };
    for (auto [input, expected] : cases) {
        auto result = klats::convertLayout(input, us, ru);
        CHECK_EQ(result.text, std::wstring(expected));
        CHECK(result.direction == Direction::BToA);
    }
}

TEST("Конвертация: туда и обратно даёт исходный текст") {
    const auto& us = fixtures::us();
    const auto& ru = fixtures::russian();
    for (const wchar_t* input : {L"ghbdtn", L"Ghbdtn? rfr ltkf&", L"руддщ цщкдв", L"Ntcn 123!"}) {
        CHECK_EQ(convert(convert(input, us, ru), us, ru), std::wstring(input));
    }
}

TEST("Конвертация: незнакомые символы проходят как есть") {
    CHECK_EQ(convert(L"Ntcn 123 — ok 🙂\n\tend", fixtures::us(), fixtures::russian()), std::wstring(L"Тест 123 — щл 🙂\n\tутв"));
}

TEST("Конвертация: эмодзи, флаги и переводы строк Windows не трогаются") {
    const auto& us = fixtures::us();
    const auto& ru = fixtures::russian();
    CHECK_EQ(convert(L"👍🏽 👨‍👩‍👧 🇷🇺 ghbdtn", us, ru), std::wstring(L"👍🏽 👨‍👩‍👧 🇷🇺 привет"));
    CHECK_EQ(convert(L"ghbdtn\r\nvbh", us, ru), std::wstring(L"привет\r\nмир"));
}

TEST("Конвертация: порядок раскладок в паре не влияет на результат") {
    const auto& us = fixtures::us();
    const auto& ru = fixtures::russian();
    CHECK_EQ(convert(L"ghbdtn", ru, us), std::wstring(L"привет"));
    CHECK_EQ(convert(L"руддщ", ru, us), std::wstring(L"hello"));
}

TEST("Конвертация: при ничьей решает текущая раскладка") {
    const auto& us = fixtures::us();
    const auto& ru = fixtures::russian();
    // Digits live on the same keys in both layouts: nothing to vote with, nothing to change.
    CHECK_EQ(convert(L"123", us, ru, Direction::AToB), std::wstring(L"123"));
    CHECK_EQ(convert(L"123", us, ru, Direction::BToA), std::wstring(L"123"));
    // A lone full stop exists in both layouts on different keys.
    CHECK_EQ(convert(L".", us, ru, Direction::AToB), std::wstring(L"ю"));
    CHECK_EQ(convert(L".", us, ru, Direction::BToA), std::wstring(L"/"));
}

TEST("Конвертация: разложенные символы Юникода распознаются") {
    // «й» as a base letter plus a combining breve.
    CHECK_EQ(convert(L"й", fixtures::us(), fixtures::russian()), std::wstring(L"q"));
    // A decomposed «é» exists in neither layout and passes untouched.
    CHECK_EQ(convert(L"café ghbdtn", fixtures::us(), fixtures::russian()), std::wstring(L"сфаé привет"));
}

TEST("Конвертация: пустая строка остаётся пустой") {
    CHECK_EQ(convert(L"", fixtures::us(), fixtures::russian()), std::wstring());
}

TEST("Конвертация: русская машинописная раскладка") {
    CHECK_EQ(convert(L"ghbdtn", fixtures::us(), fixtures::russianTypewriter()), std::wstring(L"привет"));
    CHECK_EQ(convert(L"руддщ", fixtures::us(), fixtures::russianTypewriter()), std::wstring(L"hello"));
}

TEST("Конвертация: другие пары раскладок") {
    CHECK_EQ(convert(L"ghbdsn", fixtures::us(), fixtures::ukrainian()), std::wstring(L"привіт"));
    CHECK_EQ(convert(L"ghsdbnfyyt", fixtures::us(), fixtures::belarusian()), std::wstring(L"прывітанне"));
    CHECK_EQ(convert(L"Ghbdtn? rfr ltkf&", fixtures::unitedKingdom(), fixtures::russian()), std::wstring(L"Привет, как дела?"));
    CHECK_EQ(convert(L"ηελλο", fixtures::us(), fixtures::greek()), std::wstring(L"hello"));
    CHECK_EQ(convert(L"שלום", fixtures::us(), fixtures::hebrew()), std::wstring(L"akuo"));
    CHECK_EQ(convert(L"ğ", fixtures::turkishQ(), fixtures::russian()), std::wstring(L"х"));
}

TEST("Конвертация: клавиши, а не буквы: AZERTY и QWERTZ") {
    // On AZERTY «a» sits where QWERTY has «q», so it pairs with «й», not with «ф».
    CHECK_EQ(convert(L"azerty", fixtures::french(), fixtures::russian()), std::wstring(L"йцукен"));
    CHECK_EQ(convert(L"Рфддщ", fixtures::german(), fixtures::russian()), std::wstring(L"Hallo"));
    CHECK_EQ(convert(L"Яуше", fixtures::german(), fixtures::russian()), std::wstring(L"Yeit"));
}

TEST("Конвертация: Дворак против QWERTY решает текущая раскладка") {
    // Every letter exists in both layouts, so nothing votes.
    CHECK_EQ(convert(L"jdpps", fixtures::us(), fixtures::dvorak(), Direction::AToB), std::wstring(L"hello"));
    CHECK_EQ(convert(L"hello", fixtures::us(), fixtures::dvorak(), Direction::BToA), std::wstring(L"jdpps"));
}

TEST("Конвертация: мёртвые клавиши пока не участвуют") {
    // In US-International the apostrophe key is a dead key and is left out of the table, so «э»
    // (Russian, same key) has no partner and passes through. Changes once dead keys are decided.
    CHECK_EQ(convert(L"эхо", fixtures::usInternational(), fixtures::russian()), std::wstring(L"э[j"));
}

TEST("Конвертация: пустая таблица ничего не меняет") {
    klats::LayoutTable empty(L"empty", {});
    CHECK(empty.empty());
    CHECK_EQ(convert(L"ghbdtn", fixtures::us(), empty), std::wstring(L"ghbdtn"));
}

TEST("Таблица раскладки: символ на нескольких клавишах берётся без Shift и с меньшим скан-кодом") {
    klats::LayoutTable table(L"t", {{{0x56, false}, L"\\"}, {{0x2B, true}, L"\\"}, {{0x2B, false}, L"\\"}});
    auto stroke = table.stroke(L"\\");
    CHECK(stroke.has_value());
    if (stroke) {
        CHECK_EQ(stroke->scanCode, 0x2B);
        CHECK(!stroke->shift);
    }
}
