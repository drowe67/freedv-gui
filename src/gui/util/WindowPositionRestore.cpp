//==========================================================================
// Name:            WindowPositionRestore.cpp
// Purpose:         Helper to enable restoration of window position.
// Created:         Jul 14, 2026
// Authors:         Barry Jackson
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

#include "WindowPositionRestore.h"
#include "DpiUtils.h"

#include <wx/frame.h>
#include <wx/display.h>
#include <algorithm>

wxRect RestoreWindowGeometry(wxFrame* frame, wxRect rect, const wxSize& minimum)
{
    if (rect.width <= 0 || rect.height <= 0)
        rect.SetSize(frame->GetSize());
    rect.width = std::max(rect.width, minimum.x);
    rect.height = std::max(rect.height, minimum.y);

    int monitor = wxNOT_FOUND;
    for (unsigned int i = 0; i < wxDisplay::GetCount(); ++i)
    {
        const wxRect area = wxDisplay(i).GetClientArea();
        const wxRect title(rect.x, rect.y, rect.width, FromDIP(frame, 32));
        const wxRect visible = title.Intersect(area);
        if (visible.width >= FromDIP(frame, 120) && visible.height >= FromDIP(frame, 20))
        {
            monitor = i;
            break;
        }
    }
    const bool recoverPosition = monitor == wxNOT_FOUND;
    if (recoverPosition)
    {
        monitor = wxDisplay::GetFromWindow(frame);
        if (monitor == wxNOT_FOUND && wxDisplay::GetCount() > 0)
            monitor = 0;
    }
    if (monitor != wxNOT_FOUND)
    {
        const wxRect area = wxDisplay(monitor).GetClientArea();
        rect.width = std::min(rect.width, std::max(area.width, minimum.x));
        rect.height = std::min(rect.height, std::max(area.height, minimum.y));
        if (recoverPosition)
        {
            rect.x = std::clamp(rect.x, area.x, area.x + std::max(0, area.width - rect.width));
            rect.y = std::clamp(rect.y, area.y, area.y + std::max(0, area.height - rect.height));
        }
    }
    else
        rect.SetPosition(wxPoint(20, 20));

    frame->SetSize(rect.GetSize());
    RestoreWindowPosition(frame, rect.x, rect.y);
    return rect;
}

#if defined(__WXGTK__) && defined(HAS_GTK3)
#include <gtk/gtk.h>

namespace
{

// How long after mapping to keep re-asserting the target position. Some
// compositors (observed: KWin on Plasma 6, labwc on Wayland) apply their
// own placement decision asynchronously, at no fixed, observable event --
// sometimes even after the position already happened to match the target
// once -- so this polls for a while rather than stopping at the first
// match.
constexpr gint64 kSettleWatchDurationUs = 2000000; // 2 seconds

struct SettlePositionContext_
{
    wxFrame* frame;
    int targetX;
    int targetY;
    gint64 deadlineUs;
    guint timeoutId;
};

void OnFrameDestroyed_(GtkWidget* widget, gpointer userData)
{
    auto* ctx = static_cast<SettlePositionContext_*>(userData);
    if (ctx->timeoutId != 0)
    {
        g_source_remove(ctx->timeoutId);
    }
    g_object_set_data(G_OBJECT(widget), "freedv-position-restore", nullptr);
    delete ctx;
}

gboolean OnSettleTick_(gpointer userData)
{
    auto* ctx = static_cast<SettlePositionContext_*>(userData);
    wxPoint pos = ctx->frame->GetPosition();

    if (pos.x != ctx->targetX || pos.y != ctx->targetY)
    {
        ctx->frame->Move(ctx->targetX, ctx->targetY);
    }

    if (g_get_monotonic_time() >= ctx->deadlineUs)
    {
        ctx->timeoutId = 0;
        g_object_set_data(G_OBJECT(ctx->frame->GetHandle()), "freedv-position-restore", nullptr);
        g_signal_handlers_disconnect_by_func(
            GTK_WIDGET(ctx->frame->GetHandle()), (gpointer)OnFrameDestroyed_, ctx);
        delete ctx;
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

gboolean OnFrameMapEvent_(GtkWidget* widget, GdkEventAny*, gpointer userData)
{
    auto* ctx = static_cast<SettlePositionContext_*>(userData);
    ctx->frame->Move(ctx->targetX, ctx->targetY);
    ctx->deadlineUs = g_get_monotonic_time() + kSettleWatchDurationUs;

    // Only the first map (initial startup) should apply the saved position --
    // later maps, e.g. un-minimizing, should leave the window where the user
    // last put it.
    g_signal_handlers_disconnect_by_func(widget, (gpointer)OnFrameMapEvent_, userData);
    ctx->timeoutId = g_timeout_add(100, OnSettleTick_, ctx);
    return FALSE;
}

} // namespace

void RestoreWindowPosition(wxFrame* frame, int x, int y)
{
    GtkWidget* widget = GTK_WIDGET(frame->GetHandle());
    auto* previous = static_cast<SettlePositionContext_*>(
        g_object_get_data(G_OBJECT(widget), "freedv-position-restore"));
    if (previous != nullptr)
    {
        g_signal_handlers_disconnect_by_data(widget, previous);
        if (previous->timeoutId != 0)
            g_source_remove(previous->timeoutId);
        delete previous;
        g_object_set_data(G_OBJECT(widget), "freedv-position-restore", nullptr);
    }
    frame->Move(x, y);
    if (frame->IsShown())
    {
        return;
    }
    auto* ctx = new SettlePositionContext_{frame, x, y, 0, 0};
    g_object_set_data(G_OBJECT(widget), "freedv-position-restore", ctx);
    g_signal_connect(widget, "map-event", G_CALLBACK(OnFrameMapEvent_), ctx);
    g_signal_connect(widget, "destroy", G_CALLBACK(OnFrameDestroyed_), ctx);
}

#else

void RestoreWindowPosition(wxFrame* frame, int x, int y)
{
    if (frame->IsShown())
    {
        frame->Move(x, y);
        return;
    }
    frame->CallAfter([frame, x, y]()
    {
        frame->Move(x, y);
    });
}

#endif // defined(__WXGTK__) && defined(HAS_GTK3)
