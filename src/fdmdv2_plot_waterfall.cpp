//==========================================================================
// Name:            fdmdv2_plot_waterfall.cpp
// Purpose:         Implements a waterfall plot derivative of fdmdv2_plot.
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
#include "wx/wx.h"
#include "fdmdv2_main.h"

extern float g_avmag[];                 // av mag spec passed in to draw() 
void fdmdv2_clickTune(float frequency); // callback to pass new click freq

BEGIN_EVENT_TABLE(PlotWaterfall, PlotPanel)
    EVT_PAINT           (PlotWaterfall::OnPaint)
    EVT_MOTION          (PlotWaterfall::OnMouseMove)
    EVT_LEFT_DCLICK     (PlotWaterfall::OnMouseLeftDoubleClick)
    EVT_RIGHT_DOWN      (PlotWaterfall::OnMouseRightDown)
    EVT_LEFT_UP         (PlotWaterfall::OnMouseLeftUp)
    EVT_MOUSEWHEEL      (PlotWaterfall::OnMouseWheelMoved)
    EVT_SIZE            (PlotWaterfall::OnSize)
    EVT_SHOW            (PlotWaterfall::OnShow)
END_EVENT_TABLE()

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class WaterfallPlot
//
// @class   WaterfallPlot
// @author  David Witten
// @date    $(Date)
// @file    $(CurrentFileName).$(CurrentFileExt)
// @brief
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
PlotWaterfall::PlotWaterfall(wxFrame* parent, bool graticule, int colour): PlotPanel(parent)
{

    for(int i = 0; i < 255; i++)
    {
        m_heatmap_lut[i] = heatmap((float)i, 0.0, 255.0);
    }
    m_graticule     = graticule;
    m_colour        = colour;
    m_Bufsz         = GetMaxClientSize();
    m_newdata       = false;
    m_firstPass     = true;
    m_line_color    = 0;
    m_modem_stats_max_f_hz = MODEM_STATS_MAX_F_HZ;

    SetLabelSize(10.0);

    m_pBmp = NULL;
    m_max_mag = MAX_MAG_DB;
    m_min_mag = MIN_MAG_DB;
}

// When the window size gets set we can work outthe size of the window
// we plot in and allocate a bit map of the correct size
void PlotWaterfall::OnSize(wxSizeEvent& event) 
{
    // resize bit map

    delete m_pBmp;
    
    m_rCtrl  = GetClientRect();

    // m_rGrid is coords of inner window we actually plot to.  We deflate it a bit
    // to leave room for axis labels.

    m_rGrid  = m_rCtrl;
    m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (XLEFT_OFFSET/2), (PLOT_BORDER + (YBOTTOM_OFFSET/2)));

    // we want a bit map the size of m_rGrid

    m_pBmp = new wxBitmap(m_rGrid.GetWidth(), m_rGrid.GetHeight(), 24);

    m_dT = DT;
}

//----------------------------------------------------------------
// paintEvent()
//
// @class $(Name)
// @author $(User)
// @date $(Date)
// @file $(CurrentFileName).$(CurrentFileExt)
// @brief
//
// Called by the system of by wxWidgets when the panel needs
// to be redrawn. You can also trigger this call by calling
// Refresh()/Update().
//----------------------------------------------------------------
void PlotWaterfall::OnPaint(wxPaintEvent & evt)
{
    wxAutoBufferedPaintDC dc(this);
    draw(dc);
}

//----------------------------------------------------------------
// OnShow()
//----------------------------------------------------------------
void PlotWaterfall::OnShow(wxShowEvent& event)
{
}

//----------------------------------------------------------------
// ~PlotWaterfall()
//----------------------------------------------------------------
PlotWaterfall::~PlotWaterfall()
{
}

//----------------------------------------------------------------
// heatmap()
// map val to a rgb colour
// from http://eddiema.ca/2011/01/21/c-sharp-heatmaps/
//----------------------------------------------------------------
unsigned PlotWaterfall::heatmap(float val, float min, float max)
{
    unsigned r = 0;
    unsigned g = 0;
    unsigned b = 0;

    val = (val - min) / (max - min);
    if(val <= 0.2)
    {
        b = (unsigned)((val / 0.2) * 255);
    }
    else if(val >  0.2 &&  val <= 0.7)
    {
        b = (unsigned)((1.0 - ((val - 0.2) / 0.5)) * 255);
    }
    if(val >= 0.2 &&  val <= 0.6)
    {
        g = (unsigned)(((val - 0.2) / 0.4) * 255);
    }
    else if(val >  0.6 &&  val <= 0.9)
    {
        g = (unsigned)((1.0 - ((val - 0.6) / 0.3)) * 255);
    }
    if(val >= 0.5)
    {
        r = (unsigned)(((val - 0.5) / 0.5) * 255);
    }
    //printf("%f %x %x %x\n", val, r, g, b);
    return  (b << 16) + (g << 8) + r;
}

bool PlotWaterfall::checkDT(void)
{
    // Check dY is > 1 pixel before proceeding. For small screens
    // and large WATERFALL_SECS_Y we might have less than one
    // block per pixel.  In this case increase m_dT and perform draw
    // less often

    float px_per_sec = (float)m_rGrid.GetHeight() / WATERFALL_SECS_Y;
    float dy = m_dT * px_per_sec;
    
    if (dy < 1.0) {
        m_dT += DT;
        return false;
    }
    else
        return true;
}

//----------------------------------------------------------------
// draw()
//----------------------------------------------------------------
void PlotWaterfall::draw(wxAutoBufferedPaintDC& dc)
{

    m_rCtrl  = GetClientRect();

    // m_rGrid is coords of inner window we actually plot to.  We deflate it a bit
    // to leave room for axis labels.

    m_rGrid = m_rCtrl;
    m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (XLEFT_OFFSET/2), (PLOT_BORDER + (YBOTTOM_OFFSET/2)));

    if (m_pBmp == NULL) 
    {
        // we want a bit map the size of m_rGrid
        m_pBmp = new wxBitmap(m_rGrid.GetWidth(), m_rGrid.GetHeight(), 24);
    }

    dc.Clear();

    if(m_newdata)
    {
        m_newdata = false;
        plotPixelData();
        dc.DrawBitmap(*m_pBmp, PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER);
        m_dT = DT;
    }
    else 
    {

        // no data to plot so just erase to black.  Blue looks nicer
        // but is same colour as low amplitude signal

        // Bug on Linux: When Stop is pressed this code doesn't erase
        // the lower 25% of the Waterfall Window

        m_rPlot = wxRect(PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER, m_rGrid.GetWidth(), m_rGrid.GetHeight());
        wxBrush ltGraphBkgBrush = wxBrush(BLACK_COLOR);
        dc.SetBrush(ltGraphBkgBrush);
        dc.SetPen(wxPen(BLACK_COLOR, 0));
        dc.DrawRectangle(m_rPlot);
    }
    drawGraticule(dc);
}

//-------------------------------------------------------------------------
// drawGraticule()
//-------------------------------------------------------------------------
void PlotWaterfall::drawGraticule(wxAutoBufferedPaintDC& dc)
{
    int      x, y, text_w, text_h;
    char     buf[15];
    wxString s;
    float    f, time, freq_hz_to_px, time_s_to_py;

    wxBrush ltGraphBkgBrush;
    ltGraphBkgBrush.SetStyle(wxBRUSHSTYLE_TRANSPARENT);
    ltGraphBkgBrush.SetColour(*wxBLACK);
    dc.SetBrush(ltGraphBkgBrush);
    dc.SetPen(wxPen(BLACK_COLOR, 1));

    freq_hz_to_px = (float)m_rGrid.GetWidth()/(MAX_F_HZ-MIN_F_HZ);
    time_s_to_py = (float)m_rGrid.GetHeight()/WATERFALL_SECS_Y;

    // upper LH coords of plot area are (PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER)
    // lower RH coords of plot area are (PLOT_BORDER + XLEFT_OFFSET + m_rGrid.GetWidth(), 
    //                                   PLOT_BORDER + m_rGrid.GetHeight())

    // Check if small screen size means text will overlap

    int textXStep = STEP_F_HZ*freq_hz_to_px;
    int textYStep = WATERFALL_SECS_STEP*time_s_to_py;
    sprintf(buf, "%4.0fHz", (float)MAX_F_HZ - STEP_F_HZ);
    GetTextExtent(buf, &text_w, &text_h);
    int overlappedText = (text_w > textXStep) || (text_h > textYStep);

    // Major Vertical gridlines and legend
    //dc.SetPen(m_penShortDash);
    for(f=STEP_F_HZ; f<MAX_F_HZ; f+=STEP_F_HZ) 
    {
        x = f*freq_hz_to_px;
        x += PLOT_BORDER + XLEFT_OFFSET;

        if (m_graticule)
            dc.DrawLine(x, m_rGrid.GetHeight() + PLOT_BORDER, x, PLOT_BORDER);
        else
            dc.DrawLine(x, m_rGrid.GetHeight() + PLOT_BORDER, x, m_rGrid.GetHeight() + PLOT_BORDER + YBOTTOM_TEXT_OFFSET);
            
        sprintf(buf, "%4.0fHz", f);
        GetTextExtent(buf, &text_w, &text_h);
        if (!overlappedText)
            dc.DrawText(buf, x - text_w/2, m_rGrid.GetHeight() + PLOT_BORDER + YBOTTOM_TEXT_OFFSET);
    }

    for(f=STEP_MINOR_F_HZ; f<MAX_F_HZ; f+=STEP_MINOR_F_HZ) 
    {
        x = f*freq_hz_to_px;
        x += PLOT_BORDER + XLEFT_OFFSET;
        dc.DrawLine(x, m_rGrid.GetHeight() + PLOT_BORDER, x, m_rGrid.GetHeight() + PLOT_BORDER + YBOTTOM_TEXT_OFFSET-5);
    }
    
    // Horizontal gridlines
    dc.SetPen(m_penDotDash);
    for(time=0; time<=WATERFALL_SECS_Y; time+=WATERFALL_SECS_STEP) {
       y = m_rGrid.GetHeight() - time*time_s_to_py;
       y += PLOT_BORDER;

        if (m_graticule)
            dc.DrawLine(PLOT_BORDER + XLEFT_OFFSET, y, 
                        (m_rGrid.GetWidth() + PLOT_BORDER + XLEFT_OFFSET), y);
        sprintf(buf, "%3.0fs", time);
	GetTextExtent(buf, &text_w, &text_h);
        if (!overlappedText)
            dc.DrawText(buf, PLOT_BORDER + XLEFT_OFFSET - text_w - XLEFT_TEXT_OFFSET, y-text_h/2);
   }

    // red rx tuning line
    dc.SetPen(wxPen(RED_COLOR, 2));
    x = m_rxFreq*freq_hz_to_px;
    x += PLOT_BORDER + XLEFT_OFFSET;
    //printf("m_rxFreq %f x %d\n", m_rxFreq, x);
    dc.DrawLine(x, m_rGrid.GetHeight()+ PLOT_BORDER, x, m_rCtrl.GetHeight());
    
}

//-------------------------------------------------------------------------
// plotPixelData()
//-------------------------------------------------------------------------
void PlotWaterfall::plotPixelData()
{
    float       spec_index_per_px;
    float       intensity_per_dB;
    float       px_per_sec;
    int         index;
    float       dy;
    int         dy_blocks;
    int         b;
    int         px;
    int         py;
    int         intensity;

    /*
      Design Notes:

      The height in pixels represents WATERFALL_SECS_Y of data.  Every DT
      seconds we get a vector of MODEM_STATS_NSPEC spectrum samples which we use
      to update the last row.  The height of each row is dy pixels, which
      maps to DT seconds.  We call each dy high rectangle of pixels a
      block.

    */

    // determine dy, the height of one "block"
    px_per_sec = (float)m_rGrid.GetHeight() / WATERFALL_SECS_Y;
    dy = m_dT * px_per_sec;

    // number of dy high blocks in spectrogram
    dy_blocks = m_rGrid.GetHeight()/ dy;

    // update min and max amplitude estimates
    float max_mag = MIN_MAG_DB;

    int min_fft_bin=((float)200/m_modem_stats_max_f_hz)*MODEM_STATS_NSPEC;
    int max_fft_bin=((float)2800/m_modem_stats_max_f_hz)*MODEM_STATS_NSPEC;

    for(int i=min_fft_bin; i<max_fft_bin; i++) 
    {
        if (g_avmag[i] > max_mag)
        {
            max_mag = g_avmag[i];
        }
    }

    m_max_mag = BETA*m_max_mag + (1 - BETA)*max_mag;
    m_min_mag = max_mag - 20.0;
    //printf("max_mag: %f m_max_mag: %f\n", max_mag, m_max_mag);
    //intensity_per_dB  = (float)256 /(MAX_MAG_DB - MIN_MAG_DB);
    intensity_per_dB  = (float)256 /(m_max_mag - m_min_mag);
    spec_index_per_px = ((float)(MAX_F_HZ)/(float)m_modem_stats_max_f_hz)*(float)MODEM_STATS_NSPEC / (float) m_rGrid.GetWidth();

    /*
    printf("h %d w %d px_per_sec %d dy %d dy_blocks %d spec_index_per_px: %f\n", 
       m_rGrid.GetHeight(), m_rGrid.GetWidth(), px_per_sec, 
       dy, dy_blocks, spec_index_per_px);
    */

    // Shift previous bit map up one row of blocks ----------------------------
    wxNativePixelData data(*m_pBmp);
    wxNativePixelData::Iterator bitMapStart(data);
    wxNativePixelData::Iterator p = bitMapStart;

    for(b = 0; b < dy_blocks - 1; b++) 
    {
        wxNativePixelData::Iterator psrc = bitMapStart;
        wxNativePixelData::Iterator pdest = bitMapStart;
        pdest.OffsetY(data, dy * b);
        psrc.OffsetY(data, dy * (b+1));

        // copy one line of blocks

        for(py = 0; py < dy; py++) 
        {
            wxNativePixelData::Iterator pdestRowStart = pdest;
            wxNativePixelData::Iterator psrcRowStart = psrc;

            for(px = 0; px < m_rGrid.GetWidth(); px++) 
            {
                pdest.Red() = psrc.Red();
                pdest.Green() = psrc.Green();
                pdest.Blue() = psrc.Blue();
                pdest++;
                psrc++;
            }
            pdest = pdestRowStart;
            pdest.OffsetY(data, 1);
            psrc = psrcRowStart;
            psrc.OffsetY(data, 1);	    
        }
    }

    // Draw last line of blocks using latest amplitude data ------------------
    p = bitMapStart;
    p.OffsetY(data, dy *(dy_blocks - 1));
    for(py = 0; py < dy; py++)
    {
        wxNativePixelData::Iterator rowStart = p;

        for(px = 0; px < m_rGrid.GetWidth(); px++)
        {
            index = px * spec_index_per_px;
            assert(index < MODEM_STATS_NSPEC);

            intensity = intensity_per_dB * (g_avmag[index] - m_min_mag);
            if(intensity > 255) intensity = 255;
            if (intensity < 0) intensity = 0;
            //printf("%d %f %d \n", index, g_avmag[index], intensity);

            switch (m_colour) {
            case 0:
                p.Red() = m_heatmap_lut[intensity] & 0xff;
                p.Green() = (m_heatmap_lut[intensity] >> 8) & 0xff;
                p.Blue() = (m_heatmap_lut[intensity] >> 16) & 0xff;
                break;
            case 1:
                p.Red() = intensity;
                p.Green() = intensity;
                p.Blue() = intensity;       
                break;
            case 2:
                p.Red() = intensity;
                p.Green() = intensity;
                if (intensity < 127)
                    p.Blue() = intensity*2;
                else
                    p.Blue() = 255;
                        
                break;
            }
            ++p;
        }
        p = rowStart;
        p.OffsetY(data, 1);
    }

}

//-------------------------------------------------------------------------
// OnMouseLeftDown()
//-------------------------------------------------------------------------
void PlotWaterfall::OnMouseLeftDoubleClick(wxMouseEvent& event)
{
    m_mouseDown = true;
    wxClientDC dc(this);

    wxPoint pt(event.GetLogicalPosition(dc));

    // map x coord to edges of actual plot
    pt.x -= PLOT_BORDER + XLEFT_OFFSET;
    pt.y -= PLOT_BORDER;

    // valid click if inside of plot
    if ((pt.x >= 0) && (pt.x <= m_rGrid.GetWidth()) && (pt.y >=0)) 
    {
        float freq_hz_to_px = (float)m_rGrid.GetWidth()/(MAX_F_HZ-MIN_F_HZ);
        float clickFreq = (float)pt.x/freq_hz_to_px;

        // communicate back to other threads
        fdmdv2_clickTune(clickFreq);
    }
}

//-------------------------------------------------------------------------
// OnMouseRightDown()
//-------------------------------------------------------------------------
void PlotWaterfall::OnMouseRightDown(wxMouseEvent& event)
{
    m_colour++;
    if (m_colour == 3)
        m_colour = 0;
}

