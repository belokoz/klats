#pragma once
#include "core/release.h"

#include <windows.h>

#include <string>

namespace klats::app {

// Once per launch Klats reads the release feed of its GitHub repository and, when a newer Windows
// version is out and its installer is really there, offers to download it. Those requests to
// github.com are the only ones the app makes: no text, no settings, no identifiers go with them.
// The download itself happens in the browser, and the user installs the new version the same way
// as the first one.
namespace updates {

// Checks on a short-lived thread. When a newer version is out and the keyboard has been quiet for
// 3 seconds, `owner` gets WM_KLATS_UPDATE with a WindowsRelease* in lParam (the receiver deletes it).
void check(HWND owner, const AppVersion& current);

// «Вышел Клац X.Y.Z» with «Скачать» and «Не сейчас»; Esc means «Не сейчас».
void offer(HWND owner, HINSTANCE instance, const WindowsRelease& release, const AppVersion& current);

}  // namespace updates

}  // namespace klats::app
