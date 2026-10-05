#include "input_sources.h"

#include "core/unicode.h"

#include <cwchar>

namespace klats::app {

namespace {

// input.dll exports EnumEnabledLayoutOrTip, which Microsoft documents but no SDK header declares.
struct LayoutOrTipProfile {
    DWORD dwProfileType;
    LANGID langid;
    CLSID clsid;
    GUID guidProfile;
    GUID catid;
    DWORD dwSubstituteLayout;
    DWORD dwFlags;
    WCHAR szId[MAX_PATH];
};
constexpr DWORD kKeyboardLayoutProfile = 2;  // LOTP_KEYBOARDLAYOUT
using EnumEnabledLayoutOrTipFn = UINT(WINAPI*)(LPCWSTR, LPCWSTR, LPCWSTR, LayoutOrTipProfile*, UINT);

const wchar_t* kLayoutsKey = L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\";

// Printable keys of the main block, by set-1 scan code. Enter, Tab, Space, Backspace and the numeric
// keypad are left out, so that «1» always means the number row.
const std::vector<uint16_t>& scanCodes() {
    static const std::vector<uint16_t> codes = [] {
        std::vector<uint16_t> list;
        for (uint16_t sc = 0x02; sc <= 0x0D; ++sc) list.push_back(sc);  // 1 2 3 4 5 6 7 8 9 0 - =
        for (uint16_t sc = 0x10; sc <= 0x1B; ++sc) list.push_back(sc);  // Q W E R T Y U I O P [ ]
        for (uint16_t sc = 0x1E; sc <= 0x29; ++sc) list.push_back(sc);  // A S D F G H J K L ; ' `
        for (uint16_t sc = 0x2B; sc <= 0x35; ++sc) list.push_back(sc);  // \ Z X C V B N M , . /
        list.push_back(0x56);  // the ISO key left of Z
        list.push_back(0x73);  // ABNT C1, JIS Ro
        list.push_back(0x7D);  // JIS Yen
        return list;
    }();
    return codes;
}

std::vector<std::wstring> enabledIds() {
    static const auto enumerate = [] {
        HMODULE module = LoadLibraryExW(L"input.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        return module ? reinterpret_cast<EnumEnabledLayoutOrTipFn>(GetProcAddress(module, "EnumEnabledLayoutOrTip")) : nullptr;
    }();
    std::vector<std::wstring> ids;
    if (!enumerate) return ids;
    UINT count = enumerate(nullptr, nullptr, nullptr, nullptr, 0);
    if (!count) return ids;
    std::vector<LayoutOrTipProfile> profiles(count);
    UINT copied = enumerate(nullptr, nullptr, nullptr, profiles.data(), count);
    for (UINT i = 0; i < copied && i < count; ++i) {
        const auto& profile = profiles[i];
        if (profile.dwProfileType != kKeyboardLayoutProfile) continue;
        std::wstring id(profile.szId, wcsnlen(profile.szId, MAX_PATH));
        // «LLLL:KKKKKKKK»; KLIDs starting with E are old-style input method editors.
        if (id.size() != 13 || id[4] != L':' || id[5] == L'E' || id[5] == L'e') continue;
        ids.push_back(std::move(id));
    }
    return ids;
}

std::optional<WORD> layoutId(const std::wstring& klid) {
    wchar_t value[16];
    DWORD size = sizeof value;
    std::wstring key = std::wstring(kLayoutsKey) + klid;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key.c_str(), L"Layout Id", RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return static_cast<WORD>(std::wcstoul(value, nullptr, 16));
}

// The loaded HKL that matches a «LLLL:KKKKKKKK» id. HKLs above 0x80000000 come back sign-extended
// on 64-bit Windows, so only the low 32 bits are compared.
HKL findLoaded(const std::wstring& id, const std::vector<HKL>& loaded) {
    auto language = static_cast<WORD>(std::wcstoul(id.substr(0, 4).c_str(), nullptr, 16));
    std::wstring klid = id.substr(5);
    WORD device = 0;
    if (klid.compare(0, 4, L"0000") == 0) {
        device = static_cast<WORD>(std::wcstoul(klid.c_str(), nullptr, 16));
    } else if (auto variant = layoutId(klid)) {
        device = static_cast<WORD>(0xF000 | *variant);
    } else {
        return nullptr;
    }
    for (HKL hkl : loaded) {
        auto value = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(hkl));
        if (LOWORD(value) == language && HIWORD(value) == device) return hkl;
    }
    return nullptr;
}

std::wstring displayName(const std::wstring& klid) {
    std::wstring keyPath = std::wstring(kLayoutsKey) + klid;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) return klid;
    wchar_t name[256];
    DWORD size = 0;
    std::wstring result;
    if (RegLoadMUIStringW(key, L"Layout Display Name", name, sizeof name, &size, 0, nullptr) == ERROR_SUCCESS) {
        result = name;
    } else {
        size = sizeof name;
        if (RegGetValueW(key, nullptr, L"Layout Text", RRF_RT_REG_SZ, nullptr, name, &size) == ERROR_SUCCESS) result = name;
    }
    RegCloseKey(key);
    return result.empty() ? klid : result;
}

std::vector<HKL> loadedLayouts() {
    int count = GetKeyboardLayoutList(0, nullptr);
    std::vector<HKL> list(static_cast<size_t>(count > 0 ? count : 0));
    if (count > 0) list.resize(static_cast<size_t>(GetKeyboardLayoutList(count, list.data())));
    return list;
}

}  // namespace

std::vector<KeyboardLayout> enabledLayouts() {
    std::vector<KeyboardLayout> layouts;
    std::vector<HKL> loaded = loadedLayouts();
    for (const std::wstring& id : enabledIds()) {
        HKL hkl = findLoaded(id, loaded);
        if (!hkl) continue;  // not loaded into this session: ToUnicodeEx would answer for another layout
        layouts.push_back({id, hkl, displayName(id.substr(5))});
    }
    return layouts;
}

std::vector<LayoutTable::Row> layoutRows(HKL layout) {
    std::vector<LayoutTable::Row> rows;
    if (!layout) return rows;  // HKL 0 would silently mean «the layout of the calling thread»
    BYTE state[256] = {};
    for (uint16_t sc : scanCodes()) {
        UINT vk = MapVirtualKeyExW(sc, MAPVK_VSC_TO_VK_EX, layout);
        if (!vk) continue;
        for (bool shift : {false, true}) {
            state[VK_SHIFT] = state[VK_LSHIFT] = shift ? 0x80 : 0;
            wchar_t buffer[16];
            // Flag 4: leave the shared keyboard state alone, so a dead key the user has pending
            // in another program stays pending. Dead keys themselves (negative result) are skipped.
            int length = ToUnicodeEx(vk, sc, state, buffer, static_cast<int>(std::size(buffer)), 0x4, layout);
            if (length <= 0) continue;
            std::wstring text(buffer, static_cast<size_t>(length));
            if (unicode::graphemeCount(text) != 1) continue;
            char32_t first = unicode::firstScalar(text);
            if (unicode::isWhitespace(first) || unicode::isControl(first)) continue;
            rows.push_back({{sc, shift}, std::move(text)});
        }
    }
    return rows;
}

LayoutSet readLayouts(const std::vector<std::wstring>& chosen) {
    LayoutSet set;
    set.layouts = enabledLayouts();
    set.tables.reserve(set.layouts.size());
    for (const auto& layout : set.layouts) set.tables.emplace_back(layout.id, layoutRows(layout.hkl));
    std::vector<const LayoutTable*> tables;
    for (const auto& table : set.tables) tables.push_back(&table);
    set.pair = choosePair(chosen, tables);
    return set;
}

void requestLayout(HWND window, HKL layout) {
    PostMessageW(window, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(layout));
}

}  // namespace klats::app
