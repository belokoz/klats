#pragma once
#include "common.h"
#include "settings_store.h"

#include <windows.h>

namespace klats::app {

// The whole job of Klats in one place: take the selected text out of the front program, transform
// it, put it back, and leave the clipboard as it was. Runs on its own thread, whose invisible
// window owns the clipboard while a conversion is in flight.
//
// It goes through the clipboard with synthetic copy and paste keys because that works in every
// program where copy and paste work.
class Pipeline {
public:
    explicit Pipeline(SettingsStore& settings) : settings_(settings) {}
    ~Pipeline() { stop(); }
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    bool start();
    // Puts the clipboard back if a conversion is in flight, then ends the thread.
    void stop();

    // Where hotkeys and the menu send WM_KLATS_ACTION.
    HWND window() const { return window_; }
    void request(Action action, bool fromMenu);

private:
    static DWORD WINAPI threadMain(void* self);
    void run(Action action, bool fromMenu, DWORD postedAt);

    SettingsStore& settings_;
    HANDLE thread_ = nullptr;
    HANDLE ready_ = nullptr;
    HANDLE cancel_ = nullptr;
    HWND window_ = nullptr;
    DWORD lastRunEnd_ = 0;
};

}  // namespace klats::app
