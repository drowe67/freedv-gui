//==========================================================================
// Name:            PopupMenuWrapper.h
//
// Purpose:         Wraps wxWindow::PopupMenu() so that the GUI test agent
//                  can stand in for the user choosing from the menu.
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
#ifndef __POPUP_MENU_WRAPPER__
#define __POPUP_MENU_WRAPPER__

#include <functional>

#include "wx/wx.h"

// Set by the GUI test agent (see test_agent.cpp) while a test script is
// driving FreeDV. Receives the window and menu of each popup menu in place of
// it being displayed, and selects whatever the script asked it to (if
// anything) before returning.
inline std::function<void(wxWindow*, wxMenu*)> g_popupMenuTestHook;

// Displays a popup menu as window->PopupMenu(menu, pos) would.
inline bool ShowPopupMenu(wxWindow* window, wxMenu* menu, const wxPoint& pos = wxDefaultPosition)
{
    if (g_popupMenuTestHook)
    {
        g_popupMenuTestHook(window, menu);
        return true;
    }
    return window->PopupMenu(menu, pos);
}

#endif // __POPUP_MENU_WRAPPER__
