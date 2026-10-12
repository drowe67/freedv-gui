//==========================================================================
// Name:            PathDialogWrapper.h
//
// Purpose:         Wraps the display of file and folder choosers so that
//                  the GUI test agent can stand in for the user.
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
#ifndef __PATH_DIALOG_WRAPPER__
#define __PATH_DIALOG_WRAPPER__

#include <functional>

#include "wx/wx.h"

// Set by the GUI test agent (see test_agent.cpp) while a test script is
// driving FreeDV. Receives the title of each file or folder chooser in place
// of it being displayed. Returns false if the script wants it cancelled;
// otherwise, provides what the script wants chosen.
inline std::function<bool(const wxString&, wxString&)> g_pathDialogTestHook;

// Displays a wxFileDialog or wxDirDialog as dialog.ShowModal() would. If
// something was chosen (i.e. this doesn't return wxID_CANCEL), path is set
// to it.
template<typename DialogType>
int ShowPathDialog(DialogType& dialog, wxString& path)
{
    if (g_pathDialogTestHook)
    {
        return g_pathDialogTestHook(dialog.GetMessage(), path) ? wxID_OK : wxID_CANCEL;
    }

    int result = dialog.ShowModal();
    if (result != wxID_CANCEL)
    {
        path = dialog.GetPath();
    }
    return result;
}

#endif // __PATH_DIALOG_WRAPPER__
