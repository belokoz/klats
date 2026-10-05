#pragma once

// «Launch at login» through the Run key of the current user, the place Task Manager's Startup tab
// and Settings → Apps → Startup manage. Turning it off there leaves the Run value in place and
// marks it in StartupApproved, so both are read to show the true state.
namespace klats::app::login_item {

enum class State {
    Off,
    On,
    DisabledInTaskManager,  // the Run value is there, but the user switched it off in Task Manager
};

State state();

// Returns false when the change did not happen, so a checkbox can stay where it was. Turning it on
// from Klats is an explicit wish, so it also clears Task Manager's «disabled» mark.
bool setEnabled(bool enabled);

// After a manual start (not from the Run key): when the exe has moved since autostart was turned
// on, the Run value is pointed at the new place. A program started from the Run key must not
// rewrite it, and needs not: the path it was started from is the right one.
void refreshLocation(bool startedAtLogin);

}  // namespace klats::app::login_item
