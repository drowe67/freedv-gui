// Centralized, platform-neutral UI design values for FreeDV.
#ifndef FREEDV_GUI_THEME_H
#define FREEDV_GUI_THEME_H

#include <wx/colour.h>
#include <wx/font.h>

class wxWindow;

namespace FreeDVTheme
{
enum class AppearanceMode
{
    System = 0,
    Light = 1,
    Dark = 2
};

struct Palette
{
    wxColour windowBackground;
    wxColour panelSurface;
    wxColour primaryText;
    wxColour secondaryText;
    wxColour border;
    wxColour accent;
    wxColour success;
    wxColour warning;
    wxColour danger;
};

enum class TypographyRole
{
    Body,
    Secondary,
    Emphasized,
    Heading
};

// Session-only selection, disabled by default. Set on the UI thread during
// startup, before creating windows; this does not restyle existing windows.
void SetDarkModeEnabled(bool enabled);

// UI-thread APIs, to be called after wxWidgets initialization. The palette
// supplies dark design values without changing system appearance or preferences.
const Palette& GetPalette();

// Existing FreeDV signal display colour schemes. Numeric values match the
// persisted /Waterfall/Color setting for backward compatibility.
enum class SignalDisplayStyle
{
    Multicolor = 0,
    BlackAndWhite = 1,
    BlueTint = 2
};

void SetSignalDisplayStyle(SignalDisplayStyle style);
SignalDisplayStyle GetSignalDisplayStyle();

// Maps a normalized signal value to the selected display colour.
// Position is clamped to 0.0-1.0.
wxColour GetSignalDisplayColour(double position);

// Returns a higher-visibility version of the selected signal display colour
// for signal plots drawn over dark backgrounds.
wxColour GetSignalTraceColour(double position);

// Maps a normalized signal value to a gauge-appropriate version of the
// selected display style.
wxColour GetSignalGaugeColour(double position);

// Derive roles from the platform's default GUI font, preserving its face and
// using point sizes rather than fixed pixels. Applying fonts is opt-in.
wxFont GetFont(TypographyRole role);

// No-op unless dark mode is enabled. Apply only this window's client background
// and primary foreground using non-inheritable colours. Does not traverse
// children, change fonts, or theme native window decorations. Existing semantic
// control colours stay separate.
void ApplyWindowSurface(wxWindow& window);
}

#endif // FREEDV_GUI_THEME_H
