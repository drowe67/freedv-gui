#include "DisplayFrame.h"

#include <wx/sizer.h>

DisplayFrame::DisplayFrame(wxWindow* owner, const wxString& caption, const wxSize& plotSize)
    : wxFrame(owner, wxID_ANY, caption)
{
    SetSizer(new wxBoxSizer(wxVERTICAL));
    SetClientSize(plotSize);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        Layout();
        // Plot size handlers rebuild geometry, but partial data repaints do
        // not erase old axes. The notebook gets this from MainFrame's resize.
        if (plot_ != nullptr)
            plot_->Refresh();
        event.Skip();
    });
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
        Hide();
        if (event.CanVeto())
            event.Veto();
        if (hideHandler_)
            hideHandler_();
        // Do not invoke the default handler, which would destroy the plot.
    });
}

void DisplayFrame::SetHideHandler(std::function<void()> handler)
{
    hideHandler_ = std::move(handler);
}

bool DisplayFrame::Attach(wxWindow& plot)
{
    if (plot_ != nullptr)
        return plot_ == &plot;
    if (plot.GetParent() != this && !plot.Reparent(this))
        return false;
    if (GetSizer()->Add(&plot, 1, wxEXPAND) == nullptr)
        return false;
    plot_ = &plot;
    plot.Show();
    Layout();
    return true;
}

bool DisplayFrame::Detach()
{
    if (plot_ == nullptr)
        return true;
    if (!GetSizer()->Detach(plot_))
        return false;
    plot_->Hide();
    plot_ = nullptr;
    return true;
}
