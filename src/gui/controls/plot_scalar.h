//==========================================================================
// Name:            plot_scalar.h
// Purpose:         Defines a scalar plot derivative of plot.
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
#ifndef __FDMDV2_PLOT_SCALAR__
#define __FDMDV2_PLOT_SCALAR__

#include <wx/graphics.h>

#include "plot.h"
#include "defines.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class PlotScalar
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class PlotScalar: public PlotPanel
{
    public:

    PlotScalar(wxWindow* parent,
               float t_secs, 
               float sample_period_secs,
               float a_min,
               float a_max,
               float graticule_t_step,   
               float graticule_a_step,
               const char  a_fmt[],
               int   mini,
               const char* plotName = ""
               );
        ~PlotScalar();
         void add_new_sample(float sample);
         void add_new_samples(float samples[], int length);
         void add_new_short_samples(short samples[], int length, float scale_factor);
         void setBarGraph(int bar_graph) { m_bar_graph = bar_graph; }
         void setLogY(int logy) { m_logy = logy; }

         void clearSamples();
         virtual void refreshData() override;

    private:
        struct MinMaxPoints
        {
            int y1;
            int y2;
        };

        MinMaxPoints* lineMap_;
 
    protected:

         float    m_t_secs;
         float    m_sample_period_secs;
         float    m_a_min;
         float    m_a_max;
         float    m_graticule_t_step;   
         float    m_graticule_a_step;
         char     m_a_fmt[15];
         int      m_mini;
         int      m_samples;
         float   *m_mem;   
         int      m_bar_graph;                 // non zero to plot bar graphs 
         int      m_logy;                      // plot graph on log scale
         int      leftOffset_;
         int      bottomOffset_;

         wxBitmap* plotArea_;
         int addedPoints_;
         wxMemoryDC* plotAreaDC_;

         void draw(wxGraphicsContext* ctx, bool repaintDataOnly = false) override;
         void drawGraticuleFast(wxGraphicsContext* ctx, bool repaintDataOnly);
         void OnSize(wxSizeEvent& event) override;
         void OnShow(wxShowEvent& event) override;

         virtual bool repaintAll_(wxPaintEvent& evt) override;

         DECLARE_EVENT_TABLE()
};

#endif // __FDMDV2_PLOT_SCALAR__

