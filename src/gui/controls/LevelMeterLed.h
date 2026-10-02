//==========================================================================
// Name:            LevelMeterLed.h
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
#ifndef __FREEDV_LEVEL_METER_LED__
#define __FREEDV_LEVEL_METER_LED__

#include <wx/wx.h>

// LED bargraph level meter. A wxGauge has only integer resolution across a
// narrow width and no smoothing of its own, so a steady dB/sec decay looks
// jittery on it; discrete LED steps are the expected look for a peak meter.
// Custom-drawn so it looks and behaves the same on every platform.
//
// Segments are segmentDb wide, spanning minDb..maxDb, butted together with
// no gaps. With zone colours on, segments are green, then amber from
// amberStartDb, then red from redStartDb. With zone colours off, every
// segment is the same fixed blue. Fixed colours (not theme colours) keep
// the meter readable when the window is unfocused. Unlit segments show a
// dim version of their lit colour.
class LevelMeterLed : public wxWindow
{
    public:
        LevelMeterLed(
            wxWindow* parent, wxWindowID id,
            float minDb, float maxDb, float segmentDb,
            float amberStartDb, float redStartDb,
            int segmentWidthPx, int segmentHeightPx,
            const wxPoint& pos = wxDefaultPosition);
        virtual ~LevelMeterLed() = default;

        // Sets the displayed level, clamped to [minDb, maxDb]. Only
        // repaints if the number of lit segments changes.
        void SetLevelDb(float db);

        // Equivalent to SetLevelDb(minDb) -- all segments off.
        void Reset();

        // true: green/amber/red zones. false (default): all segments blue.
        void SetZoneColours(bool enabled);

        virtual wxSize DoGetBestSize() const override;

    private:
        void OnPaint(wxPaintEvent& event);

        float minDb_;
        float maxDb_;
        float segmentDb_;
        float amberStartDb_;
        float redStartDb_;
        int segmentWidthPx_;
        int segmentHeightPx_;
        int numSegments_;
        float currentDb_;
        int litSegments_;
        bool zoneColours_;
};

#endif // __FREEDV_LEVEL_METER_LED__
