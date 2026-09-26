// Presentation coordination for the existing FreeDV plots.
#ifndef FREEDV_DISPLAY_WORKSPACE_H
#define FREEDV_DISPLAY_WORKSPACE_H

#include <array>
#include <cstddef>
#include <functional>
#include <vector>
#include <wx/string.h>
#include <wx/gdicmn.h>
#include "WindowSnapManager.h"

class wxAuiNotebook;
class wxWindow;
class DisplayFrame;

// Stable identities, independent of notebook order and translated captions.
enum class DisplayId
{
    Waterfall = 0,
    Spectrum = 1,
    FrmRadio = 2,
    FrmMic = 3,
    FrmDecoder = 4,
    SNR = 5,
    Count
};

// UI-thread-only presentation controller. Controls belong to either the
// notebook or a frame owned by MainFrame, never to this controller.
class DisplayWorkspace
{
public:
    explicit DisplayWorkspace(wxAuiNotebook& notebook);
    ~DisplayWorkspace();

    void RegisterDisplay(DisplayId id, wxWindow& plot);
    void ShowDisplay(DisplayId id);
    void SetDisplayVisible(DisplayId id, bool visible);
    // User-selected visibility, excluding temporary Frm Mic presentation.
    bool IsDisplayVisible(DisplayId id) const;
    void SetVisibilityChangedHandler(std::function<void()> handler);

    // Keep the existing notebook-index return value for currentNotebookTab.
    // Capture the active page in Frm Mic's split group, not just the notebook's
    // global selection. Configuration ownership remains with MainFrame.
    int CaptureMicReturnPage() const;
    void RestoreAfterMic(long page);

    // Preserve the existing RX-return repaint of every notebook page.
    void RefreshAll();

    // Layout serialization stays with the application's existing version-
    // specific implementation. The detached snapshot also serves config export.
    void SetLayoutHandlers(std::function<wxString()> save,
                           std::function<void(const wxString&)> restore);
    wxString GetNotebookLayout() const;
    bool SetIndependent(bool independent, bool showDisplays = true);
    wxRect GetDisplayGeometry(DisplayId id) const;
    wxRect RestoreDisplayGeometry(DisplayId id, const wxRect& rect);
    bool IsIndependent() const { return presentation_ == Presentation::Independent; }
    bool HasActiveDisplay() const;
    void FocusOperatingWindow();
    void SetSnappingEnabled(bool enabled);
    void RegisterSnapWindow(wxTopLevelWindow& window);

private:
    enum class Presentation { Notebook, Independent };
    struct Page
    {
        wxWindow* plot;
        wxString caption;
    };
    bool ReturnToNotebook();
    DisplayFrame* FrameFor(wxWindow* plot) const;

    wxAuiNotebook& notebook_;
    WindowSnapManager snapManager_;
    std::array<wxWindow*, static_cast<std::size_t>(DisplayId::Count)> plots_{};
    std::array<DisplayFrame*, static_cast<std::size_t>(DisplayId::Count)> frames_{};
    std::vector<Page> pages_;
    std::function<wxString()> saveLayout_;
    std::function<void(const wxString&)> restoreLayout_;
    std::function<void()> visibilityChanged_;
    wxString layout_;
    int selection_ = -1;
    int micReturnPage_ = -1;
    Presentation presentation_ = Presentation::Notebook;
    bool switching_ = false;
    bool micOperationActive_ = false;
    bool micTransient_ = false;
};

#endif // FREEDV_DISPLAY_WORKSPACE_H
