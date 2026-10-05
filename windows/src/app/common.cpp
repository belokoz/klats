#include "common.h"

#include <cwctype>

namespace klats::app {

std::wstring utf16(std::string_view utf8) {
    if (utf8.empty()) return {};
    int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), length);
    return result;
}

std::string utf8(std::wstring_view utf16) {
    if (utf16.empty()) return {};
    int length = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring processName(DWORD processId) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return {};
    wchar_t path[MAX_PATH * 2];
    DWORD size = static_cast<DWORD>(std::size(path));
    std::wstring name;
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        std::wstring_view full(path, size);
        size_t slash = full.find_last_of(L"\\/");
        name = std::wstring(slash == std::wstring_view::npos ? full : full.substr(slash + 1));
        for (wchar_t& c : name) c = static_cast<wchar_t>(std::towlower(c));
    }
    CloseHandle(process);
    return name;
}

std::wstring windowClass(HWND window) {
    if (!window) return {};
    wchar_t name[256];
    int length = GetClassNameW(window, name, static_cast<int>(std::size(name)));
    return std::wstring(name, static_cast<size_t>(length > 0 ? length : 0));
}

}  // namespace klats::app
