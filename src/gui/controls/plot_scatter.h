//==========================================================================
// Name:            plot_scatter.h
// Purpose:         A scatter plot derivative of plot.
// Created:         June 24, 2012
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
#ifndef __FDMDV2_PLOT_SCATTER__
#define __FDMDV2_PLOT_SCATTER__

#include <wx/graphics.h>

#include "comp.h"
#include "plot.h"
#include "../../defines.h"

#define PLOT_SCATTER_MODE_SCATTER            0
#define PLOT_SCATTER_MODE_EYE                1
#define PLOT_SCATTER_EYE_MAX_SAMPLES_ROW    80

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class PlotScatter
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class PlotScatter : public PlotPanel
{
    public:
        PlotScatter(wxWindow* parent);
        ~PlotScatter(){};
        void add_new_samples_scatter(COMP samples[]);
        void add_new_samples_eye(float samples[], int n);
        void setNc(int Nc);
        void setEyeScatter(int eye_mode) {mode = eye_mode;}
        void clearCurrentSamples();

    protected:
        int  mode;
        COMP m_mem[SCATTER_MEM_SYMS_MAX];
        COMP m_new_samples[MODEM_STATS_NC_MAX+1];
        float eye_mem[SCATTER_EYE_MEM_ROWS][PLOT_SCATTER_EYE_MAX_SAMPLES_ROW];

        void draw(wxGraphicsContext* ctx, bool repaintDataOnly = false);
        void OnSize(wxSizeEvent& event);
        void OnShow(wxShowEvent& event);

        DECLARE_EVENT_TABLE()

    private:
        int   Nsym;
        int   Ncol;
        int   scatterMemSyms;
        float m_filter_max_xy, m_filter_max_y;

        bool pointsInBounds_(int x, int y);
};

#endif //__FDMDV2_PLOT_SCATTER__
