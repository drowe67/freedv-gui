//==========================================================================
// Name:            plot_waterfall.h
// Purpose:         Defines a waterfall plot derivative of plot.
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
#ifndef __FDMDV2_PLOT_WATERFALL__
#define __FDMDV2_PLOT_WATERFALL__

#include <wx/graphics.h>

#include "plot.h"
#include "../../defines.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class PlotWaterfall
//
// @class $(Name)
// @author $(User)
// @date $(Date)
// @file $(CurrentFileName).$(CurrentFileExt)
// @brief
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class PlotWaterfall : public PlotPanel
{
    public:
        PlotWaterfall(wxWindow* parent, bool graticule, int colour);
        ~PlotWaterfall();
        bool checkDT(void);
        void setGreyscale(bool greyscale) { m_greyscale = greyscale; }
        void setRxFreq(float rxFreq) { m_rxFreq = rxFreq; }
        void setFs(int fs) { m_modem_stats_max_f_hz = fs/2; }
        void setColor(int color) { m_colour = color; }
        
    protected:
        unsigned    m_heatmap_lut[256];

        unsigned    heatmap(float val, float min, float max);

        void        OnSize(wxSizeEvent& event);
        void        OnShow(wxShowEvent& event);
        void        drawGraticule(wxGraphicsContext* ctx);
        void        draw(wxGraphicsContext* gc);
        void        plotPixelData();
        void        OnMouseLeftDoubleClick(wxMouseEvent& event);
        void        OnMouseRightDoubleClick(wxMouseEvent& event);
        void        OnMouseMiddleDown(wxMouseEvent& event);
        void        OnMouseWheelMoved(wxMouseEvent& event);
        void        OnKeyDown(wxKeyEvent& event);

    private:
        float       m_dT;
        float       m_rxFreq;
        bool        m_graticule;
        float       m_min_mag;
        float       m_max_mag;
        int         m_colour;
        int         m_modem_stats_max_f_hz;

        wxBitmap* m_fullBmp;
        int m_imgHeight;
        int m_imgWidth;
        
        void        OnDoubleClickCommon(wxMouseEvent& event);

        DECLARE_EVENT_TABLE()
};

#endif //__FDMDV2_PLOT_WATERFALL__
