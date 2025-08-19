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

#include <wx/regex.h>
#include <wx/wrapsizer.h>
#include <wx/aui/tabmdi.h>

#include "topFrame.h"
#include "gui/util/NameOverrideAccessible.h"
#include "gui/util/LabelOverrideAccessible.h"
#include "util/logging/ulog.h"

extern int g_playFileToMicInEventId;
extern int g_recFileFromRadioEventId;
extern int g_playFileFromRadioEventId;
extern int g_recFileFromModulatorEventId;
extern int g_txLevel;

// THIS IS VERY MUCH A HACK! wxTabFrame is not in the public interface and should
// not be here, even named as something else. Unfortunately this is needed to get 
// the tab state loaded and saved. Here's hoping this interface remains stable.
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
  
       // add tab id's
       size_t page_count = tabframe->m_tabs->GetPageCount();
       for (size_t p = 0; p < page_count; ++p)
       {
          wxAuiNotebookPage& page = tabframe->m_tabs->GetPage(p);
          const size_t page_idx = m_tabs.GetIdxFromWindow(page.window);
     
          if (p) tabs += wxT(",");

          if ((int)page_idx == m_curPage) tabs += wxT("*");
          else if ((int)p == tabframe->m_tabs->GetActivePage()) tabs += wxT("+");
          tabs += wxString::Format(wxT("%zu"), page_idx);
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

    wxString tabs = layout.BeforeFirst(wxT('@'));
    while (1)
     {
       const wxString tab_part = tabs.BeforeFirst(wxT('|'));
  
       // if the string is empty, we're done parsing
         if (tab_part.empty())
             break;

       // Get pane name
       const wxString pane_name = tab_part.BeforeFirst(wxT('='));

       // create a new tab frame
       wxTabFrameOurs* new_tabs = new wxTabFrameOurs();
       new_tabs->m_tabs = new wxAuiTabCtrl(this,
                                  m_tabIdCounter++);
 //                            wxDefaultPosition,
 //                            wxDefaultSize,
 //                            wxNO_BORDER|wxWANTS_CHARS);
       new_tabs->m_tabs->SetArtProvider(m_tabs.GetArtProvider()->Clone());
       new_tabs->m_tabCtrlHeight = m_tabCtrlHeight;
       new_tabs->m_tabs->SetFlags(m_flags);
       wxAuiTabCtrl *dest_tabs = new_tabs->m_tabs;

       // create a pane info structure with the information
       // about where the pane should be added
       wxAuiPaneInfo pane_info = wxAuiPaneInfo().Name(pane_name).Bottom().CaptionVisible(false);
       m_mgr.AddPane(new_tabs, pane_info);

       // Get list of tab id's and move them to pane
       wxString tab_list = tab_part.AfterFirst(wxT('='));
       size_t activePage = -1;
       while(1) {
          wxString tab = tab_list.BeforeFirst(wxT(','));
          if (tab.empty()) break;
          tab_list = tab_list.AfterFirst(wxT(','));

          // Check if this page has an 'active' marker
          const wxChar c = tab[0];
          if (c == wxT('+') || c == wxT('*')) {
             tab = tab.Mid(1); 
          }

          const size_t tab_idx = wxAtoi(tab.c_str());
          if (tab_idx >= GetPageCount()) continue;

          // Move tab to pane
          wxAuiNotebookPage& page = m_tabs.GetPage(tab_idx);
          const size_t newpage_idx = dest_tabs->GetPageCount();
          dest_tabs->InsertPage(page.window, page, newpage_idx);

          if (c == wxT('+')) activePage = newpage_idx;
          else if ( c == wxT('*')) sel_page = tab_idx;
       }
       if (activePage >= 0) dest_tabs->SetActivePage(activePage);
       dest_tabs->DoShowHide();

       tabs = tabs.AfterFirst(wxT('|'));
    }

    // Load the frame perspective
    const wxString frames = layout.AfterFirst(wxT('@'));
    m_mgr.LoadPerspective(frames);

    // Force refresh of selection
    m_curPage = -1;
    SetSelection(sel_page);

    return true;
}

//=========================================================================
// Code that lays out the main application window
//=========================================================================
TopFrame::TopFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxFrame(parent, id, title, pos, size, style)
{
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
    
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
    m_menuItemExit = new wxMenuItem(file, ID_EXIT, wxString(_("E&xit")) , _("Exit Program"), wxITEM_NORMAL);
    file->Append(m_menuItemExit);

    m_menubarMain->Append(file, _("&File"));

    tools = new wxMenu();
    wxMenuItem* m_menuItemEasySetup;
    m_menuItemEasySetup = new wxMenuItem(tools, wxID_ANY, wxString(_("&Easy Setup...")) , _("Simplified setup of FreeDV"), wxITEM_NORMAL);
    tools->Append(m_menuItemEasySetup);
    
    wxMenuItem* m_menuItemFreeDVReporter;
    m_menuItemFreeDVReporter = new wxMenuItem(tools, wxID_ANY, wxString(_("FreeDV R&eporter")) , _("Opens browser window and displays FreeDV Reporter service."), wxITEM_NORMAL);
    tools->Append(m_menuItemFreeDVReporter);
    
    wxMenuItem* toolsSeparator1 = new wxMenuItem(tools, wxID_SEPARATOR);
    tools->Append(toolsSeparator1);
    
    wxMenuItem* m_menuItemAudio;
    m_menuItemAudio = new wxMenuItem(tools, wxID_ANY, wxString(_("&Audio Config...")) , _("Configures sound cards for FreeDV"), wxITEM_NORMAL);
    tools->Append(m_menuItemAudio);

    wxMenuItem* m_menuItemRigCtrlCfg;
    m_menuItemRigCtrlCfg = new wxMenuItem(tools, wxID_ANY, wxString(_("CAT and P&TT Config...")) , _("Configures FreeDV integration with radio"), wxITEM_NORMAL);
    tools->Append(m_menuItemRigCtrlCfg);

    wxMenuItem* m_menuItemOptions;
    m_menuItemOptions = new wxMenuItem(tools, wxID_ANY, wxString(_("&Options...")) , _("Miscellaneous FreeDV configuration options"), wxITEM_NORMAL);
    tools->Append(m_menuItemOptions);

    wxMenuItem* m_menuItemFilter;
    m_menuItemFilter = new wxMenuItem(tools, wxID_ANY, wxString(_("&Filter...")) , _("Configures audio filtering"), wxITEM_NORMAL);
    tools->Append(m_menuItemFilter);
    
    wxMenuItem* toolsSeparator2 = new wxMenuItem(tools, wxID_SEPARATOR);
    tools->Append(toolsSeparator2);

    m_menuItemRecFileFromRadio = new wxMenuItem(tools, wxID_ANY, wxString(_("Start &Record File - From Radio...")) , _("Records incoming audio from the attached radio"), wxITEM_NORMAL);
    g_recFileFromRadioEventId = m_menuItemRecFileFromRadio->GetId();
    tools->Append(m_menuItemRecFileFromRadio);

    m_menuItemPlayFileFromRadio = new wxMenuItem(tools, wxID_ANY, wxString(_("Start &Play File - From Radio...")) , _("Pipes radio sound input from file"), wxITEM_NORMAL);
    g_playFileFromRadioEventId = m_menuItemPlayFileFromRadio->GetId();
    tools->Append(m_menuItemPlayFileFromRadio);
    
    m_menubarMain->Append(tools, _("&Tools"));

    help = new wxMenu();
    wxMenuItem* m_menuItemHelpUpdates;
    m_menuItemHelpUpdates = new wxMenuItem(help, wxID_ANY, wxString(_("&Check for Updates")) , _("Checks for updates to FreeDV"), wxITEM_NORMAL);
    help->Append(m_menuItemHelpUpdates);
    m_menuItemHelpUpdates->Enable(false);
    
    wxMenuItem* m_menuItemAbout;
    m_menuItemAbout = new wxMenuItem(help, ID_ABOUT, wxString(_("&About...")) , _("About this program"), wxITEM_NORMAL);
    help->Append(m_menuItemAbout);

    wxMenuItem* m_menuItemHelpManual;
    m_menuItemHelpManual = new wxMenuItem(help, wxID_ANY, wxString(_("&User Manual...")), _("Loads the user manual"), wxITEM_NORMAL);
    help->Append(m_menuItemHelpManual);
        
    m_menubarMain->Append(help, _("&Help"));

    this->SetMenuBar(m_menubarMain);

    m_panel = new wxPanel(this);

    wxBoxSizer* bSizer1;
    bSizer1 = new wxBoxSizer(wxHORIZONTAL);

    //=====================================================
    // Left side
    //=====================================================
    wxSizer* leftOuterSizer = new wxBoxSizer(wxVERTICAL);
    wxSizer* leftSizer = new wxWrapSizer(wxVERTICAL, wxREMOVE_LEADING_SPACES);

    wxStaticBoxSizer* snrSizer;
    wxStaticBox* snrBox = new wxStaticBox(m_panel, wxID_ANY, _("SNR"), wxDefaultPosition, wxSize(100,-1));
    snrSizer = new wxStaticBoxSizer(snrBox, wxVERTICAL);

    //------------------------------
    // S/N ratio Gauge (vert. bargraph)
    //------------------------------
    m_gaugeSNR = new wxGauge(snrBox, wxID_ANY, 45, wxDefaultPosition, wxSize(135,15), wxGA_SMOOTH);
    m_gaugeSNR->SetToolTip(_("Displays signal to noise ratio in dB."));
    snrSizer->Add(m_gaugeSNR, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 10);

    //------------------------------
    // Box for S/N ratio (Numeric)
    //------------------------------
    m_textSNR = new wxStaticText(snrBox, wxID_ANY, wxT("--"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    m_textSNR->SetMinSize(wxSize(70,-1));
    snrSizer->Add(m_textSNR, 0, wxALIGN_CENTER_HORIZONTAL, 1);

    //------------------------------
    // S/N ratio slow Checkbox
    //------------------------------
    m_ckboxSNR = new wxCheckBox(snrBox, wxID_ANY, _("Slow"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    m_ckboxSNR->SetToolTip(_("Smooth but slow SNR estimation"));
    snrSizer->Add(m_ckboxSNR, 0, wxALIGN_CENTER_HORIZONTAL, 5);

    leftSizer->Add(snrSizer, 0, wxEXPAND|wxALL, 2);

    //------------------------------
    // Signal Level(vert. bargraph)
    //------------------------------
    wxStaticBoxSizer* levelSizer;
    wxStaticBox* levelBox = new wxStaticBox(m_panel, wxID_ANY, _("Level"), wxDefaultPosition, wxSize(100,-1));
    levelSizer = new wxStaticBoxSizer(levelBox, wxVERTICAL);
    
    m_textLevel = new wxStaticText(levelBox, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    m_textLevel->SetForegroundColour(wxColour(255,0,0));
    levelSizer->Add(m_textLevel, 0, wxALIGN_CENTER_HORIZONTAL, 1);

    m_gaugeLevel = new wxGauge(levelBox, wxID_ANY, 100, wxDefaultPosition, wxSize(135,15), wxGA_SMOOTH);
    m_gaugeLevel->SetToolTip(_("Peak of From Radio in Rx, or peak of From Mic in Tx mode.  If Red you should reduce your levels"));
    levelSizer->Add(m_gaugeLevel, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 10);

    leftSizer->Add(levelSizer, 0, wxALL|wxEXPAND, 2);
    
    //------------------------------
    // Sync  Indicator box
    //------------------------------
    wxStaticBoxSizer* sbSizer3_33;
    wxStaticBox* syncBox = new wxStaticBox(m_panel, wxID_ANY, _("Sync"), wxDefaultPosition, wxSize(100,-1));
    sbSizer3_33 = new wxStaticBoxSizer(syncBox, wxVERTICAL);

    m_textSync = new wxStaticText(syncBox, wxID_ANY, wxT("Modem"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    sbSizer3_33->Add(m_textSync, 0, wxALIGN_CENTER_HORIZONTAL, 1);
    m_textSync->Disable();

    m_textCurrentDecodeMode = new wxStaticText(syncBox, wxID_ANY, wxT("Mode: unk"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    sbSizer3_33->Add(m_textCurrentDecodeMode, 0, wxALIGN_CENTER_HORIZONTAL, 1);
    m_textCurrentDecodeMode->Disable();
    
    m_BtnReSync = new wxButton(syncBox, wxID_ANY, _("ReS&ync"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer3_33->Add(m_BtnReSync, 0, wxALL | wxALIGN_CENTRE, 5);
    
    m_btnCenterRx = new wxButton(syncBox, wxID_ANY, _("C&enter RX"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer3_33->Add(m_btnCenterRx, 0, wxALL | wxALIGN_CENTRE, 5);
    
    leftSizer->Add(sbSizer3_33,0, wxALL|wxEXPAND|wxFIXED_MINSIZE, 2);

    //------------------------------
    // Audio Recording/Playback
    //------------------------------
    wxStaticBox* audioBox = new wxStaticBox(m_panel, wxID_ANY, _("Audio"), wxDefaultPosition, wxSize(100,-1));
    wxStaticBoxSizer* sbSizerAudioRecordPlay = new wxStaticBoxSizer(audioBox, wxVERTICAL);
    
    m_audioRecord = new wxToggleButton(audioBox, wxID_ANY, _("Record"), wxDefaultPosition, wxDefaultSize, 0);
    m_audioRecord->SetToolTip(_("Records incoming over the air signals as well as anything transmitted."));
    sbSizerAudioRecordPlay->Add(m_audioRecord, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);
    
    leftSizer->Add(sbSizerAudioRecordPlay, 0, wxALL|wxEXPAND, 2);
    
    //------------------------------
    // BER Frames box
    //------------------------------

    wxStaticBoxSizer* sbSizer_ber;
    wxStaticBox* statsBox = new wxStaticBox(m_panel, wxID_ANY, _("Stats"), wxDefaultPosition, wxSize(100,-1));
    sbSizer_ber = new wxStaticBoxSizer(statsBox, wxVERTICAL);

    m_BtnBerReset = new wxButton(statsBox, wxID_ANY, _("&Reset"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_ber->Add(m_BtnBerReset, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    m_textBits = new wxStaticText(statsBox, wxID_ANY, wxT("Bits: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textBits, 0, wxALL | wxALIGN_LEFT, 1);
    m_textErrors = new wxStaticText(statsBox, wxID_ANY, wxT("Errs: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textErrors, 0, wxALL | wxALIGN_LEFT, 1);
    m_textBER = new wxStaticText(statsBox, wxID_ANY, wxT("BER: 0.0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textBER, 0, wxALL | wxALIGN_LEFT, 1);
    m_textResyncs = new wxStaticText(statsBox, wxID_ANY, wxT("Resyncs: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textResyncs, 0, wxALL | wxALIGN_LEFT, 1);
    m_textClockOffset = new wxStaticText(statsBox, wxID_ANY, wxT("ClkOff: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_textClockOffset->SetMinSize(wxSize(125,-1));
    sbSizer_ber->Add(m_textClockOffset, 0, wxALL | wxALIGN_LEFT, 1);
    m_textFreqOffset = new wxStaticText(statsBox, wxID_ANY, wxT("FreqOff: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textFreqOffset, 0, wxALL | wxALIGN_LEFT, 1);
    m_textSyncMetric = new wxStaticText(statsBox, wxID_ANY, wxT("Sync: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textSyncMetric, 0, wxALL | wxALIGN_LEFT, 1);
    m_textCodec2Var = new wxStaticText(statsBox, wxID_ANY, wxT("Var: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textCodec2Var, 0, wxALL | wxALIGN_LEFT, 1);

    leftSizer->Add(sbSizer_ber,0, wxALL|wxEXPAND|wxFIXED_MINSIZE, 2);

    //------------------------------
    // Help button: goes to Help page on website
    //------------------------------
    wxStaticBox* helpBox = new wxStaticBox(m_panel, wxID_ANY, _("Assistance"), wxDefaultPosition, wxSize(100,-1));
    wxStaticBoxSizer* helpSizer = new wxStaticBoxSizer(helpBox, wxVERTICAL);
    
    m_btnHelp = new wxButton(helpBox, wxID_ANY, _("Get Help"), wxDefaultPosition, wxDefaultSize, 0);
    m_btnHelp->SetToolTip(_("Get help with FreeDV."));
    helpSizer->Add(m_btnHelp, 0, wxALIGN_CENTER|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    leftSizer->SetMinSize(wxSize(-1, 375));
    
#if !wxCHECK_VERSION(3,2,0)
    leftOuterSizer->Add(leftSizer, 0, wxALL | wxEXPAND | wxFIXED_MINSIZE, 1);
#else
    leftOuterSizer->Add(leftSizer, 2, wxALL | wxEXPAND | wxFIXED_MINSIZE, 1);
#endif // !wxCHECK_VERSION(3,2,0)
    leftOuterSizer->Add(helpSizer, 0, wxFIXED_MINSIZE | wxALL | wxEXPAND, 1);

    bSizer1->Add(leftOuterSizer, 0, wxALL|wxEXPAND, 5);

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

    upperSizer->Add(m_auiNbookCtrl, 1, wxALIGN_TOP|wxEXPAND, 1);
    centerSizer->Add(upperSizer, 1, wxALIGN_TOP|wxEXPAND, 0);

    // lower middle used for user ID

    wxBoxSizer* lowerSizer;
    lowerSizer = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* modeStatusSizer;
    modeStatusSizer = new wxBoxSizer(wxVERTICAL);
    m_txtModeStatus = new wxStaticText(m_panel, wxID_ANY, wxT("unk"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_txtModeStatus->Enable(false); // enabled only if Hamlib is turned on
    m_txtModeStatus->SetMinSize(wxSize(80,-1));
    modeStatusSizer->Add(m_txtModeStatus, 0, wxALL|wxEXPAND, 1);
    lowerSizer->Add(modeStatusSizer, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);

    m_BtnCallSignReset = new wxButton(m_panel, wxID_ANY, _("&Clear"), wxDefaultPosition, wxDefaultSize, 0);
    lowerSizer->Add(m_BtnCallSignReset, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);

    wxBoxSizer* bSizer15;
    bSizer15 = new wxBoxSizer(wxVERTICAL);
    m_txtCtrlCallSign = new wxTextCtrl(m_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    m_txtCtrlCallSign->SetToolTip(_("Call Sign of transmitting station will appear here"));
    m_txtCtrlCallSign->SetSizeHints(wxSize(100,-1));

    m_cboLastReportedCallsigns = new wxComboCtrl(m_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxCB_READONLY);
    m_lastReportedCallsignListView = new wxListViewComboPopup();
    m_cboLastReportedCallsigns->SetPopupControl(m_lastReportedCallsignListView);
    m_cboLastReportedCallsigns->SetSizeHints(wxSize(100,-1));
    m_cboLastReportedCallsigns->SetPopupMaxHeight(150);
    
    m_lastReportedCallsignListView->InsertColumn(0, wxT("Callsign"), wxLIST_FORMAT_LEFT, 100);
    m_lastReportedCallsignListView->InsertColumn(1, wxT("Frequency"), wxLIST_FORMAT_RIGHT, 75);
    m_lastReportedCallsignListView->InsertColumn(2, wxT("Date/Time"), wxLIST_FORMAT_LEFT, 175);
    m_lastReportedCallsignListView->InsertColumn(3, wxT("SNR"), wxLIST_FORMAT_RIGHT, 50);

    bSizer15->Add(m_txtCtrlCallSign, 1, wxALL|wxEXPAND, 5);
    bSizer15->Add(m_cboLastReportedCallsigns, 1, wxALL|wxEXPAND, 5);

    lowerSizer->Add(bSizer15, 1, wxEXPAND, 5);
    lowerSizer->SetMinSize(wxSize(375,-1));
    centerSizer->Add(lowerSizer, 0, wxEXPAND, 2);
    centerSizer->SetMinSize(wxSize(375,375));
    bSizer1->Add(centerSizer, 1, wxALL|wxEXPAND, 1);
    
    //=====================================================
    // Right side
    //=====================================================
    rightSizer = new wxWrapSizer(wxVERTICAL, wxREMOVE_LEADING_SPACES);

    //=====================================================
    // Squelch Slider Control
    //=====================================================
    wxStaticBoxSizer* sbSizer3;
    wxStaticBox* squelchBox = new wxStaticBox(m_panel, wxID_ANY, _("S&quelch"), wxDefaultPosition, wxSize(100,-1));
    sbSizer3 = new wxStaticBoxSizer(squelchBox, wxVERTICAL);

    m_sliderSQ = new wxSlider(squelchBox, wxID_ANY, 0, 0, 40, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS);
    m_sliderSQ->SetMinSize(wxSize(135,-1));

    // Add accessibility class so that the values are read back correctly.
#if wxUSE_ACCESSIBILITY
    auto squelchSliderAccessibility = new LabelOverrideAccessible([&]() {
        return m_textSQ->GetLabel();
    });
    m_sliderSQ->SetAccessible(squelchSliderAccessibility);
#endif // wxUSE_ACCESSIBILITY

    sbSizer3->Add(m_sliderSQ, 1, wxALIGN_CENTER_HORIZONTAL, 0);

    //------------------------------
    // Squelch Level static text box
    //------------------------------
    m_textSQ = new wxStaticText(squelchBox, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    m_textSQ->SetMinSize(wxSize(100,-1));
    sbSizer3->Add(m_textSQ, 0, wxALIGN_CENTER_HORIZONTAL, 0);

    //------------------------------
    // Squelch Toggle Checkbox
    //------------------------------
    m_ckboxSQ = new wxCheckBox(squelchBox, wxID_ANY, _("Enable"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);

    sbSizer3->Add(m_ckboxSQ, 0, wxALIGN_CENTER_HORIZONTAL, 0);
    rightSizer->Add(sbSizer3, 0, wxALL | wxEXPAND, 2);

    // Transmit Level slider
    wxStaticBox* txLevelBox = new wxStaticBox(m_panel, wxID_ANY, _("Mod Level TX"), wxDefaultPosition, wxSize(100,-1));
    wxBoxSizer* txLevelSizer = new wxStaticBoxSizer(txLevelBox, wxVERTICAL);

   // g_txLevel ist in 0,1 dB -> Startwert in dB (gerundet) und auf [-30..20] klemmen
   int dBInit = (g_txLevel >= 0 ? (g_txLevel + 5) : (g_txLevel - 5)) / 10; // rundet ohne <cmath>
   if (dBInit < -30) dBInit = -30;
   else if (dBInit > 20) dBInit = 20;

    m_sliderTxLevel = new wxSlider(txLevelBox, wxID_ANY, dBInit, -30, 20, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS);
    m_sliderTxLevel->SetLineSize(1);  // optional
    m_sliderTxLevel->SetMinSize(wxSize(150,-1));
    txLevelSizer->Add(m_sliderTxLevel, 1, wxALIGN_CENTER_HORIZONTAL, 0);

#if wxUSE_ACCESSIBILITY 
    // Add accessibility class so that the values are read back correctly.
    auto txSliderAccessibility = new LabelOverrideAccessible([&]() {
        return m_txtTxLevelNum->GetLabel();
    });
    m_sliderTxLevel->SetAccessible(txSliderAccessibility);
#endif // wxUSE_ACCESSIBILITY
 
    m_txtTxLevelNum = new wxStaticText(txLevelBox, wxID_ANY, wxString::Format("%d dB", dBInit), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_txtTxLevelNum->SetMinSize(wxSize(100,-1));
    txLevelSizer->Add(m_txtTxLevelNum, 0, wxALIGN_CENTER_HORIZONTAL, 0);
    
    rightSizer->Add(txLevelSizer, 0, wxALL | wxEXPAND, 2);
    
    // Mic/Speaker Level slider
    wxStaticBox* micSpeakerBox = new wxStaticBox(m_panel, wxID_ANY, _("Mic/Spkr &Level"), wxDefaultPosition, wxSize(100,-1));
    wxBoxSizer* micSpeakerLevelSizer = new wxStaticBoxSizer(micSpeakerBox, wxVERTICAL);
    
    // NEU (1 dB Schritte, keine *10-Skalierung)
    m_sliderMicSpkrLevel = new wxSlider(micSpeakerBox, wxID_ANY, 0, -20, 20, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS);
    m_sliderMicSpkrLevel->SetLineSize(1);  // optional
    m_sliderMicSpkrLevel->SetMinSize(wxSize(150,-1));
    micSpeakerLevelSizer->Add(m_sliderMicSpkrLevel, 1, wxALIGN_CENTER_HORIZONTAL, 0);

#if wxUSE_ACCESSIBILITY 
    // Add accessibility class so that the values are read back correctly.
    auto micSpkrSliderAccessibility = new LabelOverrideAccessible([&]() {
        return m_txtMicSpkrLevelNum->GetLabel();
    });
    m_sliderMicSpkrLevel->SetAccessible(micSpkrSliderAccessibility);
#endif // wxUSE_ACCESSIBILITY
 
    m_txtMicSpkrLevelNum = new wxStaticText(micSpeakerBox, wxID_ANY, wxT("0 dB"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_txtMicSpkrLevelNum->SetMinSize(wxSize(100,-1));
    micSpeakerLevelSizer->Add(m_txtMicSpkrLevelNum, 0, wxALIGN_CENTER_HORIZONTAL, 0);
    
    rightSizer->Add(micSpeakerLevelSizer, 0, wxALL | wxEXPAND, 2);
    
    //------------------------------
    // Mode box
    //------------------------------
    modeBox = new wxStaticBox(m_panel, wxID_ANY, _("&Mode"), wxDefaultPosition, wxSize(100,-1));
    sbSizer_mode = new wxStaticBoxSizer(modeBox, wxVERTICAL);

    m_rbRADE = new wxRadioButton( modeBox, wxID_ANY, wxT("RADEV1"), wxDefaultPosition, wxDefaultSize,  wxRB_GROUP);
    sbSizer_mode->Add(m_rbRADE, 0, wxALIGN_LEFT|wxALL, 1); 
    m_rb700d = new wxRadioButton( modeBox, wxID_ANY, wxT("700D"), wxDefaultPosition, wxDefaultSize,  0);
    sbSizer_mode->Add(m_rb700d, 0, wxALIGN_LEFT|wxALL, 1);
    m_rb700e = new wxRadioButton( modeBox, wxID_ANY, wxT("700E"), wxDefaultPosition, wxDefaultSize,  0);
    sbSizer_mode->Add(m_rb700e, 0, wxALIGN_LEFT|wxALL, 1);
    m_rb1600 = new wxRadioButton( modeBox, wxID_ANY, wxT("1600"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_mode->Add(m_rb1600, 0, wxALIGN_LEFT|wxALL, 1);

    rightSizer->Add(sbSizer_mode,0, wxALL | wxEXPAND, 2);

    //=====================================================
    // Control Toggles box
    //=====================================================
    wxStaticBoxSizer* sbSizer5;
    wxStaticBox* controlBox = new wxStaticBox(m_panel, wxID_ANY, _("Control"), wxDefaultPosition, wxSize(100,-1));
    sbSizer5 = new wxStaticBoxSizer(controlBox, wxVERTICAL);

    //-------------------------------
    // Stop/Stop signal processing (rx and tx)
    //-------------------------------
    m_togBtnOnOff = new wxToggleButton(controlBox, wxID_ANY, _("&Start"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnOnOff->SetToolTip(_("Begin/End receiving data."));
    sbSizer5->Add(m_togBtnOnOff, 0, wxALL | wxEXPAND, 5);

    //------------------------------
    // Analog Passthrough Toggle
    //------------------------------
    m_togBtnAnalog = new wxToggleButton(controlBox, wxID_ANY, _("A&nalog"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnAnalog->SetToolTip(_("Toggle analog/digital operation."));
    sbSizer5->Add(m_togBtnAnalog, 0, wxALL | wxEXPAND, 5);

    //------------------------------
    // Voice Keyer Toggle
    //------------------------------
    m_togBtnVoiceKeyer = new wxToggleButton(controlBox, wxID_ANY, _("Voice &Keyer"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnVoiceKeyer->SetToolTip(_("Toggle Voice Keyer. Right-click for additional options."));
    sbSizer5->Add(m_togBtnVoiceKeyer, 0, wxALL | wxEXPAND, 5);

    //------------------------------
    // PTT button: Toggle Transmit/Receive mode
    //------------------------------
    m_btnTogPTT = new wxToggleButton(controlBox, wxID_ANY, _("&PTT"), wxDefaultPosition, wxDefaultSize, 0);
    m_btnTogPTT->SetToolTip(_("Push to Talk - Switch between Receive and Transmit. Right-click for additional options."));
    sbSizer5->Add(m_btnTogPTT, 0, wxALL | wxEXPAND, 5);

    rightSizer->Add(sbSizer5, 0, wxALL|wxEXPAND, 2);

    // Frequency text field (PSK Reporter)
    m_freqBox = new wxStaticBox(m_panel, wxID_ANY, _("Report Freq. (MHz)"), wxDefaultPosition, wxSize(100,-1));

    wxBoxSizer* reportFrequencySizer = new wxStaticBoxSizer(m_freqBox, wxHORIZONTAL);
    
    //wxStaticText* reportFrequencyUnits = new wxStaticText(m_freqBox, wxID_ANY, wxT(" MHz"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    wxBoxSizer* txtReportFreqSizer = new wxBoxSizer(wxVERTICAL);
    
    m_cboReportFrequency = new wxComboBox(m_freqBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxTE_PROCESS_ENTER);
    m_cboReportFrequency->SetMinSize(wxSize(150,-1));
    txtReportFreqSizer->Add(m_cboReportFrequency, 1, wxALL, 5);
    
    reportFrequencySizer->Add(txtReportFreqSizer, 1, wxEXPAND, 1);
    //reportFrequencySizer->Add(reportFrequencyUnits, 0, wxALIGN_CENTER_VERTICAL, 1);
    
    rightSizer->Add(reportFrequencySizer, 0, wxALL, 2);
        
    bSizer1->Add(rightSizer, 0, wxALL|wxEXPAND, 3);
    
    m_panel->SetSizerAndFit(bSizer1);
    this->Layout();

    m_statusBar1 = this->CreateStatusBar(1, wxSTB_DEFAULT_STYLE, wxID_ANY);

    //=====================================================
    // End of layout
    //=====================================================
    
    //-------------------
    // Tab ordering for accessibility
    //-------------------
    m_auiNbookCtrl->MoveBeforeInTabOrder(m_BtnCallSignReset);
    m_sliderSQ->MoveBeforeInTabOrder(m_ckboxSQ);
    m_rbRADE->MoveBeforeInTabOrder(m_rb700d);
    m_rb700d->MoveBeforeInTabOrder(m_rb700e);
    m_rb700e->MoveBeforeInTabOrder(m_rb1600);
    
    m_togBtnOnOff->MoveBeforeInTabOrder(m_togBtnAnalog);
    m_togBtnAnalog->MoveBeforeInTabOrder(m_togBtnVoiceKeyer);
    m_togBtnVoiceKeyer->MoveBeforeInTabOrder(m_btnTogPTT);
    
    //-------------------
    // Connect Events
    //-------------------
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(TopFrame::topFrame_OnClose));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(TopFrame::topFrame_OnPaint));
    this->Connect(wxEVT_SIZE, wxSizeEventHandler(TopFrame::topFrame_OnSize));
    this->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::topFrame_OnUpdateUI));
    this->Connect(wxEVT_SYS_COLOUR_CHANGED, wxSysColourChangedEventHandler(TopFrame::OnSystemColorChanged));
    this->Connect(m_menuItemExit->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnExit));

    this->Connect(m_menuItemEasySetup->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsEasySetup));
    this->Connect(m_menuItemEasySetup->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsEasySetupUI));
    this->Connect(m_menuItemFreeDVReporter->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsFreeDVReporter));
    this->Connect(m_menuItemFreeDVReporter->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsFreeDVReporterUI));
    this->Connect(m_menuItemAudio->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsAudio));
    this->Connect(m_menuItemAudio->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsAudioUI));
    this->Connect(m_menuItemFilter->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsFilter));
    this->Connect(m_menuItemFilter->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsFilterUI));
    this->Connect(m_menuItemRigCtrlCfg->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsComCfg));
    this->Connect(m_menuItemRigCtrlCfg->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsComCfgUI));
    this->Connect(m_menuItemOptions->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsOptions));
    this->Connect(m_menuItemOptions->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsOptionsUI));

    this->Connect(m_menuItemRecFileFromRadio->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnRecFileFromRadio));
    this->Connect(m_menuItemPlayFileFromRadio->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnPlayFileFromRadio));

    this->Connect(m_menuItemHelpUpdates->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpCheckUpdates));
    this->Connect(m_menuItemHelpUpdates->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnHelpCheckUpdatesUI));
    this->Connect(m_menuItemAbout->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpAbout));
    this->Connect(m_menuItemHelpManual->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpManual));
    m_sliderSQ->Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_ckboxSQ->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(TopFrame::OnCheckSQClick), NULL, this);

    m_ckboxSNR->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(TopFrame::OnCheckSNRClick), NULL, this);

    m_audioRecord->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnRecord), NULL, this);
    
    m_togBtnOnOff->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnOnOff), NULL, this);
    m_togBtnAnalog->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnAnalogClick), NULL, this);
    m_togBtnVoiceKeyer->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnVoiceKeyerClick), NULL, this);
    m_togBtnVoiceKeyer->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTogBtnVoiceKeyerRightClick), NULL, this);
    m_btnTogPTT->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnPTT), NULL, this);
    m_btnTogPTT->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTogBtnPTTRightClick), NULL, this);
    m_btnHelp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnHelp), NULL, this);

    m_BtnCallSignReset->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnCallSignReset), NULL, this);
    m_BtnBerReset->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnBerReset), NULL, this);
    m_BtnReSync->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnReSync), NULL, this);
    m_btnCenterRx->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnCenterRx), NULL, this);
   
    m_rbRADE->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this); 
    m_rb1600->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb700d->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb700e->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
        
    m_sliderTxLevel->Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    
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
    
    m_cboReportFrequency->Connect(wxEVT_TEXT_ENTER, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_TEXT, wxCommandEventHandler(TopFrame::OnChangeReportFrequencyVerify), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_COMBOBOX, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencySetFocus), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_KILL_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencyKillFocus), NULL, this);
    
    m_auiNbookCtrl->Connect(wxEVT_AUINOTEBOOK_PAGE_CHANGING, wxAuiNotebookEventHandler(TopFrame::OnNotebookPageChanging), NULL, this);
}

TopFrame::~TopFrame()
{
    //-------------------
    // Disconnect Events
    //-------------------
    m_auiNbookCtrl->Disconnect(wxEVT_AUINOTEBOOK_PAGE_CHANGING, wxAuiNotebookEventHandler(TopFrame::OnNotebookPageChanging), NULL, this);
    
    this->Disconnect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(TopFrame::topFrame_OnClose));
    this->Disconnect(wxEVT_PAINT, wxPaintEventHandler(TopFrame::topFrame_OnPaint));
    this->Disconnect(wxEVT_SIZE, wxSizeEventHandler(TopFrame::topFrame_OnSize));
    this->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::topFrame_OnUpdateUI));
    this->Disconnect(ID_EXIT, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnExit));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsEasySetup));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsEasySetupUI));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsAudio));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsAudioUI));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsFilter));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsFilterUI));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsComCfg));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsComCfgUI));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnToolsOptions));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnToolsOptionsUI));

    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnRecFileFromRadio));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnPlayFileFromRadio));
    
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpCheckUpdates));
    this->Disconnect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnHelpCheckUpdatesUI));
    this->Disconnect(ID_ABOUT, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpAbout));
    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnHelpManual));
    
    m_sliderSQ->Disconnect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_ckboxSQ->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(TopFrame::OnCheckSQClick), NULL, this);

    m_togBtnOnOff->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnOnOff), NULL, this);
    m_togBtnAnalog->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnAnalogClick), NULL, this);
    m_togBtnVoiceKeyer->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnVoiceKeyerClick), NULL, this);
    m_togBtnVoiceKeyer->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTogBtnVoiceKeyerRightClick), NULL, this);
    m_btnTogPTT->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnPTT), NULL, this);
    m_btnTogPTT->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(TopFrame::OnTogBtnPTTRightClick), NULL, this);
    m_btnHelp->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnHelp), NULL, this);
    
    m_btnCenterRx->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnCenterRx), NULL, this);

    m_audioRecord->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnRecord), NULL, this);

    m_rbRADE->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb1600->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb700d->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb700e->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
        
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    
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
    
    m_cboReportFrequency->Disconnect(wxEVT_TEXT_ENTER, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    m_cboReportFrequency->Disconnect(wxEVT_TEXT, wxCommandEventHandler(TopFrame::OnChangeReportFrequencyVerify), NULL, this);
    m_cboReportFrequency->Disconnect(wxEVT_COMBOBOX, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    
    m_cboReportFrequency->Disconnect(wxEVT_SET_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencySetFocus), NULL, this);
    m_cboReportFrequency->Disconnect(wxEVT_KILL_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencyKillFocus), NULL, this);
}

void TopFrame::setVoiceKeyerButtonLabel_(wxString filename)
{
    wxString vkLabel = _("Voice Keyer");
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
