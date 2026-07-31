//==========================================================================
// Name:            topFrame.cpp
//
// Purpose:         Implements simple wxWidgets application with GUI.
// Created:         Apr. 9, 2012
// Authors:         David Rowe, David Witten
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

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include <wx/regex.h>
#include <wx/wrapsizer.h>
#include <wx/aui/tabmdi.h>
#include <wx/numformatter.h>
#include <wx/textfile.h>

#include "topFrame.h"

#if !wxCHECK_VERSION(3, 3, 0)
#include <set>
#endif // !wxCHECK_VERSION(3, 3, 0)

#include "gui/util/NameOverrideAccessible.h"
#include "gui/util/LabelOverrideAccessible.h"
#include "util/logging/ulog.h"

#if defined(__WXGTK__) && defined(HAS_GTK3)
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif // GDK_WINDOWING_WAYLAND
#endif // defined(__WXGTK__) && defined(HAS_GTK3)

extern int g_playFileToMicInEventId;

wxPoint LeftOffsetContextMenuPosition(wxWindow* btn)
{
#if defined(__WXGTK__) && defined(HAS_GTK3) && defined(GDK_WINDOWING_WAYLAND)
    GdkDisplay* display = gdk_display_get_default();
    if (display != nullptr && GDK_IS_WAYLAND_DISPLAY(display))
    {
        return wxDefaultPosition;
    }
#endif // defined(__WXGTK__) && defined(HAS_GTK3) && defined(GDK_WINDOWING_WAYLAND)

    auto sz = btn->GetSize();
    return wxPoint(-sz.GetWidth() - 25, 0);
}

extern int g_recFileFromRadioEventId;
extern int g_recFileFromDecoderEventId;
extern int g_playFileFromRadioEventId;
extern int g_recFileFromModulatorEventId;
extern int g_txLevel;

#define MIC_SPKR_LEVEL_FORMAT_STR "%s%s"
#define DECIBEL_STR "dB"

#if !wxCHECK_VERSION(3, 3, 0)
// THIS IS VERY MUCH A HACK! wxTabFrame is not in the public interface and should
// not be here, even named as something else. Unfortunately this is needed to get
// the tab state loaded and saved on wxWidgets versions older than 3.3, which lack
// wxAuiNotebook::SaveLayout()/LoadLayout(). Here's hoping this interface remains stable.
//
// (Last retrieved from wxWidgets 3.0.5.1 on August 8, 2023.)
class wxTabFrameOurs : public wxWindow
{
public:

    wxTabFrameOurs()
    {
        m_tabs = NULL;
        m_rect = wxRect(0,0,200,200);
        m_tabCtrlHeight = 20;
    }

    ~wxTabFrameOurs()
    {
        wxDELETE(m_tabs);
    }

    void SetTabCtrlHeight(int h)
    {
        m_tabCtrlHeight = h;
    }

protected:
    void DoSetSize(int x, int y,
                   int width, int height,
                   int WXUNUSED(sizeFlags = wxSIZE_AUTO))
    {
        m_rect = wxRect(x, y, width, height);
        DoSizing();
    }

    void DoGetClientSize(int* x, int* y) const
    {
        *x = m_rect.width;
        *y = m_rect.height;
    }

public:
    bool Show( bool WXUNUSED(show = true) ) { return false; }

    void DoSizing()
    {
        if (!m_tabs)
            return;

        if (m_tabs->IsFrozen() || m_tabs->GetParent()->IsFrozen())
            return;

        m_tab_rect = wxRect(m_rect.x, m_rect.y, m_rect.width, m_tabCtrlHeight);
        if (m_tabs->GetFlags() & wxAUI_NB_BOTTOM)
        {
            m_tab_rect = wxRect (m_rect.x, m_rect.y + m_rect.height - m_tabCtrlHeight, m_rect.width, m_tabCtrlHeight);
            m_tabs->SetSize     (m_rect.x, m_rect.y + m_rect.height - m_tabCtrlHeight, m_rect.width, m_tabCtrlHeight);
            m_tabs->SetRect     (wxRect(0, 0, m_rect.width, m_tabCtrlHeight));
        }
        else //TODO: if (GetFlags() & wxAUI_NB_TOP)
        {
            m_tab_rect = wxRect (m_rect.x, m_rect.y, m_rect.width, m_tabCtrlHeight);
            m_tabs->SetSize     (m_rect.x, m_rect.y, m_rect.width, m_tabCtrlHeight);
            m_tabs->SetRect     (wxRect(0, 0,        m_rect.width, m_tabCtrlHeight));
        }
        // TODO: else if (GetFlags() & wxAUI_NB_LEFT){}
        // TODO: else if (GetFlags() & wxAUI_NB_RIGHT){}

        m_tabs->Refresh();
        m_tabs->Update();

        auto& pages = m_tabs->GetPages();
        size_t i, page_count = pages.GetCount();

        for (i = 0; i < page_count; ++i)
        {
            wxAuiNotebookPage& page = pages.Item(i);
            int border_space = m_tabs->GetArtProvider()->GetAdditionalBorderSpace(page.window);

            int height = m_rect.height - m_tabCtrlHeight - border_space;
            if ( height < 0 )
            {
                // avoid passing negative height to wxWindow::SetSize(), this
                // results in assert failures/GTK+ warnings
                height = 0;
            }
            int width = m_rect.width - 2 * border_space;
            if (width < 0)
                width = 0;

            if (m_tabs->GetFlags() & wxAUI_NB_BOTTOM)
            {
                page.window->SetSize(m_rect.x + border_space,
                                     m_rect.y + border_space,
                                     width,
                                     height);
            }
            else //TODO: if (GetFlags() & wxAUI_NB_TOP)
            {
                page.window->SetSize(m_rect.x + border_space,
                                     m_rect.y + m_tabCtrlHeight,
                                     width,
                                     height);
            }
            // TODO: else if (GetFlags() & wxAUI_NB_LEFT){}
            // TODO: else if (GetFlags() & wxAUI_NB_RIGHT){}
        }
    }

protected:
    void DoGetSize(int* x, int* y) const
    {
        if (x)
            *x = m_rect.GetWidth();
        if (y)
            *y = m_rect.GetHeight();
    }

public:
    void Update()
    {
        // does nothing
    }

    wxRect m_rect;
    wxRect m_tab_rect;
    wxAuiTabCtrl* m_tabs;
    int m_tabCtrlHeight;
};
#endif // !wxCHECK_VERSION(3, 3, 0)

TabFreeAuiNotebook::TabFreeAuiNotebook() : wxAuiNotebook()
{
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
}
TabFreeAuiNotebook::TabFreeAuiNotebook(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, long style)
        : wxAuiNotebook(parent, id, pos, size, style) { }

bool TabFreeAuiNotebook::AcceptsFocus() const { return false; }
bool TabFreeAuiNotebook::AcceptsFocusFromKeyboard() const { return false; }
bool TabFreeAuiNotebook::AcceptsFocusRecursively() const { return false; }

#if !wxCHECK_VERSION(3, 3, 0)
// SavePerspective and LoadPerspective below credit https://forums.kirix.com/viewtopicdafe.html?f=15&t=542
// with minor modifications to make it compile on modern wxWidgets.
wxString TabFreeAuiNotebook::SavePerspective() {
    // Build list of panes/tabs
    wxString tabs;
    wxAuiPaneInfoArray& all_panes = m_mgr.GetAllPanes();
     const size_t pane_count = all_panes.GetCount();

     for (size_t i = 0; i < pane_count; ++i)
     {
       wxAuiPaneInfo& pane = all_panes.Item(i);
       if (pane.name == wxT("dummy"))
             continue;

         wxTabFrameOurs* tabframe = (wxTabFrameOurs*)pane.window;

       if (!tabs.empty()) tabs += wxT("|");
       tabs += pane.name;
       tabs += wxT("=");
  
       // Add tabs, keyed by caption rather than position. Position (AddPage() call
       // order) isn't stable across app versions if a tab is ever added, removed, or
       // reordered, which would silently corrupt previously-saved layouts.
       size_t page_count = tabframe->m_tabs->GetPageCount();
       for (size_t p = 0; p < page_count; ++p)
       {
          wxAuiNotebookPage& page = tabframe->m_tabs->GetPage(p);
          const size_t page_idx = m_tabs.GetIdxFromWindow(page.window);

          if (p) tabs += wxT(",");

          if ((int)page_idx == m_curPage) tabs += wxT("*");
          else if ((int)p == tabframe->m_tabs->GetActivePage()) tabs += wxT("+");
          tabs += page.caption;
       }
    }
    tabs += wxT("@");

    // Add frame perspective
    tabs += m_mgr.SavePerspective();

    return tabs;
}

bool TabFreeAuiNotebook::LoadPerspective(const wxString& layout) {
    // Remove all tab ctrls (but still keep them in main index)
    const size_t tab_count = m_tabs.GetPageCount();
    std::set<wxString> readdedTabs;

    // Caption -> master index lookup, so saved entries resolve by tab identity
    // rather than by position (see SavePerspective()).
    std::map<wxString, size_t> captionToIdx;
    for (size_t i = 0; i < tab_count; ++i) {
        captionToIdx[m_tabs.GetPage(i).caption] = i;
    }

    for (size_t i = 0; i < tab_count; ++i) {
       wxWindow* wnd = m_tabs.GetWindowFromIdx(i);

       // find out which onscreen tab ctrl owns this tab
       wxAuiTabCtrl* ctrl;
       int ctrl_idx;
       if (!FindTab(wnd, &ctrl, &ctrl_idx))
          return false;

       // remove the tab from ctrl
       if (!ctrl->RemovePage(wnd))
          return false;
    }
    RemoveEmptyTabFrames();

    size_t sel_page = 0;

    // Creates a new (empty) tab group pane, docked at the bottom, named paneName.
    auto createTabGroup = [&](const wxString& paneName) -> wxAuiTabCtrl* {
        wxTabFrameOurs* new_tabs = new wxTabFrameOurs();
        new_tabs->m_tabs = new wxAuiTabCtrl(this, m_tabIdCounter++);
        new_tabs->m_tabs->SetArtProvider(m_tabs.GetArtProvider()->Clone());
        new_tabs->m_tabCtrlHeight = m_tabCtrlHeight;
        new_tabs->m_tabs->SetFlags(m_flags);

        wxAuiPaneInfo pane_info = wxAuiPaneInfo().Name(paneName).Bottom().CaptionVisible(false);
        m_mgr.AddPane(new_tabs, pane_info);

        return new_tabs->m_tabs;
    };

    wxString tabs = layout.BeforeFirst(wxT('@'));
    wxAuiTabCtrl *dest_tabs = nullptr; // last group known to hold >= 1 page
    bool anyEmptyGroupsCreated = false;
    while (1)
     {
       const wxString tab_part = tabs.BeforeFirst(wxT('|'));

       // if the string is empty, we're done parsing
         if (tab_part.empty())
             break;

       // Get pane name
       const wxString pane_name = tab_part.BeforeFirst(wxT('='));
       wxAuiTabCtrl* new_group = createTabGroup(pane_name);

       // Get list of tab id's and move them to pane
       wxString tab_list = tab_part.AfterFirst(wxT('='));
       ssize_t activePage = -1;
       while(1) {
          wxString tab = tab_list.BeforeFirst(wxT(','));
          if (tab.empty()) break;
          tab_list = tab_list.AfterFirst(wxT(','));

          // Check if this page has an 'active' marker
          const wxChar c = tab[0];
          if (c == wxT('+') || c == wxT('*')) {
             tab = tab.Mid(1);
          }

          auto captionIt = captionToIdx.find(tab);
          if (captionIt == captionToIdx.end()) continue; // tab no longer exists (e.g. removed in a newer version)
          const size_t tab_idx = captionIt->second;

          // Move tab to pane
          wxAuiNotebookPage& page = m_tabs.GetPage(tab_idx);
          const size_t newpage_idx = new_group->GetPageCount();
          new_group->InsertPage(page.window, page, newpage_idx);
          readdedTabs.insert(tab);

          if (c == wxT('+')) activePage = newpage_idx;
          else if ( c == wxT('*')) sel_page = tab_idx;
       }

       if (new_group->GetPageCount() == 0)
       {
           // None of this group's saved entries resolved to a current tab (e.g. an
           // old, pre-caption-keyed saved layout - see SavePerspective()). Leaving a
           // zero-page tab group behind confuses wxAuiNotebook's own drag-and-drop
           // handling (crashes with an assertion failure in GetPage() when the user
           // later drags a tab near it), so sweep it up below instead.
           anyEmptyGroupsCreated = true;
       }
       else
       {
           if (activePage >= 0) new_group->SetActivePage(activePage);
           new_group->DoShowHide();
           dest_tabs = new_group;
       }

       tabs = tabs.AfterFirst(wxT('|'));
    }

    if (anyEmptyGroupsCreated)
    {
        RemoveEmptyTabFrames();
    }

    // Load the frame perspective
    const wxString frames = layout.AfterFirst(wxT('@'));
    bool framesLoaded = m_mgr.LoadPerspective(frames);

    bool ok = true;
    if (dest_tabs == nullptr)
    {
        // Saved layout string parsed to zero tab groups (e.g. empty/corrupted). Rather
        // than crash on the dereference below, fall back to one default group holding
        // every tab, matching what a first-run/no-saved-layout state looks like.
        log_warn("Tab layout persistence: saved layout produced no tab groups; falling back to default layout.");
        dest_tabs = createTabGroup(wxT("default"));
        ok = false;
    }
    else if (!framesLoaded)
    {
        log_warn("Tab layout persistence: failed to restore frame perspective; layout may not match what was saved.");
        ok = false;
    }

    // Reinsert tabs that weren't persisted before
    for (size_t i = 0; i < tab_count; ++i) {
        wxAuiNotebookPage& page = m_tabs.GetPage(i);
        if (readdedTabs.find(page.caption) != readdedTabs.end())
        {
            continue;
        }
        const size_t newpage_idx = dest_tabs->GetPageCount();
        dest_tabs->InsertPage(page.window, page, newpage_idx);
    }

    // Force refresh of selection
    m_curPage = -1;
    SetSelection(sel_page);

    return ok;
}
#endif // !wxCHECK_VERSION(3, 3, 0)

namespace {
    // Tint colour/strength applied by GroupBoxBackgroundColour() below, set
    // via SetGroupBoxTint() from the persisted Display options once
    // configuration is loaded. Defaults match the original hardcoded values
    // so the very first run (before any config exists) looks the same as
    // before this became configurable.
    wxColour s_groupBoxTintColour(0, 85, 255);
    int s_groupBoxTintPercent = 20;

    // Every currently-live TintedGroupBox, so a tint change from Options can
    // be re-applied immediately rather than only on next restart.
    std::vector<TintedGroupBox*> s_tintedGroupBoxes;

#if !defined(__WXGTK__) || !defined(HAS_GTK3)
    // Fallback-only (non-GTK3 builds -- see GetGroupBoxBaseColour() below):
    // real (1x1, otherwise untouched) child of the main frame, so at least
    // a live *widget* is consulted rather than only wxSystemSettings.
    // Created in TopFrame's constructor, before any TintedGroupBox/
    // PlotPanel exists to need it.
    wxWindow* s_colourReferenceWindow = nullptr;
#endif
}

wxColour GetGroupBoxBaseColour()
{
#if defined(__WXGTK__) && defined(HAS_GTK3)
    // wx's own colour caching -- wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW),
    // and (confirmed by testing) even a real live widget's own
    // GetBackgroundColour() -- doesn't reliably track theme changes under
    // XWayland: switching produces a value that's a full theme-switch
    // behind reality, not just briefly stale. Confirmed via direct
    // instrumentation of ~/.config/gtk-3.0/settings.ini that the
    // underlying desktop state itself (GTK's own
    // gtk-application-prefer-dark-theme setting) updates correctly and
    // promptly on every switch, both directions -- the bug is entirely in
    // wx's GTK colour-cache invalidation, not upstream of it.
    //
    // Reading gtk_settings_get_default()'s own in-process property directly
    // (bypassing wx's colour APIs) fixes that, but isn't itself fully
    // reliable either -- confirmed by testing that under wx 3.2 specifically,
    // it can start wrong at launch and never correct itself for the rest of
    // the session (unlike wx 3.3, where it's correct from the first read).
    // So use it only as a fallback; prefer parsing the same settings.ini
    // file directly, since that's the one thing confirmed accurate and
    // prompt in every test done today, independent of wx version.
    //
    // Trade-off either way: two representative light/dark tones (close to
    // common themes' actual window colour, e.g. Breeze) rather than the
    // exact live wxSYS_COLOUR_WINDOW shade, since that exact value isn't
    // reliably obtainable at all right now under this combination --
    // correct light/dark direction, promptly, beats exact colour-matching
    // that's proven unreliable to read live.
    bool preferDark = false;
    bool foundInSettingsFile = false;
    // Deliberately not wxStandardPaths::GetUserConfigDir() -- on Unix that
    // returns plain $HOME (a wx compatibility quirk), not ~/.config.
    wxString settingsPath = wxGetHomeDir() + wxT("/.config/gtk-3.0/settings.ini");
    wxTextFile settingsFile;
    if (settingsFile.Open(settingsPath))
    {
        for (wxString line = settingsFile.GetFirstLine(); !settingsFile.Eof(); line = settingsFile.GetNextLine())
        {
            wxString trimmed = line;
            trimmed.Trim(false).Trim(true);
            if (trimmed.StartsWith(wxT("gtk-application-prefer-dark-theme")))
            {
                preferDark = trimmed.Lower().Contains(wxT("true")) || trimmed.Contains(wxT("=1"));
                foundInSettingsFile = true;
                break;
            }
        }
        settingsFile.Close();
    }
    if (!foundInSettingsFile)
    {
        gboolean gtkPreferDark = FALSE;
        g_object_get(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &gtkPreferDark, NULL);
        preferDark = gtkPreferDark;
    }
    return preferDark ? wxColour(20, 22, 24) : wxColour(255, 255, 255);
#else
    if (s_colourReferenceWindow != nullptr)
    {
        return s_colourReferenceWindow->GetBackgroundColour();
    }
    return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
#endif
}

wxColour GetGroupBoxForegroundColour()
{
    wxColour base = GetGroupBoxBaseColour();
    bool isDark = base.GetLuminance() < 0.5;
    return isDark ? wxColour(255, 255, 255) : wxColour(0, 0, 0);
}

// Returns a background colour offset from the system window colour so that
// grouped control boxes read as a distinct "card" instead of blending into
// the surrounding panel. Works out from the live system colour rather than
// a hardcoded value so it tracks whatever light/dark theme is active.
wxColour GroupBoxBackgroundColour()
{
    wxColour base = GetGroupBoxBaseColour();
    bool isDark = base.GetLuminance() < 0.5;
    wxColour shaded = base.ChangeLightness(isDark ? 115 : 93);

    // Nudge the shaded card colour towards blue (by default). Purely a
    // lightness shift is invisible on themes (e.g. Breeze Light) whose
    // window colour has no saturation to begin with, so blend in a small
    // amount of hue directly.
    const wxColour& tint = s_groupBoxTintColour;
    int tintPct = s_groupBoxTintPercent;

    unsigned char r = (unsigned char)((shaded.Red()   * (100 - tintPct) + tint.Red()   * tintPct) / 100);
    unsigned char g = (unsigned char)((shaded.Green() * (100 - tintPct) + tint.Green() * tintPct) / 100);
    unsigned char b = (unsigned char)((shaded.Blue()  * (100 - tintPct) + tint.Blue()  * tintPct) / 100);
    return wxColour(r, g, b);
}

void SetGroupBoxTint(const wxColour& colour, int percent)
{
    s_groupBoxTintColour = colour;
    s_groupBoxTintPercent = percent;
}

void RefreshGroupBoxTints()
{
    wxColour newColour = GroupBoxBackgroundColour();
    for (auto box : s_tintedGroupBoxes)
    {
        box->SetBackgroundColour(newColour);
        box->Refresh();
    }
}

TintedGroupBox::TintedGroupBox(wxWindow* parent, const wxString& title, wxOrientation orientation, int contextMenuBoxIndex)
    : wxPanel(parent, wxID_ANY)
{
    SetBackgroundColour(GroupBoxBackgroundColour());

    wxBoxSizer* outerSizer = new wxBoxSizer(wxVERTICAL);

    // An empty title omits the label entirely (and the space it would
    // reserve), for boxes that don't need one.
    m_title = new wxStaticText(this, wxID_ANY, title);
    if (title.IsEmpty())
    {
        m_title->Hide();
    }
    else
    {
        wxFont titleFont = m_title->GetFont();
        titleFont.SetWeight(wxFONTWEIGHT_BOLD);
        m_title->SetFont(titleFont);
        outerSizer->Add(m_title, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxLEFT | wxRIGHT, 6);
    }

    m_contentSizer = new wxBoxSizer(orientation);
    outerSizer->Add(m_contentSizer, 1, wxEXPAND | wxALL, 6);

    SetSizer(outerSizer);

    s_tintedGroupBoxes.push_back(this);

    // Only the movable "Show menu" boxes are constructed with a real
    // contextMenuBoxIndex -- wire those up so right-clicking the title or
    // background pops up the Hide/Move menu (Stage 10). Forwards straight to
    // TopFrame::OnGroupBoxRightClick() rather than through the wx event
    // system, since there's nothing else that needs to intercept it.
    if (contextMenuBoxIndex >= 0)
    {
        auto forward = [this, contextMenuBoxIndex]()
        {
            static_cast<TopFrame*>(wxGetTopLevelParent(this))->OnGroupBoxRightClick(contextMenuBoxIndex);
        };

#if wxCHECK_VERSION(3, 3, 0) && defined(__WXGTK__)
        // See the equivalent comment by m_txLevelBox's own context menu
        // wiring further down this file: wxGTK 3.3+ fires wxEVT_CONTEXT_MENU
        // on button-press for these widget types, causing GTK to dismiss
        // PopupMenu on button release; use RIGHT_UP instead. MSW/OSX are
        // unaffected, so this is GTK-specific.
        this->Bind(wxEVT_RIGHT_UP, [forward](wxMouseEvent&) { forward(); });
        m_title->Bind(wxEVT_RIGHT_UP, [forward](wxMouseEvent&) { forward(); });
#else
        // wxGTK < 3.3 does not generate RIGHT_UP for windowless widget types;
        // CONTEXT_MENU works without the dismiss issue. (Also used as-is on
        // MSW/OSX regardless of wx version -- see above.)
        this->Bind(wxEVT_CONTEXT_MENU, [forward](wxContextMenuEvent&) { forward(); });
        m_title->Bind(wxEVT_CONTEXT_MENU, [forward](wxContextMenuEvent&) { forward(); });
#endif
    }
}

TintedGroupBox::~TintedGroupBox()
{
    s_tintedGroupBoxes.erase(
        std::remove(s_tintedGroupBoxes.begin(), s_tintedGroupBoxes.end(), this),
        s_tintedGroupBoxes.end());
}

void TintedGroupBox::SetLabel(const wxString& label)
{
    m_title->SetLabel(label);
}

wxString TintedGroupBox::GetLabel() const
{
    return m_title->GetLabel();
}

void TintedGroupBox::SetToolTip(const wxString& tip)
{
    wxPanel::SetToolTip(tip);
    m_title->SetToolTip(tip);
}

//=========================================================================
// Code that lays out the main application window
//=========================================================================
TopFrame::TopFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxFrame(parent, id, title, pos, size, style)
{
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);

#if !defined(__WXGTK__) || !defined(HAS_GTK3)
    // See s_colourReferenceWindow's declaration above for why this exists.
    // GTK3 builds don't need it (GetGroupBoxBaseColour() reads GTK settings
    // directly there) -- skipped entirely on them, since an extra untracked
    // child sitting directly on the frame outside any sizer turned out to
    // interfere with the frame's own resize/layout propagation.
    s_colourReferenceWindow = new wxWindow(this, wxID_ANY, wxPoint(0, 0), wxSize(1, 1));
#endif

#if wxUSE_ACCESSIBILITY
    // Initialize accessibility logic
    SetAccessible(new NameOverrideAccessible([&]() {
        auto labelStr = GetLabel(); // note: should be equivalent to title.

        // Ensures NVDA reads back version numbers as "x point y ..." rather
        // than as a date.
        wxRegEx rePoint("\\.");
        rePoint.ReplaceAll(&labelStr, _(" point "));
        
        return labelStr;
    }));
#endif // wxUSE_ACCESSIBILITY
    
    this->SetSizeHints(wxDefaultSize, wxDefaultSize);
    
    //=====================================================
    // Menubar Setup
    //=====================================================
    m_menubarMain = new wxMenuBar(wxMB_DOCKABLE);
    file = new wxMenu();

#if !defined(__WXGTK__)
    /* "On Top" isn't reliable on Linux, so there's no point in having it visible. */
    wxMenuItem* m_menuItemOnTop;
    m_menuItemOnTop = new wxMenuItem(file, wxID_ANY, wxString(_("Keep &On Top")) , _("Always keeps FreeDV above other windows"), wxITEM_CHECK);
    file->Append(m_menuItemOnTop);
    this->Connect(m_menuItemOnTop->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnTop));
#endif // !defined(__WXGTK__)

    wxMenuItem* m_menuItemExit;
    m_menuItemExit = new wxMenuItem(file, wxID_EXIT, wxString(_("E&xit")) , _("Exit Program"), wxITEM_NORMAL);
    file->Append(m_menuItemExit);

    m_menubarMain->Append(file, _("&File"));

    settings = new wxMenu();

    // wxID_PREFERENCES causes wxWidgets to move this item into the application
    // menu on macOS automatically. On other platforms it stays in Settings.
    wxMenuItem* m_menuItemOptions;
#ifdef __WXMAC__
    m_menuItemOptions = new wxMenuItem(settings, wxID_PREFERENCES, wxString(_("&Edit Settings...\tCTRL-,")) , _("Miscellaneous FreeDV configuration options"), wxITEM_NORMAL);
#else
    m_menuItemOptions = new wxMenuItem(settings, wxID_PREFERENCES, wxString(_("&Edit Settings...")) , _("Miscellaneous FreeDV configuration options"), wxITEM_NORMAL);
#endif // __WXMAC__
    settings->Append(m_menuItemOptions);

    wxMenuItem* m_menuItemSetupWizard;
    m_menuItemSetupWizard = new wxMenuItem(settings, wxID_ANY, wxString(_("Setup &Wizard...")), _("Re-run the FreeDV Setup Wizard"), wxITEM_NORMAL);
    settings->Append(m_menuItemSetupWizard);

    settings->AppendSeparator();

    m_menuItemExportConfig = new wxMenuItem(settings, wxID_ANY, wxString(_("&Export Configuration...")) , _("Exports the current FreeDV configuration to a file"), wxITEM_NORMAL);
    settings->Append(m_menuItemExportConfig);

    m_menuItemImportConfig = new wxMenuItem(settings, wxID_ANY, wxString(_("&Use Configuration...")) , _("Loads a FreeDV configuration from a file"), wxITEM_NORMAL);
    settings->Append(m_menuItemImportConfig);

    wxMenuItem* m_menuItemLoadDefaultConfig;
    m_menuItemLoadDefaultConfig = new wxMenuItem(settings, wxID_ANY, wxString(_("Load &Default Configuration")) , _("Resets FreeDV to its default configuration"), wxITEM_NORMAL);
    settings->Append(m_menuItemLoadDefaultConfig);

    m_menubarMain->Append(settings, _("&Settings"));

    tools = new wxMenu();

    wxMenuItem* m_menuItemFreeDVReporter;
    m_menuItemFreeDVReporter = new wxMenuItem(tools, wxID_ANY, wxString(_("FreeDV R&eporter")) , _("Opens browser window and displays FreeDV Reporter service."), wxITEM_NORMAL);
    tools->Append(m_menuItemFreeDVReporter);

    wxMenuItem* m_menuItemFilter;
    m_menuItemFilter = new wxMenuItem(tools, wxID_ANY, wxString(_("Filter &Audio...")) , _("Configures audio filtering"), wxITEM_NORMAL);
    tools->Append(m_menuItemFilter);

    tools->AppendSeparator();

    m_menuItemPlayFileFromRadio = new wxMenuItem(tools, wxID_ANY, wxString(_("Start &Play File - From Radio...")) , _("Pipes radio sound input from file"), wxITEM_NORMAL);
    g_playFileFromRadioEventId = m_menuItemPlayFileFromRadio->GetId();
    tools->Append(m_menuItemPlayFileFromRadio);

    m_menubarMain->Append(tools, _("&Tools"));

    // "Show" menu: lets the user hide optional group boxes to build a
    // leaner display, independent of the feature-driven visibility already
    // used for Squelch/Mode/Radio Freq/Stats. Order here is index order for
    // OnShowGroupBox's event ID offset (ID_SHOW_GROUPBOX_BASE + index) --
    // keep the two in sync if adding/removing an entry. Checked state here
    // is just the default (visible); TopFrame has no access to
    // wxGetApp()/appConfiguration (that's MainFrame's job), so the real
    // persisted state is applied to both the boxes and these checkmarks in
    // MainFrame::loadConfiguration_(), same as statsBox/modeBox/etc.
    showMenu_ = new wxMenu();
    std::vector<wxString> showMenuItems {
        _("SNR"),
        _("Level"),
        _("Sync"),
        _("Audio Recording"),
        _("Logging"),
        _("FDV Reporting"),
        _("TX Attenuation"),
        _("Speaker Level"),
    };
    for (size_t index = 0; index < showMenuItems.size(); index++)
    {
        auto menuItem = showMenu_->Append(ID_SHOW_GROUPBOX_BASE + index, showMenuItems[index], wxEmptyString, wxITEM_CHECK);
        menuItem->Check(true);
        this->Connect(ID_SHOW_GROUPBOX_BASE + index, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnShowGroupBox));
    }
    m_menubarMain->Append(showMenu_, _("&Show"));

    help = new wxMenu();
    wxMenuItem* m_menuItemHelpUpdates;
    m_menuItemHelpUpdates = new wxMenuItem(help, wxID_ANY, wxString(_("&Check for Updates")) , _("Checks for updates to FreeDV"), wxITEM_NORMAL);
    help->Append(m_menuItemHelpUpdates);
    m_menuItemHelpUpdates->Enable(false);
    
    wxMenuItem* m_menuItemAbout;
    m_menuItemAbout = new wxMenuItem(help, wxID_ABOUT, wxString(_("&About FreeDV")) , _("About this program"), wxITEM_NORMAL);
    help->Append(m_menuItemAbout);

    wxMenuItem* m_menuItemHelpManual;
    m_menuItemHelpManual = new wxMenuItem(help, wxID_ANY, wxString(_("&User Manual...")), _("Loads the user manual"), wxITEM_NORMAL);
    help->Append(m_menuItemHelpManual);

    wxMenuItem* m_menuItemHelpGetAssistance;
    m_menuItemHelpGetAssistance = new wxMenuItem(help, wxID_ANY, wxString(_("&Get Assistance")), _("Gets assistance with FreeDV setup"), wxITEM_NORMAL);
    help->Append(m_menuItemHelpGetAssistance);
        
    m_menubarMain->Append(help, _("&Help"));

    this->SetMenuBar(m_menubarMain);

    m_panel = new wxPanel(this);

    wxBoxSizer* bSizer1;
    bSizer1 = new wxBoxSizer(wxHORIZONTAL);

    //=====================================================
    // Left side
    //=====================================================
    wxSizer* leftOuterSizer = new wxBoxSizer(wxVERTICAL);
    leftSizer = new wxWrapSizer(wxVERTICAL, wxREMOVE_LEADING_SPACES);

    snrBox = new TintedGroupBox(m_panel, _("SNR"), wxVERTICAL, 0);

    //------------------------------
    // S/N ratio Gauge (vert. bargraph)
    //------------------------------
    m_gaugeSNR = new wxGauge(snrBox, wxID_ANY, 45, wxDefaultPosition, wxSize(135,15), wxGA_SMOOTH);
    m_gaugeSNR->SetToolTip(_("Displays signal to noise ratio in dB."));
    snrBox->GetContentSizer()->Add(m_gaugeSNR, 1, wxALIGN_CENTER_HORIZONTAL|static_cast<int>(wxALL), 10);

    //------------------------------
    // Box for S/N ratio (Numeric)
    //------------------------------
    m_textSNR = new wxStaticText(snrBox, wxID_ANY, wxT("--"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    m_textSNR->SetMinSize(wxSize(70,-1));
    snrBox->GetContentSizer()->Add(m_textSNR, 0, wxALIGN_CENTER_HORIZONTAL, 1);

    //------------------------------
    // S/N ratio slow Checkbox
    //------------------------------
    m_ckboxSNR = new wxCheckBox(snrBox, wxID_ANY, _("Slow"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    m_ckboxSNR->SetToolTip(_("Smooth but slow SNR estimation"));
    snrBox->GetContentSizer()->Add(m_ckboxSNR, 0, wxALIGN_CENTER_HORIZONTAL, 5);

    leftSizer->Add(snrBox, 0, static_cast<int>(wxEXPAND)|static_cast<int>(wxALL), 2);

    //------------------------------
    // Signal Level(vert. bargraph)
    //------------------------------
    levelBox = new TintedGroupBox(m_panel, _("Level"), wxHORIZONTAL, 1);

    m_gaugeLevel = new wxGauge(levelBox, wxID_ANY, 100, wxDefaultPosition, wxSize(100,15), wxGA_SMOOTH);
    m_gaugeLevel->SetToolTip(_("Peak of From Radio in Rx, or peak of From Mic in Tx mode.  If Red you should reduce your levels"));
    levelBox->GetContentSizer()->Add(m_gaugeLevel, 1, wxALIGN_CENTER_VERTICAL|static_cast<int>(wxALL), 10);

    m_textLevel = new wxStaticText(levelBox, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(35,-1), wxALIGN_CENTRE);
    m_textLevel->SetForegroundColour(wxColour(255,0,0));
    levelBox->GetContentSizer()->Add(m_textLevel, 0, wxALIGN_CENTER_VERTICAL, 1);

    leftSizer->Add(levelBox, 0, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 2);
    //------------------------------
    // Sync  Indicator box
    //------------------------------
    syncBox = new TintedGroupBox(m_panel, _("Sync"), wxVERTICAL, 2);
    wxString syncBoxToolTip = _("Shows the current FreeDV mode. Green indicates the modem is synchronised with the received signal; red indicates no sync.");
    syncBox->SetToolTip(syncBoxToolTip);

    m_textSync = new wxStaticText(syncBox, wxID_ANY, wxT("unk"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    syncBox->GetContentSizer()->Add(m_textSync, 0, wxALIGN_CENTER_HORIZONTAL, 1);
    m_textSync->Disable();
    m_textSync->SetToolTip(syncBoxToolTip);

    leftSizer->Add(syncBox,0, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 2);

    //------------------------------
    // Audio Recording/Playback
    //------------------------------
    audioBox = new TintedGroupBox(m_panel, _("Audio Recording"), wxVERTICAL, 3);

    m_audioRecord = new wxToggleButton(audioBox, wxID_ANY, _("Record"), wxDefaultPosition, wxDefaultSize, 0);
    m_audioRecord->SetToolTip(_("Records incoming over the air signals as well as anything transmitted."));
    audioBox->GetContentSizer()->Add(m_audioRecord, 0, static_cast<int>(wxALL) | wxALIGN_CENTER_HORIZONTAL, 5);

    leftSizer->Add(audioBox, 0, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 2);

    //------------------------------
    // QSO logging
    //------------------------------
    logBox = new TintedGroupBox(m_panel, _("Logging"), wxVERTICAL, 4);

    m_logQSO = new wxButton(logBox, wxID_ANY, _("Log QSO"), wxDefaultPosition, wxDefaultSize, 0);
    m_logQSO->SetToolTip(_("Logs most recent QSO."));
    m_logQSO->Disable();
    logBox->GetContentSizer()->Add(m_logQSO, 0, static_cast<int>(wxALL) | wxALIGN_CENTER_HORIZONTAL, 5);

    leftSizer->Add(logBox, 0, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 2);

    //------------------------------
    // FreeDV Reporter quick options
    //------------------------------
    reporterBox = new TintedGroupBox(m_panel, _("FDV Reporting"), wxVERTICAL, 5);

    m_reporterHidden = new wxToggleButton(reporterBox, wxID_ANY, _("Turn Off"), wxDefaultPosition, wxDefaultSize, 0);
    m_reporterHidden->SetToolTip(_("Quick ON/OFF for FreeDV Reporting, when enabled in Tools->Settings->Reporting."));
    reporterBox->GetContentSizer()->Add(m_reporterHidden, 0, static_cast<int>(wxALL) | wxALIGN_CENTER_HORIZONTAL, 5);

    leftSizer->Add(reporterBox, 0, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 2);

    //------------------------------
    // BER Frames box
    //------------------------------

    statsBox = new TintedGroupBox(m_panel, _("Stats"), wxVERTICAL);

    m_BtnBerReset = new wxButton(statsBox, wxID_ANY, _("&Reset"), wxDefaultPosition, wxDefaultSize, 0);
    statsBox->GetContentSizer()->Add(m_BtnBerReset, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|static_cast<int>(wxALL), 5);

    m_textBits = new wxStaticText(statsBox, wxID_ANY, wxT("Bits: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    statsBox->GetContentSizer()->Add(m_textBits, 0, static_cast<int>(wxALL) | wxALIGN_LEFT, 1);
    m_textErrors = new wxStaticText(statsBox, wxID_ANY, wxT("Errs: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    statsBox->GetContentSizer()->Add(m_textErrors, 0, static_cast<int>(wxALL) | wxALIGN_LEFT, 1);
    m_textBER = new wxStaticText(statsBox, wxID_ANY, wxT("BER: 0.0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    statsBox->GetContentSizer()->Add(m_textBER, 0, static_cast<int>(wxALL) | wxALIGN_LEFT, 1);
    m_textResyncs = new wxStaticText(statsBox, wxID_ANY, wxT("Resyncs: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    statsBox->GetContentSizer()->Add(m_textResyncs, 0, static_cast<int>(wxALL) | wxALIGN_LEFT, 1);
    m_textClockOffset = new wxStaticText(statsBox, wxID_ANY, wxT("ClkOff: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_textClockOffset->SetMinSize(wxSize(125,-1));
    statsBox->GetContentSizer()->Add(m_textClockOffset, 0, static_cast<int>(wxALL) | wxALIGN_LEFT, 1);
    m_textFreqOffset = new wxStaticText(statsBox, wxID_ANY, wxT("FreqOff: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    statsBox->GetContentSizer()->Add(m_textFreqOffset, 0, static_cast<int>(wxALL) | wxALIGN_LEFT, 1);
    m_textSyncMetric = new wxStaticText(statsBox, wxID_ANY, wxT("Sync: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    statsBox->GetContentSizer()->Add(m_textSyncMetric, 0, static_cast<int>(wxALL) | wxALIGN_LEFT, 1);
    m_textCodec2Var = new wxStaticText(statsBox, wxID_ANY, wxT("Var: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    statsBox->GetContentSizer()->Add(m_textCodec2Var, 0, static_cast<int>(wxALL) | wxALIGN_LEFT, 1);

    leftSizer->Add(statsBox,0, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 2);

    leftSizer->SetMinSize(wxSize(-1, 375));
    
#if !wxCHECK_VERSION(3,2,0)
    leftOuterSizer->Add(leftSizer, 0, static_cast<int>(wxALL) | static_cast<int>(wxEXPAND) | wxFIXED_MINSIZE, 1);
#else
    leftOuterSizer->Add(leftSizer, 2, static_cast<int>(wxALL) | static_cast<int>(wxEXPAND) | wxFIXED_MINSIZE, 1);
#endif // !wxCHECK_VERSION(3,2,0)

    bSizer1->Add(leftOuterSizer, 0, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 5);

    //=====================================================
    // Center Section
    //=====================================================
    wxBoxSizer* centerSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* upperSizer = new wxBoxSizer(wxVERTICAL);

    //=====================================================
    // Tabbed Notebook control containing display graphs
    //=====================================================

    long nb_style = wxAUI_NB_BOTTOM | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS;
    m_auiNbookCtrl = new TabFreeAuiNotebook(m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, nb_style);
    // This line sets the fontsize for the tabs on the notebook control
    m_auiNbookCtrl->SetMinSize(wxSize(375,375));

    upperSizer->Add(m_auiNbookCtrl, 1, wxALIGN_TOP|static_cast<int>(wxEXPAND), 1);
    centerSizer->Add(upperSizer, 1, wxALIGN_TOP|static_cast<int>(wxEXPAND), 0);

    // lower middle used for user ID

    TintedGroupBox* stationBox = new TintedGroupBox(m_panel, wxEmptyString, wxHORIZONTAL);

    wxBoxSizer* modeStatusSizer;
    modeStatusSizer = new wxBoxSizer(wxVERTICAL);
    m_txtModeStatus = new wxStaticText(stationBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_txtModeStatus->Enable(false); // enabled only if Hamlib is turned on
    m_txtModeStatus->SetMinSize(wxSize(80,-1));
    modeStatusSizer->Add(m_txtModeStatus, 0, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 1);
    stationBox->GetContentSizer()->Add(modeStatusSizer, 0, wxALIGN_CENTER_VERTICAL|static_cast<int>(wxALL), 1);

    wxBoxSizer* bSizer15;
    bSizer15 = new wxBoxSizer(wxVERTICAL);
    m_txtCtrlCallSign = new wxTextCtrl(stationBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    m_txtCtrlCallSign->SetToolTip(_("Call Sign of transmitting station will appear here"));
    m_txtCtrlCallSign->SetSizeHints(wxSize(100,-1));

    m_cboLastReportedCallsigns = new wxComboCtrl(stationBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxCB_READONLY);
    m_lastReportedCallsignListView = new wxListViewComboPopup(m_cboLastReportedCallsigns);
    m_cboLastReportedCallsigns->SetPopupControl(m_lastReportedCallsignListView);
    m_cboLastReportedCallsigns->SetSizeHints(wxSize(400,-1));
    m_cboLastReportedCallsigns->SetPopupMaxHeight(150);

    m_lastReportedCallsignListView->InsertColumn(0, wxT("Callsign"), wxLIST_FORMAT_LEFT, 100);
    m_lastReportedCallsignListView->InsertColumn(1, wxT("Frequency"), wxLIST_FORMAT_RIGHT, 75);
    m_lastReportedCallsignListView->InsertColumn(2, wxT("Date/Time"), wxLIST_FORMAT_LEFT, 175);
    m_lastReportedCallsignListView->InsertColumn(3, wxT("SNR"), wxLIST_FORMAT_RIGHT, 50);

    bSizer15->Add(m_txtCtrlCallSign, 1, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 5);
    bSizer15->Add(m_cboLastReportedCallsigns, 1, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 5);

    stationBox->GetContentSizer()->Add(bSizer15, 1, static_cast<int>(wxEXPAND), 5);
    stationBox->SetMinSize(wxSize(375,-1));
    centerSizer->Add(stationBox, 0, static_cast<int>(wxEXPAND), 2);
    centerSizer->SetMinSize(wxSize(375,375));
    bSizer1->Add(centerSizer, 1, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 1);
    
    //=====================================================
    // Right side
    //=====================================================
    rightSizer = new wxWrapSizer(wxVERTICAL, wxREMOVE_LEADING_SPACES);

    // Transmit Level slider
    m_txLevelBox = new TintedGroupBox(m_panel, _("TX &Attenuation"), wxVERTICAL, 6);

    wxBoxSizer* txBtnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_btnTxLevelMM = new wxButton(m_txLevelBox, wxID_ANY, _("<<"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_btnTxLevelM  = new wxButton(m_txLevelBox, wxID_ANY, _("<"),  wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_btnTxLevelP  = new wxButton(m_txLevelBox, wxID_ANY, _(">"),  wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_btnTxLevelPP = new wxButton(m_txLevelBox, wxID_ANY, _(">>"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_btnTxLevelMM->SetToolTip(_("Decrease output by 1.0dB"));
    m_btnTxLevelM ->SetToolTip(_("Decrease output by 0.2dB"));
    m_btnTxLevelP ->SetToolTip(_("Increase output by 0.2dB"));
    m_btnTxLevelPP->SetToolTip(_("Increase output by 1.0dB"));
    txBtnSizer->Add(m_btnTxLevelMM, 1, static_cast<int>(wxEXPAND), 0);
    txBtnSizer->Add(m_btnTxLevelM,  1, static_cast<int>(wxEXPAND), 0);
    txBtnSizer->Add(m_btnTxLevelP,  1, static_cast<int>(wxEXPAND), 0);
    txBtnSizer->Add(m_btnTxLevelPP, 1, static_cast<int>(wxEXPAND), 0);
    wxString fmtString = wxString::Format(MIC_SPKR_LEVEL_FORMAT_STR, wxNumberFormatter::ToString((double)0, 1), DECIBEL_STR);

    m_txtTxLevelNum = new wxStaticText(m_txLevelBox, wxID_ANY, fmtString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER | wxST_NO_AUTORESIZE);
    m_txtTxLevelNum->SetToolTip(_("Use mouse scroll wheel to adjust up or down\nRight click for more options"));
    m_txtTxLevelNum->SetMinSize(wxSize(100,-1));
    m_txLevelBox->GetContentSizer()->Add(m_txtTxLevelNum, 0, wxEXPAND, 0);

    m_txLevelBox->GetContentSizer()->Add(txBtnSizer, 0, wxEXPAND, 0);

    rightSizer->Add(m_txLevelBox, 0, static_cast<int>(wxALL) | static_cast<int>(wxEXPAND), 2);

    // Mic/Speaker Level slider
    micSpeakerBox = new TintedGroupBox(m_panel, _("Speaker &Level"), wxVERTICAL, 7);

    // Sliders are integer values, so we're multiplying min/max by 10 here to allow 1 decimal precision.
    m_sliderMicSpkrLevel = new wxSlider(micSpeakerBox, wxID_ANY, 0, -200, 200, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS);
    m_sliderMicSpkrLevel->SetMinSize(wxSize(150,-1));
    micSpeakerBox->GetContentSizer()->Add(m_sliderMicSpkrLevel, 1, wxALIGN_CENTER_HORIZONTAL, 0);
    m_sliderMicSpkrLevel->Enable(false);

#if wxUSE_ACCESSIBILITY
    // Add accessibility class so that the values are read back correctly.
    auto micSpkrSliderAccessibility = new LabelOverrideAccessible([&]() {
        return m_txtMicSpkrLevelNum->GetLabel();
    });
    m_sliderMicSpkrLevel->SetAccessible(micSpkrSliderAccessibility);
#endif // wxUSE_ACCESSIBILITY

    fmtString = wxString::Format(MIC_SPKR_LEVEL_FORMAT_STR, wxNumberFormatter::ToString((double)0, 1), DECIBEL_STR);

    m_txtMicSpkrLevelNum = new wxStaticText(micSpeakerBox, wxID_ANY, fmtString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_txtMicSpkrLevelNum->SetMinSize(wxSize(100,-1));
    micSpeakerBox->GetContentSizer()->Add(m_txtMicSpkrLevelNum, 0, wxALIGN_CENTER_HORIZONTAL, 0);

    rightSizer->Add(micSpeakerBox, 0, static_cast<int>(wxALL) | static_cast<int>(wxEXPAND), 2);

    // Frequency text field (PSK Reporter)
    m_freqBox = new TintedGroupBox(m_panel, _("Radio Freq. (MHz)"), wxHORIZONTAL);

    //wxStaticText* reportFrequencyUnits = new wxStaticText(m_freqBox, wxID_ANY, wxT(" MHz"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    wxBoxSizer* txtReportFreqSizer = new wxBoxSizer(wxVERTICAL);

    m_cboReportFrequency = new wxComboBox(m_freqBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxTE_PROCESS_ENTER);
    m_cboReportFrequency->SetMinSize(wxSize(150,-1));
    txtReportFreqSizer->Add(m_cboReportFrequency, 1, static_cast<int>(wxALL), 5);

    m_freqBox->GetContentSizer()->Add(txtReportFreqSizer, 1, static_cast<int>(wxEXPAND), 1);
    //m_freqBox->GetContentSizer()->Add(reportFrequencyUnits, 0, wxALIGN_CENTER_VERTICAL, 1);

    rightSizer->Add(m_freqBox, 0, static_cast<int>(wxALL), 2);

    /* new --- */

    //=====================================================
    // Control Toggles box
    //=====================================================
    controlBox = new TintedGroupBox(m_panel, _("Control"), wxVERTICAL);

    //-------------------------------
    // Stop/Stop signal processing (rx and tx)
    //-------------------------------
    m_togBtnOnOff = new wxToggleButton(controlBox, wxID_ANY, _("&Start Modem"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnOnOff->SetToolTip(_("Begin/End receiving data."));
    controlBox->GetContentSizer()->Add(m_togBtnOnOff, 0, static_cast<int>(wxLEFT) | static_cast<int>(wxRIGHT) | static_cast<int>(wxTOP) | static_cast<int>(wxEXPAND), 5);
    controlBox->GetContentSizer()->AddSpacer(4);

    //------------------------------
    // Analog Passthrough Toggle
    //------------------------------
    m_togBtnAnalog = new wxToggleButton(controlBox, wxID_ANY, _("Switch to A&nalog"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnAnalog->SetToolTip(_("Toggle analog/digital operation."));
    controlBox->GetContentSizer()->Add(m_togBtnAnalog, 0, static_cast<int>(wxLEFT) | static_cast<int>(wxRIGHT) | static_cast<int>(wxEXPAND), 5);
    controlBox->GetContentSizer()->AddSpacer(4);

    //------------------------------
    // Tune Toggle
    //------------------------------
    m_btnTogTune = new wxToggleButton(controlBox, wxID_ANY, _("&Tune"), wxDefaultPosition, wxDefaultSize, 0);
    m_btnTogTune->SetToolTip(_("Emits 1500 Hz carrier to enable rig/antenna tuning.\nRight click for more options"));
    controlBox->GetContentSizer()->Add(m_btnTogTune, 0, static_cast<int>(wxLEFT) | static_cast<int>(wxRIGHT) | static_cast<int>(wxBOTTOM) | static_cast<int>(wxEXPAND), 5);
    m_btnTogTune->Enable(false);

    //------------------------------
    // Voice Keyer Toggle
    //------------------------------
    m_togBtnVoiceKeyer = new wxToggleButton(controlBox, wxID_ANY, _("Start Voice &Keyer"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnVoiceKeyer->SetToolTip(_("Toggle Voice Keyer. Right-click for additional options."));
    controlBox->GetContentSizer()->Add(m_togBtnVoiceKeyer, 0, static_cast<int>(wxALL) | static_cast<int>(wxEXPAND), 5);

    //------------------------------
    // PTT button: Toggle Transmit/Receive mode
    //------------------------------
    m_btnTogPTT = new wxToggleButton(controlBox, wxID_ANY, _("&XMIT"), wxDefaultPosition, wxDefaultSize, 0);
    m_btnTogPTT->SetToolTip(_("Switch between Receive and Transmit. Right-click for additional options."));
    controlBox->GetContentSizer()->Add(m_btnTogPTT, 0, static_cast<int>(wxALL) | static_cast<int>(wxEXPAND), 5);

    rightSizer->Add(controlBox, 0, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 2);

    bSizer1->Add(rightSizer, 0, static_cast<int>(wxALL)|static_cast<int>(wxEXPAND), 3);

    // Playback/recording status: a tinted info bar spanning the full window
    // width, in place of the old native status bar. It occupies zero height
    // until ShowPlaybackStatus() gives it a message, then slides back down
    // to zero when dismissed, rather than permanently reserving a blank line.
    wxBoxSizer* outerSizer = new wxBoxSizer(wxVERTICAL);
    outerSizer->Add(bSizer1, 1, static_cast<int>(wxEXPAND), 0);

    m_infoBar = new wxInfoBarGeneric(m_panel);
    m_infoBar->SetBackgroundColour(GroupBoxBackgroundColour());
    outerSizer->Add(m_infoBar, 0, static_cast<int>(wxEXPAND), 0);

    // Natural one-line height of the info bar, used by ShowPlaybackStatus()
    // to compensate the frame's size when showing/dismissing it. Measured
    // via GetBestSize() (font/icon/padding driven, so stable regardless of
    // current Show() state) rather than observed reactively via a size
    // event -- Dismiss() doesn't reliably produce a matching "back to zero"
    // wxEVT_SIZE on this widget, so an event-driven approach only catches
    // the first-ever appearance and silently misses every subsequent
    // show/dismiss cycle.
    m_lastInfoBarHeight = m_infoBar->GetBestSize().GetHeight();

    m_panel->SetSizerAndFit(outerSizer);
    this->Layout();

    //=====================================================
    // End of layout
    //=====================================================
    
    //-------------------
    // Tab ordering for accessibility
    //-------------------
    m_auiNbookCtrl->MoveBeforeInTabOrder(stationBox);
    
    m_togBtnOnOff->MoveBeforeInTabOrder(m_togBtnAnalog);
    m_togBtnAnalog->MoveBeforeInTabOrder(m_btnTogTune);
    m_btnTogTune->MoveBeforeInTabOrder(m_togBtnVoiceKeyer);
    m_togBtnVoiceKeyer->MoveBeforeInTabOrder(m_btnTogPTT);
    
    //-------------------
    // Connect Events
    //-------------------
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(TopFrame::topFrame_OnClose));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(TopFrame::topFrame_OnPaint));
    this->Connect(wxEVT_SIZE, wxSizeEventHandler(TopFrame::topFrame_OnSize));
    this->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::topFrame_OnUpdateUI));
    this->Connect(wxEVT_ACTIVATE, wxActivateEventHandler(TopFrame::OnActivateWindow));
    this->Connect(wxEVT_SYS_COLOUR_CHANGED, wxSysColourChangedEventHandler(TopFrame::OnSystemColorChanged));
    this->Connect(m_menuItemExit->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnExit));

    this->Connect(m_menuItemFreeDVReporter->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsFreeDVReporter));
    this->Connect(m_menuItemFreeDVReporter->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsFreeDVReporterUI));
    this->Connect(m_menuItemSetupWizard->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsSetupWizard));
    this->Connect(m_menuItemSetupWizard->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsSetupWizardUI));
    this->Connect(m_menuItemFilter->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsFilter));
    this->Connect(m_menuItemFilter->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsFilterUI));
    this->Connect(wxID_PREFERENCES, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsOptions));
    this->Connect(wxID_PREFERENCES, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsOptionsUI));

    this->Connect(m_menuItemPlayFileFromRadio->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnPlayFileFromRadio));

    this->Connect(m_menuItemExportConfig->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsExportConfig));
    this->Connect(m_menuItemExportConfig->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsExportConfigUI));
    this->Connect(m_menuItemImportConfig->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsImportConfig));
    this->Connect(m_menuItemImportConfig->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsImportConfigUI));
    this->Connect(m_menuItemLoadDefaultConfig->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsLoadDefaultConfig));
    this->Connect(m_menuItemLoadDefaultConfig->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsLoadDefaultConfigUI));

    this->Connect(m_menuItemHelpUpdates->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpCheckUpdates));
    this->Connect(m_menuItemHelpUpdates->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnHelpCheckUpdatesUI));
    this->Connect(m_menuItemAbout->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpAbout));
    this->Connect(m_menuItemHelpManual->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpManual));
    this->Connect(m_menuItemHelpGetAssistance->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelp));
    
    
    m_ckboxSNR->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(TopFrame::OnCheckSNRClick), NULL, this);

    m_audioRecord->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnRecord), NULL, this);

    m_logQSO->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnLogQSO), NULL, this);
    
    m_togBtnOnOff->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnOnOff), NULL, this);
    m_togBtnAnalog->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnAnalogClick), NULL, this);
    m_togBtnVoiceKeyer->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnVoiceKeyerClick), NULL, this);
    m_btnTogPTT->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnPTT), NULL, this);

#if defined(__WXGTK__)
    // wxGTK fires wxEVT_CONTEXT_MENU on button-press for these widgets,
    // causing GTK to dismiss PopupMenu on button release; use RIGHT_UP
    // instead. MSW/OSX are unaffected (MSW generates the event on
    // button-up already; OSX uses ctrl-click), so this is GTK-specific.
    // Confirmed present on both wxGTK 3.2 and 3.3+, unlike the windowless
    // widget case below, so no version gate here.
    m_togBtnVoiceKeyer->Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent&) { wxContextMenuEvent ctx; OnTogBtnVoiceKeyerRightClick(ctx); });
    m_btnTogPTT->Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent&) { wxContextMenuEvent ctx; OnTogBtnPTTRightClick(ctx); });
#else
    m_togBtnVoiceKeyer->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTogBtnVoiceKeyerRightClick), NULL, this);
    m_btnTogPTT->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTogBtnPTTRightClick), NULL, this);
#endif

    m_BtnBerReset->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnBerReset), NULL, this);

        
    m_btnTxLevelMM->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTxLevelDecrBig), NULL, this);
    m_btnTxLevelM->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTxLevelDecr), NULL, this);
    m_btnTxLevelP->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTxLevelIncr), NULL, this);
    m_btnTxLevelPP->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTxLevelIncrBig), NULL, this);

    m_txLevelBox->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(TopFrame::OnTxLevelMouseWheel), NULL, this);
    m_txtTxLevelNum->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(TopFrame::OnTxLevelMouseWheel), NULL, this);
    m_btnTxLevelMM->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(TopFrame::OnTxLevelMouseWheel), NULL, this);
    m_btnTxLevelM->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(TopFrame::OnTxLevelMouseWheel), NULL, this);
    m_btnTxLevelP->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(TopFrame::OnTxLevelMouseWheel), NULL, this);
    m_btnTxLevelPP->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(TopFrame::OnTxLevelMouseWheel), NULL, this);
    m_btnTogTune->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(TopFrame::OnTxLevelMouseWheel), NULL, this);

#if wxCHECK_VERSION(3, 3, 0) && defined(__WXGTK__)
    // wxGTK 3.3+ fires wxEVT_CONTEXT_MENU on button-press for these widget types,
    // causing GTK to dismiss PopupMenu on button release; use RIGHT_UP instead.
    // MSW/OSX are unaffected (MSW generates the event on button-up already;
    // OSX uses ctrl-click), so this is GTK-specific.
    m_txLevelBox->Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent&) { wxContextMenuEvent ctx; OnTxLevelContextMenu(ctx); });
    m_txtTxLevelNum->Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent&) { wxContextMenuEvent ctx; OnTxLevelContextMenu(ctx); });
    m_btnTogTune->Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent&) { wxContextMenuEvent ctx; OnTuneAttenContextMenu(ctx); });
#else
    // wxGTK < 3.3 does not generate RIGHT_UP for windowless widget types
    // (wxStaticBox, wxStaticText); CONTEXT_MENU works without the dismiss issue.
    // (Also used as-is on MSW/OSX regardless of wx version -- see above.)
    m_txLevelBox->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTxLevelContextMenu), NULL, this);
    m_txtTxLevelNum->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTxLevelContextMenu), NULL, this);
    m_btnTogTune->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTuneAttenContextMenu), NULL, this);
#endif

    m_sliderMicSpkrLevel->Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Connect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Connect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Connect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Connect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Connect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Connect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Connect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(TopFrame::OnResetMicSpkrLevel), NULL, this);
    m_txtMicSpkrLevelNum->Connect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(TopFrame::OnResetMicSpkrLevel), NULL, this);

    m_cboReportFrequency->Connect(wxEVT_TEXT_ENTER, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_TEXT, wxCommandEventHandler(TopFrame::OnChangeReportFrequencyVerify), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_COMBOBOX, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencySetFocus), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_KILL_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencyKillFocus), NULL, this);
    
    m_cboLastReportedCallsigns->Connect(wxEVT_COMBOBOX_DROPDOWN, wxCommandEventHandler(TopFrame::OnOpenCallsignList), NULL, this);
    m_cboLastReportedCallsigns->Connect(wxEVT_COMBOBOX_CLOSEUP, wxCommandEventHandler(TopFrame::OnCloseCallsignList), NULL, this);
    m_cboLastReportedCallsigns->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(TopFrame::OnRightClickCallsignList), NULL, this);

    m_reporterHidden->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnToggleReporterVisibility), NULL, this);

    m_btnTogTune->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnTune), NULL, this);
}

TopFrame::~TopFrame()
{
    //-------------------
    // Disconnect Events
    //-------------------   
    this->Disconnect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(TopFrame::topFrame_OnClose));
    this->Disconnect(wxEVT_PAINT, wxPaintEventHandler(TopFrame::topFrame_OnPaint));
    this->Disconnect(wxEVT_SIZE, wxSizeEventHandler(TopFrame::topFrame_OnSize));
    this->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::topFrame_OnUpdateUI));
    this->Disconnect(wxEVT_ACTIVATE, wxActivateEventHandler(TopFrame::OnActivateWindow));
    this->Disconnect(ID_EXIT, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnExit));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsSetupWizard));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsSetupWizardUI));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsFilter));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsFilterUI));
    this->Disconnect(wxID_PREFERENCES, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsOptions));
    this->Disconnect(wxID_PREFERENCES, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsOptionsUI));

    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnPlayFileFromRadio));

    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsExportConfig));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsExportConfigUI));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsImportConfig));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsImportConfigUI));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsLoadDefaultConfig));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsLoadDefaultConfigUI));

    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpCheckUpdates));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnHelpCheckUpdatesUI));
    this->Disconnect(ID_ABOUT, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpAbout));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpManual));
    
    m_togBtnOnOff->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnOnOff), NULL, this);
    m_togBtnAnalog->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnAnalogClick), NULL, this);
    m_togBtnVoiceKeyer->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnVoiceKeyerClick), NULL, this);
    m_btnTogPTT->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnPTT), NULL, this);
#if !defined(__WXGTK__)
    m_togBtnVoiceKeyer->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTogBtnVoiceKeyerRightClick), NULL, this);
    m_btnTogPTT->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTogBtnPTTRightClick), NULL, this);
#endif

    m_audioRecord->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnRecord), NULL, this);
    
    m_logQSO->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnLogQSO), NULL, this);
        
    m_btnTxLevelMM->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTxLevelDecrBig), NULL, this);
    m_btnTxLevelM->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTxLevelDecr), NULL, this);
    m_btnTxLevelP->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTxLevelIncr), NULL, this);
    m_btnTxLevelPP->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTxLevelIncrBig), NULL, this);
#if !(wxCHECK_VERSION(3, 3, 0) && defined(__WXGTK__))
    m_txLevelBox->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTxLevelContextMenu), NULL, this);
    m_txtTxLevelNum->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTxLevelContextMenu), NULL, this);
    m_btnTogTune->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTuneAttenContextMenu), NULL, this);
#endif

    m_sliderMicSpkrLevel->Disconnect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Disconnect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Disconnect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Disconnect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Disconnect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Disconnect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Disconnect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Disconnect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Disconnect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Disconnect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeMicSpkrLevel), NULL, this);
    m_sliderMicSpkrLevel->Disconnect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(TopFrame::OnResetMicSpkrLevel), NULL, this);
    m_txtMicSpkrLevelNum->Disconnect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(TopFrame::OnResetMicSpkrLevel), NULL, this);
    
    m_cboReportFrequency->Disconnect(wxEVT_TEXT_ENTER, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    m_cboReportFrequency->Disconnect(wxEVT_TEXT, wxCommandEventHandler(TopFrame::OnChangeReportFrequencyVerify), NULL, this);
    m_cboReportFrequency->Disconnect(wxEVT_COMBOBOX, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    
    m_cboReportFrequency->Disconnect(wxEVT_SET_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencySetFocus), NULL, this);
    m_cboReportFrequency->Disconnect(wxEVT_KILL_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencyKillFocus), NULL, this);

    m_cboLastReportedCallsigns->Disconnect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(TopFrame::OnRightClickCallsignList), NULL, this);
    m_cboLastReportedCallsigns->Disconnect(wxEVT_COMBOBOX_DROPDOWN, wxCommandEventHandler(TopFrame::OnOpenCallsignList), NULL, this);
    m_cboLastReportedCallsigns->Disconnect(wxEVT_COMBOBOX_CLOSEUP, wxCommandEventHandler(TopFrame::OnCloseCallsignList), NULL, this);

    m_reporterHidden->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnToggleReporterVisibility), NULL, this);

    m_btnTogTune->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnTune), NULL, this);
}

void TopFrame::setVoiceKeyerButtonLabel_(wxString filename)
{
    wxString vkLabel = _("Start Voice &Keyer");
    int vkLabelWidth = 0;
    int filenameWidth = 0;
    int tmp = 0;
    
    wxSize buttonSize = m_togBtnVoiceKeyer->GetSize();
    vkLabelWidth = buttonSize.GetWidth() * 0.95;
    m_togBtnVoiceKeyer->GetTextExtent(filename, &filenameWidth, &tmp);
        
    // Truncate filename as required to ensure button isn't made wider than needed.
    bool isTruncated = false;
    while (filename.size() > 1 && filenameWidth > vkLabelWidth)
    {
        isTruncated = true;
        filename = filename.Mid(0, filename.size() - 1);
        
        wxString tmpString = filename + _("...");
        m_togBtnVoiceKeyer->GetTextExtent(tmpString, &filenameWidth, &tmp);
    }
    
    if (filename.size() > 0)
    {
        m_togBtnVoiceKeyer->SetLabel(vkLabel + _("\n") + filename + (isTruncated ? _("...") : _("")));
    }
    else
    {
        m_togBtnVoiceKeyer->SetLabel(vkLabel);
    }
    
    // Resize button height as needed.
    wxSize currentSize = m_togBtnVoiceKeyer->GetSize();
    wxSize bestSize = m_togBtnVoiceKeyer->GetBestSize();
    currentSize.SetHeight(bestSize.GetHeight());
    m_togBtnVoiceKeyer->SetSize(currentSize);
    m_togBtnVoiceKeyer->Refresh();
    
    // XXX - wxWidgets doesn't handle button height changes properly until the user resizes 
    // the window (even if only by a pixel or two). As a really hacky workaround, we 
    // emulate this behavior when changing the button height.
    wxSize winSize = GetSize();
    SetSize(winSize.GetWidth(), winSize.GetHeight());
    SetSize(winSize.GetWidth(), winSize.GetHeight() - 1);
    SetSize(winSize.GetWidth(), winSize.GetHeight());
        
    log_info("Set voice keyer button label to %s", (const char*)m_togBtnVoiceKeyer->GetLabel().ToUTF8());
}
