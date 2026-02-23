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
#include <vector>

#include "plot_scalar.h"

BEGIN_EVENT_TABLE(PlotScalar, PlotPanel)
    EVT_PAINT           (PlotScalar::OnPaint)
    EVT_MOTION          (PlotScalar::OnMouseMove)
    EVT_MOUSEWHEEL      (PlotScalar::OnMouseWheelMoved)
    EVT_SIZE            (PlotScalar::OnSize)
    EVT_SHOW            (PlotScalar::OnShow)
END_EVENT_TABLE()

constexpr int STR_LENGTH = 15;

//----------------------------------------------------------------
// PlotScalar()
//----------------------------------------------------------------
PlotScalar::PlotScalar(wxWindow* parent, 
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
    
    plotArea_ = nullptr;
    plotLines_ = nullptr;
    addedPoints_ = 0;

    int i;

    m_rCtrl = GetClientRect();

    lineMap_ = nullptr;
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
    leftOffset_ = 0;
    bottomOffset_ = 0;

    // work out number of samples we will store and allocate storage

    m_samples = m_t_secs/m_sample_period_secs;
    m_mem = new float[m_samples];
    for(i = 0; i < m_samples; i++)
    {
        m_mem[i] = 0.0;
    }

    plotAreaDC_ = new wxMemoryDC();
    assert(plotAreaDC_ != nullptr);
}

//----------------------------------------------------------------
// ~PlotScalar()
//----------------------------------------------------------------
PlotScalar::~PlotScalar()
{
    delete[] m_mem;
    delete[] lineMap_;

    delete plotAreaDC_;

    if (plotArea_ != nullptr)
    {
        delete plotArea_;
        plotArea_ = nullptr;
    }

    if (plotLines_ != nullptr)
    {
        delete plotLines_;
        plotLines_ = nullptr;
    }
}

//----------------------------------------------------------------
// add_new_sample()
//----------------------------------------------------------------
void PlotScalar::add_new_sample(float sample)
{
    for(int i = 0; i < m_samples-1; i++)
    {
        m_mem[i] = m_mem[i+1];
    }
    
    m_mem[m_samples-1] = sample;
    addedPoints_++;
}

//----------------------------------------------------------------
// add_new_samples()
//----------------------------------------------------------------
void  PlotScalar::add_new_samples(float samples[], int length)
{
    int i;

    for(i = 0; i < m_samples-length; i++)
        m_mem[i] = m_mem[i+length];
    
    for(i = m_samples-length; i < m_samples; i++)
        m_mem[i] = *samples++;

    addedPoints_ += length;
}

//----------------------------------------------------------------
// add_new_short_samples()
//----------------------------------------------------------------
void  PlotScalar::add_new_short_samples(short samples[], int length, float scale_factor)
{
    int i;

    for(i = 0; i < m_samples-length; i++)
            m_mem[i] = m_mem[i+length];
    
    for(i = m_samples-length; i < m_samples; i++)
        m_mem[i] = (float)*samples++/scale_factor;

    addedPoints_ += length;
}

bool PlotScalar::repaintAll_(wxPaintEvent&)
{
    if (m_mini) return true;

    wxRect plotRegion(
        PLOT_BORDER + leftOffset_, 
        PLOT_BORDER,
        m_rGrid.GetWidth(),
        m_rGrid.GetHeight());
    wxRegionIterator upd(GetUpdateRegion());
    while (upd)
    {
        wxRect rect(upd.GetRect());
        if (!plotRegion.Contains(rect))
        {
            return true;
        }
        upd++;
    }
    return false;
}

void PlotScalar::refreshData()
{
    if (!m_mini)
    {
        wxRect plotRegion(
            PLOT_BORDER + leftOffset_, 
            PLOT_BORDER,
            m_rGrid.GetWidth(),
            m_rGrid.GetHeight());
        RefreshRect(plotRegion);
    }
    else
    {
        Refresh();
    }
}

//----------------------------------------------------------------
// draw()
//----------------------------------------------------------------
void PlotScalar::draw(wxGraphicsContext* ctx, bool repaintDataOnly)
{
    float index_to_px;
    float a_to_py;
    int   i;
    float a;

    m_rCtrl = GetClientRect();
    m_rGrid = m_rCtrl;
    if (!m_mini)
        m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (leftOffset_/2), (PLOT_BORDER + (bottomOffset_/2)));

    // black background
    int plotX = 0, plotY = 0, plotWidth = 0, plotHeight = 0;
    if (!m_mini)
    {
        plotX = PLOT_BORDER + leftOffset_;
        plotY = PLOT_BORDER;
    }

    plotWidth = m_rGrid.GetWidth();
    plotHeight = m_rGrid.GetHeight();

    if (plotWidth <= 0 || plotHeight <= 0) return;
 
    if (plotArea_ == nullptr)
    {
        plotArea_ = new wxBitmap(plotWidth, plotHeight);
        assert(plotArea_ != nullptr);

        addedPoints_ = 0; // force rendering of all points
    }

    plotAreaDC_->SelectObject(*plotArea_);

    wxBrush ltGraphBkgBrush = wxBrush(BLACK_COLOR);
    plotAreaDC_->SetBrush(ltGraphBkgBrush);
    plotAreaDC_->SetPen(wxPen(BLACK_COLOR, 0));

    index_to_px = (float)plotWidth/m_samples;
    int pixelsUpdated = index_to_px * addedPoints_;

    if (addedPoints_ == 0)
    {
        plotAreaDC_->DrawRectangle(0, 0, plotWidth, plotHeight);
    }
    else
    {
        // Clear only the area that we're updating
        plotAreaDC_->Blit(0, 0, plotWidth - pixelsUpdated, plotHeight, plotAreaDC_, pixelsUpdated, 0);
        plotAreaDC_->DrawRectangle(plotWidth - pixelsUpdated, 0, pixelsUpdated, plotHeight);
    }
    
    a_to_py = (float)plotHeight/(m_a_max - m_a_min);

    wxPen pen;
    pen.SetColour(DARK_GREEN_COLOR);
    pen.SetWidth(1);
    plotAreaDC_->SetPen(pen);
    plotAreaDC_->SetBrush(wxBrush(DARK_GREEN_COLOR));

    // plot each channel     

    // x -> (y1, y2)
    if (lineMap_ == nullptr)
    {
        lineMap_ = new MinMaxPoints[plotWidth];
        assert(lineMap_ != nullptr);
    }

    for (int index = 0; index < plotWidth; index++)
    {
        lineMap_[index].y1 = INT_MAX;
        lineMap_[index].y2 = INT_MIN;
    }

    ctx->BeginLayer(1.0);

    int x, y;

    for(i = 0; i < m_samples; i++) {
        a = m_mem[i];
        if (a < m_a_min) a = m_a_min;
        if (a > m_a_max) a = m_a_max;

        // invert y axis and offset by minimum

        y = plotHeight - a_to_py * a + m_a_min*a_to_py;

        // regular point-point line graph

        x = index_to_px * i;

        // put inside plot window

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
            x1 += PLOT_BORDER + leftOffset_; x2 += PLOT_BORDER + leftOffset_;
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
                auto item = &lineMap_[x];
                item->y1 = std::min(item->y1, y);
                item->y2 = std::max(item->y2, y);
            }
        }
    }

    if (!m_bar_graph)
    {
        std::vector<wxPoint> points;
        int from = addedPoints_ > 0 ? plotWidth - pixelsUpdated : 0;
        for (int index = from; index < plotWidth; index++)
        {
            auto item = &lineMap_[index];
            int x = index;
            if (index == from) points.push_back(wxPoint(x, item->y1));
            else points.push_back(wxPoint(x, item->y1));
        }
        for (int index = plotWidth - 1; index >= from; index--)
        {
            auto item = &lineMap_[index];
            int x = index;
            points.push_back(wxPoint(x, item->y2));
        }
        points.push_back(wxPoint(from, lineMap_[from].y1));

        plotAreaDC_->DrawPolygon(points.size(), &points[0]);
    }

    plotAreaDC_->SelectObject(wxNullBitmap);
    ctx->DrawBitmap(*plotArea_, plotX, plotY, plotWidth, plotHeight); 
    ctx->EndLayer();

    addedPoints_ = 0;
    drawGraticuleFast(ctx, repaintDataOnly);
}

//-------------------------------------------------------------------------
// drawGraticuleFast()
//-------------------------------------------------------------------------
void PlotScalar::drawGraticuleFast(wxGraphicsContext* ctx, bool repaintDataOnly)
{
    float    t, a;
    int      x, y, text_w, text_h;
    char     buf[STR_LENGTH];
    float    sec_to_px;
    float    a_to_py;

    int plotWidth = m_rGrid.GetWidth();
    int plotHeight = m_rGrid.GetHeight();

    wxGraphicsContext* plotCtx = nullptr;
    bool drawPlotLines = false;
    if (plotLines_ == nullptr)
    {
        plotLines_ = new wxImage(plotWidth, plotHeight);
        assert(plotLines_ != nullptr);
        drawPlotLines = true;
        
        plotCtx = wxGraphicsContext::Create(*plotLines_);
        assert(plotCtx != nullptr);
        plotCtx->SetInterpolationQuality(wxINTERPOLATION_NONE);
        plotCtx->SetAntialiasMode(wxANTIALIAS_NONE);
    }

    ctx->SetPen(wxPen(BLACK_COLOR, 1));

    if (!repaintDataOnly)
    {
        wxGraphicsFont tmpFont = ctx->CreateFont(GetFont(), GetForegroundColour());
        ctx->SetFont(tmpFont);
    }
 
    sec_to_px = (float)plotWidth/m_t_secs;
    a_to_py = (float)plotHeight/(m_a_max - m_a_min);

    // upper LH coords of plot area are (PLOT_BORDER + leftOffset_, PLOT_BORDER)
    // lower RH coords of plot area are (PLOT_BORDER + leftOffset_ + plotWidth, 
    //                                   PLOT_BORDER + plotHeight)

    // Vertical gridlines

    if (drawPlotLines) plotCtx->SetPen(m_penShortDash);
    for(t=0; t<=m_t_secs; t+=m_graticule_t_step)
    {
        x = t*sec_to_px;
        if (m_mini && drawPlotLines) 
        {
            plotCtx->StrokeLine(x, plotHeight, x, 0);
        }
        else 
        {
            if (drawPlotLines) plotCtx->StrokeLine(x, plotHeight, x, 0);
            x += PLOT_BORDER + leftOffset_;
            if (!repaintDataOnly) 
            {
                snprintf(buf, STR_LENGTH, "%2.1fs", t);
                GetTextExtent(buf, &text_w, &text_h);
                int left = x - text_w/2;
                if (t == 0)
                {
                    left += text_w/2;
                }
                else if (t == m_t_secs)
                {
                    left -= text_w/2;
                }
                ctx->DrawText(buf, left, plotHeight + PLOT_BORDER + YBOTTOM_TEXT_OFFSET);
            }
        }
    }

    // Horizontal gridlines

    if (drawPlotLines) plotCtx->SetPen(m_penDotDash);
    for(a=m_a_min; a<m_a_max; ) 
    {
        if (m_logy) 
        {
            float norm = (log10(a) - log10(m_a_min))/(log10(m_a_max) - log10(m_a_min));
            y = plotHeight*(1.0 - norm);
        }
        else 
        {
            y = plotHeight - a*a_to_py + m_a_min*a_to_py;
        }
        if (m_mini && drawPlotLines) 
        {
            plotCtx->StrokeLine(0, y, plotWidth, y);
        }
        else 
        {
            if (drawPlotLines) plotCtx->StrokeLine(0, y, plotWidth, y);
            y += PLOT_BORDER;
            if (!repaintDataOnly) 
            {
                auto top = y-text_h/2;
                if (a == m_a_min)
                {
                    top -= text_h/2;
                }
                else if ((a + m_graticule_a_step) >= m_a_max)
                {
                    top += text_h/2;
                }

                snprintf(buf, STR_LENGTH, m_a_fmt, a);
                GetTextExtent(buf, &text_w, &text_h);
                ctx->DrawText(buf, PLOT_BORDER + leftOffset_ - text_w - XLEFT_TEXT_OFFSET, top);
            }
        }
 
        if (m_logy) 
        {
            // m_graticule_a_step ==  0.1 means 10 steps/decade
            float log10_step_size = floor(log10(a));
            a += pow(10,log10_step_size);
        }
        else 
        {
            a += m_graticule_a_step;
        }
   }

   if (drawPlotLines) 
   {
       delete plotCtx;
       
       plotLines_->SetMaskColour(0, 0, 0);
       plotLines_->InitAlpha();       
       
       plotLinesBMP_ = ctx->CreateBitmap(*plotLines_);
   }

   if (m_mini)
   {
       ctx->DrawBitmap(plotLinesBMP_, 0, 0, plotWidth, plotHeight);
   }
   else
   {
       ctx->DrawBitmap(plotLinesBMP_, PLOT_BORDER + leftOffset_, PLOT_BORDER, plotWidth, plotHeight);
   }
}

void PlotScalar::clearSamples()
{
    memset(m_mem, 0, sizeof(float) * m_samples);
}

//----------------------------------------------------------------
// OnSize()
//----------------------------------------------------------------
void PlotScalar::OnSize(wxSizeEvent&)
{
    leftOffset_ = 0;
    bottomOffset_ = 0;
    for(auto a=m_a_min; a<m_a_max; )
    {
        int      text_w, text_h;
        char     buf[STR_LENGTH];
        snprintf(buf, STR_LENGTH, m_a_fmt, a);
        GetTextExtent(buf, &text_w, &text_h);
        leftOffset_ = std::max(leftOffset_, text_w);
        bottomOffset_ = std::max(bottomOffset_, text_h);

        if (m_logy)
        {
            // m_graticule_a_step ==  0.1 means 10 steps/decade
            float log10_step_size = floor(log10(a));
            a += pow(10,log10_step_size);
        }
        else
        {
            a += m_graticule_a_step;
        }
    }

    m_rCtrl = GetClientRect();
    m_rGrid = m_rCtrl;
    if (!m_mini)
        m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (leftOffset_/2), (PLOT_BORDER + (bottomOffset_/2)));

    if (plotArea_ != nullptr)
    {
        delete plotArea_;
        plotArea_ = nullptr;
    }

    if (plotLines_ != nullptr)
    {
        delete plotLines_;
        plotLines_ = nullptr;
    }

    int plotWidth = m_rGrid.GetWidth();
    delete[] lineMap_;

    lineMap_ = new MinMaxPoints[plotWidth];
    assert(lineMap_ != nullptr);

    int plotHeight = m_rGrid.GetHeight();
    if (plotWidth <= 0 || plotHeight <= 0) return;

    plotArea_ = new wxBitmap(plotWidth, plotHeight);
    assert(plotArea_ != nullptr);
}

//----------------------------------------------------------------
// OnShow()
//----------------------------------------------------------------
void PlotScalar::OnShow(wxShowEvent&)
{
}
