#include "app.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    // System DLLs come from System32 only, never from the folder Klats was started from.
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
    return klats::app::run(instance, commandLine ? commandLine : L"");
}
