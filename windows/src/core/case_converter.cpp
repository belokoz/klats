#include "case_converter.h"

#include "unicode.h"

namespace klats {

std::wstring convertCase(std::wstring_view text, CaseMode mode) {
    if (mode == CaseMode::Lowercase) return unicode::lowercased(text);
    std::wstring result;
    result.reserve(text.size());
    unicode::forEachGrapheme(text, [&](std::wstring_view character) {
        if (unicode::isLowercase(character)) {
            result += unicode::uppercased(character);
        } else if (unicode::isUppercase(character)) {
            result += unicode::lowercased(character);
        } else {
            result += character;
        }
    });
    return result;
}

}  // namespace klats
