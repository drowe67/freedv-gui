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
    using DeleteKeyCallback = std::function<void(int)>;

    struct Options
    {
        bool sorted = true;      // keep the items in alphabetical order
        bool openAbove = true;   // open above the combo rather than below it
        bool hoverPicks = true;  // closing the list in any way takes the
                                 // highlighted item, not only a click on it
    };

    explicit MsgListPopup(ContextMenuCallback onRightClick);
    MsgListPopup(ContextMenuCallback onRightClick, Options const& options);

    // Called with the highlighted item when Del is pressed while the list
    // is open. The list stays open.
    void SetDeleteKeyCallback(DeleteKeyCallback onDeleteKey) { onDeleteKey_ = std::move(onDeleteKey); }

    bool Create(wxWindow* parent) override;
    wxWindow* GetControl() override { return this; }
    void SetStringValue(const wxString& s) override;
    wxString GetStringValue() const override;
    void OnPopup() override;
    wxSize GetAdjustedSize(int minWidth, int prefHeight, int maxHeight) override;

private:
    static constexpr int DESELECT_TIMEOUT_MS_ = 20;

    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnRightDown(wxMouseEvent& event);
    void OnRightUp(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnDeselect(wxTimerEvent& event);

    ContextMenuCallback onRightClick_;
    DeleteKeyCallback onDeleteKey_;
    Options options_;
    bool picked_ = false;
    int pendingContextItem_ = wxNOT_FOUND;
    wxTimer deselTimer_;
    wxPoint mouseDownPos_ = wxDefaultPosition;
};

#endif // MSG_LIST_POPUP_H
