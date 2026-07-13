#pragma once

class wxFrame;

// Restores a top-level frame's position from saved configuration.
//
// Must be called before the frame is shown. On Linux/GTK3, some window
// managers/compositors (observed: KWin on Plasma 6, labwc on Wayland)
// apply their own asynchronous initial-placement policy shortly after the
// window is mapped, silently overriding whatever position the client
// already set -- no matter how early or late the client's own move
// happens, and sometimes even after the position has already happened to
// match once. This re-asserts the requested position for a couple of
// seconds after mapping so it can catch and correct a delayed override,
// then stops so it won't fight the user's own later repositioning. On
// other platforms this is just a straightforward deferred move.
void RestoreWindowPosition(wxFrame* frame, int x, int y);
