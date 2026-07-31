//==========================================================================
// Name:            topFrame.h
//
// Purpose:         Implements simple wxWidgets application with GUI.
// Created:         Apr. 9, 2012
// Authors:         David Rowe, David Witten
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================
#ifndef __TOPFRAME_H__
#define __TOPFRAME_H__

#include "git_version.h"
#include <wx/version.h>
#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
#include <wx/string.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/menu.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/gauge.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/aui/auibook.h>
#include <wx/tglbtn.h>
#include <wx/slider.h>
#include <wx/checkbox.h>
#include <wx/frame.h>
#include <wx/infobar.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/radiobut.h>
#include <wx/combobox.h>
#include <wx/panel.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/listctrl.h>
#include <wx/collpane.h>
#include <wx/combo.h>

#include "gui/util/wxListViewComboPopup.h"

///////////////////////////////////////////////////////////////////////////

#define ID_OPEN 1000
#define ID_SAVE 1001
#define ID_CLOSE 1002
#define ID_EXIT 1003
#define ID_COPY 1004
#define ID_CUT 1005
#define ID_PASTE 1006
#define ID_OPTIONS 1007
#define ID_ABOUT 1008

#define ID_MODE_COLLAPSE 1100

// Base ID for the main window's "Show" menu's group-box visibility toggle
// items; 8 consecutive IDs from here (see OnShowGroupBox).
#define ID_SHOW_GROUPBOX_BASE 1200

class wxListViewComboPopup;

// Popup position for a right-click menu anchored to the left of btn (to
// avoid running off the right screen edge on X11). On GTK/Wayland, GDK's
// own popup placement already avoids screen edges correctly, and this
// manual offset doesn't translate to Wayland's surface-relative anchor
// model (it ends up placed at the toplevel's origin instead), so this
// returns wxDefaultPosition there and lets GTK position it automatically.
wxPoint LeftOffsetContextMenuPosition(wxWindow* btn);

// A self-contained "card" that stands in for a wxStaticBox/wxStaticBoxSizer
// group, with a tinted background that covers the whole box including its
// title. A native wxStaticBox paints its title directly on the border,
// outside the area any child window can draw into, and largely ignores
// SetBackgroundColour on Windows/macOS, so neither tinting the box directly
// nor nesting a panel inside it can cover the whole group cleanly. Add
// children to this panel (as their parent) and to GetContentSizer().
//
// Passing a non-negative contextMenuBoxIndex (the same 0-7 index used by the
// Show menu/ID_SHOW_GROUPBOX_BASE) makes the box's title/background
// right-clickable: it wires up its own context-menu handling and forwards to
// TopFrame::OnGroupBoxRightClick(index) so the box can be hidden/moved
// (Stage 10). This is deliberately a separate index, not the box's own
// wxWindowID -- the box's actual id stays wxID_ANY regardless, so this
// doesn't collide with the Show menu's own wxMenuItem ids, which happen to
// live in the same numeric ID space (ID_SHOW_GROUPBOX_BASE onwards) but are
// a completely different kind of object. Boxes not part of that set
// (Control, Radio Freq, Stats, etc.) just leave the default -1 and get no
// context menu.
class TintedGroupBox : public wxPanel
{
    public:
        TintedGroupBox(wxWindow* parent, const wxString& title, wxOrientation orientation, int contextMenuBoxIndex = -1);
        ~TintedGroupBox();

        virtual void SetLabel(const wxString& label) override;
        virtual wxString GetLabel() const override;

        // Applies to the title and background as well as the panel itself,
        // so it shows no matter where in the box the mouse is (barring
        // content widgets that set their own, more specific tooltip).
        void SetToolTip(const wxString& tip);

        wxSizer* GetContentSizer() const { return m_contentSizer; }

    private:
        wxStaticText* m_title;
        wxSizer* m_contentSizer;
};

// Returns the same tinted "card" background colour used by TintedGroupBox,
// for other controls (e.g. plot graticule label margins) that want to match.
wxColour GroupBoxBackgroundColour();

// Sets the tint colour/strength used by GroupBoxBackgroundColour() (persisted
// via the Display options tab). RefreshGroupBoxTints() then re-applies the
// resulting colour to every currently-live TintedGroupBox so the change is
// visible immediately, without needing a restart.
void SetGroupBoxTint(const wxColour& colour, int percent);
void RefreshGroupBoxTints();


///////////////////////////////////////////////////////////////////////////////
/// Class TopFrame
///////////////////////////////////////////////////////////////////////////////
class TopFrame : public wxFrame
{        
    protected:
        wxPanel* m_panel;
        wxMenuBar* m_menubarMain;
        wxMenu* file;
        wxMenu* edit;
        wxMenu* settings;
        wxMenu* tools;
        wxMenu* help;
        wxMenu* showMenu_;
        TintedGroupBox* snrBox;
        wxGauge* m_gaugeSNR;
        wxStaticText* m_textSNR;
        wxCheckBox* m_ckboxSNR;
        TintedGroupBox* levelBox;
        wxGauge* m_gaugeLevel;
        wxStaticText* m_textLevel;

        wxTextCtrl*   m_txtCtrlCallSign;
        
        wxComboCtrl*   m_cboLastReportedCallsigns;
        wxListViewComboPopup* m_lastReportedCallsignListView;
        
        wxStaticText* m_txtModeStatus;

        wxStaticText* m_txtTxLevelNum;
        wxButton* m_btnTxLevelMM;
        wxButton* m_btnTxLevelM;
        wxButton* m_btnTxLevelP;
        wxButton* m_btnTxLevelPP;
        wxSlider* m_sliderMicSpkrLevel;
        wxStaticText* m_txtMicSpkrLevelNum;
        
        // Explicitly wxInfoBarGeneric (not the wxInfoBar macro, which resolves
        // to a native GtkInfoBar-backed class on GTK) so that SetBackgroundColour
        // below reliably tints it the same way as TintedGroupBox, rather than
        // being overridden by the native widget's own GTK theme/CSS.
        wxInfoBarGeneric* m_infoBar;
        int m_lastInfoBarHeight = 0;
        bool m_playbackStatusVisible = false;

        TintedGroupBox* statsBox;
        wxButton*     m_BtnBerReset;
        wxStaticText  *m_textBits;
        wxStaticText  *m_textErrors;
        wxStaticText  *m_textBER;
        wxStaticText  *m_textResyncs;
        wxStaticText  *m_textClockOffset;
        wxStaticText  *m_textFreqOffset;
        wxStaticText  *m_textSyncMetric;
        wxStaticText  *m_textCodec2Var;

        TintedGroupBox* syncBox;
        wxStaticText  *m_textSync;

        TintedGroupBox* audioBox;
        wxToggleButton      *m_audioRecord;

        TintedGroupBox* logBox;
        wxButton*     m_logQSO;

        // Both need to be reachable at runtime (not just constructor-local)
        // so a box can be moved between sides / reordered (Stage 10).
        wxSizer* leftSizer;
        wxSizer* rightSizer;

        TintedGroupBox* modeBox;

        // Fixed (non-movable) box on the right, kept last in rightSizer
        // across any reorder -- see MainFrame::reflowGroupBoxes_().
        TintedGroupBox* controlBox;

        wxMenuItem* m_menuItemPlayFileFromRadio;
        wxMenuItem* m_menuItemExportConfig;
        wxMenuItem* m_menuItemImportConfig;

        TintedGroupBox* reporterBox;
        wxToggleButton *m_reporterHidden;
    
        // Virtual event handlers, override them in your derived class
        virtual void OnActivateWindow(wxActivateEvent& event) { event.Skip(); }
        virtual void topFrame_OnClose( wxCloseEvent& event ) { event.Skip(); }
        virtual void topFrame_OnPaint( wxPaintEvent& event ) { event.Skip(); }
        virtual void topFrame_OnSize( wxSizeEvent& event ) { event.Skip(); }
        virtual void topFrame_OnUpdateUI( wxUpdateUIEvent& event ) { event.Skip(); }

        virtual void OnExit( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTop( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsFreeDVReporter( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsFreeDVReporterUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnToolsFilter( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsFilterUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnToolsSetupWizard( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsSetupWizardUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnToolsOptions( wxCommandEvent& event ) { event.Skip(); }

        virtual void OnToolsUDP( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsOptionsUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnPlayFileFromRadio( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsExportConfig( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsExportConfigUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnToolsImportConfig( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsImportConfigUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnToolsLoadDefaultConfig( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsLoadDefaultConfigUI( wxUpdateUIEvent& event ) { event.Skip(); }

        virtual void OnHelpCheckUpdates( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnHelpCheckUpdatesUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnHelpAbout( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnHelpManual( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnCheckSNRClick( wxCommandEvent& event ) { event.Skip(); }

        virtual void OnTogBtnOnOff( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTogBtnAnalogClick( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTogBtnVoiceKeyerClick( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTogBtnVoiceKeyerRightClick( wxContextMenuEvent& event ) { event.Skip(); }

        virtual void OnTogBtnPTT( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTogBtnPTTRightClick( wxContextMenuEvent& event ) { event.Skip(); }

        virtual void OnHelp( wxCommandEvent& event ) { event.Skip(); }
        
        virtual void OnTogBtnRecord( wxCommandEvent& event ) { event.Skip(); }
        
        virtual void OnLogQSO(wxCommandEvent& event) { event.Skip(); }

        virtual void OnTogBtnAnalogClickUI(wxUpdateUIEvent& event) { event.Skip(); }
        virtual void OnTogBtnRxIDUI(wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnTogBtnTxIDUI(wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnTogBtnPTT_UI(wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnTogBtnOnOffUI(wxUpdateUIEvent& event ) { event.Skip(); }

        virtual void OnBerReset( wxCommandEvent& event ) { event.Skip(); }
        
        virtual void OnChangeTxMode( wxCommandEvent& event ) { event.Skip(); }
        
        virtual void OnTxLevelDecrBig( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTxLevelDecr( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTxLevelIncr( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTxLevelIncrBig( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTxLevelMouseWheel( wxMouseEvent& event ) { event.Skip(); }
        virtual void OnTxLevelContextMenu( wxContextMenuEvent& event ) { event.Skip(); }
        virtual void OnTuneAttenContextMenu( wxContextMenuEvent& event ) { event.Skip(); }

        virtual void OnChangeMicSpkrLevel( wxScrollEvent& event ) { event.Skip(); }
        
        virtual void OnChangeReportFrequency( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnChangeReportFrequencyVerify( wxCommandEvent& event ) { event.Skip(); }
                
        virtual void OnReportFrequencySetFocus(wxFocusEvent& event) { event.Skip(); }
        virtual void OnReportFrequencyKillFocus(wxFocusEvent& event) { event.Skip(); }

        virtual void OnSystemColorChanged(wxSysColourChangedEvent& event) { event.Skip(); }
                
        virtual void OnResetMicSpkrLevel(wxMouseEvent& event) { event.Skip(); }
        
        virtual void OnRightClickCallsignList(wxMouseEvent& event) { event.Skip(); }

        virtual void OnOpenCallsignList( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnCloseCallsignList( wxCommandEvent& event ) { event.Skip(); }

        virtual void OnToggleReporterVisibility (wxCommandEvent& event) { event.Skip(); }
        
        virtual void OnTogBtnTune(wxCommandEvent& event) { event.Skip(); }

        virtual void OnShowGroupBox(wxCommandEvent& event) { event.Skip(); }

        void setVoiceKeyerButtonLabel_(wxString filename);

    public:
        // Called directly (not routed through the wx event system, hence
        // public rather than protected like the other virtual handlers
        // above -- TintedGroupBox is a sibling class, not a subclass) by a
        // TintedGroupBox constructed with a real id when its title/background
        // is right-clicked, with boxIndex = id - ID_SHOW_GROUPBOX_BASE. Empty
        // here for the same reason as OnShowGroupBox: TopFrame has no access
        // to wxGetApp()/appConfiguration, so the real menu (Hide/Move to
        // other side/Move up/down) is only built in MainFrame's override.
        virtual void OnGroupBoxRightClick(int) { }

        wxToggleButton* m_togBtnOnOff;
        wxToggleButton* m_togBtnAnalog;
        wxToggleButton* m_togBtnVoiceKeyer;
        wxToggleButton* m_btnTogPTT;
        wxToggleButton* m_btnTogTune;
        wxAuiNotebook* m_auiNbookCtrl;
        wxComboBox*   m_cboReportFrequency;
        TintedGroupBox*  m_freqBox;
        TintedGroupBox*  m_txLevelBox;
        TintedGroupBox* micSpeakerBox;

        TopFrame( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("FreeDV "), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(561,300 ), long style = wxDEFAULT_FRAME_STYLE|wxRESIZE_BORDER );

        ~TopFrame();
};

// Override for wxAuiNotebook to prevent tabbing to it.
class TabFreeAuiNotebook : public wxAuiNotebook
{
public:
    TabFreeAuiNotebook();
    TabFreeAuiNotebook(wxWindow *parent, wxWindowID id=wxID_ANY, const wxPoint &pos=wxDefaultPosition, const wxSize &size=wxDefaultSize, long style=wxAUI_NB_DEFAULT_STYLE);
    virtual ~TabFreeAuiNotebook() = default;

    bool AcceptsFocus() const;
    bool AcceptsFocusFromKeyboard() const;
    bool AcceptsFocusRecursively() const;

#if !wxCHECK_VERSION(3, 3, 0)
    // wxAuiNotebook::SaveLayout()/LoadLayout() are only available in
    // wxWidgets 3.3+. Older wxWidgets (e.g. as still packaged by some
    // Linux distributions) falls back to this hand-rolled implementation
    // so that tab layout persistence is still available.
    wxString SavePerspective();
    bool LoadPerspective(const wxString& layout);
#endif // !wxCHECK_VERSION(3, 3, 0)
};

#endif //__TOPFRAME_H__
