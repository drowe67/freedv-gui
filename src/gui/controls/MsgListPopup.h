//==========================================================================
// Name:            MsgListPopup.h
// Purpose:         Custom wxComboPopup with per-item right-click context menus
// Created:         April 2026
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

#ifndef MSG_LIST_POPUP_H
#define MSG_LIST_POPUP_H

#include <functional>
#include <wx/combo.h>
#include <wx/listbox.h>
#include <wx/timer.h>

class MsgListPopup : public wxListBox, public wxComboPopup
{
public:
    using ContextMenuCallback = std::function<void(int)>;

    explicit MsgListPopup(ContextMenuCallback onRightClick);

    bool Create(wxWindow* parent) override;
    wxWindow* GetControl() override { return this; }
    void SetStringValue(const wxString& s) override;
    wxString GetStringValue() const override;
    void OnPopup() override;
    wxSize GetAdjustedSize(int minWidth, int prefHeight, int maxHeight) override;

private:
    void OnLeftUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnRightDown(wxMouseEvent& event);
    void OnRightUp(wxMouseEvent& event);
    void OnDeselect(wxTimerEvent& event);

    ContextMenuCallback onRightClick_;
    int pendingContextItem_ = wxNOT_FOUND;
    wxTimer deselTimer_;
};

#endif // MSG_LIST_POPUP_H
