//==========================================================================
// Name:            fdmdv2_plot.cpp
// Purpose:         Implements simple wxWidgets application with GUI.
// Created:         Apr. 9, 2012
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
#include "fdmdv2_plot.h"

BEGIN_EVENT_TABLE(PlotPanel, wxPanel)
    EVT_PAINT           (PlotPanel::OnPaint)
    EVT_MOTION          (PlotPanel::OnMouseMove)
    EVT_LEFT_DOWN       (PlotPanel::OnMouseLeftDown)
    EVT_LEFT_UP         (PlotPanel::OnMouseLeftUp)
    EVT_RIGHT_DOWN      (PlotPanel::OnMouseRightDown)
    EVT_MOUSEWHEEL      (PlotPanel::OnMouseWheelMoved)
    EVT_SIZE            (PlotPanel::OnSize)
    EVT_SHOW            (PlotPanel::OnShow)
END_EVENT_TABLE()

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class PlotPanel(wxFrame* parent) : wxPanel(parent)
//
// @class $(Name)
// @author $(User)
// @date $(Date)
// @file $(CurrentFileName).$(CurrentFileExt)
// @brief
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
PlotPanel::PlotPanel(wxFrame* parent) : wxPanel(parent)
{
    m_pNoteBook         = (wxAuiNotebook *) parent;
    m_pTopFrame         = (MainFrame *)m_pNoteBook->GetParent();
    m_zoomFactor        = 1.0;
    m_pBmp              = NULL;
    m_pPix              = NULL;
    m_firstPass         = true;
    m_line_color        = 0;
    m_newdata           = false;
    m_clip              = false;
    m_use_bitmap        = true;
    m_rubberBand        = false;
    m_mouseDown         = false;
    m_penShortDash      = wxPen(wxColor(0xA0, 0xA0, 0xA0), 1, wxPENSTYLE_SHORT_DASH);
    m_penDotDash        = wxPen(wxColor(0xD0, 0xD0, 0xD0), 1, wxPENSTYLE_DOT_DASH);
    m_penSolid          = wxPen(wxColor(0x00, 0x00, 0x00), 1, wxPENSTYLE_SOLID);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetLabelSize(10.0);
}

//-------------------------------------------------------------------------
// ~PlotPanel()
//-------------------------------------------------------------------------
PlotPanel::~PlotPanel()
{
    if(m_pBmp != NULL)
    {
        delete m_pBmp;
    }
}

//-------------------------------------------------------------------------
// GetLabelSize()
//-------------------------------------------------------------------------
double PlotPanel::GetLabelSize()
{
    return m_label_size;
}

//-------------------------------------------------------------------------
// SetLabelSize()
//-------------------------------------------------------------------------
void PlotPanel::SetLabelSize(double size)
{
    m_label_size = size;
}

//-------------------------------------------------------------------------
// OnShow()
//-------------------------------------------------------------------------
void PlotPanel::OnShow(wxShowEvent& event)
{
    this->Refresh();
}

//-------------------------------------------------------------------------
// OnErase()
//-------------------------------------------------------------------------
void PlotPanel::OnErase(wxEraseEvent& event)
{
    event.Skip();
}

//-------------------------------------------------------------------------
// OnSize()
//-------------------------------------------------------------------------
void PlotPanel::OnSize(wxSizeEvent& event)
{
    m_rCtrlPrev = m_rCtrl;
    m_rCtrl     = GetClientRect();
    if(m_use_bitmap)
    {
        if(!m_oImage.IsOk())
        {
            m_oImage.Create(m_rCtrl.GetWidth(), m_rCtrl.GetHeight(), true);
        }
        else
        {
            m_oImage.Rescale(m_rCtrl.GetWidth(), m_rCtrl.GetHeight());
        }
        m_pBmp = new wxBitmap(m_oImage, wxBITMAP_SCREEN_DEPTH);
        m_firstPass = true;
    }
    this->Refresh();
}

//-------------------------------------------------------------------------
// OnMouseMove()
//-------------------------------------------------------------------------
void PlotPanel::OnMouseMove(wxMouseEvent& event)
{
//    if(m_mouseDown)
//    {
//        paintNow();
//    }
}

//-------------------------------------------------------------------------
// OnMouseLeftDown()
//-------------------------------------------------------------------------
void PlotPanel::OnMouseLeftDown(wxMouseEvent& event)
{
}

//-------------------------------------------------------------------------
// OnMouseRightDown()
//-------------------------------------------------------------------------
void PlotPanel::OnMouseRightDown(wxMouseEvent& event)
{
}

//-------------------------------------------------------------------------
// OnMouseWheelMoved()
//-------------------------------------------------------------------------
void PlotPanel::OnMouseWheelMoved(wxMouseEvent& event)
{
}

//-------------------------------------------------------------------------
// OnMouseLeftUp()
//-------------------------------------------------------------------------
void PlotPanel::OnMouseLeftUp(wxMouseEvent& event)
{
    m_mouseDown = false;
}

//-------------------------------------------------------------------------
// SetZoomFactor()
//-------------------------------------------------------------------------
double PlotPanel::SetZoomFactor(double zf)
{
    if((zf > 0) && (zf < 5.0))
    {
        m_zoomFactor = zf;
    }
    return zf;
}

//-------------------------------------------------------------------------
// GetZoomFactor()
//-------------------------------------------------------------------------
double PlotPanel::GetZoomFactor(double zf)
{
    return m_zoomFactor;
}

//-------------------------------------------------------------------------
// draw()
//-------------------------------------------------------------------------
void PlotPanel::draw(wxAutoBufferedPaintDC& pDC)
{
    printf("PlotPanel::draw()");
    wxMemoryDC m_mDC;
    m_mDC.SelectObject(*m_pBmp);
    m_rCtrl  = GetClientRect();
    m_rGrid  = m_rCtrl;

    m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (XLEFT_OFFSET/2), (PLOT_BORDER + (YBOTTOM_OFFSET/2)));
    m_rGrid.Offset(PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER);

    pDC.Clear();
    m_rPlot = wxRect(PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER, m_rGrid.GetWidth(), m_rGrid.GetHeight());
    if(m_firstPass)
    {
        m_firstPass = false;
        m_mDC.FloodFill(0, 0, VERY_LTGREY_COLOR);

        // Draw a filled rectangle with aborder
        wxBrush ltGraphBkgBrush = wxBrush(DARK_BLUE_COLOR);
        m_mDC.SetBrush(ltGraphBkgBrush);
        m_mDC.SetPen(wxPen(BLACK_COLOR, 0));
        m_mDC.DrawRectangle(m_rPlot);
    }
    if(m_newdata)
    {
        m_newdata = false;
        int t = m_rPlot.GetTop();
        int l = m_rPlot.GetLeft();
//        int r = m_rPlot.GetRight();
        int h = m_rPlot.GetHeight();
        int w = m_rPlot.GetWidth();
        pDC.Blit(l, t, w, h, &m_mDC, l, t);
    }
    drawGraticule(pDC);
    m_mDC.SetBrush(wxNullBrush);
    m_mDC.SelectObject(wxNullBitmap);
}

//-------------------------------------------------------------------------
// drawGraticule()
//-------------------------------------------------------------------------
void PlotPanel::drawGraticule(wxAutoBufferedPaintDC& pDC)
{
    int p;
    char buf[15];
    wxString s;

    // Vertical gridlines
    pDC.SetPen(m_penShortDash);
    for(p = (PLOT_BORDER + XLEFT_OFFSET + GRID_INCREMENT); p < ((m_rGrid.GetWidth() - XLEFT_OFFSET) + GRID_INCREMENT); p += GRID_INCREMENT)
    {
        pDC.DrawLine(p, (m_rGrid.GetHeight() + PLOT_BORDER), p, PLOT_BORDER);
    }
    // Horizontal gridlines
    pDC.SetPen(m_penDotDash);
    for(p = (m_rGrid.GetHeight() - GRID_INCREMENT); p > PLOT_BORDER; p -= GRID_INCREMENT)
    {
        pDC.DrawLine(PLOT_BORDER + XLEFT_OFFSET, (p + PLOT_BORDER), (m_rGrid.GetWidth() + PLOT_BORDER + XLEFT_OFFSET), (p + PLOT_BORDER));
    }
    // Label the X-Axis
    pDC.SetPen(wxPen(GREY_COLOR, 1));
    for(p = GRID_INCREMENT; p < (m_rGrid.GetWidth() - YBOTTOM_OFFSET); p += GRID_INCREMENT)
    {
        sprintf(buf, "%1.1f Hz",(double)(p / 10));
        pDC.DrawText(buf, p - PLOT_BORDER + XLEFT_OFFSET, m_rGrid.GetHeight() + YBOTTOM_OFFSET/2);
    }
    // Label the Y-Axis
    //for(p = GRID_INCREMENT; p < (h - YBOTTOM_OFFSET); p += GRID_INCREMENT)
    for(p = (m_rGrid.GetHeight() - GRID_INCREMENT); p > PLOT_BORDER; p -= GRID_INCREMENT)
    {
        sprintf(buf, "%1.0f", (double)((m_rGrid.GetHeight() - p) * -10));
        pDC.DrawText(buf, XLEFT_TEXT_OFFSET, p);
    }
}

//-------------------------------------------------------------------------
// paintEvent()
//
// Called by the system of by wxWidgets when the panel needs
// to be redrawn. You can also trigger this call by calling
// Refresh()/Update().
//-------------------------------------------------------------------------
void PlotPanel::OnPaint(wxPaintEvent & evt)
{
    wxAutoBufferedPaintDC pdc(this);
    draw(pdc);
}

