#pragma once
#include "modifiers.h"

namespace klats {

// Detects a hotkey made of modifier keys only, such as Win+Alt.
//
// Windows cannot register such a hotkey, so Klats watches modifier state itself. The chord fires on
// release, and only if nothing else happened while it was held. That keeps Win+Alt+R, Win+Alt+D,
// Alt-drag and every other real shortcut working exactly as before. A port of the Mac detector.
class ChordDetector {
public:
    // Holding the chord longer than maxHold seconds means the user was doing something else.
    explicit ChordDetector(Modifiers chord, double maxHold = 1.0) : chord_(chord), maxHold_(maxHold) {}

    Modifiers chord() const { return chord_; }

    // Feed every change of the set of held modifiers, with the time in seconds. Returns true when
    // the chord fires. Key repeat of a held modifier is not a change and must not be fed.
    bool flagsChanged(Modifiers now, double time);

    // Feed every key press, mouse press and scroll. Any of them while a modifier is down means the
    // user is in the middle of a real shortcut, so this press can no longer be the chord.
    void otherInput();

    // Forget everything, for example after the session was locked mid-press.
    void reset();

private:
    enum class State { Idle, Armed, Blocked };

    Modifiers chord_;
    double maxHold_;
    State state_ = State::Idle;
    double armedAt_ = 0;
    Modifiers last_;
};

}  // namespace klats
