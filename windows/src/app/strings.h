#pragma once

namespace klats::app {

// Russian is the source language: the keys are the Russian strings themselves, as on the Mac.
// Windows in any other display language gets English.
const wchar_t* tr(const wchar_t* russian);
bool interfaceIsRussian();

}  // namespace klats::app
