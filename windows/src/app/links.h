#pragma once
#include <windows.h>

namespace klats::app {

// Links in Klats's windows are SysLink controls drawn as plain blue text: SysLink underlines its
// links and has no style against it. A SysLink with LWS_USECUSTOMTEXT asks its parent before it
// draws each piece of text; for a link the parent hands it the dialog's own font and the link
// colour, and the control draws as usual. Call from the dialog procedure on WM_NOTIFY; returns
// whether the notification was this one.
bool drawPlainLink(HWND dialog, LPARAM lParam);

// Opens a link in the browser.
void openLink(const wchar_t* url);

}  // namespace klats::app
