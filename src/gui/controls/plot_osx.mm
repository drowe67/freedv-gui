//==========================================================================
// Name:            plot_osx.mm
// Purpose:         macOS-specific helpers for PlotPanel
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
//  for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#import <Cocoa/Cocoa.h>

#include <wx/wx.h>

// Gives the window's view a layer of its own.
//
// Otherwise AppKit draws the plots into an ancestor's layer (the notebook's). A layer
// only tracks one dirty rectangle, so each plot's refresh grew it to cover every plot
// on screen, and everything in between -- the notebook and all of its tab strips --
// was redrawn with them on every refresh.
void GivePlotOwnLayer(wxWindow* window)
{
    NSView* view = (NSView*)window->GetHandle();
    if (view != nil)
    {
        view.wantsLayer = YES;
    }
}
