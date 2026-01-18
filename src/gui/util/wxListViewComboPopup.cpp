#include "wxListViewComboPopup.h"

#include <wx/combo.h>
#include <wx/listctrl.h>

void wxListViewComboPopup::Init()
{
    m_value = -1;
}

// Create popup control
bool wxListViewComboPopup::Create(wxWindow* parent)
{
    return wxListView::Create(parent,1,wxPoint(0,0),wxDefaultSize,wxLC_REPORT | wxLC_SINGLE_SEL);
}

// Return pointer to the created control
wxWindow *wxListViewComboPopup::GetControl() { return this; }

// Translate string into a list selection
void wxListViewComboPopup::SetStringValue(const wxString& s)
{
    int n = wxListView::FindItem(-1,s);
    if ( n >= 0 && n < wxListView::GetItemCount() )
    {
        wxListView::Select(n);
        m_value = n;
    }
    else
    {
        if (m_value != -1)
        {
            wxListView::Select(m_value, false);
        }
        m_value = -1;
    }
}

// Get list selection as a string
wxString wxListViewComboPopup::GetStringValue() const
{
    if ( m_value >= 0 )
    {
        return wxListView::GetItemText(m_value);
    }
    return wxEmptyString;
}

// Do mouse hot-tracking (which is typical in list popups)
void wxListViewComboPopup::OnMouseMove(wxMouseEvent& event)
{
    const wxPoint pt(event.GetX(), event.GetY());

    int flags = wxLIST_HITTEST_ONITEM;
    auto index = wxListView::HitTest(pt, flags);

    if (index != m_value)
    {
        if (m_value != -1)
        {
            Select(m_value, false);
        }
        m_value = index;
        if (m_value != 1)
        {
            Select(m_value, true);
        }
    }
}

// On mouse left up, set the value and close the popup
void wxListViewComboPopup::OnMouseClick(wxMouseEvent& event)
{
    m_value = wxListView::GetFirstSelected();
    if (m_value >= 0)
    {
        // TODO: Send event as well
        Dismiss();
    }
    else
    {
        event.Skip();
    }
}

wxSize wxListViewComboPopup::GetAdjustedSize(
        int	minWidth,
        int	prefHeight,
        int	)
{
    return wxSize(400 < minWidth ? minWidth : 400, prefHeight);
}

wxBEGIN_EVENT_TABLE(wxListViewComboPopup, wxListView)
    EVT_MOTION(wxListViewComboPopup::OnMouseMove)
    EVT_LEFT_UP(wxListViewComboPopup::OnMouseClick)
wxEND_EVENT_TABLE()
