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

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <wx/wx.h>
#include <wx/graphics.h>

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

// A pixel buffer that a plot rasterizes into on the CPU and then draws as a single
// image (see PlotScalar). Pixels are opaque 32-bit host-order premultiplied ARGB in
// the window's color space: CoreGraphics' own format, so drawing the image in copy
// mode is a plain memory copy, with no per-frame conversion or color matching.
//
// Images point straight at the pixels rather than copying them. There are two
// buffers, used alternately, so a frame never overwrites the one the previous
// frame's image points to; each buffer is reference counted and freed once the
// PlotPixelBuffer and every image made from it are gone.
struct PlotPixelBufferData
{
    std::atomic<int> refs;
    uint32_t* pixels;
};

static void ReleasePlotPixelBufferData_(void* info, const void*, size_t)
{
    PlotPixelBufferData* data = (PlotPixelBufferData*)info;
    if (data->refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        free(data->pixels);
        delete data;
    }
}

struct PlotPixelBuffer;
void DestroyPlotPixelBuffer(PlotPixelBuffer* buffer);

struct PlotPixelBuffer
{
    CGColorSpaceRef colorSpace;
    int width;
    int height;
    PlotPixelBufferData* buffers[2];
    int current; // buffer the next frame is rendered into
};

PlotPixelBuffer* CreatePlotPixelBuffer(wxWindow* window, int width, int height)
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
        // Not on screen yet. The caller tries again on a later frame.
        return nullptr;
    }

    PlotPixelBuffer* buffer = new PlotPixelBuffer { CGColorSpaceRetain(colorSpace), width, height, { nullptr, nullptr }, 0 };
    for (auto& data : buffer->buffers)
    {
        uint32_t* pixels = (uint32_t*)calloc((size_t)width * height, sizeof(uint32_t));
        data = pixels != nullptr ? new PlotPixelBufferData { {1}, pixels } : nullptr;
    }
    if (buffer->buffers[0] == nullptr || buffer->buffers[1] == nullptr)
    {
        DestroyPlotPixelBuffer(buffer);
        return nullptr;
    }
    return buffer;
}

void DestroyPlotPixelBuffer(PlotPixelBuffer* buffer)
{
    if (buffer != nullptr)
    {
        for (auto data : buffer->buffers)
        {
            if (data != nullptr)
            {
                ReleasePlotPixelBufferData_(data, nullptr, 0);
            }
        }
        CGColorSpaceRelease(buffer->colorSpace);
        delete buffer;
    }
}

bool PlotPixelBufferHasSize(const PlotPixelBuffer* buffer, int width, int height)
{
    return buffer != nullptr && buffer->width == width && buffer->height == height;
}

uint32_t* PlotPixelBufferGetPixels(PlotPixelBuffer* buffer)
{
    return buffer->buffers[buffer->current]->pixels;
}

// Converts an (sRGB) wxColour to the buffer's color space and pixel format, so it
// looks the same as when drawn with a pen or brush.
uint32_t PlotPixelBufferGetPixel(const PlotPixelBuffer* buffer, const wxColour& colour)
{
    CGColorRef matched = CGColorCreateCopyByMatchingToColorSpace(
        buffer->colorSpace, kCGRenderingIntentDefault, colour.GetCGColor(), nullptr);
    uint32_t pixel = 0xFF000000u;
    if (matched != nullptr && CGColorGetNumberOfComponents(matched) >= 3)
    {
        const CGFloat* c = CGColorGetComponents(matched);
        auto toByte = [](CGFloat v) { return (uint32_t)std::lround(std::min(std::max(v, (CGFloat)0), (CGFloat)1) * 255); };
        pixel |= (toByte(c[0]) << 16) | (toByte(c[1]) << 8) | toByte(c[2]);
    }
    if (matched != nullptr)
    {
        CGColorRelease(matched);
    }
    return pixel;
}

// Wraps the pixels rendered this frame in an image and switches to the other buffer
// for the next frame.
wxGraphicsBitmap PlotPixelBufferFinishFrame(PlotPixelBuffer* buffer, wxGraphicsContext* gc)
{
    PlotPixelBufferData* data = buffer->buffers[buffer->current];
    buffer->current ^= 1;

    size_t stride = (size_t)buffer->width * sizeof(uint32_t);
    data->refs.fetch_add(1, std::memory_order_relaxed);
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        data, data->pixels, stride * buffer->height, ReleasePlotPixelBufferData_);
    if (provider == nullptr)
    {
        ReleasePlotPixelBufferData_(data, nullptr, 0);
        return wxNullGraphicsBitmap;
    }

    CGImageRef image = CGImageCreate(
        buffer->width, buffer->height, 8, 32, stride, buffer->colorSpace,
        (CGBitmapInfo)kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
        provider, nullptr, false, kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    if (image == nullptr)
    {
        return wxNullGraphicsBitmap;
    }

    // The returned wxGraphicsBitmap takes ownership of image.
    return gc->GetRenderer()->CreateBitmapFromNativeBitmap(image);
}
