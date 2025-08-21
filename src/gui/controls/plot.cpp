//==========================================================================
// Name:            plot.cpp
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
#include <algorithm>
#include "plot.h"
#include <wx/graphics.h>

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
PlotPanel::PlotPanel(wxWindow* parent, const char* plotName) : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, plotName)
{
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
    
    m_zoomFactor        = 1.0;
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
// OnMouseMove()
//-------------------------------------------------------------------------
void PlotPanel::OnMouseMove(wxMouseEvent& event)
{
    // Default implementation is empty.
}

//-------------------------------------------------------------------------
// OnMouseLeftDown()
//-------------------------------------------------------------------------
void PlotPanel::OnMouseLeftDown(wxMouseEvent& event)
{
    // Default implementation is empty.
}

//-------------------------------------------------------------------------
// OnMouseRightDown()
//-------------------------------------------------------------------------
void PlotPanel::OnMouseRightDown(wxMouseEvent& event)
{
    // Default implementation is empty.
}

//-------------------------------------------------------------------------
// OnMouseWheelMoved()
//-------------------------------------------------------------------------
void PlotPanel::OnMouseWheelMoved(wxMouseEvent& event)
{
    // Default implementation is empty.
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
// drawGraticule()
//-------------------------------------------------------------------------
void PlotPanel::drawGraticule(wxGraphicsContext* ctx)
{
    const int STR_LENGTH = 15;
    
    int p;
    char buf[STR_LENGTH];
    wxString s;

    wxGraphicsFont tmpFont = ctx->CreateFont(GetFont(), GetForegroundColour());
    ctx->SetFont(tmpFont);
    
    // Vertical gridlines
    ctx->SetPen(m_penShortDash);
    for(p = (PLOT_BORDER + XLEFT_OFFSET + GRID_INCREMENT); p < ((m_rGrid.GetWidth() - XLEFT_OFFSET) + GRID_INCREMENT); p += GRID_INCREMENT)
    {
        ctx->StrokeLine(p, (m_rGrid.GetHeight() + PLOT_BORDER), p, PLOT_BORDER);
    }
    // Horizontal gridlines
    ctx->SetPen(m_penDotDash);
    for(p = (m_rGrid.GetHeight() - GRID_INCREMENT); p > PLOT_BORDER; p -= GRID_INCREMENT)
    {
        ctx->StrokeLine(PLOT_BORDER + XLEFT_OFFSET, (p + PLOT_BORDER), (m_rGrid.GetWidth() + PLOT_BORDER + XLEFT_OFFSET), (p + PLOT_BORDER));
    }
    // Label the X-Axis
    ctx->SetPen(wxPen(GREY_COLOR, 1));
    for(p = GRID_INCREMENT; p < (m_rGrid.GetWidth() - YBOTTOM_OFFSET); p += GRID_INCREMENT)
    {
        snprintf(buf, STR_LENGTH, "%1.1f Hz",(double)(p / 10));
        ctx->DrawText(buf, p - PLOT_BORDER + XLEFT_OFFSET, m_rGrid.GetHeight() + YBOTTOM_OFFSET/2);
    }
    // Label the Y-Axis
    //for(p = GRID_INCREMENT; p < (h - YBOTTOM_OFFSET); p += GRID_INCREMENT)
    for(p = (m_rGrid.GetHeight() - GRID_INCREMENT); p > PLOT_BORDER; p -= GRID_INCREMENT)
    {
        snprintf(buf, STR_LENGTH, "%1.0f", (double)((m_rGrid.GetHeight() - p) * -10));
        ctx->DrawText(buf, XLEFT_TEXT_OFFSET, p);
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
    wxAutoBufferedPaintDC dc(this);

    bool repaintDataOnly = !repaintAll_(evt);
    if (!repaintDataOnly)
    {
        // TBD -- move to wxGraphicsContext?
        dc.Clear();
    }
 
    wxGraphicsContext *gc = dc.GetGraphicsContext();
    gc->SetInterpolationQuality(wxINTERPOLATION_NONE);
    
    draw(gc, repaintDataOnly);
}

