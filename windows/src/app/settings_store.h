#pragma once
#include "core/case_converter.h"
#include "core/hotkey.h"

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace klats::app {

// How Klats copies and pastes: Ctrl+Insert and Shift+Insert, which never interrupt a terminal, or
// Ctrl+C and Ctrl+V for people whose Insert key is turned off by a remapper.
enum class CopyKeys { Insert, Ctrl };

struct Settings {
    std::optional<Hotkey> layoutHotkey = defaults::layoutHotkey;
    std::optional<Hotkey> caseHotkey = defaults::caseHotkey;
    bool switchLayoutAfterConversion = true;
    CaseMode caseMode = CaseMode::Invert;
    // «LANGID:KLID» ids of the pair to convert between. Empty means «pick automatically».
    std::vector<std::wstring> layoutPair;
    bool onboardingCompleted = false;
    bool checkForUpdates = true;
    CopyKeys copyKeys = CopyKeys::Insert;

    // Runtime only: never remembered across launches.
    bool paused = false;
};

// Every preference, persisted in HKCU\Software\Klats. Safe to use from any thread.
class SettingsStore {
public:
    SettingsStore();

    Settings snapshot() const;
    // Applies `change` and writes whatever it changed to the registry.
    void update(const std::function<void(Settings&)>& change);

private:
    mutable std::mutex mutex_;
    Settings settings_;
};

}  // namespace klats::app
