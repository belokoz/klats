#include "log.h"

#include "common.h"

#include <shlobj.h>

#include <cstdio>
#include <mutex>

namespace klats::app::log {

namespace {

std::mutex mutex;
std::wstring path;

std::wstring folder() {
    PWSTR local = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) result = std::wstring(local) + L"\\Klats";
    CoTaskMemFree(local);
    return result;
}

}  // namespace

void open() {
    std::lock_guard lock(mutex);
    std::wstring directory = folder();
    if (directory.empty()) return;
    CreateDirectoryW(directory.c_str(), nullptr);
    path = directory + L"\\klats.log";
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes) &&
        (attributes.nFileSizeHigh > 0 || attributes.nFileSizeLow > 1024 * 1024)) {
        MoveFileExW(path.c_str(), (path + L".old").c_str(), MOVEFILE_REPLACE_EXISTING);
    }
}

void write(std::wstring_view message) {
    SYSTEMTIME time;
    GetLocalTime(&time);
    wchar_t stamp[32];
    swprintf_s(stamp, L"%04u-%02u-%02u %02u:%02u:%02u.%03u ", time.wYear, time.wMonth, time.wDay, time.wHour,
               time.wMinute, time.wSecond, time.wMilliseconds);
    std::string line = utf8(std::wstring(stamp) + std::wstring(message) + L"\n");

    std::lock_guard lock(mutex);
    if (path.empty()) return;
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    CloseHandle(file);
}

std::wstring filePath() {
    std::lock_guard lock(mutex);
    return path;
}

}  // namespace klats::app::log
