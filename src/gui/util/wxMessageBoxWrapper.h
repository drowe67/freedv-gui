//==========================================================================
// Name:            wxMessageBoxWrapper.h
//
// Purpose:         Wraps wxMessageBox to fail tests when called during a
//                  test run (i.e. testName is non-empty), or to hand the
//                  message to the GUI test agent instead of displaying it.
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
#ifndef __WX_MESSAGE_BOX_WRAPPER__
#define __WX_MESSAGE_BOX_WRAPPER__

#include <functional>

#include "wx/wx.h"
#include "wx/msgdlg.h"
#include "util/logging/ulog.h"

extern wxString testName;

// Set by the GUI test agent (see test_agent.cpp) while a test script is
// driving FreeDV. Receives the message, caption and style of each message box
// in place of it being displayed, and returns the button to report as pressed.
inline std::function<int(const wxString&, const wxString&, int)> g_wxMessageBoxTestHook;

// NOTE: This function is defined before the #define below so that the call to
// wxMessageBox inside its body resolves to the real wx function, not this
// wrapper (the macro is not yet active during preprocessing of this body).
inline int wxMessageBoxWrapper(const wxString& message,
                                const wxString& caption = wxMessageBoxCaptionStr,
                                int style = wxOK | wxCENTRE,
                                wxWindow* parent = NULL,
                                int x = wxDefaultCoord,
                                int y = wxDefaultCoord)
{
    if (!testName.empty())
    {
        log_fatal("wxMessageBox called during test '%s': %s",
                  (const char*)testName.ToUTF8(),
                  (const char*)message.ToUTF8());
        exit(1); // NOLINT
    }
    if (g_wxMessageBoxTestHook)
    {
        return g_wxMessageBoxTestHook(message, caption, style);
    }
    return wxMessageBox(message, caption, style, parent, x, y);
}

#define wxMessageBox wxMessageBoxWrapper

// Displays a wxMessageDialog as dialog.ShowModal() would, unless a test
// script is driving FreeDV (see g_wxMessageBoxTestHook).
inline int ShowMessageDialog(wxMessageDialog& dialog)
{
    if (g_wxMessageBoxTestHook)
    {
        switch (g_wxMessageBoxTestHook(dialog.GetMessage(), dialog.GetCaption(), dialog.GetMessageDialogStyle()))
        {
            case wxYES:
                return wxID_YES;
            case wxNO:
                return wxID_NO;
            case wxCANCEL:
                return wxID_CANCEL;
            default:
                return wxID_OK;
        }
    }
    return dialog.ShowModal();
}

#endif // __WX_MESSAGE_BOX_WRAPPER__
