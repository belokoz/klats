#pragma once
#include "settings_store.h"

#include <windows.h>

namespace klats::app {

// The key presses Klats sends on the user's behalf: copy, paste and the menu mask, nothing else.
// Every event carries kInjectedMarker, so Klats's own hook lets it through untouched.
namespace synthetic {

// Ctrl+Insert, or Ctrl+C. One SendInput call, so the user's typing cannot get in between.
void copy(CopyKeys keys, HKL layout);
// Shift+Insert, or Ctrl+V.
void paste(CopyKeys keys, HKL layout);

// A press and release of the unassigned key 0xE8. Sent while Win or Alt is still down, it makes
// Windows think another key was pressed with them, so releasing them opens neither Start nor the
// menu bar (the AutoHotkey technique).
void mask();

// True while the user still holds Shift, Ctrl, Alt or Win. Sending Ctrl+Insert while Win is
// physically down would reach the app as Win+Ctrl+Insert, so the pipeline waits for them first.
bool modifiersAreDown();

}  // namespace synthetic

}  // namespace klats::app
