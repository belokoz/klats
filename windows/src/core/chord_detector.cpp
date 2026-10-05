#include "chord_detector.h"

namespace klats {

bool ChordDetector::flagsChanged(Modifiers now, double time) {
    Modifiers last = last_;
    last_ = now;
    switch (state_) {
    case State::Idle:
        if (now == chord_ && last.isStrictSubsetOf(chord_)) {
            state_ = State::Armed;
            armedAt_ = time;
        } else if (!now.isSubsetOf(chord_)) {
            state_ = State::Blocked;
        }
        return false;

    case State::Armed:
        if (now == chord_) return false;
        if (now.isStrictSubsetOf(chord_)) {
            // Released. Require a full release before arming again, so letting go of the second
            // key never fires a second time.
            state_ = now.empty() ? State::Idle : State::Blocked;
            return time - armedAt_ <= maxHold_;
        }
        state_ = State::Blocked;
        return false;

    case State::Blocked:
        if (now.empty()) state_ = State::Idle;
        return false;
    }
    return false;
}

void ChordDetector::otherInput() {
    if (!last_.empty()) state_ = State::Blocked;
}

void ChordDetector::reset() {
    state_ = State::Idle;
    last_ = {};
    armedAt_ = 0;
}

}  // namespace klats
