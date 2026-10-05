#pragma once
#include "core/hotkey.h"

#include <windows.h>

#include <optional>
#include <string>

namespace klats::app {

// A field that records a shortcut, including one made of modifiers only, such as Win+Alt. Click,
// Space or Enter to record, press the combination; Esc cancels, Backspace removes the shortcut.
//
// The field is an owner-drawn button, so screen readers know it as a button named after its
// label and value. While it records, the keys come from Klats's low-level hook: a window of its
// own would never see Win combinations, and releasing Win would open Start.
class HotkeyRecorder {
public:
    enum class Outcome { None, Cancelled, Cleared, Candidate };

    void attach(HWND button, std::wstring label, std::optional<Hotkey> value);
    HWND button() const { return button_; }

    std::optional<Hotkey> value() const { return value_; }
    void setValue(std::optional<Hotkey> value);

    bool recording() const { return recording_; }
    void startRecording();
    void stopRecording();

    // One key event while recording: `held` is the set of modifiers down after it.
    Outcome key(UINT vk, bool pressed, Modifiers held, Hotkey* candidate);

    void draw(const DRAWITEMSTRUCT& item) const;

private:
    void refresh();

    HWND button_ = nullptr;
    std::wstring label_;
    std::optional<Hotkey> value_;
    bool recording_ = false;
    Modifiers held_;          // every modifier pressed during this attempt
    bool keyPressed_ = false; // a non-modifier key was pressed during this attempt
};

}  // namespace klats::app
