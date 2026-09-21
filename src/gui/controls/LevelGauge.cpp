#include "LevelGauge.h"

#include "gui/theme/FreeDVTheme.h"

#include <algorithm>

#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/settings.h>

LevelGauge::LevelGauge(wxWindow* parent, wxWindowID id, int range,
                       const wxPoint& pos, const wxSize& size,
                       FillStyle fillStyle)
    : wxControl(parent, id, pos, size),
      range_(std::max(1, range)),
      value_(0),
      fillStyle_(fillStyle)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

void LevelGauge::SetValue(int value)
{
    const int newValue = std::clamp(value, 0, range_);
    if (newValue != value_)
    {
        value_ = newValue;
        Refresh(false);
    }
}

void LevelGauge::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
    dc.Clear();

    const wxSize size = GetClientSize();
    if (size.x <= 0 || size.y <= 0)
    {
        return;
    }

    auto context = wxGraphicsContext::Create(dc);
    if (!context)
    {
        return;
    }

    const wxColour track = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    context->SetPen(*wxTRANSPARENT_PEN);
    context->SetBrush(wxBrush(track));
    context->DrawRectangle(0, 0, size.x, size.y);

    const double fraction = static_cast<double>(value_) / range_;
    const double fillWidth = size.x * fraction;

    if (fillWidth > 0.0)
    {
        if (fillStyle_ == FillStyle::Gradient)
        {
            wxGraphicsGradientStops stops(
                FreeDVTheme::GetSignalGaugeColour(0.0),
                FreeDVTheme::GetSignalGaugeColour(1.0));
            stops.Add(wxGraphicsGradientStop(
                FreeDVTheme::GetSignalGaugeColour(0.5), 0.50));
            context->SetBrush(context->CreateLinearGradientBrush(
                0, 0, size.x, 0, stops));
        }
        else
        {
            context->SetBrush(wxBrush(
                FreeDVTheme::GetSignalDisplayColour(0.6)));
        }

        context->DrawRectangle(0, 0, fillWidth, size.y);
    }

    delete context;
}

wxBEGIN_EVENT_TABLE(LevelGauge, wxControl)
    EVT_PAINT(LevelGauge::OnPaint)
wxEND_EVENT_TABLE()
