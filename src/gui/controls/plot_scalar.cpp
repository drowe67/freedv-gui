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

#if defined(__APPLE__)
// On macOS, the plot area -- background, waveform and gridlines -- is rasterized on the
// CPU into a pixel buffer at the display's pixel density and drawn as a single image,
// in copy mode (see PlotPixelBuffer in plot_osx.mm). As paths, CoreGraphics filled the
// background and the waveform over the whole plot area at 2x and stroked every dash of
// the gridlines separately on every frame. Here the waveform is a vertical span per
// pixel column and the gridlines are a precomputed list of pixels. The axis labels are
// still drawn as text.
constexpr bool RASTERIZE_WAVEFORM = true;

PlotPixelBuffer* CreatePlotPixelBuffer(wxWindow* window, int width, int height);
void DestroyPlotPixelBuffer(PlotPixelBuffer* buffer);
bool PlotPixelBufferHasSize(const PlotPixelBuffer* buffer, int width, int height);
uint32_t* PlotPixelBufferGetPixels(PlotPixelBuffer* buffer);
uint32_t PlotPixelBufferGetPixel(const PlotPixelBuffer* buffer, const wxColour& colour);
wxGraphicsBitmap PlotPixelBufferFinishFrame(PlotPixelBuffer* buffer, wxGraphicsContext* gc);
#else
constexpr bool RASTERIZE_WAVEFORM = false;
#endif // defined(__APPLE__)

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
                       bool disableFirstLastLabels)
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

    int i;

    m_rCtrl = GetClientRect();

    lineMap_ = nullptr;
#if defined(__APPLE__)
    pixelBuffer_ = nullptr;
    backgroundPixel_ = 0;
    waveformPixel_ = 0;
    verticalGridPixel_ = 0;
    horizontalGridPixel_ = 0;
#endif // defined(__APPLE__)
    gridlinesRasterized_ = false;
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
#if defined(__APPLE__)
    DestroyPlotPixelBuffer(pixelBuffer_);
#endif // defined(__APPLE__)

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
    bool rasterize = RASTERIZE_WAVEFORM && !halfPlot_ && !m_bar_graph;
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
        if (!rasterize)
        {
            ctx->SetPen(*wxTRANSPARENT_PEN);
            ctx->SetBrush(wxBrush(BLACK_COLOR));
            ctx->DrawRectangle(0, 0, plotWidth, plotHeight);
        }
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

#if defined(__APPLE__)
    if (rasterize && !rasterizeWaveform_(ctx, plotWidth, plotHeight))
    {
        // No pixel buffer (e.g. not on screen yet), so fall back to vectors.
        rasterize = false;
        ctx->SetPen(*wxTRANSPARENT_PEN);
        ctx->SetBrush(wxBrush(BLACK_COLOR));
        ctx->DrawRectangle(0, 0, plotWidth, plotHeight);
    }
#endif // defined(__APPLE__)

    if (!m_bar_graph && !rasterize)
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
            plotCtx->FillPath(path);
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

#if defined(__APPLE__)
//-------------------------------------------------------------------------
// rasterizeWaveform_()
//-------------------------------------------------------------------------
// Fills the pixel buffer with the background and the waveform and draws it into ctx
// (translated to the plot area). The waveform is the same polygon draw() would fill
// otherwise -- the upper edge through (x, lineMap_[x].y1) left to right, the lower edge
// through (x, lineMap_[x].y2) back. Compared with CoreGraphics filling that polygon
// (unantialiased, at 2x), a few dozen edge pixels out of ~170,000 differ per frame.
// Returns false if there's no pixel buffer to draw with.
bool PlotScalar::rasterizeWaveform_(wxGraphicsContext* ctx, int plotWidth, int plotHeight)
{
    double scale = GetContentScaleFactor();
    int width = std::max(1, (int)std::lround(plotWidth * scale));
    int height = std::max(1, (int)std::lround(plotHeight * scale));
    if (!PlotPixelBufferHasSize(pixelBuffer_, width, height))
    {
        DestroyPlotPixelBuffer(pixelBuffer_);
        pixelBuffer_ = CreatePlotPixelBuffer(this, width, height);
        if (pixelBuffer_ == nullptr)
        {
            return false;
        }
        backgroundPixel_ = PlotPixelBufferGetPixel(pixelBuffer_, BLACK_COLOR);
        waveformPixel_ = PlotPixelBufferGetPixel(pixelBuffer_, DARK_GREEN_COLOR);
        verticalGridPixel_ = PlotPixelBufferGetPixel(pixelBuffer_, m_penShortDash.GetColour());
        horizontalGridPixel_ = PlotPixelBufferGetPixel(pixelBuffer_, m_penDotDash.GetColour());
        computeGridOffsets_(plotWidth, plotHeight, width, height, scale);
    }

    // Start from the background everywhere. Besides being the fastest way to clear it,
    // this leaves the buffer in the cache for the waveform's column spans below, which
    // touch a different cache line for every pixel. (Clearing just the last frame's
    // waveform instead, or writing every pixel row by row, both cost several times more.)
    uint32_t* pixels = PlotPixelBufferGetPixels(pixelBuffer_);
    memset_pattern4(pixels, &backgroundPixel_, (size_t)width * height * sizeof(uint32_t));

    // Same adjustment as the path version: keep a line visible when fully silent.
    for (int column = 0; column < plotWidth; column++)
    {
        if (lineMap_[column].y1 == lineMap_[column].y2)
        {
            lineMap_[column].y1--;
        }
    }

    // Waveform.
    for (int px = 0; px < width; px++)
    {
        // The polygon spans x = 0 .. plotWidth - 1 (in points).
        double x = (px + 0.5) / scale;
        int x0 = (int)x;
        if (x0 >= plotWidth - 1)
        {
            // Only the vertical edge at plotWidth - 1 is out here.
            break;
        }

        const MinMaxPoints& a = lineMap_[x0];
        const MinMaxPoints& b = lineMap_[x0 + 1];
        if (a.y1 == INT_MAX || b.y1 == INT_MAX)
        {
            // No samples in this column.
            continue;
        }

        // Fill everything between the edges anywhere across this pixel column, not just
        // at its center: CoreGraphics' unantialiased fill also includes the pixels that
        // steep edges pass through.
        double tl = px / scale - x0;
        double tr = std::min((px + 1) / scale - x0, 1.0);
        double upperL = a.y1 + (b.y1 - a.y1) * tl, upperR = a.y1 + (b.y1 - a.y1) * tr;
        double lowerL = a.y2 + (b.y2 - a.y2) * tl, lowerR = a.y2 + (b.y2 - a.y2) * tr;
        int firstRow = std::max(0, (int)std::ceil(std::min(upperL, upperR) * scale - 0.5));
        int lastRow = std::min(height - 1, (int)std::floor(std::max(lowerL, lowerR) * scale - 0.5));

        uint32_t* column = pixels + px;
        for (int row = firstRow; row <= lastRow; row++)
        {
            column[(size_t)row * width] = waveformPixel_;
        }
    }

    // Gridlines, drawn over the waveform like drawGraticuleFast() does otherwise.
    for (uint32_t offset : verticalGridOffsets_)
    {
        pixels[offset] = verticalGridPixel_;
    }
    for (uint32_t offset : horizontalGridOffsets_)
    {
        pixels[offset] = horizontalGridPixel_;
    }
    gridlinesRasterized_ = true;

    wxGraphicsBitmap bitmap = PlotPixelBufferFinishFrame(pixelBuffer_, ctx);
    if (bitmap.IsNull())
    {
        gridlinesRasterized_ = false;
        return false;
    }

    // Opaque, so copy it rather than blending: a plain memory copy.
    wxCompositionMode compositionMode = ctx->GetCompositionMode();
    ctx->SetCompositionMode(wxCOMPOSITION_SOURCE);
    ctx->DrawBitmap(bitmap, 0, 0, plotWidth, plotHeight);
    ctx->SetCompositionMode(compositionMode);
    return true;
}

//-------------------------------------------------------------------------
// computeGridOffsets_()
//-------------------------------------------------------------------------
// Works out which pixels of the pixel buffer the gridlines cover, the same as
// drawGraticuleFast() strokes them otherwise: 1 point wide, unantialiased, dashed like
// wxPENSTYLE_SHORT_DASH (vertical, from the bottom up) and wxPENSTYLE_DOT_DASH
// (horizontal, left to right), with the dashes restarting at the start of each line.
void PlotScalar::computeGridOffsets_(int plotWidth, int plotHeight, int width, int height, double scale)
{
    verticalGridOffsets_.clear();
    horizontalGridOffsets_.clear();

    std::vector<int> xs, ys;
    getGridlines_(plotWidth, plotHeight, xs, ys);
    auto isDashOn = [](double distance, const double* pattern, int count, double period)
    {
        double phase = std::fmod(distance, period);
        for (int i = 0; i < count; i++)
        {
            if (phase < pattern[i]) return (i % 2) == 0;
            phase -= pattern[i];
        }
        return false;
    };
    static const double shortDash[] = { 9, 6 };
    static const double dotDash[] = { 9, 6, 3, 3 };
    int lineWidth = std::max(1, (int)std::lround(scale));

    for (int x : xs)
    {
        // A 1 point line centered on x covers [x - 0.5, x + 0.5).
        int firstCol = (int)std::ceil((x - 0.5) * scale - 0.5);
        for (int row = 0; row < height; row++)
        {
            if (!isDashOn(plotHeight - (row + 0.5) / scale, shortDash, 2, 15)) continue;
            for (int col = std::max(0, firstCol); col < std::min(width, firstCol + lineWidth); col++)
            {
                verticalGridOffsets_.push_back((uint32_t)(row * width + col));
            }
        }
    }
    for (int y : ys)
    {
        // Drawn one point higher than y; see drawGraticuleFast().
        int firstRow = (int)std::ceil((y - 1 - 0.5) * scale - 0.5);
        for (int row = std::max(0, firstRow); row < std::min(height, firstRow + lineWidth); row++)
        {
            for (int col = 0; col < width; col++)
            {
                if (isDashOn((col + 0.5) / scale, dotDash, 4, 21))
                {
                    horizontalGridOffsets_.push_back((uint32_t)(row * width + col));
                }
            }
        }
    }
}

//-------------------------------------------------------------------------
// getGridlines_()
//-------------------------------------------------------------------------
// Positions of the vertical and horizontal gridlines within the plot area, in points.
// Must match the loops in drawGraticuleFast().
void PlotScalar::getGridlines_(int plotWidth, int plotHeight, std::vector<int>& xs, std::vector<int>& ys)
{
    float sec_to_px = (float)plotWidth/m_t_secs;
    float a_to_py = (float)plotHeight/(m_a_max - m_a_min);

    for (float t = 0; t <= m_t_secs; t += m_graticule_t_step)
    {
        xs.push_back(t*sec_to_px);
    }

    for (float a = m_a_min; a <= m_a_max; )
    {
        if (m_logy)
        {
            float norm = (log10(a) - log10(m_a_min))/(log10(m_a_max) - log10(m_a_min));
            ys.push_back(plotHeight*(1.0 - norm));
            float log10_step_size = floor(log10(a));
            a += pow(10,log10_step_size);
        }
        else
        {
            ys.push_back(plotHeight - a*a_to_py + m_a_min*a_to_py);
            a += m_graticule_a_step;
        }
    }
}
#endif // defined(__APPLE__)

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
        // Already drawn into the pixel buffer when rasterizing; see RASTERIZE_WAVEFORM.
        drawPlotLines = !gridlinesRasterized_;
        gridlinesRasterized_ = false;
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

   if (DRAW_DIRECTLY && !drawPlotLines)
   {
       return;
   }

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
