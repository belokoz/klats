cmake_minimum_required(VERSION 3.25)
project(Klats VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# The static CRT: Klats.exe needs nothing that Windows 10 does not already have.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    # Windows quietly unhooks low-level hooks of 32-bit processes after a few slow calls.
    message(FATAL_ERROR "Klats is 64-bit only")
endif()

# Optimise for size: Klats spends its life waiting, and a small exe loads and pages in faster.
set(CMAKE_CXX_FLAGS_RELEASE "/O1 /Gy /Gw /DNDEBUG")
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus /guard:cf)
add_link_options(/GUARD:CF /CETCOMPAT $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)

# Pure logic, covered by tests. Unicode comes from the ICU inside Windows 10 1903+.
add_library(klats_core STATIC
    src/core/case_converter.cpp
    src/core/chord_detector.cpp
    src/core/hotkey.cpp
    src/core/layout_converter.cpp
    src/core/layout_table.cpp
    src/core/release.cpp
    src/core/unicode.cpp
)
target_include_directories(klats_core PUBLIC src)
target_link_libraries(klats_core PUBLIC icu)

add_executable(klats_tests
    tests/test_main.cpp
    tests/case_converter_tests.cpp
    tests/chord_detector_tests.cpp
    tests/hotkey_tests.cpp
    tests/layout_converter_tests.cpp
    tests/release_tests.cpp
)
target_include_directories(klats_tests PRIVATE tests)
target_link_libraries(klats_tests PRIVATE klats_core)

enable_testing()
add_test(NAME core COMMAND klats_tests)

# Developer tool: regenerates tests/fixtures/layouts.h. Run it with `cmake --build <dir> --target fixtures`.
add_executable(dump_layouts tools/dump_layouts.cpp)
target_link_libraries(dump_layouts PRIVATE klats_core user32)
add_custom_target(fixtures
    COMMAND dump_layouts "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/layouts.h"
    DEPENDS dump_layouts
    COMMENT "Regenerating tests/fixtures/layouts.h from the layout files of Windows"
)
