#pragma once
#include <string>
#include <string_view>

// Diagnostics in %LOCALAPPDATA%\Klats\klats.log. It never records the text being converted: only
// lengths, timings, outcomes and the name of the program that was in front.
namespace klats::app::log {

// Creates the folder and moves a log over 1 MB aside to klats.log.old.
void open();
void write(std::wstring_view message);
std::wstring filePath();

}  // namespace klats::app::log
