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
// Segments are segmentDb wide, spanning minDb..maxDb. Each is drawn as a
// separate LED, centred in its segment with a small gap to its neighbours.
// With zone colours on, LEDs are green, then amber from amberStartDb, then
// red from redStartDb. With zone colours off, every LED is the same fixed
// blue. Fixed colours (not theme colours) keep the meter readable when the
// window is unfocused. Each LED is outlined in its own colour; unlit LEDs
// are left empty.
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

        // For automated testing: the level last set (after clamping), the
        // meter's range and where its amber and red zones start, all in dB.
        float GetLevelDb() const { return currentDb_; }
        float GetMinDb() const { return minDb_; }
        float GetMaxDb() const { return maxDb_; }
        float GetAmberStartDb() const { return amberStartDb_; }
        float GetRedStartDb() const { return redStartDb_; }
        bool GetZoneColours() const { return zoneColours_; }

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
