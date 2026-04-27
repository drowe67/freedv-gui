//==========================================================================
// Name:            dlg_capture_ptt.cpp
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

#include "dlg_capture_ptt.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>

PTTKeyCaptureDlg::PTTKeyCaptureDlg(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("Set PTT Key"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxWANTS_CHARS)
    , m_keyCode(WXK_SPACE)
{
    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(this, wxID_ANY, _("Press any key to use for PTT...")),
               0, wxALL | wxALIGN_CENTER, 15);
    sizer->Add(new wxButton(this, wxID_CANCEL, _("Cancel")),
               0, wxALL | wxALIGN_CENTER, 5);
    SetSizer(sizer);
    sizer->Fit(this);

    wxEvtHandler::AddFilter(this);
}

PTTKeyCaptureDlg::~PTTKeyCaptureDlg()
{
    wxEvtHandler::RemoveFilter(this);
}

int PTTKeyCaptureDlg::GetPTTKeyCode() const 
{ 
    return m_keyCode; 
}

int PTTKeyCaptureDlg::FilterEvent(wxEvent& event)
{
    if (event.GetEventType() != wxEVT_KEY_DOWN || !IsShown())
        return Event_Skip;


    int keyCode = static_cast<wxKeyEvent&>(event).GetKeyCode();
    // Ignore modifier-only and Escape keypresses
    if (keyCode == WXK_SHIFT || keyCode == WXK_CONTROL || keyCode == WXK_ALT ||
        keyCode == WXK_CAPITAL || keyCode == WXK_NUMLOCK || keyCode == WXK_SCROLL ||
        keyCode == WXK_ESCAPE || keyCode == WXK_NONE)
    {
        return Event_Skip;
    }
    m_keyCode = keyCode;
    EndModal(wxID_OK);
    return Event_Processed;
}