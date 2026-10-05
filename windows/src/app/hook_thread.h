#pragma once
#include "common.h"

#include "core/hotkey.h"

#include <windows.h>

#include <atomic>
#include <vector>

namespace klats::app {

struct Binding {
    Action action;
    Hotkey hotkey;
};

// One low-level keyboard hook for every hotkey, on a thread of its own.
//
// A chord such as Win+Alt is made of modifiers only, which Windows cannot register as a hotkey, so
// Klats watches modifier state itself. Registering Win+Alt+Z through RegisterHotKey next to it
// would not work: the chord detector would never learn that Z was pressed, and letting go of
// Win+Alt afterwards would fire the layout conversion too. With one hook every key arrives in order.
//
// Windows silently skips, and eventually removes, a hook that takes longer than a second to
// answer, so the thread does nothing but run the detector; the work happens on the pipeline thread.
class HookThread {
public:
    HookThread() = default;
    ~HookThread() { stop(); }
    HookThread(const HookThread&) = delete;
    HookThread& operator=(const HookThread&) = delete;

    // Actions go to `actionTarget` as WM_KLATS_ACTION; WM_KLATS_HOOK_FAILED goes to `statusTarget`.
    bool start(HWND actionTarget, HWND statusTarget, bool installHook);
    void stop();

    void configure(const std::vector<Binding>& bindings);
    void setPaused(bool paused);
    // After unlocking and waking up: Windows may have dropped the hook without a word.
    void reinstall();
    // After locking: key releases went to the secure desktop and never came here.
    void reset();

    bool hookInstalled() const { return hookInstalled_.load(); }

private:
    static DWORD WINAPI threadMain(void* self);

    HANDLE thread_ = nullptr;
    DWORD threadId_ = 0;
    HANDLE ready_ = nullptr;
    HWND actionTarget_ = nullptr;
    HWND statusTarget_ = nullptr;
    bool installHook_ = true;
    std::atomic<bool> hookInstalled_{false};
};

}  // namespace klats::app
