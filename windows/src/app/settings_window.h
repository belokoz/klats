#pragma once
#include "hotkey_recorder.h"
#include "input_sources.h"
#include "settings_store.h"

#include <windows.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace klats::app {

class HookThread;

// «Параметры Клаца»: one window, four groups, no OK or Cancel — every change applies at once, as on
// the Mac. A modeless dialog: the main loop passes its messages through IsDialogMessage.
class SettingsWindow {
public:
    SettingsWindow(HINSTANCE instance, SettingsStore& settings, HookThread& hooks, std::function<void()> hotkeysChanged)
        : instance_(instance), settings_(settings), hooks_(hooks), hotkeysChanged_(std::move(hotkeysChanged)) {}

    void show();
    HWND window() const { return window_; }

private:
    static INT_PTR CALLBACK dialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR handle(UINT message, WPARAM wParam, LPARAM lParam);

    void initialize();
    void loadLayouts();
    void refreshAutostart();
    void refreshResetButtons();
    void startRecording(HotkeyRecorder& recorder);
    void stopRecording();
    void recordKey(UINT vk, bool pressed, Modifiers held);
    void commit(HotkeyRecorder& recorder, std::optional<Hotkey> hotkey);
    void showError(const std::wstring& message, bool warning = false);
    std::optional<Hotkey>& stored(HotkeyRecorder& recorder, Settings& settings);
    HotkeyRecorder& other(HotkeyRecorder& recorder) { return &recorder == &layout_ ? case_ : layout_; }
    void collapse(int fromDialogUnitsY, int byDialogUnits, std::initializer_list<int> hidden);

    HINSTANCE instance_;
    SettingsStore& settings_;
    HookThread& hooks_;
    std::function<void()> hotkeysChanged_;
    HWND window_ = nullptr;
    HFONT headerFont_ = nullptr;
    HotkeyRecorder layout_;
    HotkeyRecorder case_;
    HotkeyRecorder* recording_ = nullptr;
    std::vector<KeyboardLayout> layouts_;
    COLORREF errorColor_ = RGB(0xC4, 0x2B, 0x1C);
    bool errorIsWarning_ = false;
};

}  // namespace klats::app
