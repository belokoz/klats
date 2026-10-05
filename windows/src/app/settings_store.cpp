#include "settings_store.h"

#include "log.h"

#include <windows.h>

namespace klats::app {

namespace {

const wchar_t* kKey = L"Software\\Klats";

std::optional<DWORD> readDword(const wchar_t* name) {
    DWORD value = 0;
    DWORD size = sizeof value;
    if (RegGetValueW(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) return std::nullopt;
    return value;
}

std::optional<std::wstring> readString(const wchar_t* name) {
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS) return std::nullopt;
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_SZ, nullptr, value.data(), &size) != ERROR_SUCCESS) return std::nullopt;
    value.resize(wcsnlen(value.c_str(), value.size()));
    return value;
}

std::vector<std::wstring> readStrings(const wchar_t* name) {
    std::vector<std::wstring> result;
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_MULTI_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS) return result;
    std::wstring buffer(size / sizeof(wchar_t) + 2, L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_MULTI_SZ, nullptr, buffer.data(), &size) != ERROR_SUCCESS) return result;
    for (const wchar_t* item = buffer.c_str(); *item; item += wcslen(item) + 1) result.emplace_back(item);
    return result;
}

void writeDword(const wchar_t* name, DWORD value) {
    LSTATUS status = RegSetKeyValueW(HKEY_CURRENT_USER, kKey, name, REG_DWORD, &value, sizeof value);
    if (status != ERROR_SUCCESS) log::write(L"settings: could not save " + std::wstring(name) + L", error " + std::to_wstring(status));
}

void writeString(const wchar_t* name, const std::wstring& value) {
    LSTATUS status = RegSetKeyValueW(HKEY_CURRENT_USER, kKey, name, REG_SZ, value.c_str(),
                                     static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    if (status != ERROR_SUCCESS) log::write(L"settings: could not save " + std::wstring(name) + L", error " + std::to_wstring(status));
}

void writeStrings(const wchar_t* name, const std::vector<std::wstring>& values) {
    std::wstring buffer;
    for (const auto& value : values) {
        buffer += value;
        buffer += L'\0';
    }
    buffer += L'\0';
    LSTATUS status = RegSetKeyValueW(HKEY_CURRENT_USER, kKey, name, REG_MULTI_SZ, buffer.c_str(),
                                     static_cast<DWORD>(buffer.size() * sizeof(wchar_t)));
    if (status != ERROR_SUCCESS) log::write(L"settings: could not save " + std::wstring(name) + L", error " + std::to_wstring(status));
}

}  // namespace

SettingsStore::SettingsStore() {
    if (auto value = readDword(L"LayoutHotkey")) settings_.layoutHotkey = decodeHotkey(*value, defaults::layoutHotkey);
    if (auto value = readDword(L"CaseHotkey")) settings_.caseHotkey = decodeHotkey(*value, defaults::caseHotkey);
    if (auto value = readDword(L"SwitchLayoutAfterConversion")) settings_.switchLayoutAfterConversion = *value != 0;
    if (auto value = readString(L"CaseMode")) settings_.caseMode = *value == L"lowercase" ? CaseMode::Lowercase : CaseMode::Invert;
    settings_.layoutPair = readStrings(L"LayoutPair");
    if (settings_.layoutPair.size() != 2) settings_.layoutPair.clear();
    if (auto value = readDword(L"OnboardingCompleted")) settings_.onboardingCompleted = *value != 0;
    if (auto value = readDword(L"CheckForUpdates")) settings_.checkForUpdates = *value != 0;
    if (auto value = readString(L"CopyKeys")) settings_.copyKeys = *value == L"ctrl" ? CopyKeys::Ctrl : CopyKeys::Insert;
}

Settings SettingsStore::snapshot() const {
    std::lock_guard lock(mutex_);
    return settings_;
}

void SettingsStore::update(const std::function<void(Settings&)>& change) {
    std::lock_guard lock(mutex_);
    Settings before = settings_;
    change(settings_);
    const Settings& after = settings_;
    if (after.layoutHotkey != before.layoutHotkey) writeDword(L"LayoutHotkey", encodeHotkey(after.layoutHotkey));
    if (after.caseHotkey != before.caseHotkey) writeDword(L"CaseHotkey", encodeHotkey(after.caseHotkey));
    if (after.switchLayoutAfterConversion != before.switchLayoutAfterConversion) {
        writeDword(L"SwitchLayoutAfterConversion", after.switchLayoutAfterConversion ? 1 : 0);
    }
    if (after.caseMode != before.caseMode) writeString(L"CaseMode", after.caseMode == CaseMode::Lowercase ? L"lowercase" : L"invert");
    if (after.layoutPair != before.layoutPair) writeStrings(L"LayoutPair", after.layoutPair);
    if (after.onboardingCompleted != before.onboardingCompleted) writeDword(L"OnboardingCompleted", after.onboardingCompleted ? 1 : 0);
    if (after.checkForUpdates != before.checkForUpdates) writeDword(L"CheckForUpdates", after.checkForUpdates ? 1 : 0);
    if (after.copyKeys != before.copyKeys) writeString(L"CopyKeys", after.copyKeys == CopyKeys::Ctrl ? L"ctrl" : L"insert");
}

}  // namespace klats::app
