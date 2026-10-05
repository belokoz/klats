// Generates tests/fixtures/layouts.h from the keyboard layout files of Windows (System32\KBD*.DLL).
//
// The files are read directly through KbdLayerDescriptor and the SDK's kbd.h, so nothing is
// installed or loaded into the session and the output is the same on every PC. For layouts that
// are installed here, the result is cross-checked against ToUnicodeEx, which the app itself uses.
//
//   dump_layouts.exe <output header>
#include "core/layout_table.h"
#include "core/unicode.h"

#include <windows.h>
#include <kbd.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <vector>

using klats::KeyStroke;
using klats::LayoutTable;

namespace {

struct Source {
    const char* function;  // name of the fixture in the header
    const wchar_t* file;
    const wchar_t* id;     // LANGID:KLID, as Windows names input methods
    const char* name;
};

const Source kSources[] = {
    {"us", L"KBDUS.DLL", L"0409:00000409", "US"},
    {"russian", L"KBDRU.DLL", L"0419:00000419", "Russian"},
    {"russianTypewriter", L"KBDRU1.DLL", L"0419:00010419", "Russian - Typewriter"},
    {"ukrainian", L"KBDUR.DLL", L"0422:00000422", "Ukrainian"},
    {"belarusian", L"KBDBLR.DLL", L"0423:00000423", "Belarusian"},
    {"kazakh", L"KBDKAZ.DLL", L"043F:0000043F", "Kazakh"},
    {"unitedKingdom", L"KBDUK.DLL", L"0809:00000809", "United Kingdom"},
    {"usInternational", L"KBDUSX.DLL", L"0409:00020409", "United States-International"},
    {"dvorak", L"KBDDV.DLL", L"0409:00010409", "United States-Dvorak"},
    {"french", L"KBDFR.DLL", L"040C:0000040C", "French (AZERTY)"},
    {"german", L"KBDGR.DLL", L"0407:00000407", "German (QWERTZ)"},
    {"greek", L"KBDHE.DLL", L"0408:00000408", "Greek"},
    {"hebrew", L"KBDHEB.DLL", L"040D:0000040D", "Hebrew"},
    {"turkishQ", L"KBDTUQ.DLL", L"041F:0000041F", "Turkish Q"},
};

// Printable keys of the main block, by set-1 scan code: the same list the app probes.
std::vector<uint16_t> scanCodes() {
    std::vector<uint16_t> codes;
    for (uint16_t sc = 0x02; sc <= 0x0D; ++sc) codes.push_back(sc);  // 1 2 3 4 5 6 7 8 9 0 - =
    for (uint16_t sc = 0x10; sc <= 0x1B; ++sc) codes.push_back(sc);  // Q W E R T Y U I O P [ ]
    for (uint16_t sc = 0x1E; sc <= 0x29; ++sc) codes.push_back(sc);  // A S D F G H J K L ; ' `
    for (uint16_t sc = 0x2B; sc <= 0x35; ++sc) codes.push_back(sc);  // \ Z X C V B N M , . /
    codes.push_back(0x56);  // the ISO key left of Z
    codes.push_back(0x73);  // ABNT C1, JIS Ro
    codes.push_back(0x7D);  // JIS Yen
    return codes;
}

// The same rule the app applies: one grapheme, not whitespace, not a control character.
bool usable(const std::wstring& text) {
    if (text.empty() || klats::unicode::graphemeCount(text) != 1) return false;
    char32_t first = klats::unicode::firstScalar(text);
    return !klats::unicode::isWhitespace(first) && !klats::unicode::isControl(first);
}

int shiftColumn(PKBDTABLES tables, bool shift) {
    BYTE bits = 0;
    if (shift) {
        for (PVK_TO_BIT entry = tables->pCharModifiers->pVkToBit; entry->Vk; ++entry) {
            if (entry->Vk == VK_SHIFT) bits |= entry->ModBits;
        }
    }
    if (bits > tables->pCharModifiers->wMaxModBits) return -1;
    BYTE column = tables->pCharModifiers->ModNumber[bits];
    return column == SHFT_INVALID ? -1 : column;
}

// What the key types in this column. Dead keys are skipped, as on the Mac; ligatures come back
// whole and are then dropped by `usable` when they are more than one grapheme.
std::optional<std::wstring> keyCharacter(PKBDTABLES tables, BYTE vk, int column) {
    if (column < 0) return std::nullopt;
    for (PVK_TO_WCHAR_TABLE table = tables->pVkToWcharTable; table->pVkToWchars; ++table) {
        for (auto* row = reinterpret_cast<BYTE*>(table->pVkToWchars);; row += table->cbSize) {
            auto* entry = reinterpret_cast<PVK_TO_WCHARS1>(row);
            if (entry->VirtualKey == 0) break;
            if (entry->VirtualKey != vk) continue;
            if (column >= table->nModifications) return std::nullopt;
            WCHAR character = entry->wch[column];
            if (character == WCH_NONE || character == WCH_DEAD) return std::nullopt;
            if (character == WCH_LGTR) {
                for (auto* ligature = reinterpret_cast<BYTE*>(tables->pLigature); ligature; ligature += tables->cbLgEntry) {
                    auto* lig = reinterpret_cast<PLIGATURE1>(ligature);
                    if (lig->VirtualKey == 0) break;
                    if (lig->VirtualKey == vk && lig->ModificationNumber == column) {
                        std::wstring text;
                        for (int i = 0; i < tables->nLgMax && lig->wch[i] != WCH_NONE; ++i) text += lig->wch[i];
                        return text;
                    }
                }
                return std::nullopt;
            }
            return std::wstring(1, character);
        }
    }
    return std::nullopt;
}

std::vector<LayoutTable::Row> rowsFromFile(const wchar_t* file) {
    std::vector<LayoutTable::Row> rows;
    HMODULE module = LoadLibraryExW(file, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) return rows;
    auto descriptor = reinterpret_cast<PKBDTABLES (*)()>(GetProcAddress(module, "KbdLayerDescriptor"));
    PKBDTABLES tables = descriptor ? descriptor() : nullptr;
    if (tables) {
        for (uint16_t sc : scanCodes()) {
            BYTE vk = sc < tables->bMaxVSCtoVK ? static_cast<BYTE>(tables->pusVSCtoVK[sc] & 0xFF) : 0;
            if (!vk) continue;
            for (bool shift : {false, true}) {
                auto text = keyCharacter(tables, vk, shiftColumn(tables, shift));
                if (text && usable(*text)) rows.push_back({{sc, shift}, *text});
            }
        }
    }
    FreeLibrary(module);
    return rows;
}

// The app's own way: MapVirtualKeyEx and ToUnicodeEx with flag 4, which leaves the shared keyboard
// state (pending dead keys) alone.
std::vector<LayoutTable::Row> rowsFromToUnicodeEx(HKL layout) {
    std::vector<LayoutTable::Row> rows;
    BYTE state[256] = {};
    for (uint16_t sc : scanCodes()) {
        UINT vk = MapVirtualKeyExW(sc, MAPVK_VSC_TO_VK_EX, layout);
        if (!vk) continue;
        for (bool shift : {false, true}) {
            state[VK_SHIFT] = state[VK_LSHIFT] = shift ? 0x80 : 0;
            wchar_t buffer[16];
            int length = ToUnicodeEx(vk, sc, state, buffer, 16, 0x4, layout);
            if (length <= 0) continue;
            std::wstring text(buffer, static_cast<size_t>(length));
            if (usable(text)) rows.push_back({{sc, shift}, text});
        }
    }
    return rows;
}

std::optional<HKL> installedLayout(const wchar_t* id) {
    // Only plain KLIDs (0000xxxx) map straight to an HKL; that covers what this check needs.
    std::wstring klid = std::wstring(id).substr(5);
    if (klid.substr(0, 4) != L"0000") return std::nullopt;
    auto wanted = static_cast<DWORD>(std::wcstoul(klid.c_str(), nullptr, 16));
    int count = GetKeyboardLayoutList(0, nullptr);
    std::vector<HKL> layouts(static_cast<size_t>(count));
    GetKeyboardLayoutList(count, layouts.data());
    for (HKL layout : layouts) {
        auto value = static_cast<DWORD>(reinterpret_cast<UINT_PTR>(layout));
        if (LOWORD(value) == wanted && HIWORD(value) == wanted) return layout;
    }
    return std::nullopt;
}

std::string literal(const std::wstring& text) {
    std::string out;
    for (wchar_t c : text) {
        if (c == L'\\' || c == L'"') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c >= 0x20 && c < 0x7F) {
            out += static_cast<char>(c);
        } else {
            char escape[16];
            std::snprintf(escape, sizeof escape, "\\x%04X", static_cast<unsigned>(c));
            out += escape;
            // A following hex digit would be swallowed by the escape, so close the literal.
            out += "\" L\"";
        }
    }
    return out;
}

// The ids and file names in kSources are ASCII.
std::string narrow(const std::wstring& text) {
    std::string out;
    for (wchar_t c : text) out += static_cast<char>(c);
    return out;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: dump_layouts <output header>\n");
        return 2;
    }
    FILE* out = nullptr;
    if (_wfopen_s(&out, argv[1], L"wb") != 0 || !out) {
        std::fprintf(stderr, "cannot write the output file\n");
        return 1;
    }
    std::fprintf(out, "// Generated by tools/dump_layouts from the keyboard layout files of Windows. Do not edit by hand.\n");
    std::fprintf(out, "#pragma once\n#include \"core/layout_table.h\"\n\nnamespace fixtures {\n");

    int problems = 0;
    for (const Source& source : kSources) {
        auto rows = rowsFromFile(source.file);
        if (rows.empty()) {
            std::fprintf(stderr, "%s: no layout file\n", narrow(source.file).c_str());
            ++problems;
            continue;
        }
        if (auto layout = installedLayout(source.id)) {
            std::map<KeyStroke, std::wstring> fromFile(rows.begin(), rows.end());
            auto viaApi = rowsFromToUnicodeEx(*layout);
            std::map<KeyStroke, std::wstring> fromApi(viaApi.begin(), viaApi.end());
            bool same = fromFile == fromApi;
            std::printf("%s: %zu keys, ToUnicodeEx %s\n", source.name, rows.size(), same ? "agrees" : "DIFFERS");
            if (!same) ++problems;
        } else {
            std::printf("%s: %zu keys\n", source.name, rows.size());
        }

        std::fprintf(out, "\n// %s, %s\ninline const klats::LayoutTable& %s() {\n", source.name, narrow(source.file).c_str(), source.function);
        std::fprintf(out, "    static const klats::LayoutTable table(L\"%s\", {\n", narrow(source.id).c_str());
        for (const auto& [stroke, character] : rows) {
            std::fprintf(out, "        {{0x%02X, %s}, L\"%s\"},\n", stroke.scanCode, stroke.shift ? "true" : "false", literal(character).c_str());
        }
        std::fprintf(out, "    });\n    return table;\n}\n");
    }
    std::fprintf(out, "\n}  // namespace fixtures\n");
    std::fclose(out);
    return problems == 0 ? 0 : 1;
}
