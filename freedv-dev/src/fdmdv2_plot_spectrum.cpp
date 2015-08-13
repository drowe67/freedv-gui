//==========================================================================
// Name:            fdmdv2_plot_waterfall.cpp
// Purpose:         Implements a waterfall plot derivative of fdmdv2_plot.
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
#include "wx/wx.h"

#include "fdmdv2_main.h"

extern float g_avmag[];                 // average mag data passed to draw() 
void fdmdv2_clickTune(float frequency); // callback to pass new click freq

BEGIN_EVENT_TABLE(PlotSpectrum, PlotPanel)
    EVT_MOTION          (PlotSpectrum::OnMouseMove)
    EVT_LEFT_DOWN       (PlotSpectrum::OnMouseLeftDown)
    EVT_LEFT_UP         (PlotSpectrum::OnMouseLeftUp)
    EVT_MOUSEWHEEL      (PlotSpectrum::OnMouseWheelMoved)
    EVT_PAINT           (PlotSpectrum::OnPaint)
    EVT_SHOW            (PlotSpectrum::OnShow)
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
PlotSpectrum::PlotSpectrum(wxFrame* parent, float *magdB, int n_magdB, 
                           float min_mag_db, float max_mag_db, bool clickTune): PlotPanel(parent)
{
    m_greyscale     = 0;
    m_Bufsz         = GetMaxClientSize();
    m_newdata       = false;
    m_firstPass     = true;
    m_line_color    = 0;
    SetLabelSize(10.0);

    m_magdB         = magdB;
    m_n_magdB       = n_magdB;     // number of points in magdB that covers 0 ... MAX_F_HZ of spectrum
    m_max_mag_db    = max_mag_db;
    m_min_mag_db    = min_mag_db;
    m_rxFreq        = 0.0;
    m_clickTune     = clickTune;
}

//----------------------------------------------------------------
// ~PlotSpectrum()
//----------------------------------------------------------------
PlotSpectrum::~PlotSpectrum()
{
}

//----------------------------------------------------------------
// OnSize()
//----------------------------------------------------------------
void PlotSpectrum::OnSize(wxSizeEvent& event) {
}

//----------------------------------------------------------------
// OnPaint()
//----------------------------------------------------------------
void PlotSpectrum::OnPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(this);
    draw(dc);
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
void PlotSpectrum::draw(wxAutoBufferedPaintDC& dc)
{
    m_rCtrl  = GetClientRect();

    // m_rGrid is coords of inner window we actually plot to.  We deflate it a bit
    // to leave room for axis labels.  We need to work this out every time we draw
    // as OnSize() may not be called before OnPaint(), for example when a new tab
    // is selected

    m_rGrid  = m_rCtrl;
    m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (XLEFT_OFFSET/2), (PLOT_BORDER + (YBOTTOM_OFFSET/2)));

    dc.Clear();

    // black background

    m_rPlot = wxRect(PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER, m_rGrid.GetWidth(), m_rGrid.GetHeight());
    wxBrush ltGraphBkgBrush = wxBrush(BLACK_COLOR);
    dc.SetBrush(ltGraphBkgBrush);
    dc.SetPen(wxPen(BLACK_COLOR, 0));
    dc.DrawRectangle(m_rPlot);

    // draw spectrum

    int   x, y, prev_x, prev_y, index;
    float index_to_px, mag_dB_to_py, mag;

    m_newdata = false;

    wxPen pen;
    pen.SetColour(DARK_GREEN_COLOR);
    pen.SetWidth(1);
    dc.SetPen(pen);

    index_to_px = (float)m_rGrid.GetWidth()/m_n_magdB;
    mag_dB_to_py = (float)m_rGrid.GetHeight()/(m_max_mag_db - m_min_mag_db);

    prev_x = PLOT_BORDER + XLEFT_OFFSET;
    prev_y = PLOT_BORDER;
    for(index = 0; index < m_n_magdB; index++)
    {
        x = index*index_to_px;
        mag = m_magdB[index];
        if (mag > m_max_mag_db) mag = m_max_mag_db;
        if (mag < m_min_mag_db) mag = m_min_mag_db;
        y = -(mag - m_max_mag_db) * mag_dB_to_py;

        x += PLOT_BORDER + XLEFT_OFFSET;
        y += PLOT_BORDER;

        if (index)
            dc.DrawLine(x, y, prev_x, prev_y);
        prev_x = x; prev_y = y;
    }

    // and finally draw Graticule

    drawGraticule(dc);

}

//-------------------------------------------------------------------------
// drawGraticule()
//-------------------------------------------------------------------------
void PlotSpectrum::drawGraticule(wxAutoBufferedPaintDC&  dc)
{
    int      x, y, text_w, text_h;
    char     buf[15];
    wxString s;
    float    f, mag, freq_hz_to_px, mag_dB_to_py;

    wxBrush ltGraphBkgBrush;
    ltGraphBkgBrush.SetStyle(wxBRUSHSTYLE_TRANSPARENT);
    ltGraphBkgBrush.SetColour(*wxBLACK);
    dc.SetBrush(ltGraphBkgBrush);
    dc.SetPen(wxPen(BLACK_COLOR, 1));

    freq_hz_to_px = (float)m_rGrid.GetWidth()/(MAX_F_HZ-MIN_F_HZ);
    mag_dB_to_py = (float)m_rGrid.GetHeight()/(m_max_mag_db - m_min_mag_db);

    // upper LH coords of plot area are (PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER)
    // lower RH coords of plot area are (PLOT_BORDER + XLEFT_OFFSET + m_rGrid.GetWidth(), 
    //                                   PLOT_BORDER + m_rGrid.GetHeight())

    // Check if small screen size means text will overlap

    int textXStep = STEP_F_HZ*freq_hz_to_px;
    int textYStep = STEP_MAG_DB*mag_dB_to_py;
    sprintf(buf, "%4.0fHz", (float)MAX_F_HZ - STEP_F_HZ);
    GetTextExtent(buf, &text_w, &text_h);
    int overlappedText = (text_w > textXStep) || (text_h > textYStep);
    //printf("text_w: %d textXStep: %d text_h: %d textYStep: %d  overlappedText: %d\n", text_w, textXStep, 
    //      text_h, textYStep, overlappedText);

    // Vertical gridlines

    for(f=STEP_F_HZ; f<MAX_F_HZ; f+=STEP_F_HZ) {
	x = f*freq_hz_to_px;
	x += PLOT_BORDER + XLEFT_OFFSET;

        dc.SetPen(m_penShortDash);
        dc.DrawLine(x, m_rGrid.GetHeight() + PLOT_BORDER, x, PLOT_BORDER);
        dc.SetPen(wxPen(BLACK_COLOR, 1));
        dc.DrawLine(x, m_rGrid.GetHeight() + PLOT_BORDER, x, m_rGrid.GetHeight() + PLOT_BORDER + YBOTTOM_TEXT_OFFSET);

        sprintf(buf, "%4.0fHz", f);
	GetTextExtent(buf, &text_w, &text_h);
        if (!overlappedText)
            dc.DrawText(buf, x - text_w/2, m_rGrid.GetHeight() + PLOT_BORDER + YBOTTOM_TEXT_OFFSET);
    }

    dc.SetPen(wxPen(BLACK_COLOR, 1));
    for(f=STEP_MINOR_F_HZ; f<MAX_F_HZ; f+=STEP_MINOR_F_HZ) 
    {
        x = f*freq_hz_to_px;
        x += PLOT_BORDER + XLEFT_OFFSET;
        dc.DrawLine(x, m_rGrid.GetHeight() + PLOT_BORDER, x, m_rGrid.GetHeight() + PLOT_BORDER + YBOTTOM_TEXT_OFFSET-5);
    }
    
    // Horizontal gridlines

    dc.SetPen(m_penDotDash);
    for(mag=m_min_mag_db; mag<=m_max_mag_db; mag+=STEP_MAG_DB) {
	y = -(mag - m_max_mag_db) * mag_dB_to_py;
	y += PLOT_BORDER;
	dc.DrawLine(PLOT_BORDER + XLEFT_OFFSET, y, 
		    (m_rGrid.GetWidth() + PLOT_BORDER + XLEFT_OFFSET), y);
        sprintf(buf, "%3.0fdB", mag);
	GetTextExtent(buf, &text_w, &text_h);
        if (!overlappedText)
            dc.DrawText(buf, PLOT_BORDER + XLEFT_OFFSET - text_w - XLEFT_TEXT_OFFSET, y-text_h/2);
    }

    // red rx tuning line
    
    if (m_rxFreq != 0.0) {
        dc.SetPen(wxPen(RED_COLOR, 2));
        x = m_rxFreq*freq_hz_to_px;
        x += PLOT_BORDER + XLEFT_OFFSET;
        //printf("m_rxFreq %f x %d\n", m_rxFreq, x);
        dc.DrawLine(x, m_rGrid.GetHeight()+ PLOT_BORDER, x, m_rCtrl.GetHeight());
    }

}

//-------------------------------------------------------------------------
// OnMouseDown()
//-------------------------------------------------------------------------
void PlotSpectrum::OnMouseLeftDown(wxMouseEvent& event)
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
        float clickFreq = (float)pt.x/freq_hz_to_px;

        // see PlotWaterfall::OnMouseDown()

        fdmdv2_clickTune(clickFreq);
    }
}
