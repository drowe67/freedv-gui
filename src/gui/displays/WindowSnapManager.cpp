#include "WindowSnapManager.h"
#include "gui/util/DpiUtils.h"

#include <algorithm>
#include <cstdlib>
#include <wx/toplevel.h>
#include <wx/utils.h>

WindowSnapManager::~WindowSnapManager()
{
    for (auto* window : windows_)
    {
#ifdef __WXMSW__
        window->Unbind(wxEVT_MOVING, &WindowSnapManager::OnMove, this);
        window->Unbind(wxEVT_SIZING, &WindowSnapManager::OnSizing, this);
        window->Unbind(wxEVT_MOVE_START, &WindowSnapManager::OnMoveBoundary, this);
        window->Unbind(wxEVT_MOVE_END, &WindowSnapManager::OnMoveBoundary, this);
#else
        window->Unbind(wxEVT_MOVE, &WindowSnapManager::OnMove, this);
        window->Unbind(wxEVT_IDLE, &WindowSnapManager::OnIdle, this);
#endif
        window->Unbind(wxEVT_DESTROY, &WindowSnapManager::OnDestroy, this);
    }
}

void WindowSnapManager::AddWindow(wxTopLevelWindow& window)
{
    if (std::find(windows_.begin(), windows_.end(), &window) != windows_.end())
        return;
    windows_.push_back(&window);
#ifdef __WXMSW__
    window.Bind(wxEVT_MOVING, &WindowSnapManager::OnMove, this);
    window.Bind(wxEVT_SIZING, &WindowSnapManager::OnSizing, this);
    window.Bind(wxEVT_MOVE_START, &WindowSnapManager::OnMoveBoundary, this);
    window.Bind(wxEVT_MOVE_END, &WindowSnapManager::OnMoveBoundary, this);
#else
    window.Bind(wxEVT_MOVE, &WindowSnapManager::OnMove, this);
    window.Bind(wxEVT_IDLE, &WindowSnapManager::OnIdle, this);
#endif
    window.Bind(wxEVT_DESTROY, &WindowSnapManager::OnDestroy, this);
}

void WindowSnapManager::SetEnabled(bool enabled)
{
    enabled_ = false;
    moving_ = nullptr;
    resizeOrigin_.reset();
    const auto generation = ++enableGeneration_;
    if (enabled)
    {
        // Let queued placement events finish before accepting user moves.
        CallAfter([this, generation]() {
            if (generation == enableGeneration_)
                enabled_ = true;
        });
    }
}

wxRect WindowSnapManager::SnapRect(const wxRect& moving,
                                  const std::vector<wxRect>& targets, int threshold,
                                  int sideOverlap)
{
    int dx = 0;
    int dy = 0;
    int closestX = threshold + 1;
    int closestY = threshold + 1;
    const auto consider = [](int delta, int& closest, int& adjustment) {
        if (std::abs(delta) < closest)
        {
            closest = std::abs(delta);
            adjustment = delta;
        }
    };
    for (const auto& target : targets)
    {
        // Use exclusive right/bottom edges so adjacent frames do not overlap.
        if (moving.y <= target.y + target.height + threshold &&
            target.y <= moving.y + moving.height + threshold)
        {
            consider(target.x + target.width - moving.x - sideOverlap, closestX, dx);
            consider(target.x - moving.x - moving.width + sideOverlap, closestX, dx);
            consider(target.x - moving.x, closestX, dx);
            consider(target.x + target.width - moving.x - moving.width, closestX, dx);
        }
        if (moving.x <= target.x + target.width + threshold &&
            target.x <= moving.x + moving.width + threshold)
        {
            consider(target.y + target.height - moving.y, closestY, dy);
            consider(target.y - moving.y - moving.height, closestY, dy);
            consider(target.y - moving.y, closestY, dy);
            consider(target.y + target.height - moving.y - moving.height, closestY, dy);
        }
    }
    return wxRect(moving.GetPosition() + wxPoint(dx, dy), moving.GetSize());
}

void WindowSnapManager::OnMove(wxMoveEvent& event)
{
    event.Skip();
    auto* window = wxDynamicCast(event.GetEventObject(), wxTopLevelWindow);
    if (!enabled_ || adjusting_ || window == nullptr || window->IsBeingDeleted() ||
        !window->IsShown() || window->IsIconized() || window->IsMaximized())
        return;
#ifdef __WXMSW__
    wxRect moving = event.GetRect();
    // MSW's event conversion treats the native exclusive edges as inclusive.
    moving.width--;
    moving.height--;
#else
    // Portable move events also report programmatic placement. Only follow
    // an active mouse drag; some window managers do not expose these live.
    if (!window->IsActive() || !wxGetMouseState().LeftIsDown())
        return;
    const wxRect moving = window->GetRect();
    if (event.GetPosition() != moving.GetPosition())
        return;
    const wxPoint mouse = wxGetMousePosition();
    if (!moving.Contains(mouse) || mouse.y >= window->ClientToScreen(wxPoint(0, 0)).y)
        return;
#endif
    if (moving_ != window)
    {
        moving_ = window;
        horizontal_ = {};
        vertical_ = {};
    }
#ifdef __WXMSW__
    // Windows includes an invisible resize margin in top-level window bounds.
    const int sideOverlap = FromDIP(window, 10);
#else
    const int sideOverlap = 0;
#endif
    wxRect snapped = SnapRect(moving, Targets(window), FromDIP(window, 7), sideOverlap);
    const wxPoint cursor = wxGetMousePosition();
    snapped.x = horizontal_.Apply(moving.x, snapped.x, cursor.x, FromDIP(window, 10), FromDIP(window, 7));
    snapped.y = vertical_.Apply(moving.y, snapped.y, cursor.y, FromDIP(window, 10), FromDIP(window, 7));
#ifdef __WXMSW__
    snapped.width++;
    snapped.height++;
    event.SetRect(snapped);
    event.Skip(false);
#else
    if (snapped.GetPosition() != moving.GetPosition())
    {
        adjusting_ = true;
        window->Move(snapped.GetPosition());
        CallAfter([this]() { adjusting_ = false; });
    }
#endif
}

void WindowSnapManager::OnDestroy(wxWindowDestroyEvent& event)
{
    auto* window = event.GetEventObject();
    if (window == moving_)
        moving_ = nullptr;
    windows_.erase(std::remove(windows_.begin(), windows_.end(), window), windows_.end());
    event.Skip();
}

int WindowSnapManager::MoveAxis::Apply(int proposed, int snapped, int cursor,
                                     int releaseDistance, int captureDistance)
{
    if (released)
    {
        // Suppress recapture only until the released snap's capture band is cleared.
        if (std::abs(proposed - position) <= captureDistance)
            return proposed;
        capture.reset();
        released = false;
    }
    if (capture)
    {
        const int distance = cursor - *capture;
        if (std::abs(distance) <= releaseDistance)
            return position;
        released = true;
        return position + distance;
    }
    if (!capture && proposed != snapped)
    {
        // Anchor the pointer at alignment, not at the beginning of attraction.
        capture = cursor + snapped - proposed;
        position = snapped;
    }
    return snapped;
}

void WindowSnapManager::OnMoveBoundary(wxMoveEvent& event)
{
    moving_ = nullptr;
    resizeOrigin_.reset();
    auto* window = wxDynamicCast(event.GetEventObject(), wxTopLevelWindow);
    if (event.GetEventType() == wxEVT_MOVE_START && window != nullptr)
        resizeOrigin_ = window->GetRect();
    event.Skip();
}

void WindowSnapManager::OnIdle(wxIdleEvent& event)
{
    if (!wxGetMouseState().LeftIsDown())
        moving_ = nullptr;
    event.Skip();
}

std::vector<wxRect> WindowSnapManager::Targets(wxTopLevelWindow* moving) const
{
    std::vector<wxRect> targets;
    for (auto* target : windows_)
        if (target != moving && !target->IsBeingDeleted() && target->IsShown() &&
            !target->IsIconized() && !target->IsMaximized())
            targets.push_back(target->GetRect());
    return targets;
}

wxRect WindowSnapManager::SnapResizeRect(const wxRect& proposed, const wxRect& current,
    const std::vector<wxRect>& targets, int threshold,
    const wxSize& minimum, const wxSize& maximum)
{
    wxRect result = proposed;
    const auto snapAxis = [&](int start, int length, int oldStart, int oldLength,
                              int crossStart, int crossLength, bool horizontal,
                              int minLength, int maxLength, int& newStart, int& newLength) {
        const bool leading = start != oldStart;
        const bool trailing = start + length != oldStart + oldLength;
        if (leading == trailing)
            return;
        const int edge = leading ? start : start + length;
        int closest = threshold + 1;
        for (const auto& target : targets)
        {
            const int targetCross = horizontal ? target.y : target.x;
            const int targetCrossLength = horizontal ? target.height : target.width;
            if (crossStart > targetCross + targetCrossLength + threshold ||
                targetCross > crossStart + crossLength + threshold)
                continue;
            const int targetStart = horizontal ? target.x : target.y;
            const int targetLength = horizontal ? target.width : target.height;
            for (int candidate : {targetStart, targetStart + targetLength})
            {
                const int delta = candidate - edge;
                const int size = length + (leading ? -delta : delta);
                if (std::abs(delta) < closest && size >= std::max(1, minLength) &&
                    (maxLength < 0 || size <= maxLength))
                {
                    closest = std::abs(delta);
                    newStart = leading ? candidate : start;
                    newLength = size;
                }
            }
        }
    };
    snapAxis(proposed.x, proposed.width, current.x, current.width,
             proposed.y, proposed.height, true, minimum.x, maximum.x,
             result.x, result.width);
    snapAxis(proposed.y, proposed.height, current.y, current.height,
             proposed.x, proposed.width, false, minimum.y, maximum.y,
             result.y, result.height);
    return result;
}

void WindowSnapManager::OnSizing(wxSizeEvent& event)
{
    event.Skip();
    auto* window = wxDynamicCast(event.GetEventObject(), wxTopLevelWindow);
    if (!enabled_ || window == nullptr || window->IsBeingDeleted() ||
        !window->IsShown() || window->IsIconized() || window->IsMaximized())
        return;
    wxRect proposed = event.GetRect();
    proposed.width--;
    proposed.height--;
    wxRect snapped = SnapResizeRect(proposed, resizeOrigin_.value_or(window->GetRect()),
                                   Targets(window), FromDIP(window, 7),
                                   window->GetMinSize(), window->GetMaxSize());
    snapped.width++;
    snapped.height++;
    event.SetRect(snapped);
    event.Skip(false);
}
