//==========================================================================
// Name:            plot_scatter.cpp
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
#include <string.h>
#include "wx/wx.h"
#include "plot_scatter.h"

BEGIN_EVENT_TABLE(PlotScatter, PlotPanel)
    EVT_PAINT           (PlotScatter::OnPaint)
    EVT_MOTION          (PlotScatter::OnMouseMove)
    EVT_MOUSEWHEEL      (PlotScatter::OnMouseWheelMoved)
    EVT_SIZE            (PlotScatter::OnSize)
    EVT_SHOW            (PlotScatter::OnShow)
//    EVT_ERASE_BACKGROUND(PlotScatter::OnErase)
END_EVENT_TABLE()

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// PlotScatter
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
PlotScatter::PlotScatter(wxFrame* parent) : PlotPanel(parent)
{
    int i;

    for(i=0; i < SCATTER_MEM_SYMS_MAX; i++)
    {
        m_mem[i].real = 0.0;
        m_mem[i].imag = 0.0;
    }

    m_filter_max_xy = m_filter_max_y = 0.1;

    // defaults so we start off with something sensible

    Nsym = 14+1;
    scatterMemSyms = ((int)(SCATTER_MEM_SECS*(Nsym/DT)));
    assert(scatterMemSyms <= SCATTER_MEM_SYMS_MAX);

    Ncol = 0;
    memset(eye_mem, 0, sizeof(eye_mem));

    mode = PLOT_SCATTER_MODE_SCATTER;
}

// changing number of carriers changes number of symbols to plot
void PlotScatter::setNc(int Nc) {
    Nsym = Nc;
    assert(Nsym <= (MODEM_STATS_NC_MAX+1));
    scatterMemSyms = ((int)(SCATTER_MEM_SECS*(Nsym/DT)));
    assert(scatterMemSyms <= SCATTER_MEM_SYMS_MAX);
}

//----------------------------------------------------------------
// draw()
//----------------------------------------------------------------
void PlotScatter::draw(wxAutoBufferedPaintDC& dc)
{
    float x_scale;
    float y_scale;
    int   i,j;
    int   x;
    int   y;
    wxColour sym_to_colour[] = {wxColor(0,0,255), 
                                wxColor(0,255,0),
                                wxColor(0,255,255),
                                wxColor(255,0,0),
                                wxColor(255,0,255), 
                                wxColor(255,255,0),
                                wxColor(255,255,255),
                                wxColor(0,0,255), 
                                wxColor(0,255,0),
                                wxColor(0,255,255),
                                wxColor(255,0,0),
                                wxColor(255,0,255), 
                                wxColor(255,255,0),
                                wxColor(255,255,255),
                                wxColor(0,0,255), 
                                wxColor(0,255,0),
                                wxColor(0,255,255),
                                wxColor(255,0,0),
                                wxColor(255,0,255)
    };

    m_rCtrl = GetClientRect();
    m_rGrid = m_rCtrl;
    m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (XLEFT_OFFSET/2), (PLOT_BORDER + (YBOTTOM_OFFSET/2)));

    // black background

    dc.Clear();
    m_rPlot = wxRect(PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER, m_rGrid.GetWidth(), m_rGrid.GetHeight());
    wxBrush ltGraphBkgBrush = wxBrush(BLACK_COLOR);
    dc.SetBrush(ltGraphBkgBrush);
    dc.SetPen(wxPen(BLACK_COLOR, 0));
    dc.DrawRectangle(m_rPlot);
 
    wxPen pen;
    pen.SetWidth(1); // note this is ignored by DrawPoint

    if (mode == PLOT_SCATTER_MODE_SCATTER) {

        // automatically scale, first measure the maximum value

        float max_xy = 1E-12;
        float real,imag,mag;
        for(i=0; i< scatterMemSyms; i++) {
            real = fabs(m_mem[i].real);
            imag = fabs(m_mem[i].imag);
            mag = sqrt(pow(real,2) + pow(imag, 2));
            if (mag > max_xy)
            {
                max_xy = mag;
            }
        }

        // smooth it out and set a lower limit to prevent divide by 0 issues

        m_filter_max_xy = BETA*m_filter_max_xy + (1 - BETA)*2.5*max_xy;
        if (m_filter_max_xy < 0.001)
            m_filter_max_xy = 0.001;

        // quantise to log steps to prevent scatter scaling bobbing about too
        // much as scaling varies

        float quant_m_filter_max_xy = exp(floor(0.5+log(m_filter_max_xy)));
        //printf("max_xy: %f m_filter_max_xy: %f quant_m_filter_max_xy: %f\n", max_xy, m_filter_max_xy, quant_m_filter_max_xy);

        x_scale = (float)m_rGrid.GetWidth()/quant_m_filter_max_xy;
        y_scale = (float)m_rGrid.GetHeight()/quant_m_filter_max_xy;

        // draw all samples

        for(i = 0; i < scatterMemSyms; i++) {
            x = x_scale * m_mem[i].real + m_rGrid.GetWidth()/2;
            y = y_scale * m_mem[i].imag + m_rGrid.GetHeight()/2;
            x += PLOT_BORDER + XLEFT_OFFSET;
            y += PLOT_BORDER;
            
            if (pointsInBounds_(x, y))
            {
                pen.SetColour(DARK_GREEN_COLOR);
                dc.SetPen(pen);
                dc.DrawPoint(x, y);
            }
        }
    }

    if (mode == PLOT_SCATTER_MODE_EYE) {

        // The same color will be used for all eye traces
        pen.SetColour(DARK_GREEN_COLOR);
        pen.SetWidth(1);
        dc.SetPen(pen);

        // automatically scale, first measure the maximum Y value

        float max_y = 1E-12;
        float min_y = 1E+12;
        for(i=0; i<SCATTER_EYE_MEM_ROWS; i++) {
            for(j=0; j<Ncol; j++) {
                if (eye_mem[i][j] > max_y) {
                    max_y = eye_mem[i][j];
                }
                if (eye_mem[i][j] < min_y) {
                    min_y = eye_mem[i][j];
                }
            }
        }
 
        // smooth it out and set a lower limit to prevent divide by 0 issues

        m_filter_max_y = BETA*m_filter_max_y + (1 - BETA)*2.5*max_y;
        if (m_filter_max_y < 0.001)
            m_filter_max_y = 0.001;

        // quantise to log steps to prevent scatter scaling bobbing about too
        // much as scaling varies

        float quant_m_filter_max_y = exp(floor(0.5+log(m_filter_max_y)));
        //printf("min_y: %4.3f max_y: %4.3f quant_m_filter_max_y: %4.3f\n", min_y, max_y, quant_m_filter_max_y);

        x_scale = (float)m_rGrid.GetWidth()/Ncol;
        y_scale = (float)m_rGrid.GetHeight()/quant_m_filter_max_y;
        //printf("GetWidth(): %d GetHeight(): %d\n", m_rGrid.GetWidth(), m_rGrid.GetHeight());

        // plot eye traces row by row

        int prev_x, prev_y;
        prev_x = prev_y = 0;
        for(i=0; i<SCATTER_EYE_MEM_ROWS; i++) {
            //printf("row: ");
            for(j=0; j<Ncol; j++) {
                x = x_scale * j;
                y = m_rGrid.GetHeight()*0.75 - y_scale * eye_mem[i][j];
               //printf("%4d,%4d  ", x, y);
                x += PLOT_BORDER + XLEFT_OFFSET;
                y += PLOT_BORDER;
                
                if (pointsInBounds_(x,y) && pointsInBounds_(prev_x, prev_y))
                {
                    if (j)
                        dc.DrawLine(x, y, prev_x, prev_y);
                }
                prev_x = x; prev_y = y;
            }
            //printf("\n");
        }
        
    }
}

//----------------------------------------------------------------
// add_new_samples()
//----------------------------------------------------------------
void PlotScatter::add_new_samples_scatter(COMP samples[])
{
    int i,j;

    // shift memory

    for(i = 0; i < scatterMemSyms - Nsym; i++)
    {
        m_mem[i] = m_mem[i+Nsym];
    }

    // new samples

    for(j=0; i < scatterMemSyms; i++,j++)
    {
        m_mem[i] = samples[j];
    }
}

/* add a row of eye samples, updating buffer */

void PlotScatter::add_new_samples_eye(float samples[], int n)
{
    int i,j;

    Ncol = n; /* this should be constant for a given modem config */

    assert(n <= PLOT_SCATTER_EYE_MAX_SAMPLES_ROW);

    // eye traces are arrnaged in rows, shift memory of traces

    for(i=0; i<SCATTER_EYE_MEM_ROWS-1; i++) {
        for(j=0; j<Ncol; j++) {
            eye_mem[i][j] = eye_mem[i+1][j];
        }
    }

    // new samples in last row

    for(j=0; j<Ncol; j++) {
        eye_mem[SCATTER_EYE_MEM_ROWS-1][j] = samples[j];
    }
}

//----------------------------------------------------------------
// OnPaint()
//----------------------------------------------------------------
void PlotScatter::OnPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(this);
    draw(dc);
}

//----------------------------------------------------------------
// OnSize()
//----------------------------------------------------------------
void PlotScatter::OnSize(wxSizeEvent& event)
{
    // todo: clear screen
}

//----------------------------------------------------------------
// OnShow()
//----------------------------------------------------------------
void PlotScatter::OnShow(wxShowEvent& event)
{
}

//----------------------------------------------------------------
// Ensures that points stay within the bounding box.
//----------------------------------------------------------------
bool PlotScatter::pointsInBounds_(int x, int y)
{
    bool inBounds = true;
    inBounds = !(x <= (PLOT_BORDER + XLEFT_OFFSET));
    inBounds = inBounds && !(x >= m_rGrid.GetWidth());
    inBounds = inBounds && !(y <= PLOT_BORDER);
    inBounds = inBounds && !(y >= m_rGrid.GetHeight());
    
    return inBounds;
}
