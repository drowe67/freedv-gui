#ifndef FREEDV_DISPLAY_FRAME_H
#define FREEDV_DISPLAY_FRAME_H

#include <wx/frame.h>
#include <functional>

// An ordinary frame hosting one existing plot. Closing hides it; its owning
// main frame destroys it only after the application's normal shutdown.
class DisplayFrame : public wxFrame
{
public:
    DisplayFrame(wxWindow* owner, const wxString& caption, const wxSize& plotSize);
    bool Attach(wxWindow& plot);
    bool Detach();
    void SetHideHandler(std::function<void()> handler);

private:
    wxWindow* plot_ = nullptr;
    std::function<void()> hideHandler_;
};

#endif
