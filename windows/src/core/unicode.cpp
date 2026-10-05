#include "unicode.h"

#include <icu.h>

#include <climits>

namespace klats::unicode {

namespace {

const UChar* icuText(std::wstring_view text) { return reinterpret_cast<const UChar*>(text.data()); }

bool isHigh(wchar_t c) { return c >= 0xD800 && c <= 0xDBFF; }
bool isLow(wchar_t c) { return c >= 0xDC00 && c <= 0xDFFF; }

// Length in UTF-16 units of the scalar that starts `text`.
size_t scalarLength(std::wstring_view text) {
    if (text.size() >= 2 && isHigh(text[0]) && isLow(text[1])) return 2;
    return text.empty() ? 0 : 1;
}

template <class Body>
void forEachScalar(std::wstring_view text, Body&& body) {
    while (!text.empty()) {
        size_t n = scalarLength(text);
        body(text.substr(0, n));
        text.remove_prefix(n);
    }
}

bool isSingleScalar(std::wstring_view text, char32_t* scalar) {
    if (text.empty() || scalarLength(text) != text.size()) return false;
    *scalar = firstScalar(text);
    return true;
}

std::wstring mapScalars(std::wstring_view text, bool upper) {
    std::wstring result;
    result.reserve(text.size());
    forEachScalar(text, [&](std::wstring_view scalar) {
        UChar mapped[16];  // the longest full case mapping is three scalars
        UErrorCode error = U_ZERO_ERROR;
        int32_t length = upper
            ? u_strToUpper(mapped, 16, icuText(scalar), static_cast<int32_t>(scalar.size()), "", &error)
            : u_strToLower(mapped, 16, icuText(scalar), static_cast<int32_t>(scalar.size()), "", &error);
        if (U_FAILURE(error)) {
            result += scalar;
        } else {
            result.append(reinterpret_cast<const wchar_t*>(mapped), static_cast<size_t>(length));
        }
    });
    return result;
}

}  // namespace

char32_t firstScalar(std::wstring_view text) {
    if (text.empty()) return 0;
    if (scalarLength(text) == 2) {
        return 0x10000 + ((static_cast<char32_t>(text[0]) - 0xD800) << 10) + (static_cast<char32_t>(text[1]) - 0xDC00);
    }
    return text[0];
}

void forEachGrapheme(std::wstring_view text, const std::function<void(std::wstring_view)>& body) {
    if (text.empty()) return;
    UErrorCode error = U_ZERO_ERROR;
    UBreakIterator* iterator = text.size() <= INT32_MAX
        ? ubrk_open(UBRK_CHARACTER, "", icuText(text), static_cast<int32_t>(text.size()), &error)
        : nullptr;
    if (!iterator || U_FAILURE(error)) {
        // Without ICU's segmentation, scalars are the next best unit.
        if (iterator) ubrk_close(iterator);
        forEachScalar(text, [&](std::wstring_view scalar) { body(scalar); });
        return;
    }
    int32_t start = ubrk_first(iterator);
    for (int32_t end = ubrk_next(iterator); end != UBRK_DONE; start = end, end = ubrk_next(iterator)) {
        body(text.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)));
    }
    ubrk_close(iterator);
}

size_t graphemeCount(std::wstring_view text) {
    size_t count = 0;
    forEachGrapheme(text, [&](std::wstring_view) { ++count; });
    return count;
}

std::wstring nfc(std::wstring_view text) {
    char32_t scalar;
    if (isSingleScalar(text, &scalar) && u_getIntPropertyValue(static_cast<UChar32>(scalar), UCHAR_NFC_QUICK_CHECK) != UNORM_NO) {
        return std::wstring(text);
    }
    UErrorCode error = U_ZERO_ERROR;
    const UNormalizer2* normalizer = unorm2_getNFCInstance(&error);
    if (U_FAILURE(error) || text.size() > INT32_MAX / 4) return std::wstring(text);
    int32_t capacity = static_cast<int32_t>(text.size()) * 3 + 16;  // NFC grows by at most 3x
    std::wstring result(static_cast<size_t>(capacity), L'\0');
    int32_t length = unorm2_normalize(normalizer, icuText(text), static_cast<int32_t>(text.size()),
                                      reinterpret_cast<UChar*>(result.data()), capacity, &error);
    if (U_FAILURE(error)) return std::wstring(text);
    result.resize(static_cast<size_t>(length));
    return result;
}

bool canonicallyEqual(std::wstring_view a, std::wstring_view b) {
    if (a == b) return true;
    // Normalising a cluster of thousands of out-of-order combining marks takes quadratic time. The
    // callers compare a cluster with its own case mapping, which never yields a different but
    // canonically equivalent spelling, so plain comparison is exact for long clusters.
    if (a.size() > 32 || b.size() > 32) return false;
    return nfc(a) == nfc(b);
}

std::wstring uppercased(std::wstring_view text) { return mapScalars(text, true); }
std::wstring lowercased(std::wstring_view text) { return mapScalars(text, false); }

bool isCased(std::wstring_view grapheme) {
    char32_t scalar;
    if (isSingleScalar(grapheme, &scalar) && u_hasBinaryProperty(static_cast<UChar32>(scalar), UCHAR_CASED)) return true;
    return !canonicallyEqual(grapheme, uppercased(grapheme)) || !canonicallyEqual(grapheme, lowercased(grapheme));
}

bool isUppercase(std::wstring_view grapheme) {
    char32_t scalar;
    if (isSingleScalar(grapheme, &scalar) && u_isUUppercase(static_cast<UChar32>(scalar))) return true;
    return canonicallyEqual(grapheme, uppercased(grapheme)) && isCased(grapheme);
}

bool isLowercase(std::wstring_view grapheme) {
    char32_t scalar;
    if (isSingleScalar(grapheme, &scalar) && u_isULowercase(static_cast<UChar32>(scalar))) return true;
    return canonicallyEqual(grapheme, lowercased(grapheme)) && isCased(grapheme);
}

bool isWhitespace(char32_t scalar) { return u_isUWhiteSpace(static_cast<UChar32>(scalar)) != 0; }
bool isControl(char32_t scalar) { return u_charType(static_cast<UChar32>(scalar)) == U_CONTROL_CHAR; }

}  // namespace klats::unicode
