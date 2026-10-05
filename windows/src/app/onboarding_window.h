#pragma once
#include "settings_store.h"

#include <windows.h>

#include <functional>

namespace klats::app {

// «Добро пожаловать в Клац»: shown once, at the first start. No permission step, unlike the Mac:
// Windows asks for none. Instead it tells where the icon lives, because Windows hides new
// notification icons behind the ^ arrow.
class OnboardingWindow {
public:
    OnboardingWindow(HINSTANCE instance, SettingsStore& settings) : instance_(instance), settings_(settings) {}

    void show();
    HWND window() const { return window_; }

private:
    static INT_PTR CALLBACK dialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR handle(UINT message, WPARAM wParam, LPARAM lParam);
    void initialize();
    void finish();

    HINSTANCE instance_;
    SettingsStore& settings_;
    HWND window_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT headerFont_ = nullptr;
    HICON icon_ = nullptr;
};

}  // namespace klats::app
