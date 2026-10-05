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
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/W4> $<$<COMPILE_LANGUAGE:CXX>:/permissive->
                    $<$<COMPILE_LANGUAGE:CXX>:/utf-8> $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
                    $<$<COMPILE_LANGUAGE:CXX>:/guard:cf>)
add_link_options(/GUARD:CF /CETCOMPAT $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)

configure_file(res/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h")

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

# The layout reader of the app, shared with the fixture generator.
add_library(klats_layouts STATIC src/app/common.cpp src/app/input_sources.cpp)
target_link_libraries(klats_layouts PUBLIC klats_core advapi32 user32)

add_executable(Klats WIN32
    src/app/app.cpp
    src/app/clipboard.cpp
    src/app/foreground.cpp
    src/app/hook_thread.cpp
    src/app/hotkey_display.cpp
    src/app/log.cpp
    src/app/main.cpp
    src/app/pipeline.cpp
    src/app/settings_store.cpp
    src/app/strings.cpp
    src/app/synthetic_keys.cpp
    src/app/tray_icon.cpp
    res/klats.rc
    res/klats.manifest
)
target_include_directories(Klats PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(Klats PRIVATE klats_layouts comctl32 gdi32 gdiplus imm32 ole32 shell32 winmm wtsapi32)
# Statically imported DLLs that are not KnownDLLs load from System32 only, never from Downloads.
target_link_options(Klats PRIVATE /DEPENDENTLOADFLAG:0x800)
set_source_files_properties(res/klats.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/res/klats.ico")

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

# End-to-end check on the real desktop, against a running Klats. Not part of ctest: it takes over
# the keyboard focus for half a minute.
add_executable(klats_e2e tests/e2e/e2e.cpp)
target_link_libraries(klats_e2e PRIVATE user32)

# Developer tools. `cmake --build <dir> --target fixtures` regenerates tests/fixtures/layouts.h;
# `--target icon` regenerates res/klats.ico from the site's 1024-pixel icon.
add_executable(dump_layouts tools/dump_layouts.cpp)
target_link_libraries(dump_layouts PRIVATE klats_layouts)
add_custom_target(fixtures
    COMMAND dump_layouts "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/layouts.h"
    DEPENDS dump_layouts
    COMMENT "Regenerating tests/fixtures/layouts.h from the layout files of Windows"
)
add_executable(make_icon tools/make_icon.cpp)
add_custom_target(icon
    COMMAND make_icon "${CMAKE_CURRENT_SOURCE_DIR}/../site/assets/icon-1024.png" "${CMAKE_CURRENT_SOURCE_DIR}/res/klats.ico"
    DEPENDS make_icon
    COMMENT "Regenerating res/klats.ico from site/assets/icon-1024.png"
)
