#pragma once
#include <string>

namespace klats::app {

// Russian is the source language: the keys are the Russian strings themselves, as on the Mac.
// Windows in any other display language gets English.
const wchar_t* tr(const wchar_t* russian);
// The same, with «{}» in the text replaced by `argument`: word order differs between the languages.
std::wstring tr(const wchar_t* russian, const std::wstring& argument);
bool interfaceIsRussian();

}  // namespace klats::app
