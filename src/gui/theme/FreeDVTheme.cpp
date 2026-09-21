#include "FreeDVTheme.h"

#include <algorithm>
#include <wx/settings.h>
#include <wx/window.h>

namespace FreeDVTheme
{
namespace
{
bool darkModeEnabled = false;
SignalDisplayStyle signalDisplayStyle = SignalDisplayStyle::Multicolor;
}

void SetDarkModeEnabled(bool enabled)
{
    darkModeEnabled = enabled;
}

const Palette& GetPalette()
{
    static const Palette palette = {
        wxColour(24, 26, 30),    // window background
        wxColour(34, 37, 43),    // elevated panel surface
        wxColour(235, 237, 240), // primary text
        wxColour(170, 177, 188), // secondary text
        wxColour(74, 81, 93),    // border / separator
        wxColour(103, 174, 255), // accent
        wxColour(103, 204, 142), // success
        wxColour(240, 193, 91),  // warning
        wxColour(242, 120, 120)  // danger / error
    };
    return palette;
}

void SetSignalDisplayStyle(SignalDisplayStyle style)
{
    signalDisplayStyle = style;
}

SignalDisplayStyle GetSignalDisplayStyle()
{
    return signalDisplayStyle;
}

wxColour GetSignalDisplayColour(double position)
{
    position = std::clamp(position, 0.0, 1.0);

    unsigned r = 0;
    unsigned g = 0;
    unsigned b = 0;

    switch (signalDisplayStyle)
    {
        case SignalDisplayStyle::Multicolor:
            if (position <= 0.2)
            {
                b = static_cast<unsigned>((position / 0.2) * 255);
            }
            else if (position <= 0.7)
            {
                b = static_cast<unsigned>((1.0 - ((position - 0.2) / 0.5)) * 255);
            }

            if (position >= 0.2 && position <= 0.6)
            {
                g = static_cast<unsigned>(((position - 0.2) / 0.4) * 255);
            }
            else if (position > 0.6 && position <= 0.9)
            {
                g = static_cast<unsigned>((1.0 - ((position - 0.6) / 0.3)) * 255);
            }

            if (position >= 0.5)
            {
                r = static_cast<unsigned>(((position - 0.5) / 0.5) * 255);
            }
            break;

        case SignalDisplayStyle::BlackAndWhite:
            r = g = b = static_cast<unsigned>(position * 255);
            break;

        case SignalDisplayStyle::BlueTint:
        {
            const unsigned intensity = static_cast<unsigned>(position * 255);
            r = intensity;
            g = intensity;
            b = intensity < 127 ? intensity * 2 : 255;
            break;
        }
    }

    return wxColour(r, g, b);
}

wxColour GetSignalTraceColour(double position)
{
    const wxColour colour = GetSignalDisplayColour(position);
    constexpr double visibilityLift = 0.35;

    const auto lift = [](unsigned char component)
    {
        return static_cast<unsigned char>(
            component + (255 - component) * visibilityLift);
    };

    return wxColour(
        lift(colour.Red()),
        lift(colour.Green()),
        lift(colour.Blue()));
}

wxColour GetSignalGaugeColour(double position)
{
    position = std::clamp(position, 0.0, 1.0);

    if (signalDisplayStyle == SignalDisplayStyle::Multicolor)
    {
        const wxColour blue(35, 110, 220);
        const wxColour green(45, 210, 75);
        const wxColour red(220, 55, 45);

        const wxColour& start = position <= 0.5 ? blue : green;
        const wxColour& end = position <= 0.5 ? green : red;
        const double amount =
            position <= 0.5 ? position * 2.0 : (position - 0.5) * 2.0;

        const auto interpolate = [amount](unsigned char from, unsigned char to)
        {
            return static_cast<unsigned char>(from + (to - from) * amount);
        };

        return wxColour(
            interpolate(start.Red(), end.Red()),
            interpolate(start.Green(), end.Green()),
            interpolate(start.Blue(), end.Blue()));
    }

    return GetSignalTraceColour(position);
}

wxFont GetFont(TypographyRole role)
{
    wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    switch (role)
    {
        case TypographyRole::Body:
            break;
        case TypographyRole::Secondary:
            font.SetPointSize(std::max(1, font.GetPointSize() - 1));
            break;
        case TypographyRole::Emphasized:
            font.SetWeight(wxFONTWEIGHT_BOLD);
            break;
        case TypographyRole::Heading:
            font.SetPointSize(font.GetPointSize() + 2);
            font.SetWeight(wxFONTWEIGHT_BOLD);
            break;
    }
    return font;
}

void ApplyWindowSurface(wxWindow& window)
{
    if (!darkModeEnabled)
    {
        return;
    }

    const auto& palette = GetPalette();
    window.SetOwnBackgroundColour(palette.windowBackground);
    window.SetOwnForegroundColour(palette.primaryText);
}
}
