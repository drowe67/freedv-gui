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

#include <wx/wx.h>
#include <wx/graphics.h>

#include "plot_scatter.h"

BEGIN_EVENT_TABLE(PlotScatter, PlotPanel)
    EVT_PAINT           (PlotScatter::OnPaint)
    EVT_MOTION          (PlotScatter::OnMouseMove)
    EVT_MOUSEWHEEL      (PlotScatter::OnMouseWheelMoved)
    EVT_SIZE            (PlotScatter::OnSize)
    EVT_SHOW            (PlotScatter::OnShow)
        END_EVENT_TABLE()

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// PlotScatter
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
PlotScatter::PlotScatter(wxWindow* parent) : PlotPanel(parent)
{
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
    
    clearCurrentSamples();

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

void PlotScatter::clearCurrentSamples() {
    m_filter_max_xy = m_filter_max_y = 0.1;
    for(int i=0; i < SCATTER_MEM_SYMS_MAX; i++)
    {
        m_mem[i].real = 0.0;
        m_mem[i].imag = 0.0;
    }
}

//----------------------------------------------------------------
// draw()
//----------------------------------------------------------------
void PlotScatter::draw(wxGraphicsContext* ctx, bool repaintDataOnly)
{
    float x_scale;
    float y_scale;
    int   i,j;
    int   x;
    int   y;

    m_rCtrl = GetClientRect();
    m_rGrid = m_rCtrl;
    m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (XLEFT_OFFSET/2), (PLOT_BORDER + (YBOTTOM_OFFSET/2)));

    // black background

    wxBrush ltGraphBkgBrush = wxBrush(BLACK_COLOR);
    ctx->SetBrush(ltGraphBkgBrush);
    ctx->SetPen(wxPen(BLACK_COLOR, 0));
    ctx->DrawRectangle(PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER, m_rGrid.GetWidth(), m_rGrid.GetHeight());
 
    wxPen pen;
    pen.SetWidth(1); // note this is ignored by DrawPoint

    if (mode == PLOT_SCATTER_MODE_SCATTER) {

        // automatically scale, first measure the maximum magnitude, in other words
        // the max distance from the origin

        float max_mag = 1E-12;
        float real,imag,mag;
        for(i=0; i< scatterMemSyms; i++) {
            real = fabs(m_mem[i].real);
            imag = fabs(m_mem[i].imag);
            mag = sqrt(pow(real,2) + pow(imag, 2));
            if (mag > max_mag)
            {
                max_mag = mag;
            }
        }

        // the maximum side to side distance needs to be at least twice the maximum distance 
        // from the centre, add a little extra so the scatter blobs are just inside the outer edges 

        float max_xy = 2.5*max_mag;
        
        // smooth the estimate so we don't get rapid changes in the scaling due to short term 
        // noise fluctuations. Set a lower limit to prevent divide by 0 issues

        m_filter_max_xy = BETA*m_filter_max_xy + (1 - BETA)*max_xy;
        if (m_filter_max_xy < 0.001)
            m_filter_max_xy = 0.001;

        // quantise to log steps to prevent scatter scaling bobbing about too much 

        float quant_m_filter_max_xy = exp(floor(0.5+log(m_filter_max_xy)));

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
                ctx->SetPen(pen);
                wxGraphicsPath path = ctx->CreatePath();
                path.AddCircle(x, y, 1);
                // Uncomment to show boundaries of plot:
                // path.AddCircle(PLOT_BORDER+XLEFT_OFFSET, PLOT_BORDER, 2); 
                // path.AddCircle(m_rGrid.GetWidth()+PLOT_BORDER+XLEFT_OFFSET, m_rGrid.GetHeight()+PLOT_BORDER, 2);
                ctx->DrawPath(path);
            }
        }
    }

    if (mode == PLOT_SCATTER_MODE_EYE) {

        // The same color will be used for all eye traces
        pen.SetColour(DARK_GREEN_COLOR);
        pen.SetWidth(1);
        ctx->SetPen(pen);

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

        x_scale = (float)m_rGrid.GetWidth()/Ncol;
        y_scale = (float)m_rGrid.GetHeight()/quant_m_filter_max_y;

        // plot eye traces row by row

        int prev_x, prev_y;
        prev_x = prev_y = 0;
        for(i=0; i<SCATTER_EYE_MEM_ROWS; i++) {
            for(j=0; j<Ncol; j++) {
                x = x_scale * j;
                y = m_rGrid.GetHeight()*0.75 - y_scale * eye_mem[i][j];
                x += PLOT_BORDER + XLEFT_OFFSET;
                y += PLOT_BORDER;
                
                if (pointsInBounds_(x,y) && pointsInBounds_(prev_x, prev_y))
                {
                    if (j)
                        ctx->StrokeLine(x, y, prev_x, prev_y);
                }
                prev_x = x; prev_y = y;
            }
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

    // eye traces are arranged in rows, shift memory of traces

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

/*----------------------------------------------------------------
  Ensure that points stay within the bounding box:
    upper LH bounding point (PLOT_BORDER+XLEFT_OFFSET, PLOT_BORDER)
    lower RH bounding point (PLOT_BORDER+XLEFT_OFFSET + GetWidth(), PLOT_BORDER + GetHeight())
----------------------------------------------------------------*/
bool PlotScatter::pointsInBounds_(int x, int y)
{
    bool inBounds;
    inBounds = x >= PLOT_BORDER + XLEFT_OFFSET;
    inBounds = inBounds && x <= PLOT_BORDER + XLEFT_OFFSET + m_rGrid.GetWidth();
    inBounds = inBounds && y >= PLOT_BORDER;
    inBounds = inBounds && y <= PLOT_BORDER + m_rGrid.GetHeight();
    
    return inBounds;
}
