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

#include <deque>
#include <vector>
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
        PlotWaterfall(wxWindow* parent, float *magdB, bool graticule, int colour);
        ~PlotWaterfall();
        bool checkDT(void);
        void setGreyscale(bool greyscale) { m_greyscale = greyscale; }
        void setRxFreq(float rxFreq) {
            bool zeroTransition = 
                (rxFreq != 0 && m_rxFreq == 0) ||
                (rxFreq == 0 && m_rxFreq != 0);

            m_rxFreq = rxFreq; 
            if (zeroTransition)
            {
                // Trigger a full redraw when going to/from RADE mode 
                // to make sure frequency indicator renders properly.
                Refresh();
            }
        }
        void setFs(int fs) { m_modem_stats_max_f_hz = fs/2; }
        void setColor(int color) { m_colour = color; }

        virtual void refreshData() override;
        
    protected:
        unsigned    m_heatmap_lut[256];

        unsigned    heatmap(float val, float min, float max);

        void        OnSize(wxSizeEvent& event) override;
        void        OnShow(wxShowEvent& event) override;
        void        drawGraticule(wxGraphicsContext* ctx) override;
        void        draw(wxGraphicsContext* gc, bool repaintDataOnly = false) override;
        void        plotPixelData(wxGraphicsContext* gc);
        void        OnMouseLeftDoubleClick(wxMouseEvent& event);
        void        OnMouseRightDoubleClick(wxMouseEvent& event);
        void        OnMouseMiddleDown(wxMouseEvent& event);
        void        OnMouseWheelMoved(wxMouseEvent& event) override;
        void        OnKeyDown(wxKeyEvent& event);

        virtual bool repaintAll_(wxPaintEvent& evt) override;

    private:
        float*      m_magDb;
        float       m_dT;
        float       m_rxFreq;
        bool        m_graticule;
        float       m_min_mag;
        float       m_max_mag;
        int         m_colour;
        int         m_modem_stats_max_f_hz;
        unsigned char* dyImageData_;
        int dy_;
        wxImage* tmpImage_;

        int m_imgHeight;
        int m_imgWidth;
        
        int      leftOffset_;

        // One "block" of the waterfall: dy pixel rows of spectrum.
        //
        // bitmap is the render target plotPixelData() blits into; gfxBitmap is the
        // renderer-native copy that draw() actually composites. Compositing the wxBitmap
        // directly is what we're avoiding here: on macOS
        // wxGraphicsContext::DrawBitmap(wxBitmap) wraps it in a freshly allocated NSImage
        // every call (wxBitmapRefData::GetImage() caches nothing) and draws through
        // -[NSImage drawInRect:], whereas the wxGraphicsBitmap overload goes straight to
        // CGContextDrawImage. At one call per block and m_imgHeight/dy blocks on screen,
        // that wrapper was the bulk of the paint.
        struct WaterfallSlice
        {
            wxBitmap* bitmap;
            wxGraphicsBitmap gfxBitmap;
        };
        std::deque<WaterfallSlice> waterfallSlices_;

        // Graticule labels only move when the control is resized, but drawGraticule() runs
        // on every frame. Measuring them there is not cheap -- wxWindowMac::DoGetTextExtent
        // builds and destroys a whole wxGraphicsContext per call -- so the laid out text is
        // cached and rebuilt only when the geometry changes.
        struct GraticuleLabel
        {
            wxString text;
            int x;
            int y;
        };
        std::vector<GraticuleLabel> freqLabels_;
        std::vector<GraticuleLabel> timeLabels_;
        bool graticuleLabelsValid_;

        void        OnDoubleClickCommon(wxMouseEvent& event);

        void cleanupSlices_();
        void rebuildGraticuleLabels_();

        DECLARE_EVENT_TABLE()
};

#endif //__FDMDV2_PLOT_WATERFALL__
