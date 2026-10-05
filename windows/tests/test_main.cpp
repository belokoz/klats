#include "test.h"

#include <windows.h>

#include <cstring>

// Runs every test, or only those whose name contains the first argument.
int wmain(int argc, wchar_t** argv) {
    UINT previousCodePage = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    std::string filter = argc > 1 ? test::utf8(argv[1]) : std::string();

    int run = 0;
    int failed = 0;
    for (const auto& testCase : test::registry()) {
        if (!filter.empty() && !std::strstr(testCase.name, filter.c_str())) continue;
        ++run;
        int before = test::failures();
        testCase.body();
        bool ok = test::failures() == before;
        if (!ok) ++failed;
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", testCase.name);
    }
    std::printf("%d tests, %d failed\n", run, failed);
    std::fflush(stdout);
    SetConsoleOutputCP(previousCodePage);
    return failed == 0 && run > 0 ? 0 : 1;
}
