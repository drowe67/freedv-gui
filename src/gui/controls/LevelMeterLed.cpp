//==========================================================================
// Name:            LevelMeterLed.cpp
// Purpose:         A compact, custom-drawn LED-style level meter.
// Authors:         Claude Code (for Barry Jackson, G4MKT)
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

#include <cmath>
#include <algorithm>

#include <wx/dcbuffer.h>
#include <wx/settings.h>

#include "LevelMeterLed.h"

LevelMeterLed::LevelMeterLed(
    wxWindow* parent, wxWindowID id,
    float minDb, float maxDb, float segmentDb,
    float amberStartDb, float redStartDb,
    int segmentWidthPx, int segmentHeightPx,
    const wxPoint& pos)
    : wxWindow()
    , minDb_(minDb)
    , maxDb_(maxDb)
    , segmentDb_(segmentDb)
    , amberStartDb_(amberStartDb)
    , redStartDb_(redStartDb)
    , segmentWidthPx_(segmentWidthPx)
    , segmentHeightPx_(segmentHeightPx)
    , currentDb_(minDb)
    , litSegments_(0)
    , zoneColours_(false)
{
    numSegments_ = (int)std::lround((maxDb_ - minDb_) / segmentDb_);

    Create(parent, id, pos, wxSize(segmentWidthPx_ * numSegments_, segmentHeightPx_));
    SetBackgroundStyle(wxBG_STYLE_PAINT); // we draw every pixel ourselves
    Bind(wxEVT_PAINT, &LevelMeterLed::OnPaint, this);
}

wxSize LevelMeterLed::DoGetBestSize() const
{
    return wxSize(segmentWidthPx_ * numSegments_, segmentHeightPx_);
}

void LevelMeterLed::Reset()
{
    SetLevelDb(minDb_);
}

void LevelMeterLed::SetZoneColours(bool enabled)
{
    if (enabled != zoneColours_)
    {
        zoneColours_ = enabled;
        Refresh(false);
    }
}

void LevelMeterLed::SetLevelDb(float db)
{
    if (db < minDb_) db = minDb_;
    if (db > maxDb_) db = maxDb_;
    currentDb_ = db;

    // Segment i lights as soon as the level rises above its lower bound
    // (minDb_ + i*segmentDb_); ceil() so that exactly minDb_ lights none.
    int newLit = (int)std::ceil((currentDb_ - minDb_) / segmentDb_);
    newLit = std::max(0, std::min(numSegments_, newLit));

    if (newLit != litSegments_)
    {
        litSegments_ = newLit;
        Refresh(false); // false: we repaint every pixel ourselves, no need to erase first
    }
}

void LevelMeterLed::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);

    // Normally fully covered by segments; the clear only matters if the
    // widget is ever given more space than DoGetBestSize(). Unstyled
    // parents (e.g. a wxStaticBox on GTK) don't report a usable colour.
    wxWindow* parent = GetParent();
    wxColour background = parent->UseBackgroundColour() ? parent->GetBackgroundColour() : wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
    dc.SetBackground(wxBrush(background));
    dc.Clear();

    static const wxColour GREEN(0, 200, 0);
    static const wxColour AMBER(230, 160, 0);
    static const wxColour RED(220, 30, 30);
    static const wxColour BLUE(30, 120, 230);
    const float DIM_FACTOR = 0.22f;

    dc.SetPen(*wxTRANSPARENT_PEN);

    for (int i = 0; i < numSegments_; i++)
    {
        float lowerDb = minDb_ + i * segmentDb_;
        wxColour zoneColour = !zoneColours_ ? BLUE :
            (lowerDb >= redStartDb_) ? RED : (lowerDb >= amberStartDb_) ? AMBER : GREEN;

        bool lit = i < litSegments_;
        wxColour fillColour = lit
            ? zoneColour
            : wxColour(
                (unsigned char)(zoneColour.Red() * DIM_FACTOR),
                (unsigned char)(zoneColour.Green() * DIM_FACTOR),
                (unsigned char)(zoneColour.Blue() * DIM_FACTOR));

        dc.SetBrush(wxBrush(fillColour));
        dc.DrawRectangle(i * segmentWidthPx_, 0, segmentWidthPx_, segmentHeightPx_);
    }
}
