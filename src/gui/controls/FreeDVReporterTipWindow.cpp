// NOTE (MS, 2026-01-08): Copied original file from wxWidgets 3.3.1 and modified 
// to enable FreeDV Reporter specific functionality. Original copyright is below:

///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/tipwin.cpp
// Purpose:     implementation of wxTipWindow
// Author:      Vadim Zeitlin
// Created:     10.09.00
// Copyright:   (c) 2000 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wx.h"

#include "FreeDVReporterTipWindow.h"

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
    #include "wx/timer.h"
    #include "wx/settings.h"
#endif // WX_PRECOMP

#include "wx/display.h"
#include "wx/vector.h"

#if defined(WIN32)
#include "wx/graphics.h"
#endif // defined(WIN32)

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

static const wxCoord TEXT_MARGIN_X = 3;
static const wxCoord TEXT_MARGIN_Y = 3;

// ----------------------------------------------------------------------------
// FreeDVReporterTipWindowView
// ----------------------------------------------------------------------------

// Viewer window to put in the frame
class FreeDVReporterTipWindowView : public wxWindow
{
public:
    FreeDVReporterTipWindowView(wxWindow *parent);

    // event handlers
    void OnPaint(wxPaintEvent& event);
    void OnLeftMouseClick(wxMouseEvent& event);
    void OnMiddleMouseClick(wxMouseEvent& event);
    void OnRightMouseClick(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);

    // calculate the client rect we need to display the text
    void Adjust(const wxString& text, wxCoord maxLength);

private:
    FreeDVReporterTipWindow* m_parent;

    wxVector<wxString> m_textLines;
    wxCoord m_heightLine;


    wxDECLARE_EVENT_TABLE();
    wxDECLARE_NO_COPY_CLASS(FreeDVReporterTipWindowView);
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// event tables
// ----------------------------------------------------------------------------

wxBEGIN_EVENT_TABLE(FreeDVReporterTipWindow, wxPopupTransientWindow)
    EVT_LEFT_DOWN(FreeDVReporterTipWindow::OnLeftMouseClick)
    EVT_RIGHT_DOWN(FreeDVReporterTipWindow::OnRightMouseClick)
    EVT_MIDDLE_DOWN(FreeDVReporterTipWindow::OnMiddleMouseClick)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(FreeDVReporterTipWindowView, wxWindow)
    EVT_PAINT(FreeDVReporterTipWindowView::OnPaint)

    EVT_LEFT_DOWN(FreeDVReporterTipWindowView::OnLeftMouseClick)
    EVT_RIGHT_DOWN(FreeDVReporterTipWindowView::OnRightMouseClick)
    EVT_MIDDLE_DOWN(FreeDVReporterTipWindowView::OnMiddleMouseClick)

    EVT_MOTION(FreeDVReporterTipWindowView::OnMouseMove)
wxEND_EVENT_TABLE()

// ----------------------------------------------------------------------------
// FreeDVReporterTipWindow
// ----------------------------------------------------------------------------

FreeDVReporterTipWindow::FreeDVReporterTipWindow(wxWindow *parent,
                         const wxString& text,
                         std::function<void(wxMouseEvent&)> mouseLeftHandler,
                         std::function<void(wxMouseEvent&)> mouseRightHandler,
                         wxCoord maxLength,
                         FreeDVReporterTipWindow** windowPtr,
                         wxRect *rectBounds)
           : wxPopupTransientWindow(parent)
           , mouseLeftHandler_(mouseLeftHandler)
           , mouseRightHandler_(mouseRightHandler)
{
    SetTipWindowPtr(windowPtr);
    if ( rectBounds )
    {
        SetBoundingRect(*rectBounds);
    }

    // set colours
    SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT));
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));

    int x, y;
    wxGetMousePosition(&x, &y);

    // move to the center of the target display so wxTipWindowView will use the
    // correct DPI
    wxPoint posScreen;
    wxSize sizeScreen;

    const int displayNum = wxDisplay::GetFromPoint(wxPoint(x, y));
    if ( displayNum != wxNOT_FOUND )
    {
        const wxRect rectScreen = wxDisplay(displayNum).GetGeometry();
        posScreen = rectScreen.GetPosition();
        sizeScreen = rectScreen.GetSize();
    }
    else // outside of any display?
    {
        // just use the primary one then
        posScreen = wxPoint(0, 0);
        sizeScreen = wxGetDisplaySize();
    }
    wxPoint center(posScreen.x + sizeScreen.GetWidth() / 2,
                   posScreen.y + sizeScreen.GetHeight() / 2);
    Move(center, wxSIZE_NO_ADJUSTMENTS);

    // set size, position and show it
    m_view = new FreeDVReporterTipWindowView(this);
    m_view->Adjust(text, FromDIP(parent->ToDIP(maxLength)) );

    // we want to show the tip below the mouse, not over it, make sure to not
    // overflow into the next display
    //
    // NB: the reason we use "/ 2" here is that we don't know where the current
    //     cursors hot spot is... it would be nice if we could find this out
    //     though
    int cursorOffset = wxSystemSettings::GetMetric(wxSYS_CURSOR_Y, this) / 2;
    if (y + cursorOffset >= posScreen.y + sizeScreen.GetHeight())
        cursorOffset = posScreen.y + sizeScreen.GetHeight() - y - 1;
    y += cursorOffset;

    Position(wxPoint(x, y), wxSize(0,0));
    Popup(m_view);
    #ifdef __WXGTK__
        m_view->CaptureMouse();
    #endif
}

FreeDVReporterTipWindow::~FreeDVReporterTipWindow()
{
    if ( m_windowPtr )
    {
        *m_windowPtr = nullptr;
    }
    #ifdef __WXGTK__
        if ( m_view->HasCapture() )
            m_view->ReleaseMouse();
    #endif
}

void FreeDVReporterTipWindow::OnLeftMouseClick(wxMouseEvent& event)
{
    mouseLeftHandler_(event);
    Close();
}

void FreeDVReporterTipWindow::OnMiddleMouseClick(wxMouseEvent& WXUNUSED(event))
{
    Close();
}
void FreeDVReporterTipWindow::OnRightMouseClick(wxMouseEvent& event)
{
    mouseRightHandler_(event);
    Close();
}

void FreeDVReporterTipWindow::OnDismiss()
{
    Close();
}

void FreeDVReporterTipWindow::SetBoundingRect(const wxRect& rectBound)
{
    m_rectBound = rectBound;
}

void FreeDVReporterTipWindow::Close()
{
    if ( m_windowPtr )
    {
        *m_windowPtr = nullptr;
        m_windowPtr = nullptr;
    }

    Show(false);
    #ifdef __WXGTK__
        if ( m_view->HasCapture() )
            m_view->ReleaseMouse();
    #endif
    // Under OS X and Qt we get destroyed because of wxEVT_KILL_FOCUS generated by
    // Show(false).
    #if !defined(__WXOSX__) && !defined(__WXQT__)
        Destroy();
    #endif
}

// ----------------------------------------------------------------------------
// wxTipWindowView
// ----------------------------------------------------------------------------

FreeDVReporterTipWindowView::FreeDVReporterTipWindowView(wxWindow *parent)
               : wxWindow(parent, wxID_ANY,
                          wxDefaultPosition, wxDefaultSize,
                          wxNO_BORDER)
{
    // set colours
    SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT));
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));

    m_parent = (FreeDVReporterTipWindow*)parent;

    m_heightLine = 0;
}

void FreeDVReporterTipWindowView::Adjust(const wxString& text, wxCoord maxLength)
{
    wxInfoDC dc(this);

    // calculate the length: we want each line be no longer than maxLength
    // pixels and we only break lines at words boundary
    wxString current;
    wxCoord height, width,
            widthMax = 0;

    bool breakLine = false;
    for ( const wxChar *p = text.c_str(); ; p++ )
    {
        if ( *p == wxT('\n') || *p == wxT('\0') )
        {
            dc.GetTextExtent(current, &width, &height);
            if ( width > widthMax )
                widthMax = width;

            if ( height > m_heightLine )
                m_heightLine = height;

            m_textLines.push_back(current);

            if ( !*p )
            {
                // end of text
                break;
            }

            current.clear();
            breakLine = false;
        }
        else if ( breakLine && (*p == wxT(' ') || *p == wxT('\t')) )
        {
            // word boundary - break the line here
            m_textLines.push_back(current);
            current.clear();
            breakLine = false;
        }
        else // line goes on
        {
            current += *p;
            dc.GetTextExtent(current, &width, &height);
            if ( width > maxLength )
                breakLine = true;

            if ( width > widthMax )
                widthMax = width;

            if ( height > m_heightLine )
                m_heightLine = height;
        }
    }

    // take into account the border size and the margins
    width  = 2*(TEXT_MARGIN_X + 1) + widthMax;
    height = 2*(TEXT_MARGIN_Y + 1) + wx_truncate_cast(wxCoord, m_textLines.size())*m_heightLine;
    m_parent->SetClientSize(width, height);
    SetSize(0, 0, width, height);
}

void FreeDVReporterTipWindowView::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

    wxRect rect;
    wxSize size = GetClientSize();
    rect.width = size.x;
    rect.height = size.y;

#if defined(WIN32)
    // Tooltips should be rendered with Direct2D if at all possible.
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetDirect2DRenderer();
    wxGraphicsContext* context = nullptr;
    if (renderer != nullptr)
    {
        context = renderer->CreateContextFromUnknownDC(dc);
        if (context != nullptr)
        {
            // first fill the background
            context->SetBrush(wxBrush(GetBackgroundColour(), wxBRUSHSTYLE_SOLID));
            context->SetPen(wxPen(GetForegroundColour(), 1, wxPENSTYLE_SOLID));
            context->DrawRectangle(0, 0, rect.width - 1, rect.height - 1);

            // and then draw the text line by line
            context->SetFont(GetFont(), GetForegroundColour());
        }
    }

    if (context == nullptr)
#endif // defined(WIN32)
    {
        // first filll the background
        dc.SetBrush(wxBrush(GetBackgroundColour(), wxBRUSHSTYLE_SOLID));
        dc.SetPen(wxPen(GetForegroundColour(), 1, wxPENSTYLE_SOLID));
        dc.DrawRectangle(rect);

        // and then draw the text line by line
        dc.SetTextBackground(GetBackgroundColour());
        dc.SetTextForeground(GetForegroundColour());
        dc.SetFont(GetFont());
    }

    wxPoint pt;
    pt.x = TEXT_MARGIN_X;
    pt.y = TEXT_MARGIN_Y;
    const size_t count = m_textLines.size();
    for ( size_t n = 0; n < count; n++ )
    {
#if defined(WIN32)
        if (context != nullptr)
        {
            context->DrawText(m_textLines[n], pt.x, pt.y);
        }
        else
#endif // defined(WIN32)
        {
            dc.DrawText(m_textLines[n], pt);
        }

        pt.y += m_heightLine;
    }

#if defined(WIN32)
    if (context != nullptr)
    {
        delete context;
    }
#endif // defined(WIN32)
}

void FreeDVReporterTipWindowView::OnLeftMouseClick(wxMouseEvent& event)
{
    m_parent->mouseLeftHandler_(event);
    m_parent->Close();
}

void FreeDVReporterTipWindowView::OnMiddleMouseClick(wxMouseEvent& WXUNUSED(event))
{
    m_parent->Close();
}

void FreeDVReporterTipWindowView::OnRightMouseClick(wxMouseEvent& event)
{
    m_parent->mouseRightHandler_(event);
    m_parent->Close();
}

void FreeDVReporterTipWindowView::OnMouseMove(wxMouseEvent& event)
{
    const wxRect& rectBound = m_parent->m_rectBound;

    if ( rectBound.width &&
            !rectBound.Contains(ClientToScreen(event.GetPosition())) )
    {
        // mouse left the bounding rect, disappear
        m_parent->Close();
    }
    else
    {
        event.Skip();
    }
}