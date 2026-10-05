#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

// Text is UTF-16, as the Windows clipboard holds it. A «character» is an extended grapheme cluster,
// like Swift's Character, and two characters are equal when their NFC forms are, like Swift's
// canonical equivalence. The Unicode data comes from the ICU that ships with Windows 10 1903+.
namespace klats::unicode {

// Calls `body` for every extended grapheme cluster of `text`, in order (UAX #29).
void forEachGrapheme(std::wstring_view text, const std::function<void(std::wstring_view)>& body);
size_t graphemeCount(std::wstring_view text);

std::wstring nfc(std::wstring_view text);
bool canonicallyEqual(std::wstring_view a, std::wstring_view b);

// Swift's String.uppercased() and lowercased(): every scalar through its full, context-free and
// locale-free mapping. «ß» becomes «SS»; there is no final-sigma rule and no Turkish dotless i.
std::wstring uppercased(std::wstring_view text);
std::wstring lowercased(std::wstring_view text);

// Swift's Character.isUppercase, isLowercase and isCased, for one grapheme.
bool isUppercase(std::wstring_view grapheme);
bool isLowercase(std::wstring_view grapheme);
bool isCased(std::wstring_view grapheme);

// The first scalar of `text`, 0 for an empty string. A lone surrogate is returned as itself.
char32_t firstScalar(std::wstring_view text);
bool isWhitespace(char32_t scalar);
bool isControl(char32_t scalar);

}  // namespace klats::unicode
