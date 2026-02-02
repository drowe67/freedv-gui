#ifndef WX_LIST_VIEW_COMBO_POPUP_H
#define WX_LIST_VIEW_COMBO_POPUP_H

#include <wx/combo.h>
#include <wx/listctrl.h>

class wxListViewComboPopup : public wxListView, public wxComboPopup
{
public:
    wxListViewComboPopup(wxWindow* focusCtrlOnDeselect = nullptr);
    virtual ~wxListViewComboPopup() = default;

    // Initialize member variables
    virtual void Init();
    
    // Create popup control
    virtual bool Create(wxWindow* parent);
    
    // Return pointer to the created control
    virtual wxWindow *GetControl();
    
    // Translate string into a list selection
    virtual void SetStringValue(const wxString& s);
    
    // Get list selection as a string
    virtual wxString GetStringValue() const;
    
    // Do mouse hot-tracking (which is typical in list popups)
    void OnMouseMove(wxMouseEvent& event);
    
    // On mouse left up, set the value and close the popup
    void OnMouseClick(wxMouseEvent& WXUNUSED(event));

    // On right mouse click, deselect and close
    void OnRightMouseClick(wxMouseEvent& WXUNUSED(event));

    virtual wxSize GetAdjustedSize(
        int	minWidth,
        int	prefHeight,
        int	maxHeight);

protected:
    int m_value; // current item index

private:
    wxWindow* focusCtrlOnDeselect_;
    wxDECLARE_EVENT_TABLE();
};

#endif // WX_LIST_VIEW_COMBO_POPUP_H
