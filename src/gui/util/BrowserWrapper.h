//==========================================================================
// Name:            BrowserWrapper.h
//
// Purpose:         Wraps the opening of web pages so that the GUI test
//                  agent can stand in for the user's browser.
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
#ifndef __BROWSER_WRAPPER__
#define __BROWSER_WRAPPER__

#include <functional>

#include "wx/wx.h"
#include "wx/utils.h"

// Set by the GUI test agent (see test_agent.cpp) while a test script is
// driving FreeDV. Receives the address of each web page in place of it being
// opened.
inline std::function<void(const wxString&)> g_browserTestHook;

// Opens a web page as wxLaunchDefaultBrowser() would.
inline bool LaunchBrowser(const wxString& url)
{
    if (g_browserTestHook)
    {
        g_browserTestHook(url);
        return true;
    }

    return wxLaunchDefaultBrowser(url);
}

#endif // __BROWSER_WRAPPER__
