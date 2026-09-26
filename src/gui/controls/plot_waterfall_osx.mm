//==========================================================================
// Name:            plot_waterfall_osx.mm
// Purpose:         macOS-specific helpers for PlotWaterfall
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
#include <cstdint>

#include <wx/wx.h>
#include <wx/graphics.h>

// Builds a waterfall block as a CGImage in the color space and pixel layout of the
// window's backing store.
//
// wxGraphicsContext::CreateBitmapFromImage() goes through wxBitmap, which tags the
// image as sRGB. The window is almost never sRGB (e.g. Display P3 on Apple displays),
// so CoreGraphics color matched every block on every frame it spent on screen --
// about a tenth of the GUI thread's time. The waterfall's colors are a synthetic
// palette anyway, so we tag the pixels with the window's own color space and let
// them be copied through unchanged.
wxGraphicsBitmap CreateWaterfallBitmapInWindowColorSpace(wxGraphicsContext* gc, wxWindow* window, const wxImage& image)
{
    CGColorSpaceRef colorSpace = nullptr;
    NSView* view = (NSView*)window->GetHandle();
    NSWindow* nsWindow = view != nil ? view.window : nil;
    if (nsWindow != nil)
    {
        colorSpace = nsWindow.colorSpace.CGColorSpace;
        if (colorSpace == nullptr)
        {
            colorSpace = nsWindow.screen.colorSpace.CGColorSpace;
        }
    }
    if (colorSpace == nullptr)
    {
        return gc->CreateBitmapFromImage(image);
    }

    int width = image.GetWidth();
    int height = image.GetHeight();

    // 32-bit host-order xRGB matches the backing store, so drawing is a straight copy.
    CGContextRef bitmapContext = CGBitmapContextCreate(
        nullptr, width, height, 8, 0, colorSpace,
        kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host);
    if (bitmapContext == nullptr)
    {
        return gc->CreateBitmapFromImage(image);
    }

    const unsigned char* src = image.GetData();
    unsigned char* dstBase = (unsigned char*)CGBitmapContextGetData(bitmapContext);
    size_t dstStride = CGBitmapContextGetBytesPerRow(bitmapContext);
    for (int y = 0; y < height; y++)
    {
        uint32_t* dst = (uint32_t*)(dstBase + y * dstStride);
        for (int x = 0; x < width; x++, src += 3)
        {
            dst[x] = 0xFF000000u | ((uint32_t)src[0] << 16) | ((uint32_t)src[1] << 8) | src[2];
        }
    }

    CGImageRef cgImage = CGBitmapContextCreateImage(bitmapContext);
    CGContextRelease(bitmapContext);
    if (cgImage == nullptr)
    {
        return gc->CreateBitmapFromImage(image);
    }

    // The returned wxGraphicsBitmap takes ownership of cgImage.
    return gc->GetRenderer()->CreateBitmapFromNativeBitmap(cgImage);
}
