#ifndef FREEDV_GUI_LEVEL_GAUGE_H
#define FREEDV_GUI_LEVEL_GAUGE_H

#include <wx/control.h>

class LevelGauge : public wxControl
{
public:
    enum class FillStyle
    {
        Gradient,
        Solid
    };

    LevelGauge(wxWindow* parent, wxWindowID id, int range,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               FillStyle fillStyle = FillStyle::Gradient);

    void SetValue(int value);
    int GetValue() const { return value_; }

private:
    void OnPaint(wxPaintEvent& event);

    int range_;
    int value_;
    FillStyle fillStyle_;

    wxDECLARE_EVENT_TABLE();
};

#endif // FREEDV_GUI_LEVEL_GAUGE_H
