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
#include <wx/statusbr.h>
#include <wx/frame.h>
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

#include "freedv_api.h" // for FREEDV_MODE_*

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

class wxListViewComboPopup;

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
        wxMenu* tools;
        wxMenu* help;
        wxGauge* m_gaugeSNR;
        wxStaticText* m_textSNR;
        wxCheckBox* m_ckboxSNR;
        wxGauge* m_gaugeLevel;
        wxStaticText* m_textLevel;

        wxButton*     m_BtnCallSignReset;
        wxTextCtrl*   m_txtCtrlCallSign;
        
        wxComboCtrl*   m_cboLastReportedCallsigns;
        wxListViewComboPopup* m_lastReportedCallsignListView;
        
        wxStaticText* m_txtModeStatus;

        wxStaticText* m_txtTxLevelNum;
        wxSlider* m_sliderTxLevel;
        wxSlider* m_sliderMicSpkrLevel;
        wxStaticText* m_txtMicSpkrLevelNum;
        
        wxSlider* m_sliderSQ;        
        wxCheckBox* m_ckboxSQ;
        wxStaticText* m_textSQ;
        wxStatusBar* m_statusBar1;

        wxStaticBox* statsBox;
        wxButton*     m_BtnBerReset;
        wxStaticText  *m_textCurrentDecodeMode;
        wxStaticText  *m_textBits;
        wxStaticText  *m_textErrors;
        wxStaticText  *m_textBER;
        wxStaticText  *m_textResyncs;
        wxStaticText  *m_textClockOffset;
        wxStaticText  *m_textFreqOffset;
        wxStaticText  *m_textSyncMetric;
        wxStaticText  *m_textCodec2Var;

        wxStaticText  *m_textSync;
        wxButton      *m_BtnReSync;
        wxButton      *m_btnCenterRx;
        
        wxToggleButton      *m_audioRecord;

        wxRadioButton *m_rbRADE;
        wxRadioButton *m_rb700d;
        wxRadioButton *m_rb700e;
        wxRadioButton *m_rb1600;

        wxSizer* rightSizer;
        
        wxStaticBox* modeBox;
        wxStaticBoxSizer* sbSizer_mode;
        
        wxMenuItem* m_menuItemRecFileFromRadio;
        wxMenuItem* m_menuItemPlayFileFromRadio;
    
        // Virtual event handlers, override them in your derived class
        virtual void topFrame_OnClose( wxCloseEvent& event ) { event.Skip(); }
        virtual void topFrame_OnPaint( wxPaintEvent& event ) { event.Skip(); }
        virtual void topFrame_OnSize( wxSizeEvent& event ) { event.Skip(); }
        virtual void topFrame_OnUpdateUI( wxUpdateUIEvent& event ) { event.Skip(); }

        virtual void OnExit( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTop( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsEasySetup( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsEasySetupUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnToolsFreeDVReporter( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsFreeDVReporterUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnToolsAudio( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsAudioUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnToolsFilter( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsFilterUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnToolsOptions( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnCenterRx( wxCommandEvent& event ) { event.Skip(); }

        virtual void OnToolsUDP( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsOptionsUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnToolsComCfg( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnToolsComCfgUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnRecFileFromRadio( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnPlayFileFromRadio( wxCommandEvent& event ) { event.Skip(); }

        virtual void OnHelpCheckUpdates( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnHelpCheckUpdatesUI( wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnHelpAbout( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnHelpManual( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnCmdSliderScroll( wxScrollEvent& event ) { event.Skip(); }
        virtual void OnCheckSQClick( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnCheckSNRClick( wxCommandEvent& event ) { event.Skip(); }

        virtual void OnTogBtnLoopRx( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTogBtnLoopTx( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTogBtnOnOff( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTogBtnAnalogClick( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTogBtnVoiceKeyerClick( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTogBtnVoiceKeyerRightClick( wxContextMenuEvent& event ) { event.Skip(); }
        
        virtual void OnTogBtnPTT( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnTogBtnPTTRightClick( wxContextMenuEvent& event ) { event.Skip(); }

        virtual void OnHelp( wxCommandEvent& event ) { event.Skip(); }
        
        virtual void OnTogBtnRecord( wxCommandEvent& event ) { event.Skip(); }

        virtual void OnTogBtnAnalogClickUI(wxUpdateUIEvent& event) { event.Skip(); }
        virtual void OnTogBtnRxIDUI(wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnTogBtnTxIDUI(wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnTogBtnPTT_UI(wxUpdateUIEvent& event ) { event.Skip(); }
        virtual void OnTogBtnOnOffUI(wxUpdateUIEvent& event ) { event.Skip(); }

        virtual void OnCallSignReset( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnBerReset( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnReSync( wxCommandEvent& event ) { event.Skip(); }
        
        virtual void OnChangeTxMode( wxCommandEvent& event ) { event.Skip(); }
        
        virtual void OnChangeTxLevel( wxScrollEvent& event ) { event.Skip(); }
        
        virtual void OnChangeMicSpkrLevel( wxScrollEvent& event ) { event.Skip(); }
        
        virtual void OnChangeReportFrequency( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnChangeReportFrequencyVerify( wxCommandEvent& event ) { event.Skip(); }
                
        virtual void OnReportFrequencySetFocus(wxFocusEvent& event) { event.Skip(); }
        virtual void OnReportFrequencyKillFocus(wxFocusEvent& event) { event.Skip(); }

        virtual void OnSystemColorChanged(wxSysColourChangedEvent& event) { event.Skip(); }
        
        virtual void OnNotebookPageChanging(wxAuiNotebookEvent& event) { event.Skip(); }
        
        virtual void OnResetMicSpkrLevel(wxMouseEvent& event) { event.Skip(); }
        
        void setVoiceKeyerButtonLabel_(wxString filename);
        
    public:
        wxToggleButton* m_togBtnOnOff;
        wxToggleButton* m_togBtnAnalog;
        wxToggleButton* m_togBtnVoiceKeyer;
        wxToggleButton* m_btnTogPTT;
        wxAuiNotebook* m_auiNbookCtrl;
        wxComboBox*   m_cboReportFrequency;
        wxStaticBox*  m_freqBox;
        wxStaticBox*  squelchBox;

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
    
    wxString SavePerspective();
    bool LoadPerspective(const wxString& layout);
};

#endif //__TOPFRAME_H__
