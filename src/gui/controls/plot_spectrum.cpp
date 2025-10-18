//==========================================================================
// Name:            plot_waterfall.cpp
// Purpose:         Implements a waterfall plot derivative of plot.
// Created:         June 23, 2012
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

#include "plot_spectrum.h"
#include "codec2_fdmdv.h" // for FDMDV_FCENTRE

#define HZ_GRANULARITY 5

void clickTune(float frequency); // callback to pass new click freq
extern float           g_RxFreqOffsetHz;

BEGIN_EVENT_TABLE(PlotSpectrum, PlotPanel)
    EVT_MOTION          (PlotSpectrum::OnMouseMove)
    EVT_LEFT_DOWN       (PlotSpectrum::OnMouseLeftDown)
    EVT_LEFT_DCLICK     (PlotSpectrum::OnMouseLeftDoubleClick)
    EVT_LEFT_UP         (PlotSpectrum::OnMouseLeftUp)
    EVT_MIDDLE_DOWN     (PlotSpectrum::OnMouseMiddleDown)
    EVT_RIGHT_DCLICK    (PlotSpectrum::OnMouseRightDoubleClick)
    EVT_MOUSEWHEEL      (PlotSpectrum::OnMouseWheelMoved)
    EVT_PAINT           (PlotSpectrum::OnPaint)
    EVT_SHOW            (PlotSpectrum::OnShow)
    EVT_KEY_DOWN        (PlotSpectrum::OnKeyDown)
END_EVENT_TABLE()

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class PlotSpectrum
//
// @class $(Name)
// @author $(User)
// @date $(Date)
// @file $(CurrentFileName).$(CurrentFileExt)
// @brief
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
PlotSpectrum::PlotSpectrum(wxWindow* parent, float *magdB, int n_magdB, 
                           float min_mag_db, float max_mag_db, bool clickTune): PlotPanel(parent)
{
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
    
    m_greyscale     = 0;
    m_Bufsz         = GetMaxClientSize();
    m_newdata       = false;
    m_firstPass     = true;
    m_line_color    = 0;
    m_numSampleAveraging = 1;
    SetLabelSize(10.0);

    m_magdB         = magdB;
    m_n_magdB       = n_magdB;     // number of points in magdB that covers 0 ... MAX_F_HZ of spectrum
    m_max_mag_db    = max_mag_db;
    m_min_mag_db    = min_mag_db;
    m_rxFreq        = 0.0;
    m_clickTune     = clickTune;
    
    m_prevMagDB = new float[n_magdB];
    assert(m_prevMagDB != nullptr);
    
    m_nextPrevMagDB = new float[n_magdB];
    assert(m_nextPrevMagDB != nullptr);
    
    for (int index = 0; index < n_magdB; index++)
    {
        m_prevMagDB[index] = 0;
        m_nextPrevMagDB[index] = 0;
    }
}

//----------------------------------------------------------------
// ~PlotSpectrum()
//----------------------------------------------------------------
PlotSpectrum::~PlotSpectrum()
{
    delete[] m_prevMagDB;
    m_prevMagDB = nullptr;
    
    delete[] m_nextPrevMagDB;
    m_nextPrevMagDB = nullptr;
}

//----------------------------------------------------------------
// OnSize()
//----------------------------------------------------------------
void PlotSpectrum::OnSize(wxSizeEvent& event) {
}

//----------------------------------------------------------------
// OnShow()
//----------------------------------------------------------------
void PlotSpectrum::OnShow(wxShowEvent& event)
{
}

//----------------------------------------------------------------
// draw()
//----------------------------------------------------------------
void PlotSpectrum::draw(wxGraphicsContext* ctx, bool repaintDataOnly)
{
    m_rCtrl  = GetClientRect();

    // m_rGrid is coords of inner window we actually plot to.  We deflate it a bit
    // to leave room for axis labels.  We need to work this out every time we draw
    // as OnSize() may not be called before OnPaint(), for example when a new tab
    // is selected

    m_rGrid  = m_rCtrl;
    m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (XLEFT_OFFSET/2), (PLOT_BORDER + (YBOTTOM_OFFSET/2)));

    // black background

    wxBrush ltGraphBkgBrush = wxBrush(BLACK_COLOR);
    ctx->SetBrush(ltGraphBkgBrush);
    ctx->SetPen(wxPen(BLACK_COLOR, 0));
    ctx->DrawRectangle(PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER, m_rGrid.GetWidth(), m_rGrid.GetHeight());

    // draw spectrum

    int   x, y, prev_x, prev_y, index;
    float index_to_px, mag_dB_to_py, mag;

    m_newdata = false;

    wxPen pen;
    pen.SetColour(DARK_GREEN_COLOR);
    pen.SetWidth(1);
    ctx->SetPen(pen);

    index_to_px = (float)m_rGrid.GetWidth()/m_n_magdB;
    mag_dB_to_py = (float)m_rGrid.GetHeight()/(m_max_mag_db - m_min_mag_db);

    prev_x = PLOT_BORDER + XLEFT_OFFSET;
    prev_y = PLOT_BORDER;

    auto freq_hz_to_px = (float)m_rGrid.GetWidth()/(MAX_F_HZ-MIN_F_HZ);

    ctx->BeginLayer(1.0);
    wxGraphicsPath path = ctx->CreatePath();
    for(index = 0; index < m_n_magdB; index++)
    {
        x = index*index_to_px;

        switch(m_numSampleAveraging)
        {
            case 1:
                mag = m_magdB[index];
                break;
            case 2:
                mag = (m_magdB[index] + m_prevMagDB[index]) / 2;
                break;
            case 3:
                mag = (m_magdB[index] + m_prevMagDB[index] + m_nextPrevMagDB[index]) / 3;
                break;
            default:
                assert(0);
                mag = m_magdB[index]; // assume m_numSampleAveraging = 1
                break;
        }
        
        m_nextPrevMagDB[index] = m_prevMagDB[index];
        m_prevMagDB[index] = m_magdB[index];
        
        if (mag > m_max_mag_db) mag = m_max_mag_db;
        if (mag < m_min_mag_db) mag = m_min_mag_db;
        y = -(mag - m_max_mag_db) * mag_dB_to_py;

        x += PLOT_BORDER + XLEFT_OFFSET;
        y += PLOT_BORDER;

        if (index && (int)abs(x - prev_y) >= (int)(HZ_GRANULARITY*freq_hz_to_px) && abs(y - prev_y) >= mag_dB_to_py)
        {
            path.AddLineToPoint(x, y);
            prev_x = x; prev_y = y;
        }
        if (!index)
        {
            path.MoveToPoint(prev_x, prev_y);
        }
    }
    ctx->StrokePath(path);
    ctx->EndLayer();

    // and finally draw Graticule

    drawGraticule(ctx);

}

//-------------------------------------------------------------------------
// drawGraticule()
//-------------------------------------------------------------------------
void PlotSpectrum::drawGraticule(wxGraphicsContext* ctx)
{
    const int STR_LENGTH = 15;
    
    int      x, y, text_w, text_h;
    char     buf[STR_LENGTH];
    float    f, mag, freq_hz_to_px, mag_dB_to_py;

    wxBrush ltGraphBkgBrush;
    wxColour foregroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    ltGraphBkgBrush.SetStyle(wxBRUSHSTYLE_TRANSPARENT);
    ltGraphBkgBrush.SetColour(foregroundColor);
    ctx->SetBrush(ltGraphBkgBrush);
    ctx->SetPen(wxPen(foregroundColor, 1));
    
    wxGraphicsFont tmpFont = ctx->CreateFont(GetFont(), GetForegroundColour());
    ctx->SetFont(tmpFont);

    freq_hz_to_px = (float)m_rGrid.GetWidth()/(MAX_F_HZ-MIN_F_HZ);
    mag_dB_to_py = (float)m_rGrid.GetHeight()/(m_max_mag_db - m_min_mag_db);

    // upper LH coords of plot area are (PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER)
    // lower RH coords of plot area are (PLOT_BORDER + XLEFT_OFFSET + m_rGrid.GetWidth(), 
    //                                   PLOT_BORDER + m_rGrid.GetHeight())

    // Check if small screen size means text will overlap

    int textXStep = STEP_F_HZ*freq_hz_to_px;
    int textYStep = STEP_MAG_DB*mag_dB_to_py;
    snprintf(buf, STR_LENGTH, "%4.0fHz", (float)MAX_F_HZ - STEP_F_HZ);
    GetTextExtent(buf, &text_w, &text_h);
    int overlappedText = (text_w > textXStep) || (text_h > textYStep);
    //printf("text_w: %d textXStep: %d text_h: %d textYStep: %d  overlappedText: %d\n", text_w, textXStep, 
    //      text_h, textYStep, overlappedText);

    // Vertical gridlines

    for(f=STEP_F_HZ; f<MAX_F_HZ; f+=STEP_F_HZ) {
    x = f*freq_hz_to_px;
    x += PLOT_BORDER + XLEFT_OFFSET;

        ctx->SetPen(m_penShortDash);
        ctx->StrokeLine(x, m_rGrid.GetHeight() + PLOT_BORDER, x, PLOT_BORDER);
        ctx->SetPen(wxPen(foregroundColor, 1));
        ctx->StrokeLine(x, m_rGrid.GetHeight() + PLOT_BORDER, x, m_rGrid.GetHeight() + PLOT_BORDER + YBOTTOM_TEXT_OFFSET);

        snprintf(buf, STR_LENGTH, "%4.0fHz", f);
        GetTextExtent(buf, &text_w, &text_h);
        if (!overlappedText)
            ctx->DrawText(buf, x - text_w/2, m_rGrid.GetHeight() + PLOT_BORDER + YBOTTOM_TEXT_OFFSET);
    }

    ctx->SetPen(wxPen(foregroundColor, 1));
    for(f=STEP_MINOR_F_HZ; f<MAX_F_HZ; f+=STEP_MINOR_F_HZ) 
    {
        x = f*freq_hz_to_px;
        x += PLOT_BORDER + XLEFT_OFFSET;
        ctx->StrokeLine(x, m_rGrid.GetHeight() + PLOT_BORDER, x, m_rGrid.GetHeight() + PLOT_BORDER + YBOTTOM_TEXT_OFFSET-5);
    }
    
    // Horizontal gridlines

    ctx->SetPen(m_penDotDash);
    for(mag=m_min_mag_db; mag<=m_max_mag_db; mag+=STEP_MAG_DB) {
        y = -(mag - m_max_mag_db) * mag_dB_to_py;
        y += PLOT_BORDER;
        ctx->StrokeLine(PLOT_BORDER + XLEFT_OFFSET, y, 
                (m_rGrid.GetWidth() + PLOT_BORDER + XLEFT_OFFSET), y);
        snprintf(buf, STR_LENGTH, "%3.0fdB", mag);
        GetTextExtent(buf, &text_w, &text_h);
        if (!overlappedText)
            ctx->DrawText(buf, PLOT_BORDER + XLEFT_OFFSET - text_w - XLEFT_TEXT_OFFSET, y-text_h/2);
    }

    // red rx tuning line
    
    if (m_rxFreq != 0.0) {
        float verticalBarLength = m_rCtrl.GetHeight() - (m_rGrid.GetHeight()+ PLOT_BORDER);
   
        float sum = 0.0;
        for (auto& f : rxOffsets_)
        {
            sum += f;
        }
        float averageOffset = sum / rxOffsets_.size();
   
        // get average offset and draw sync tuning line
        ctx->SetPen(wxPen(sync_ ? GREEN_COLOR : ORANGE_COLOR, 3));
        x = (m_rxFreq + averageOffset) * freq_hz_to_px;
        x += PLOT_BORDER + XLEFT_OFFSET;
        ctx->StrokeLine(x, m_rGrid.GetHeight() + PLOT_BORDER, x, m_rCtrl.GetHeight());
   
        // red rx tuning line
        ctx->SetPen(wxPen(RED_COLOR, 3));
        x = m_rxFreq*freq_hz_to_px;
        x += PLOT_BORDER + XLEFT_OFFSET;
        ctx->StrokeLine(x, m_rGrid.GetHeight() + PLOT_BORDER, x, m_rCtrl.GetHeight() - verticalBarLength / 3);
    }

}

//-------------------------------------------------------------------------
// OnDoubleClickCommon()
//-------------------------------------------------------------------------
void PlotSpectrum::OnDoubleClickCommon(wxMouseEvent& event)
{
    m_mouseDown = true;
    wxClientDC dc(this);

    wxPoint pt(event.GetLogicalPosition(dc));

    // map x coord to edges of actual plot
    pt.x -= PLOT_BORDER + XLEFT_OFFSET;
    pt.y -= PLOT_BORDER;

    // valid click if inside of plot
    if ((pt.x >= 0) && (pt.x <= m_rGrid.GetWidth()) && (pt.y >=0) && m_clickTune) {
        float freq_hz_to_px = (float)m_rGrid.GetWidth()/(MAX_F_HZ-MIN_F_HZ);
        int clickFreq = (int)((float)pt.x/freq_hz_to_px);
        clickFreq -= clickFreq % 10;

        // see PlotWaterfall::OnMouseDown()

        clickTune(clickFreq);
    }
}

//-------------------------------------------------------------------------
// OnLeftMouseDown()
//-------------------------------------------------------------------------
void PlotSpectrum::OnMouseLeftDoubleClick(wxMouseEvent& event)
{
    OnDoubleClickCommon(event);
}

//-------------------------------------------------------------------------
// OnRightMouseDown()
//-------------------------------------------------------------------------
void PlotSpectrum::OnMouseRightDoubleClick(wxMouseEvent& event)
{
    OnDoubleClickCommon(event);
}

//-------------------------------------------------------------------------
// OnMouseWheelMoved()
//-------------------------------------------------------------------------
void PlotSpectrum::OnMouseWheelMoved(wxMouseEvent& event)
{
    float currRxFreq = FDMDV_FCENTRE - g_RxFreqOffsetHz;
    float direction = 1.0;
    if (event.GetWheelRotation() < 0)
    {
        direction = -1.0;
    }
    
    currRxFreq += direction * 10;
    if (currRxFreq < MIN_F_HZ)
    {
        currRxFreq = MIN_F_HZ;
    }
    else if (currRxFreq > MAX_F_HZ)
    {
        currRxFreq = MAX_F_HZ;
    }
    clickTune(currRxFreq);
}

//-------------------------------------------------------------------------
// OnKeyDown()
//-------------------------------------------------------------------------
void PlotSpectrum::OnKeyDown(wxKeyEvent& event)
{
    float currRxFreq = FDMDV_FCENTRE - g_RxFreqOffsetHz;
    float direction = 0;
    
    switch (event.GetKeyCode())
    {
        case WXK_DOWN:
            direction = -1.0;
            break;
        case WXK_UP:
            direction = 1.0;
            break;
    }
    
    if (direction)
    {
        currRxFreq += direction * 10;
        if (currRxFreq < MIN_F_HZ)
        {
            currRxFreq = MIN_F_HZ;
        }
        else if (currRxFreq > MAX_F_HZ)
        {
            currRxFreq = MAX_F_HZ;
        }
        clickTune(currRxFreq);
    }
}

//-------------------------------------------------------------------------
// OnMouseMiddleDown()
//-------------------------------------------------------------------------
void PlotSpectrum::OnMouseMiddleDown(wxMouseEvent& event)
{
    clickTune(FDMDV_FCENTRE);
}
