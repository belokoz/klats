#pragma once
#include <windows.h>

#include <string>

namespace klats::app {

// Starts Klats: the tray icon, the hook thread and the pipeline thread. Returns the exit code.
int run(HINSTANCE instance, const std::wstring& commandLine);

}  // namespace klats::app
