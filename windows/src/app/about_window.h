#pragma once
#include <windows.h>

namespace klats::app {

// «О программе»: the icon, the version, what Klats does, the links to the repository and to Diktuy,
// the licence. Modal: returns when the window closes. A second call while it is open brings it to
// the front instead.
void showAbout(HINSTANCE instance);

}  // namespace klats::app
