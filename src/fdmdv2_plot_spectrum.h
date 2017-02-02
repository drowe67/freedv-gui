//==========================================================================
// Name:            fdmdv2_plot_spectrum.h
// Purpose:         Defines a spectrum plot derived from fdmdv2_plot class.
// Created:         June 22, 2012
// Authors:         David Rowe, David Witten
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
#ifndef __FDMDV2_PLOT_SPECTRUM__
#define __FDMDV2_PLOT_SPECTRUM__

#include "fdmdv2_plot.h"
#include "fdmdv2_defines.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class Waterfall
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class PlotSpectrum : public PlotPanel
{
    public:
    PlotSpectrum(wxFrame* parent, float *magdB, int n_magdB, 
                 float min_mag_db=MIN_MAG_DB, float max_mag_db=MAX_MAG_DB, bool clickTune=true);
        ~PlotSpectrum();
        void setRxFreq(float rxFreq) { m_rxFreq = rxFreq; }
        void setFreqScale(int n_magdB) { m_n_magdB = n_magdB; }

    protected:
        void        OnPaint(wxPaintEvent& event);
        void        OnSize(wxSizeEvent& event);
        void        OnShow(wxShowEvent& event);
        void        drawGraticule(wxAutoBufferedPaintDC& dc);
        void        draw(wxAutoBufferedPaintDC& dc);
        void        OnMouseLeftDoubleClick(wxMouseEvent& event);

   private:
        float       m_rxFreq;
        float       m_max_mag_db;
        float       m_min_mag_db;
        float      *m_magdB;
        int         m_n_magdB;  
        bool        m_clickTune;

        DECLARE_EVENT_TABLE()
};

#endif //__FDMDV2_PLOT_SPECTRUM__
