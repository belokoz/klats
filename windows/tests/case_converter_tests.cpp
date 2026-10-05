#include "test.h"

#include "core/case_converter.h"

using klats::CaseMode;

TEST("Регистр: инвертирование") {
    const std::pair<const wchar_t*, const wchar_t*> cases[] = {
        {L"пРИВЕТ", L"Привет"},
        {L"Hello World", L"hELLO wORLD"},
        {L"ПРИВЕТ", L"привет"},
        {L"123 — 🙂!", L"123 — 🙂!"},
        {L"", L""},
    };
    for (auto [input, expected] : cases) CHECK_EQ(klats::convertCase(input, CaseMode::Invert), std::wstring(expected));
}

TEST("Регистр: все строчные") {
    const std::pair<const wchar_t*, const wchar_t*> cases[] = {
        {L"ПРИВЕТ Мир", L"привет мир"},
        {L"пРИВЕТ", L"привет"},
        {L"already lower", L"already lower"},
        {L"123 — 🙂!", L"123 — 🙂!"},
    };
    for (auto [input, expected] : cases) CHECK_EQ(klats::convertCase(input, CaseMode::Lowercase), std::wstring(expected));
}

TEST("Регистр: двойное инвертирование возвращает исходный текст") {
    std::wstring text = L"Съешь ЕЩЁ этих Мягких french Булок";
    CHECK_EQ(klats::convertCase(klats::convertCase(text, CaseMode::Invert), CaseMode::Invert), text);
}

TEST("Регистр: буква, которая при смене регистра становится двумя") {
    CHECK_EQ(klats::convertCase(L"straße", CaseMode::Invert), std::wstring(L"STRASSE"));
}

// The rules of Swift's Character and String, which the Mac version follows: per grapheme, full
// mappings, no locale and no context.
TEST("Регистр: правила Юникода, как в мак-версии") {
    const std::pair<const wchar_t*, const wchar_t*> invert[] = {
        {L"Hello Мир ёЁ", L"hELLO мИР Ёё"},
        {L"йй", L"ЙЙ"},  // a decomposed «й» stays decomposed
        {L"ǅ ǆ Ǆ", L"ǅ Ǆ ǆ"},  // the titlecase «ǅ» is left alone
        {L"ı İ i I", L"I i̇ I i"},  // Turkish dotted and dotless i without a Turkish locale
        {L"ﬁ ŉ ᾳ", L"FI ʼN ΑΙ"},  // uppercase that expands
        {L"ᾼ", L"ᾼ"},  // Greek titlecase
        {L"ΣΑΣ ς", L"σασ Σ"},  // no final-sigma rule
        {L"\U00010428\U00010400", L"\U00010400\U00010428"},  // Deseret, outside the BMP
        {L"Ω K", L"ω k"},  // Ohm and Kelvin signs
        {L"👨‍👩‍👧🇷🇺👍🏽", L"👨‍👩‍👧🇷🇺👍🏽"},
    };
    for (auto [input, expected] : invert) CHECK_EQ(klats::convertCase(input, CaseMode::Invert), std::wstring(expected));

    const std::pair<const wchar_t*, const wchar_t*> lower[] = {
        {L"ΣΑΣ", L"σασ"},
        {L"İ ẞ ǅ", L"i̇ ß ǆ"},
        {L"\U00010400", L"\U00010428"},
    };
    for (auto [input, expected] : lower) CHECK_EQ(klats::convertCase(input, CaseMode::Lowercase), std::wstring(expected));
}
