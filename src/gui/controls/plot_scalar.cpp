//==========================================================================
// Name:            plot_scalar.cpp
// Purpose:         Plots scalar amplitude against time
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
#include <string.h>

#include <wx/wx.h>
#include <wx/graphics.h>

#include <map>

#include "plot_scalar.h"

BEGIN_EVENT_TABLE(PlotScalar, PlotPanel)
    EVT_PAINT           (PlotScalar::OnPaint)
    EVT_MOTION          (PlotScalar::OnMouseMove)
    EVT_MOUSEWHEEL      (PlotScalar::OnMouseWheelMoved)
    EVT_SIZE            (PlotScalar::OnSize)
    EVT_SHOW            (PlotScalar::OnShow)
END_EVENT_TABLE()

//----------------------------------------------------------------
// PlotScalar()
//----------------------------------------------------------------
PlotScalar::PlotScalar(wxWindow* parent, 
                       int    channels,           // number on channels to plot
                       float  t_secs,             // time covered by entire x axis in seconds
                       float  sample_period_secs, // time between each sample in seconds
                       float  a_min,              // min ampltude of samples being plotted
                       float  a_max,              // max ampltude of samples being plotted
                       float  graticule_t_step,   // time step of x (time) axis graticule in seconds
                       float  graticule_a_step,   // step of amplitude axis graticule
                       const char a_fmt[],        // printf format string for amplitude axis labels
                       int    mini,               // true for mini-plot - don't draw graticule
                       const char* plotName)
    : PlotPanel(parent, plotName)
{
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
    
    int i;

    m_rCtrl = GetClientRect();

    m_channels = channels;
    m_t_secs = t_secs;
    m_sample_period_secs = sample_period_secs;
    m_a_min = a_min;
    m_a_max = a_max;
    m_graticule_t_step = graticule_t_step;
    m_graticule_a_step = graticule_a_step;
    assert(strlen(a_fmt) < 15);
    memset(m_a_fmt, 0, sizeof(m_a_fmt));
    strncpy(m_a_fmt, a_fmt, sizeof(m_a_fmt) - 1);
    m_mini = mini;
    m_bar_graph = 0;
    m_logy = 0;
    
    // work out number of samples we will store and allocate storage

    m_samples = m_t_secs/m_sample_period_secs;
    m_mem = new float[m_samples*m_channels];
    for(i = 0; i < m_samples*m_channels; i++)
    {
        m_mem[i] = 0.0;
    }
}

//----------------------------------------------------------------
// ~PlotScalar()
//----------------------------------------------------------------
PlotScalar::~PlotScalar()
{
    delete[] m_mem;
}

//----------------------------------------------------------------
// add_new_sample()
//----------------------------------------------------------------
void PlotScalar::add_new_sample(int channel, float sample)
{
    int offset = channel*m_samples;

    assert(channel < m_channels);

    for(int i = 0; i < m_samples-1; i++)
    {
        m_mem[offset+i] = m_mem[offset+i+1];
    }
    
    m_mem[offset+m_samples-1] = sample;
}

//----------------------------------------------------------------
// add_new_samples()
//----------------------------------------------------------------
void  PlotScalar::add_new_samples(int channel, float samples[], int length)
{
    int i;
    int offset = channel*m_samples;

    assert(channel < m_channels);

    for(i = 0; i < m_samples-length; i++)
        m_mem[offset+i] = m_mem[offset+i+length];
    
    for(i = m_samples-length; i < m_samples; i++)
        m_mem[offset+i] = *samples++;
}

//----------------------------------------------------------------
// add_new_short_samples()
//----------------------------------------------------------------
void  PlotScalar::add_new_short_samples(int channel, short samples[], int length, float scale_factor)
{
    int i;
    int offset = channel*m_samples;

    assert(channel < m_channels);

    for(i = 0; i < m_samples-length; i++)
            m_mem[offset+i] = m_mem[offset+i+length];
    
    for(i = m_samples-length; i < m_samples; i++)
        m_mem[offset+i] = (float)*samples++/scale_factor;
}

//----------------------------------------------------------------
// draw()
//----------------------------------------------------------------
void PlotScalar::draw(wxGraphicsContext* ctx)
{
    float index_to_px;
    float a_to_py;
    int   i;
    float a;

    m_rCtrl = GetClientRect();
    m_rGrid = m_rCtrl;
    if (!m_mini)
        m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (XLEFT_OFFSET/2), (PLOT_BORDER + (YBOTTOM_OFFSET/2)));

    // black background
    int plotX = 0, plotY = 0, plotWidth = 0, plotHeight = 0;
    if (!m_mini)
    {
        plotX = PLOT_BORDER + XLEFT_OFFSET;
        plotY = PLOT_BORDER;
    }

    plotWidth = m_rGrid.GetWidth();
    plotHeight = m_rGrid.GetHeight();
        
    wxBrush ltGraphBkgBrush = wxBrush(BLACK_COLOR);
    ctx->SetBrush(ltGraphBkgBrush);
    ctx->SetPen(wxPen(BLACK_COLOR, 0));
    ctx->DrawRectangle(plotX, plotY, plotWidth, plotHeight);
    
    index_to_px = (float)plotWidth/m_samples;
    a_to_py = (float)plotHeight/(m_a_max - m_a_min);

    wxPen pen;
    pen.SetColour(DARK_GREEN_COLOR);
    pen.SetWidth(1);
    ctx->SetPen(pen);
    
    // plot each channel     

    // x -> (y1, y2)
    std::map<int, std::pair<int, int>> lineMap;

    int offset, x, y;
    for(offset=0; offset<m_channels*m_samples; offset+=m_samples) {

        for(i = 0; i < m_samples; i++) {
            a = m_mem[offset + i];
            if (a < m_a_min) a = m_a_min;
            if (a > m_a_max) a = m_a_max;

            // invert y axis and offset by minimum

            y = plotHeight - a_to_py * a + m_a_min*a_to_py;

            // regular point-point line graph

            x = index_to_px * i;

            // put inside plot window

            if (!m_mini) {
                x += PLOT_BORDER + XLEFT_OFFSET;
                y += PLOT_BORDER;
            }
            
            if (m_bar_graph) {

                if (m_logy) {

                    // can't take log(0)

                    assert(m_a_min > 0.0); 
                    assert(m_a_max > 0.0);

                    float norm = (log10(a) - log10(m_a_min))/(log10(m_a_max) - log10(m_a_min));
                    y = plotHeight*(1.0 - norm);
                } else {
                    y = plotHeight - a_to_py * a + m_a_min*a_to_py;
                }

                // use points to make a bar graph

                int x1, x2, y1;

                x1 = index_to_px * ((float)i - 0.5);
                x2 = index_to_px * ((float)i + 0.5);
                y1 = plotHeight;
                x1 += PLOT_BORDER + XLEFT_OFFSET; x2 += PLOT_BORDER + XLEFT_OFFSET;
                y1 += PLOT_BORDER;

                wxGraphicsPath path = ctx->CreatePath();
                path.MoveToPoint(x1, y1);
                path.AddLineToPoint(x1, y);
                path.AddLineToPoint(x2, y);
                path.AddLineToPoint(x2, y1);
                ctx->StrokePath(path);
            }
            else {
                if (i)
                {
                    auto iter = lineMap.find(x);
                    if (iter == lineMap.end())
                    {
                        lineMap[x] = std::pair<int, int>(INT_MAX, INT_MIN);
                        iter = lineMap.find(x);
                    }
                    iter->second.first = std::min(iter->second.first, y);
                    iter->second.second = std::max(iter->second.second, y);
                }
            }
        }
    }
   
    if (lineMap.size() > 0)
    {
        for (auto& iter : lineMap)
        {
            ctx->StrokeLine(iter.first, iter.second.first, iter.first, iter.second.second);
        }
    } 
    drawGraticule(ctx);
}

//-------------------------------------------------------------------------
// drawGraticule()
//-------------------------------------------------------------------------
void PlotScalar::drawGraticule(wxGraphicsContext* ctx)
{
    const int STR_LENGTH = 15;
    
    float    t, a;
    int      x, y, text_w, text_h;
    char     buf[STR_LENGTH];
    wxString s;
    float    sec_to_px;
    float    a_to_py;

    int plotWidth = m_rGrid.GetWidth();
    int plotHeight = m_rGrid.GetHeight();

    wxBrush ltGraphBkgBrush;
    ltGraphBkgBrush.SetStyle(wxBRUSHSTYLE_TRANSPARENT);
    ltGraphBkgBrush.SetColour(*wxBLACK);
    ctx->SetBrush(ltGraphBkgBrush);
    ctx->SetPen(wxPen(BLACK_COLOR, 1));

    wxGraphicsFont tmpFont = ctx->CreateFont(GetFont(), GetForegroundColour());
    ctx->SetFont(tmpFont);
    
    sec_to_px = (float)plotWidth/m_t_secs;
    a_to_py = (float)plotHeight/(m_a_max - m_a_min);

    // upper LH coords of plot area are (PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER)
    // lower RH coords of plot area are (PLOT_BORDER + XLEFT_OFFSET + plotWidth, 
    //                                   PLOT_BORDER + plotHeight)

    // Vertical gridlines

    ctx->SetPen(m_penShortDash);
    for(t=0; t<=m_t_secs; t+=m_graticule_t_step) {
    x = t*sec_to_px;
    if (m_mini) {
            ctx->StrokeLine(x, plotHeight, x, 0);
        }
        else {
            x += PLOT_BORDER + XLEFT_OFFSET;
            ctx->StrokeLine(x, plotHeight + PLOT_BORDER, x, PLOT_BORDER);
        }
        if (!m_mini) {
            snprintf(buf, STR_LENGTH, "%2.1fs", t);
            GetTextExtent(buf, &text_w, &text_h);
            ctx->DrawText(buf, x - text_w/2, plotHeight + PLOT_BORDER + YBOTTOM_TEXT_OFFSET);
        }
    }

    // Horizontal gridlines

    ctx->SetPen(m_penDotDash);
    for(a=m_a_min; a<m_a_max; ) {
        if (m_logy) {
            float norm = (log10(a) - log10(m_a_min))/(log10(m_a_max) - log10(m_a_min));
            y = plotHeight*(1.0 - norm);
        }
    else {
            y = plotHeight - a*a_to_py + m_a_min*a_to_py;
        }
    if (m_mini) {
            ctx->StrokeLine(0, y, plotWidth, y);
        }
        else {
            y += PLOT_BORDER;
            ctx->StrokeLine(PLOT_BORDER + XLEFT_OFFSET, y, 
                        (plotWidth + PLOT_BORDER + XLEFT_OFFSET), y);
        }
        if (!m_mini) {
            snprintf(buf, STR_LENGTH, m_a_fmt, a);
            GetTextExtent(buf, &text_w, &text_h);
            ctx->DrawText(buf, PLOT_BORDER + XLEFT_OFFSET - text_w - XLEFT_TEXT_OFFSET, y-text_h/2);
        }

        if (m_logy) {
            // m_graticule_a_step ==  0.1 means 10 steps/decade
            float log10_step_size = floor(log10(a));
            a += pow(10,log10_step_size);
        }
        else {
            a += m_graticule_a_step;
        }
   }


}

void PlotScalar::clearSamples()
{
    memset(m_mem, 0, sizeof(float) * m_samples);
}

//----------------------------------------------------------------
// OnSize()
//----------------------------------------------------------------
void PlotScalar::OnSize(wxSizeEvent& event)
{
}

//----------------------------------------------------------------
// OnShow()
//----------------------------------------------------------------
void PlotScalar::OnShow(wxShowEvent& event)
{
}
