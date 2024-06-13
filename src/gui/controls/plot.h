//==========================================================================
// Name:            plot.h
// Purpose:         Declares simple wxWidgets application with GUI
// Created:         Apr. 10, 2012
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
#ifndef __FDMDV2_PLOT__
#define __FDMDV2_PLOT__

#include <deque>

#include <wx/wx.h>
#include <wx/aui/auibook.h>
#include <wx/rawbmp.h>
#include <wx/image.h>
#include <wx/dcbuffer.h>

#define MAX_ZOOM            7
#define MAX_BMP_X           (400 * MAX_ZOOM)
#define MAX_BMP_Y           (400 * MAX_ZOOM)
#define DATA_LINE_HEIGHT    10
#define TEXT_BASELINE_OFFSET_Y  -5


#define wxUSE_FILEDLG       1
#define wxUSE_LIBPNG        1
#define wxUSE_LIBJPEG       1
#define wxUSE_GIF           1
#define wxUSE_PCX           1
#define wxUSE_LIBTIFF       1

#define PLOT_BORDER         12
#define XLEFT_OFFSET        40
#define XLEFT_TEXT_OFFSET   6
#define YBOTTOM_OFFSET      20
#define YBOTTOM_TEXT_OFFSET 15
#define GRID_INCREMENT      50

#define BLACK_COLOR         wxColor(0x00, 0x00, 0x00)
#define GREY_COLOR          wxColor(0x80, 0x80, 0x80)
#define DARK_GREY_COLOR     wxColor(0x40, 0x40, 0x40)
#define MEDIUM_GREY_COLOR   wxColor(0xC0, 0xC0, 0xC0)
#define LIGHT_GREY_COLOR    wxColor(0xE0, 0xE0, 0xE0)
#define VERY_LTGREY_COLOR   wxColor(0xF8, 0xF8, 0xF8)
#define WHITE_COLOR         wxColor(0xFF, 0xFF, 0xFF)

#define DARK_BLUE_COLOR     wxColor(0x00, 0x00, 0x60)
#define BLUE_COLOR          wxColor(0x00, 0x00, 0xFF)
#define LIGHT_BLUE_COLOR    wxColor(0x80, 0x80, 0xFF)

#define RED_COLOR           wxColor(0xFF, 0x5E, 0x5E)
#define LIGHT_RED_COLOR     wxColor(0xFF, 0xE0, 0xE0)
#define DARK_RED_COLOR      wxColor(0xFF, 0x00, 0x00)
#define PINK_COLOR          wxColor(0xFF, 0x80, 0xFF)

#define LIGHT_GREEN_COLOR   wxColor(0xE3, 0xFF, 0xE0)
#define GREEN_COLOR         wxColor(0x95, 0xFF, 0x8A)
#define DARK_GREEN_COLOR    wxColor(0x20, 0xFF, 0x08)
#define VERY_GREEN_COLOR    wxColor(0x00, 0xFF, 0x00)

#define YELLOW_COLOR        wxColor(0xFF, 0xFF, 0x5E)
#define LIGHT_YELLOW_COLOR  wxColor(0xFF, 0xFF, 0xB5)
#define DARK_YELLOW_COLOR   wxColor(0xFF, 0xFF, 0x08)

#define ORANGE_COLOR        wxColor(0xFF, 0xA5, 0x00)

class MainFrame;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class PlotPanel
//
// @class $(Name)
// @author $(User)
// @date $(Date)
// @file $(CurrentFileName).$(CurrentFileExt)
// @brief
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class PlotPanel : public wxPanel
{
    public:
        PlotPanel(wxWindow* parent, const char* plotName = "");
        ~PlotPanel();
        wxPen               m_penShortDash;
        wxPen               m_penDotDash;
        wxPen               m_penSolid;
        wxRect              m_rCtrl;
        wxRect              m_rGrid;
        wxRect              m_rPlot;
        double              m_label_size;
        wxSize              m_Bufsz;
        bool                m_newdata;

        // This function is added to ignore tabbing to the plot object.  The plot 
        // is a control with no user input, thus blind hams have no reason to tab
        // to it.
        virtual bool AcceptsFocusFromKeyboard() const { return false; }
        virtual bool AcceptsFocusRecursively() const { return false; }
        
        // some useful events
        void            OnMouseMove(wxMouseEvent& event);
        virtual void    OnMouseLeftDown(wxMouseEvent& event);
        void            OnMouseLeftUp(wxMouseEvent& event);
        virtual void    OnMouseRightDown(wxMouseEvent& event);
        void            OnMouseWheelMoved(wxMouseEvent& event);
        void            OnClose(wxCloseEvent& event ){ event.Skip(); }
        virtual void    OnSize( wxSizeEvent& event ) = 0;
        void            OnErase(wxEraseEvent& event);
        virtual void    OnPaint(wxPaintEvent& event);
        //void OnUpdateUI( wxUpdateUIEvent& event ){ event.Skip(); }

        void            paintEvent(wxPaintEvent & evt);
        virtual void    draw(wxGraphicsContext* ctx) = 0;
        virtual void    drawGraticule(wxGraphicsContext* ctx);
        virtual double  SetZoomFactor(double zf);
        virtual double  GetZoomFactor(double zf);
        virtual void    OnShow(wxShowEvent& event);
        virtual double  GetLabelSize();
        virtual void    SetLabelSize(double size);
        
        void setSync(bool sync) { sync_ = sync; }
        void addOffset(float offset)
        {
            rxOffsets_.push_back(offset);
            if (rxOffsets_.size() >= 20)
            {
                // Limit to ~2 seconds worth of offsets for averaging
                rxOffsets_.pop_front();
            }
        }

    protected:
        int             m_x;
        int             m_y;
        int             m_left;
        int             m_top;
        int             m_prev_w;
        int             m_prev_h;
        int             m_prev_x;
        int             m_prev_y;
        bool            m_use_bitmap;
        bool            m_clip;
        bool            m_rubberBand;
        bool            m_mouseDown;
        bool            m_firstPass;
        double          m_zoomFactor;
        int             m_greyscale;
        int             m_line_color;
        
        std::deque<float> rxOffsets_;
        bool        sync_;
        
    DECLARE_EVENT_TABLE()
};
#endif //__FDMDV2_PLOT__
