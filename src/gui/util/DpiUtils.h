//==========================================================================
// Name:            DpiUtils.h
// Purpose:         DPI conversion compatibility helpers.
//==========================================================================

#ifndef DPI_UTILS_H
#define DPI_UTILS_H

#include <wx/gdicmn.h>
#include <wx/window.h>
#include <wx/version.h>

inline int FromDIP(wxWindow* window, int value)
{
#if wxCHECK_VERSION(3, 1, 0)
    return window->FromDIP(value);
#else
    (void)window;
    return value;
#endif
}

inline wxSize FromDIP(wxWindow* window, const wxSize& value)
{
#if wxCHECK_VERSION(3, 1, 0)
    return window->FromDIP(value);
#else
    (void)window;
    return value;
#endif
}

#endif // DPI_UTILS_H
