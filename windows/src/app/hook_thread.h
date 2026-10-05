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

    // Actions go to `actionTarget` as WM_KLATS_ACTION; WM_KLATS_HOOK_STATUS goes to `statusTarget`.
    bool start(HWND actionTarget, HWND statusTarget, bool installHook);
    void stop();

    void configure(const std::vector<Binding>& bindings);
    void setPaused(bool paused);
    // While `recorder` (the settings window) is in front, every key goes to it as
    // WM_KLATS_RECORD_KEY instead of to Windows, and the hotkeys rest. Null ends the recording.
    void setRecorder(HWND recorder);
    // After unlocking and waking up: Windows may have dropped the hook without a word.
    void reinstall();
    // After locking: key releases went to the lock screen and never came here. A switch to the
    // secure desktop (Ctrl+Alt+Del, UAC) resets the same way on its own.
    void reset();

    bool hookInstalled() const { return hookInstalled_.load(); }

    // When a key last went on to a program (nowMs), modifiers and Klats's own keys aside: a copy
    // made after it may be the user's own.
    static ULONGLONG lastTypedAt();

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
