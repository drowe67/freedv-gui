#ifndef WX_LIST_VIEW_COMBO_POPUP_H
#define WX_LIST_VIEW_COMBO_POPUP_H

#include <wx/combo.h>
#include <wx/listctrl.h>

class wxListViewComboPopup : public wxListView, public wxComboPopup
{
public:
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
    
protected:
    int m_value; // current item index
private:
    wxDECLARE_EVENT_TABLE();
};

#endif // WX_LIST_VIEW_COMBO_POPUP_H