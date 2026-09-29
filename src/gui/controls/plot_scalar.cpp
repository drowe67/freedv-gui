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

#include "gui/theme/FreeDVTheme.h"
#include "util/logging/ulog.h"

BEGIN_EVENT_TABLE(PlotScalar, PlotPanel)
    EVT_PAINT           (PlotScalar::OnPaint)
    EVT_MOTION          (PlotScalar::OnMouseMove)
    EVT_MOUSEWHEEL      (PlotScalar::OnMouseWheelMoved)
    EVT_SIZE            (PlotScalar::OnSize)
    EVT_SHOW            (PlotScalar::OnShow)
END_EVENT_TABLE()

constexpr int STR_LENGTH = 15;

#if defined(__APPLE__) || defined(_WIN32)
// On macOS and Windows the plot is drawn straight into the window as vectors instead of
// through the plotArea_/plotLines_ bitmaps. The Frm Mic plot repaints ten times a second
// while transmitting, and scrolling, converting and compositing those bitmaps on every
// frame was the largest single cost on the GUI thread:
//
// - On macOS a wxBitmap is always sRGB and 1x while the window is almost never sRGB and is
//   usually 2x, so CoreGraphics color matched and rescaled both bitmaps on every frame.
//   Keeping native bitmaps in the window's color space and pixel density avoided that but
//   still cost a 2x scroll, a snapshot copy and a pixel format conversion per frame.
// - On Windows, drawing the bitmaps through GDI+ took over twice as long as rasterizing
//   the paths directly.
//
// Linux (GTK) keeps the bitmaps, which weren't a significant cost there.
constexpr bool DRAW_DIRECTLY = true;
#else
constexpr bool DRAW_DIRECTLY = false;
#endif // defined(__APPLE__) || defined(_WIN32)

#if defined(_WIN32)
// Everything drawn directly is opaque and unantialiased, so copying pixels gives the same
// result as blending them. On Windows, telling GDI+ so made filling the plot's background
// about 40% cheaper.
constexpr bool COPY_OPAQUE_DRAWING = true;
#else
constexpr bool COPY_OPAQUE_DRAWING = false;
#endif // defined(_WIN32)

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
                       const char* plotName,
                       bool halfPlot,
                       float defaultVal,
                       bool disableFirstLastLabels,
                       TraceStyle traceStyle)
    : PlotPanel(parent, plotName)
{
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
    
    plotArea_ = nullptr;
    plotLines_ = nullptr;
    addedPoints_ = 0;
    halfPlot_ = halfPlot;
    disableFirstLastLabels_ = disableFirstLastLabels;
    traceStyle_ = traceStyle;

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
        m_mem[i] = defaultVal;
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
 
    index_to_px = (float)plotWidth/m_samples;
    int pixelsUpdated = 0;
    wxAntialiasMode antialiasMode = ctx->GetAntialiasMode();
    wxCompositionMode compositionMode = ctx->GetCompositionMode();

    if (DRAW_DIRECTLY)
    {
        // Redraw the whole plot area in plot coordinates; see DRAW_DIRECTLY.
        ctx->PushState();
        ctx->Translate(plotX, plotY);
        ctx->Clip(0, 0, plotWidth, plotHeight);
        if (COPY_OPAQUE_DRAWING)
        {
            ctx->SetCompositionMode(wxCOMPOSITION_SOURCE);
        }
        ctx->SetPen(*wxTRANSPARENT_PEN);
        ctx->SetBrush(wxBrush(BLACK_COLOR));
        ctx->DrawRectangle(0, 0, plotWidth, plotHeight);
    }
    else
    {
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

        pixelsUpdated = std::min(plotWidth, (int)std::floor(index_to_px * addedPoints_));

        if (repaintDataOnly && pixelsUpdated > 0)
        {
            // Clear only the area that we're updating
            plotAreaDC_->Blit(0, 0, plotWidth - pixelsUpdated, plotHeight, plotAreaDC_, pixelsUpdated, 0);
            plotAreaDC_->DrawRectangle(plotWidth - pixelsUpdated, 0, pixelsUpdated, plotHeight);
        }
        else
        {
            plotAreaDC_->DrawRectangle(0, 0, plotWidth, plotHeight);
            addedPoints_ = 0;
        }
    }
    
    a_to_py = (float)plotHeight/(m_a_max - m_a_min);

    wxPen pen;
    pen.SetColour(DARK_GREEN_COLOR);
    pen.SetWidth(1);
    if (!DRAW_DIRECTLY)
    {
        plotAreaDC_->SetPen(pen);
        plotAreaDC_->SetBrush(wxBrush(DARK_GREEN_COLOR));
    }

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
                for (int x2 = x; x2 <= std::ceil(index_to_px * (i + 1)) && x2 < plotWidth; x2++)
                {
                    auto item = &lineMap_[x2];
                    item->y1 = std::min(item->y1, y);

                    if (!halfPlot_)
                    {
                        item->y2 = std::max(item->y2, y);
                    }
                }
            }
        }
    }

    if (!m_bar_graph)
    {
        wxGraphicsContext* plotCtx = DRAW_DIRECTLY ? ctx : wxGraphicsContext::Create(*plotAreaDC_);
        assert(plotCtx != nullptr);

        plotCtx->SetInterpolationQuality(wxINTERPOLATION_NONE);
        plotCtx->SetAntialiasMode(wxANTIALIAS_NONE);
        plotCtx->SetPen(pen);
        plotCtx->SetBrush(wxBrush(DARK_GREEN_COLOR));

        wxGraphicsPath path = plotCtx->CreatePath();
        int from = repaintDataOnly && pixelsUpdated > 0 ? plotWidth - pixelsUpdated - 1: 0;
        for (int index = from; index < plotWidth; index++)
        {
            auto item = &lineMap_[index];
            int x = index;
            
            if (item->y1 == item->y2)
            {
                // workaround due to line at y=0 not appearing when fully silent
                // ensures there's at least a pixel or two difference between them
                item->y1--;
            }
            
            if (index == from) path.MoveToPoint(x, item->y1);
            else path.AddLineToPoint(x, item->y1);
        }
        if (!halfPlot_)
        {
            for (int index = plotWidth - 1; index >= from; index--)
            {
                auto item = &lineMap_[index];
                int x = index;
                path.AddLineToPoint(x, item->y2);
            }
            path.AddLineToPoint(from, lineMap_[from].y1);

            if (traceStyle_ == TraceStyle::MagnitudeGradient)
            {
                wxGraphicsGradientStops stops(
                    FreeDVTheme::GetSignalTraceColour(1.0),
                    FreeDVTheme::GetSignalTraceColour(1.0));
                stops.Add(wxGraphicsGradientStop(
                    FreeDVTheme::GetSignalTraceColour(0.5), 0.25));
                stops.Add(wxGraphicsGradientStop(
                    FreeDVTheme::GetSignalTraceColour(0.15), 0.50));
                stops.Add(wxGraphicsGradientStop(
                    FreeDVTheme::GetSignalTraceColour(0.5), 0.75));

                plotCtx->SetBrush(plotCtx->CreateLinearGradientBrush(
                    0, 0, 0, plotHeight, stops));
            }

            plotCtx->FillPath(path);

            if (traceStyle_ == TraceStyle::MagnitudeGradient)
            {
                const double center = plotHeight / 2.0;
                constexpr double outlineLift = 0.65;

                for (int index = from + 1; index < plotWidth; index++)
                {
                    const auto previous = &lineMap_[index - 1];
                    const auto current = &lineMap_[index];

                    for (bool upper : {true, false})
                    {
                        const int previousPos = upper ? previous->y1 : previous->y2;
                        const int currentPos = upper ? current->y1 : current->y2;
                        const double magnitude =
                            std::min(1.0,
                                std::abs(((previousPos + currentPos) / 2.0) - center) /
                                center);

                        const wxColour colour =
                            FreeDVTheme::GetSignalDisplayColour(magnitude);
                        const auto lift = [](unsigned char component)
                        {
                            return static_cast<unsigned char>(
                                component + (255 - component) * outlineLift);
                        };

                        plotCtx->SetPen(wxPen(
                            wxColour(
                                lift(colour.Red()),
                                lift(colour.Green()),
                                lift(colour.Blue())),
                            2));
                        plotCtx->StrokeLine(
                            index - 1, previousPos,
                            index, currentPos);
                    }
                }
            }
        }
        else if (traceStyle_ == TraceStyle::ValueGradient)
        {
            for (int index = from + 1; index < plotWidth; index++)
            {
                const auto previous = &lineMap_[index - 1];
                const auto current = &lineMap_[index];
                const double position = 1.0 -
                    (static_cast<double>(previous->y1 + current->y1) /
                     (2.0 * plotHeight));
                const wxColour traceColour =
                    FreeDVTheme::GetSignalTraceColour(position);
                const wxColour fillColour(
                    traceColour.Red(),
                    traceColour.Green(),
                    traceColour.Blue(),
                    80);

                wxGraphicsPath fillPath = plotCtx->CreatePath();
                fillPath.MoveToPoint(index - 1, plotHeight);
                fillPath.AddLineToPoint(index - 1, previous->y1);
                fillPath.AddLineToPoint(index, current->y1);
                fillPath.AddLineToPoint(index, plotHeight);
                fillPath.CloseSubpath();

                plotCtx->SetPen(*wxTRANSPARENT_PEN);
                plotCtx->SetBrush(wxBrush(fillColour));
                plotCtx->FillPath(fillPath);

                plotCtx->SetPen(wxPen(traceColour, 2));
                plotCtx->StrokeLine(index - 1, previous->y1,
                                    index, current->y1);
            }
        }
        else
        {
            plotCtx->StrokePath(path);
        }
        if (!DRAW_DIRECTLY)
        {
            delete plotCtx;
        }
    }

    if (DRAW_DIRECTLY)
    {
        ctx->PopState();
        ctx->SetAntialiasMode(antialiasMode);
        ctx->SetCompositionMode(compositionMode);

        addedPoints_ = 0;
        drawGraticuleFast(ctx, repaintDataOnly);
        return;
    }

    plotAreaDC_->SelectObject(wxNullBitmap);

    // Composite through a wxGraphicsBitmap rather than handing the wxBitmap straight to
    // DrawBitmap. On macOS the wxBitmap overload allocates a fresh NSImage per call and
    // draws via -[NSImage drawInRect:]; the wxGraphicsBitmap one uses CGContextDrawImage.
    // The conversion has to happen every frame here since plotArea_ is redrawn above, but
    // it is still far cheaper than the NSImage round trip. Same idea as plotLinesBMP_ in
    // drawGraticuleFast(), which has always taken this path.
    wxGraphicsBitmap plotAreaBMP = ctx->CreateBitmap(*plotArea_);
    ctx->DrawBitmap(plotAreaBMP, plotX, plotY, plotWidth, plotHeight);

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

    // With DRAW_DIRECTLY the gridlines are collected into paths and stroked into the
    // window at the end, rather than composited from the cached plotLines_ overlay.
    wxGraphicsPath verticalLines = DRAW_DIRECTLY ? ctx->CreatePath() : wxGraphicsPath();
    wxGraphicsPath horizontalLines = DRAW_DIRECTLY ? ctx->CreatePath() : wxGraphicsPath();
    auto strokeGridLine = [&](wxGraphicsPath& path, wxDouble x1, wxDouble y1, wxDouble x2, wxDouble y2)
    {
        if (DRAW_DIRECTLY)
        {
            path.MoveToPoint(x1, y1);
            path.AddLineToPoint(x2, y2);
        }
        else
        {
            plotCtx->StrokeLine(x1, y1, x2, y2);
        }
    };

    if (DRAW_DIRECTLY)
    {
        drawPlotLines = true;
    }
    else if (plotLines_ == nullptr)
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

    if (drawPlotLines && !DRAW_DIRECTLY) plotCtx->SetPen(m_penShortDash);
    for(t=0; t<=m_t_secs; t+=m_graticule_t_step)
    {
        x = t*sec_to_px;
        if (m_mini && drawPlotLines) 
        {
            strokeGridLine(verticalLines, x, plotHeight, x, 0);
        }
        else 
        {
            if (drawPlotLines) strokeGridLine(verticalLines, x, plotHeight, x, 0);
            x += PLOT_BORDER + leftOffset_;
            if (!repaintDataOnly) 
            {
                snprintf(buf, STR_LENGTH, "%2.0fs", (m_t_secs - t));
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
                
                if (!disableFirstLastLabels_ || (t > 0 && t < m_t_secs))
                {
                    ctx->DrawText(buf, left, plotHeight + PLOT_BORDER + YBOTTOM_TEXT_OFFSET);
                }
            }
        }
    }

    // Horizontal gridlines

    if (drawPlotLines && !DRAW_DIRECTLY) plotCtx->SetPen(m_penDotDash);
    for(a=m_a_min; a<=m_a_max; ) 
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
            strokeGridLine(horizontalLines, 0, y, plotWidth, y);
        }
        else 
        {
            if (drawPlotLines) strokeGridLine(horizontalLines, 0, y, plotWidth, y);
            y += PLOT_BORDER;
            if (!repaintDataOnly)
            {
                // Avoid "-0.0" from floating point drift when accumulating
                // m_graticule_a_step from m_a_min (e.g. -1.0 + 0.2*5 != 0.0).
                float labelValue = (fabsf(a) < m_graticule_a_step * 0.001f) ? 0.0f : a;
                snprintf(buf, STR_LENGTH, m_a_fmt, labelValue);
                GetTextExtent(buf, &text_w, &text_h);
                auto top = y - text_h/2;

                if (!disableFirstLastLabels_ || (a > m_a_min && a < m_a_max))
                {
                    ctx->DrawText(buf, PLOT_BORDER + leftOffset_ - text_w - XLEFT_TEXT_OFFSET, top);
                }
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

   int plotX = m_mini ? 0 : PLOT_BORDER + leftOffset_;
   int plotY = m_mini ? 0 : PLOT_BORDER;

   if (DRAW_DIRECTLY)
   {
       wxAntialiasMode antialiasMode = ctx->GetAntialiasMode();
       wxCompositionMode compositionMode = ctx->GetCompositionMode();
       ctx->PushState();
       ctx->Translate(plotX, plotY);
       ctx->Clip(0, 0, plotWidth, plotHeight);
       ctx->SetAntialiasMode(wxANTIALIAS_NONE);
       if (COPY_OPAQUE_DRAWING)
       {
           ctx->SetCompositionMode(wxCOMPOSITION_SOURCE);
       }
       ctx->SetPen(m_penShortDash);
       ctx->StrokePath(verticalLines);

#if defined(__APPLE__)
       // In the plotLines_ overlay this replaces, horizontal lines land one point above
       // their nominal y, which is where the waveform's zero line is drawn. Stroked here
       // they'd land one point below it instead, so shift them to match. (On Windows they
       // already land where the overlay put them.)
       ctx->Translate(0, -1);
#endif // defined(__APPLE__)
       ctx->SetPen(m_penDotDash);
       ctx->StrokePath(horizontalLines);
       ctx->PopState();
       ctx->SetAntialiasMode(antialiasMode);
       ctx->SetCompositionMode(compositionMode);
       return;
   }

   if (drawPlotLines) 
   {
       delete plotCtx;
       
       plotLines_->SetMaskColour(0, 0, 0);
       plotLines_->InitAlpha();       
       
       plotLinesBMP_ = ctx->CreateBitmap(*plotLines_);
   }

   ctx->DrawBitmap(plotLinesBMP_, plotX, plotY, plotWidth, plotHeight);

   if (!m_mini)
   {
       ctx->SetPen(wxPen(FreeDVTheme::GetPalette().accent, 1));
       ctx->SetBrush(*wxTRANSPARENT_BRUSH);
       ctx->DrawRectangle(
           plotX,
           plotY,
           plotWidth,
           plotHeight);
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
