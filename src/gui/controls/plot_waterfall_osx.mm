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
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>

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
static CGColorSpaceRef GetWindowColorSpace_(wxWindow* window)
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
    return colorSpace;
}

// Converts one row of 24-bit RGB (wxImage's layout) to opaque 32-bit host-order
// premultiplied ARGB (see CreateWaterfallBitmapInWindowColorSpace()).
static void ConvertRowToARGB_(const unsigned char* src, uint32_t* dst, int width)
{
    for (int x = 0; x < width; x++, src += 3)
    {
        dst[x] = 0xFF000000u | ((uint32_t)src[0] << 16) | ((uint32_t)src[1] << 8) | src[2];
    }
}

wxGraphicsBitmap CreateWaterfallBitmapInWindowColorSpace(wxGraphicsContext* gc, wxWindow* window, const wxImage& image)
{
    CGColorSpaceRef colorSpace = GetWindowColorSpace_(window);
    if (colorSpace == nullptr)
    {
        return gc->CreateBitmapFromImage(image);
    }

    int width = image.GetWidth();
    int height = image.GetHeight();

    // Opaque 32-bit host-order premultiplied ARGB is CoreGraphics' own format for
    // drawing into the backing store. With no alpha (kCGImageAlphaNoneSkipFirst) it
    // filled in the alpha channel on every draw; with the bytes in RGBA order it
    // swapped them around on every draw.
    CGContextRef bitmapContext = CGBitmapContextCreate(
        nullptr, width, height, 8, 0, colorSpace,
        (CGBitmapInfo)kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    if (bitmapContext == nullptr)
    {
        return gc->CreateBitmapFromImage(image);
    }

    const unsigned char* src = image.GetData();
    unsigned char* dstBase = (unsigned char*)CGBitmapContextGetData(bitmapContext);
    size_t dstStride = CGBitmapContextGetBytesPerRow(bitmapContext);
    for (int y = 0; y < height; y++, src += 3 * width)
    {
        ConvertRowToARGB_(src, (uint32_t*)(dstBase + y * dstStride), width);
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

// The whole waterfall as one image (see plot_waterfall.h).
//
// The pixels live in a ring buffer twice the image's height, where row r and row
// r + height always hold the same pixels. Each new block overwrites the oldest rows
// (both copies), so rows [head, head + height) are always the whole waterfall,
// newest first, in one contiguous run: a paint draws them as a single image that
// points straight at the buffer, and a new block only writes its own rows.
//
// Two simpler approaches were slower, as measured with the Time Profiler:
// - Scrolling the whole image down in memory for every block (and, with a
//   CGBitmapContext, taking copy-on-write faults on every page it touched).
// - Drawing the ring as two sub-images from CGImageCreateWithImageInRect(), which
//   CoreGraphics copied and converted row by row on every paint.
//
// The images don't copy the pixels, so the buffer is reference counted and freed
// once the canvas and every image made from it are gone. The images also aren't
// immutable: if Core Animation drew an older one again after a new block had been
// pushed, its bottom rows would show the new block. In practice each paint draws a
// new image, and Core Animation rasterizes it in the same main thread commit,
// before the next block is pushed.
struct WaterfallCanvasBuffer
{
    std::atomic<int> refs;
    unsigned char* data;
};

static void ReleaseCanvasBuffer_(void* info, const void*, size_t)
{
    WaterfallCanvasBuffer* buffer = (WaterfallCanvasBuffer*)info;
    if (buffer->refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        free(buffer->data);
        delete buffer;
    }
}

struct WaterfallCanvas
{
    WaterfallCanvasBuffer* buffer;
    CGColorSpaceRef colorSpace;
    int width;
    int height;
    size_t stride;
    int head; // row holding the newest block's first row
    wxGraphicsBitmap snapshot; // cached until the next block is pushed
};

WaterfallCanvas* CreateWaterfallCanvas(wxWindow* window, int width, int height)
{
    CGColorSpaceRef colorSpace = GetWindowColorSpace_(window);
    if (colorSpace != nullptr)
    {
        CGColorSpaceRetain(colorSpace);
    }
    else
    {
        colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    }

    size_t stride = (size_t)width * sizeof(uint32_t);
    unsigned char* data = (unsigned char*)malloc(stride * 2 * height);
    if (data == nullptr || colorSpace == nullptr)
    {
        free(data);
        CGColorSpaceRelease(colorSpace);
        return nullptr;
    }

    // Start out black, like an empty waterfall.
    std::fill((uint32_t*)data, (uint32_t*)(data + stride * 2 * height), 0xFF000000u);

    return new WaterfallCanvas {
        new WaterfallCanvasBuffer { {1}, data }, colorSpace, width, height, stride, 0, wxNullGraphicsBitmap };
}

void DestroyWaterfallCanvas(WaterfallCanvas* canvas)
{
    if (canvas != nullptr)
    {
        canvas->snapshot = wxNullGraphicsBitmap;
        ReleaseCanvasBuffer_(canvas->buffer, nullptr, 0);
        CGColorSpaceRelease(canvas->colorSpace);
        delete canvas;
    }
}

bool WaterfallCanvasHasSize(const WaterfallCanvas* canvas, int width, int height)
{
    return canvas != nullptr && canvas->width == width && canvas->height == height;
}

void WaterfallCanvasPushBlock(WaterfallCanvas* canvas, const wxImage& block)
{
    canvas->snapshot = wxNullGraphicsBitmap;

    int rows = std::min(block.GetHeight(), canvas->height);
    int width = std::min(block.GetWidth(), canvas->width);
    unsigned char* data = canvas->buffer->data;

    canvas->head = (canvas->head - rows + canvas->height) % canvas->height;
    const unsigned char* src = block.GetData();
    for (int y = 0; y < rows; y++)
    {
        int row = (canvas->head + y) % canvas->height;
        uint32_t* dst = (uint32_t*)(data + row * canvas->stride);
        ConvertRowToARGB_(src + y * 3 * block.GetWidth(), dst, width);
        memcpy(data + (row + canvas->height) * canvas->stride, dst, canvas->stride);
    }
}

wxGraphicsBitmap WaterfallCanvasGetBitmap(WaterfallCanvas* canvas, wxGraphicsContext* gc)
{
    if (canvas->snapshot.IsNull())
    {
        canvas->buffer->refs.fetch_add(1, std::memory_order_relaxed);
        CGDataProviderRef provider = CGDataProviderCreateWithData(
            canvas->buffer,
            canvas->buffer->data + canvas->head * canvas->stride,
            canvas->stride * canvas->height,
            ReleaseCanvasBuffer_);
        if (provider == nullptr)
        {
            ReleaseCanvasBuffer_(canvas->buffer, nullptr, 0);
            return wxNullGraphicsBitmap;
        }

        // Same pixel layout as CreateWaterfallBitmapInWindowColorSpace().
        CGImageRef image = CGImageCreate(
            canvas->width, canvas->height, 8, 32, canvas->stride, canvas->colorSpace,
            (CGBitmapInfo)kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
            provider, nullptr, false, kCGRenderingIntentDefault);
        CGDataProviderRelease(provider);
        if (image != nullptr)
        {
            // The returned wxGraphicsBitmap takes ownership of image.
            canvas->snapshot = gc->GetRenderer()->CreateBitmapFromNativeBitmap(image);
        }
    }
    return canvas->snapshot;
}
