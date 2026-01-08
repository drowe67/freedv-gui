// NOTE (MS, 2026-01-08): Copied original file from wxWidgets 3.3.1 and modified 
// to enable FreeDV Reporter specific functionality. Original copyright is below:

///////////////////////////////////////////////////////////////////////////////
// Name:        wx/tipwin.h
// Purpose:     wxTipWindow is a window like the one typically used for
//              showing the tooltips
// Author:      Vadim Zeitlin
// Created:     10.09.00
// Copyright:   (c) 2000 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef FREEDV_REPORTER_TIP_WINDOW_H
#define FREEDV_REPORTER_TIP_WINDOW_H

#include "wx/defs.h"
#include "wx/popupwin.h"

#include <functional>

class FreeDVReporterTipWindowView;

// ----------------------------------------------------------------------------
// wxTipWindow
// ----------------------------------------------------------------------------

class FreeDVReporterTipWindow : public wxPopupTransientWindow
{
public:
    // the mandatory ctor parameters are: the parent window and the text to
    // show
    //
    // optionally you may also specify the length at which the lines are going
    // to be broken in rows (100 pixels by default)
    //
    // windowPtr and rectBound are just passed to SetTipWindowPtr() and
    // SetBoundingRect() - see below
    FreeDVReporterTipWindow(wxWindow *parent,
                const wxString& text,
                std::function<void(wxMouseEvent&)> mouseLeftHandler,
                std::function<void(wxMouseEvent&)> mouseRightHandler,
                wxCoord maxLength = 100,
                FreeDVReporterTipWindow** windowPtr = nullptr,
                wxRect *rectBound = nullptr);

    virtual ~FreeDVReporterTipWindow();

    // If windowPtr is not null the given address will be nulled when the
    // window has closed
    void SetTipWindowPtr(FreeDVReporterTipWindow** windowPtr) { m_windowPtr = windowPtr; }

    // If rectBound is not null, the window will disappear automatically when
    // the mouse leave the specified rect: note that rectBound should be in the
    // screen coordinates!
    void SetBoundingRect(const wxRect& rectBound);

    // Hide and destroy the window
    void Close();

protected:
    // called by wxTipWindowView only
    bool CheckMouseInBounds(const wxPoint& pos);

    // event handlers
    void OnLeftMouseClick(wxMouseEvent& event);
    void OnMiddleMouseClick(wxMouseEvent& event);
    void OnRightMouseClick(wxMouseEvent& event);

    virtual void OnDismiss() override;

private:
    FreeDVReporterTipWindowView *m_view;

    FreeDVReporterTipWindow** m_windowPtr;
    wxRect m_rectBound;
    
    std::function<void(wxMouseEvent&)> mouseLeftHandler_;
    std::function<void(wxMouseEvent&)> mouseRightHandler_;

    wxDECLARE_EVENT_TABLE();

    friend class FreeDVReporterTipWindowView;

    wxDECLARE_NO_COPY_CLASS(FreeDVReporterTipWindow);
};

#endif // FREEDV_REPORTER_TIP_WINDOW_H
