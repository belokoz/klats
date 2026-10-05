#include "login_item.h"

#include "log.h"

#include <windows.h>

#include <string>

namespace klats::app::login_item {

namespace {

const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* kApprovedKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
const wchar_t* kValue = L"Klats";

std::wstring command() {
    wchar_t path[MAX_PATH * 2];
    DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    return L"\"" + std::wstring(path, length) + L"\" --autostart";
}

bool readRun(std::wstring* value) {
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValue, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS) return false;
    std::wstring data(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValue, RRF_RT_REG_SZ, nullptr, data.data(), &size) != ERROR_SUCCESS) return false;
    data.resize(wcsnlen(data.c_str(), data.size()));
    if (value) *value = data;
    return true;
}

// Task Manager writes 12 bytes: 02 (or 00, or nothing at all) means enabled; 03 plus the time it
// was switched off means disabled. Anything else is treated as disabled too, like Electron does.
bool approved() {
    BYTE data[12] = {};
    DWORD size = sizeof data;
    LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, kApprovedKey, kValue, RRF_RT_REG_BINARY, nullptr, data, &size);
    if (status == ERROR_FILE_NOT_FOUND) return true;
    if (status != ERROR_SUCCESS || size == 0) return true;
    return data[0] == 0x02 || data[0] == 0x00;
}

bool writeRun(const std::wstring& value) {
    LSTATUS status = RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kValue, REG_SZ, value.c_str(),
                                     static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    if (status != ERROR_SUCCESS) log::write(L"autostart: could not write the Run value, error " + std::to_wstring(status));
    return status == ERROR_SUCCESS;
}

bool deleteValue(const wchar_t* key, const wchar_t* name) {
    LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, key, name);
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}

}  // namespace

State state() {
    if (!readRun(nullptr)) return State::Off;
    return approved() ? State::On : State::DisabledInTaskManager;
}

bool setEnabled(bool enabled) {
    bool ok = enabled ? writeRun(command()) : deleteValue(kRunKey, kValue);
    // Either way Task Manager's mark goes: on means on, and off leaves nothing behind.
    deleteValue(kApprovedKey, kValue);
    if (ok) log::write(enabled ? L"autostart turned on" : L"autostart turned off");
    return ok && (state() == (enabled ? State::On : State::Off));
}

void refreshLocation(bool startedAtLogin) {
    std::wstring current;
    if (startedAtLogin || !readRun(&current)) return;
    std::wstring wanted = command();
    if (_wcsicmp(current.c_str(), wanted.c_str()) == 0) return;
    if (writeRun(wanted)) log::write(L"autostart now points at the new location of Klats");
}

}  // namespace klats::app::login_item
