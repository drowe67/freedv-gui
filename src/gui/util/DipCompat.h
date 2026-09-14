//==========================================================================
// Name:            DipCompat.h
// Purpose:         Compatibility shim so that unqualified FromDIP() calls
//                  still compile against wxWidgets versions older than
//                  3.1.0 (e.g. the libwxgtk3.0 packages still used on
//                  Ubuntu 22.04), where wxWindow::FromDIP() doesn't exist.
// Created:         Sep 14, 2026
// Authors:         Mooneer Salem
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

#ifndef DIP_COMPAT_H
#define DIP_COMPAT_H

#include <wx/gdicmn.h>
#include <wx/version.h>

#if !wxCHECK_VERSION(3, 1, 0)
// Unqualified FromDIP() calls inside a wxWindow-derived class normally
// resolve to the wxWindow member added in wxWidgets 3.1.0. On toolkits
// older than that, class-scope lookup finds nothing and falls through to
// these global no-op overloads instead, so DPI scaling is simply skipped
// (matching this toolkit's pre-existing, unscaled behavior).
inline wxSize FromDIP(const wxSize& sz) { return sz; }
inline wxPoint FromDIP(const wxPoint& pt) { return pt; }
inline int FromDIP(int d) { return d; }

// wxWindow::FromDIP(size, window) is used directly as a default argument
// value in a couple of constructor declarations (evaluated before any
// window exists, so it can't rely on the fallback above). Use this instead
// of the real static method in that situation.
#define WX_STATIC_FROM_DIP(sz, win) (sz)
#else
#define WX_STATIC_FROM_DIP(sz, win) wxWindow::FromDIP((sz), (win))
#endif // !wxCHECK_VERSION(3, 1, 0)

#endif // DIP_COMPAT_H
