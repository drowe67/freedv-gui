#ifndef FREEDV_WINDOW_SNAP_MANAGER_H
#define FREEDV_WINDOW_SNAP_MANAGER_H

#include <vector>
#include <optional>
#include <wx/event.h>
#include <wx/gdicmn.h>

class wxTopLevelWindow;

class WindowSnapManager : public wxEvtHandler
{
public:
    ~WindowSnapManager();
    void AddWindow(wxTopLevelWindow& window);
    void SetEnabled(bool enabled);
    static wxRect SnapRect(const wxRect& moving, const std::vector<wxRect>& targets,
                           int threshold, int sideOverlap = 0);
    static wxRect SnapResizeRect(const wxRect& proposed, const wxRect& current,
                                const std::vector<wxRect>& targets, int threshold,
                                const wxSize& minimum, const wxSize& maximum);

    struct MoveAxis
    {
        std::optional<int> capture;
        bool released = false;
        int position = 0;
        int Apply(int proposed, int snapped, int cursor, int releaseDistance, int captureDistance);
    };

private:
    void OnMove(wxMoveEvent& event);
    void OnSizing(wxSizeEvent& event);
    void OnMoveBoundary(wxMoveEvent& event);
    void OnIdle(wxIdleEvent& event);
    void OnDestroy(wxWindowDestroyEvent& event);
    std::vector<wxRect> Targets(wxTopLevelWindow* moving) const;
    std::vector<wxTopLevelWindow*> windows_;
    wxTopLevelWindow* moving_ = nullptr;
    MoveAxis horizontal_;
    MoveAxis vertical_;
    std::optional<wxRect> resizeOrigin_;
    bool enabled_ = false;
    bool adjusting_ = false;
    unsigned int enableGeneration_ = 0;
};

#endif
