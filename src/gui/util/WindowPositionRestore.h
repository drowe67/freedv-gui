//==========================================================================
// Name:            WindowPositionRestore.h
// Purpose:         Helper to enable restoration of window position.
// Created:         Jul 14, 2026
// Authors:         Barry Jackson
// 
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#ifndef WINDOW_POSITION_RESTORE_H
#define WINDOW_POSITION_RESTORE_H

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

#endif // WINDOW_POSITION_RESTORE_H
