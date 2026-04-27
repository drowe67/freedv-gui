//==========================================================================
// Name:            dlg_capture_ptt.h
// Purpose:         Captures PTT key button for user config.
//                  
// Created:         April 27, 2026
// Authors:         Mooneer Salem
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
#ifndef CAPTURE_PTT_DIALOG_H
#define CAPTURE_PTT_DIALOG_H

#include <wx/dialog.h>
#include <wx/eventfilter.h>

// Small dialog that captures a single keypress for PTT key assignment.
// Inherits wxEventFilter so it intercepts key events at the app level,
// matching the same mechanism MainApp::FilterEvent uses for PTT itself.
class PTTKeyCaptureDlg : public wxDialog, public wxEventFilter
{
public:
    PTTKeyCaptureDlg(wxWindow* parent);
    ~PTTKeyCaptureDlg();

    int GetPTTKeyCode() const;

    virtual int FilterEvent(wxEvent& event) override;

private:
    int m_keyCode;
};

#endif // CAPTURE_PTT_DIALOG_H
