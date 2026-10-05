#pragma once
#include <string>
#include <string_view>

namespace klats {

enum class CaseMode {
    Invert,     // every letter flips: «пРИВЕТ» → «Привет»; fixes text typed with Caps Lock on
    Lowercase,  // everything becomes lowercase: «ПРИВЕТ» → «привет»
};

std::wstring convertCase(std::wstring_view text, CaseMode mode);

}  // namespace klats
