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
#include <wx/wrapsizer.h>
#include "topFrame.h"

extern int g_playFileToMicInEventId;
extern int g_recFileFromRadioEventId;
extern int g_playFileFromRadioEventId;
extern int g_recFileFromModulatorEventId;
extern int g_txLevel;

// Override for wxAuiNotebook to prevent tabbing to it.
class TabFreeAuiNotebook : public wxAuiNotebook
{
public:
    TabFreeAuiNotebook() : wxAuiNotebook() { }
    TabFreeAuiNotebook(wxWindow *parent, wxWindowID id=wxID_ANY, const wxPoint &pos=wxDefaultPosition, const wxSize &size=wxDefaultSize, long style=wxAUI_NB_DEFAULT_STYLE)
        : wxAuiNotebook(parent, id, pos, size, style) { }
    
    bool AcceptsFocus() const { return false; }
    bool AcceptsFocusFromKeyboard() const { return false; }
    bool AcceptsFocusRecursively() const { return false; }
};

//=========================================================================
// Code that lays out the main application window
//=========================================================================
TopFrame::TopFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxFrame(parent, id, title, pos, size, style)
{
    this->SetSizeHints(wxDefaultSize, wxDefaultSize);
    this->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    this->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    //=====================================================
    // Menubar Setup
    m_menubarMain = new wxMenuBar(wxMB_DOCKABLE);
    file = new wxMenu();

    wxMenuItem* m_menuItemOnTop;
    m_menuItemOnTop = new wxMenuItem(file, wxID_ANY, wxString(_("&On Top")) , _("Always Top Window"), wxITEM_NORMAL);
    file->Append(m_menuItemOnTop);

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
    m_menuItemRigCtrlCfg = new wxMenuItem(tools, wxID_ANY, wxString(_("P&TT Config...")) , _("Configures FreeDV integration with radio"), wxITEM_NORMAL);
    tools->Append(m_menuItemRigCtrlCfg);

    wxMenuItem* m_menuItemOptions;
    m_menuItemOptions = new wxMenuItem(tools, wxID_ANY, wxString(_("&Options...")) , _("Miscellaneous FreeDV configuration options"), wxITEM_NORMAL);
    tools->Append(m_menuItemOptions);

    wxMenuItem* m_menuItemFilter;
    m_menuItemFilter = new wxMenuItem(tools, wxID_ANY, wxString(_("&Filter...")) , _("Configures audio filtering"), wxITEM_NORMAL);
    tools->Append(m_menuItemFilter);
    
    wxMenuItem* toolsSeparator2 = new wxMenuItem(tools, wxID_SEPARATOR);
    tools->Append(toolsSeparator2);

    m_menuItemPlayFileToMicIn = new wxMenuItem(tools, wxID_ANY, wxString(_("Start Play File - &Mic In...")) , _("Pipes microphone sound input from file"), wxITEM_NORMAL);
    g_playFileToMicInEventId = m_menuItemPlayFileToMicIn->GetId();
    tools->Append(m_menuItemPlayFileToMicIn);

    m_menuItemRecFileFromRadio = new wxMenuItem(tools, wxID_ANY, wxString(_("Start Record File - From &Radio...")) , _("Records incoming audio from the attached radio"), wxITEM_NORMAL);
    g_recFileFromRadioEventId = m_menuItemRecFileFromRadio->GetId();
    tools->Append(m_menuItemRecFileFromRadio);

    m_menuItemRecFileFromModulator = new wxMenuItem(tools, wxID_ANY, wxString(_("Start Record File - From Mo&dulator...")) , _("Records encoded audio from FreeDV"), wxITEM_NORMAL);
    g_recFileFromModulatorEventId = m_menuItemRecFileFromModulator->GetId();
    tools->Append(m_menuItemRecFileFromModulator);

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
    wxSizer* leftSizer = new wxWrapSizer(wxVERTICAL);

    wxStaticBoxSizer* snrSizer;
    wxStaticBox* snrBox = new wxStaticBox(m_panel, wxID_ANY, _("SNR"), wxDefaultPosition, wxSize(150,-1));
    snrSizer = new wxStaticBoxSizer(snrBox, wxVERTICAL);

    //------------------------------
    // S/N ratio Gauge (vert. bargraph)
    //------------------------------
    m_gaugeSNR = new wxGauge(snrBox, wxID_ANY, 25, wxDefaultPosition, wxSize(15,150), wxGA_SMOOTH|wxGA_VERTICAL);
    m_gaugeSNR->SetToolTip(_("Displays signal to noise ratio in dB."));
    snrSizer->Add(m_gaugeSNR, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 10);

    //------------------------------
    // Box for S/N ratio (Numeric)
    //------------------------------
    m_textSNR = new wxStaticText(snrBox, wxID_ANY, wxT(" 0.0 dB"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    m_textSNR->SetMinSize(wxSize(70,-1));
    snrSizer->Add(m_textSNR, 0, wxALIGN_CENTER_HORIZONTAL, 1);

    //------------------------------
    // S/N ratio slow Checkbox
    //------------------------------
    m_ckboxSNR = new wxCheckBox(snrBox, wxID_ANY, _("Slow"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    m_ckboxSNR->SetToolTip(_("Smooth but slow SNR estimation"));
    snrSizer->Add(m_ckboxSNR, 0, wxALIGN_CENTER_HORIZONTAL, 5);

    leftSizer->Add(snrSizer, 2, wxEXPAND|wxALL, 1);

    //------------------------------
    // Sync  Indicator box
    //------------------------------
    wxStaticBoxSizer* sbSizer3_33;
    wxStaticBox* syncBox = new wxStaticBox(m_panel, wxID_ANY, _("Sync"));
    sbSizer3_33 = new wxStaticBoxSizer(syncBox, wxVERTICAL);

    m_textSync = new wxStaticText(syncBox, wxID_ANY, wxT("Modem"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    sbSizer3_33->Add(m_textSync, 0, wxALIGN_CENTER_HORIZONTAL, 1);
    m_textSync->Disable();

    m_textCurrentDecodeMode = new wxStaticText(syncBox, wxID_ANY, wxT("Mode: unk"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer3_33->Add(m_textCurrentDecodeMode, 0, wxALIGN_CENTER_HORIZONTAL, 1);
    m_textCurrentDecodeMode->Disable();

    m_BtnReSync = new wxButton(syncBox, wxID_ANY, _("ReS&ync"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer3_33->Add(m_BtnReSync, 0, wxALIGN_CENTRE , 1);

    leftSizer->Add(sbSizer3_33,0, wxALL|wxEXPAND, 3);

    //------------------------------
    // Audio Recording/Playback
    //------------------------------
    wxStaticBox* audioBox = new wxStaticBox(m_panel, wxID_ANY, _("Audio"));
    wxStaticBoxSizer* sbSizerAudioRecordPlay = new wxStaticBoxSizer(audioBox, wxVERTICAL);
    
    m_audioRecord = new wxToggleButton(audioBox, wxID_ANY, _("Record"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizerAudioRecordPlay->Add(m_audioRecord, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 1);
    
    leftSizer->Add(sbSizerAudioRecordPlay, 0, wxALL|wxEXPAND, 3);
    
    //------------------------------
    // BER Frames box
    //------------------------------

    wxStaticBoxSizer* sbSizer_ber;
    wxStaticBox* statsBox = new wxStaticBox(m_panel, wxID_ANY, _("Stats"));
    sbSizer_ber = new wxStaticBoxSizer(statsBox, wxVERTICAL);

    m_BtnBerReset = new wxButton(statsBox, wxID_ANY, _("&Reset"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_ber->Add(m_BtnBerReset, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);

    m_textBits = new wxStaticText(statsBox, wxID_ANY, wxT("Bits: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textBits, 0, wxALIGN_LEFT, 1);
    m_textErrors = new wxStaticText(statsBox, wxID_ANY, wxT("Errs: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textErrors, 0, wxALIGN_LEFT, 1);
    m_textBER = new wxStaticText(statsBox, wxID_ANY, wxT("BER: 0.0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textBER, 0, wxALIGN_LEFT, 1);
    m_textResyncs = new wxStaticText(statsBox, wxID_ANY, wxT("Resyncs: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textResyncs, 0, wxALIGN_LEFT, 1);
    m_textClockOffset = new wxStaticText(statsBox, wxID_ANY, wxT("ClkOff: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_textClockOffset->SetMinSize(wxSize(125,-1));
    sbSizer_ber->Add(m_textClockOffset, 0, wxALIGN_LEFT, 1);
    m_textFreqOffset = new wxStaticText(statsBox, wxID_ANY, wxT("FreqOff: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textFreqOffset, 0, wxALIGN_LEFT, 1);
    m_textSyncMetric = new wxStaticText(statsBox, wxID_ANY, wxT("Sync: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textSyncMetric, 0, wxALIGN_LEFT, 1);
    m_textCodec2Var = new wxStaticText(statsBox, wxID_ANY, wxT("Var: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textCodec2Var, 0, wxALIGN_LEFT, 1);

    leftSizer->Add(sbSizer_ber,0, wxALL|wxEXPAND, 3);

    //------------------------------
    // Signal Level(vert. bargraph)
    //------------------------------
    wxStaticBoxSizer* levelSizer;
    wxStaticBox* levelBox = new wxStaticBox(m_panel, wxID_ANY, _("Level"));
    levelSizer = new wxStaticBoxSizer(levelBox, wxVERTICAL);
    
    m_textLevel = new wxStaticText(levelBox, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(60,-1), wxALIGN_CENTRE);
    m_textLevel->SetForegroundColour(wxColour(255,0,0));
    levelSizer->Add(m_textLevel, 0, wxALIGN_LEFT, 1);

    m_gaugeLevel = new wxGauge(levelBox, wxID_ANY, 100, wxDefaultPosition, wxSize(15,135), wxGA_SMOOTH|wxGA_VERTICAL);
    m_gaugeLevel->SetToolTip(_("Peak of From Radio in Rx, or peak of From Mic in Tx mode.  If Red you should reduce your levels"));
    levelSizer->Add(m_gaugeLevel, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 10);

    leftSizer->Add(levelSizer, 2, wxALL|wxEXPAND, 1);

    bSizer1->Add(leftSizer, 0, wxALL|wxEXPAND, 5);

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
    m_auiNbookCtrl->SetFont(wxFont(8, 70, 90, 90, false, wxEmptyString));
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
    m_txtModeStatus->SetMinSize(wxSize(40,-1));
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
    
    m_lastReportedCallsignListView->InsertColumn(0, wxT("Callsign"), wxLIST_FORMAT_LEFT, 125);
    m_lastReportedCallsignListView->InsertColumn(1, wxT("Date/Time"), wxLIST_FORMAT_CENTRE, 175);
    m_lastReportedCallsignListView->InsertColumn(2, wxT("SNR (dB)"), wxLIST_FORMAT_CENTRE, 75);

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
    rightSizer = new wxWrapSizer(wxVERTICAL);

    //=====================================================
    // Squelch Slider Control
    //=====================================================
    wxStaticBoxSizer* sbSizer3;
    wxStaticBox* squelchBox = new wxStaticBox(m_panel, wxID_ANY, _("S&quelch"), wxDefaultPosition, wxSize(150,-1));
    sbSizer3 = new wxStaticBoxSizer(squelchBox, wxVERTICAL);

    m_sliderSQ = new wxSlider(squelchBox, wxID_ANY, 0, 0, 40, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS|wxSL_INVERSE|wxSL_VERTICAL);
    m_sliderSQ->SetToolTip(_("Set Squelch level in dB."));
    m_sliderSQ->SetMinSize(wxSize(-1,150));

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
    m_ckboxSQ = new wxCheckBox(squelchBox, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);

    sbSizer3->Add(m_ckboxSQ, 0, wxALIGN_CENTER_HORIZONTAL, 0);
    rightSizer->Add(sbSizer3, 2, wxEXPAND, 0);

    // Transmit Level slider
    wxBoxSizer* txLevelSizer = new wxStaticBoxSizer(new wxStaticBox(m_panel, wxID_ANY, _("TX &Attenuation"), wxDefaultPosition, wxSize(150,-1)), wxVERTICAL);
    
    // Sliders are integer values, so we're multiplying min/max by 10 here to allow 1 decimal precision.
    m_sliderTxLevel = new wxSlider(m_panel, wxID_ANY, g_txLevel, -300, 0, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS|wxSL_INVERSE|wxSL_VERTICAL);
    m_sliderTxLevel->SetToolTip(_("Sets TX attenuation (0-30dB))."));
    m_sliderTxLevel->SetMinSize(wxSize(-1,150));
    txLevelSizer->Add(m_sliderTxLevel, 1, wxALIGN_CENTER_HORIZONTAL, 0);
    
    m_txtTxLevelNum = new wxStaticText(m_panel, wxID_ANY, wxT("0 dB"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_txtTxLevelNum->SetMinSize(wxSize(100,-1));
    txLevelSizer->Add(m_txtTxLevelNum, 0, wxALIGN_CENTER_HORIZONTAL, 0);
    
    rightSizer->Add(txLevelSizer, 2, wxEXPAND, 0);
    
    /* new --- */

    //------------------------------
    // Mode box
    //------------------------------
    modeBox = new wxStaticBox(m_panel, wxID_ANY, _("&Mode"), wxDefaultPosition, wxSize(150,-1));
    sbSizer_mode = new wxStaticBoxSizer(modeBox, wxVERTICAL);

    m_rb700d = new wxRadioButton( modeBox, wxID_ANY, wxT("700D"), wxDefaultPosition, wxDefaultSize,  wxRB_GROUP);
    sbSizer_mode->Add(m_rb700d, 0, wxALIGN_LEFT|wxALL, 1);
    m_rb700e = new wxRadioButton( modeBox, wxID_ANY, wxT("700E"), wxDefaultPosition, wxDefaultSize,  0);
    sbSizer_mode->Add(m_rb700e, 0, wxALIGN_LEFT|wxALL, 1);
    m_rb1600 = new wxRadioButton( modeBox, wxID_ANY, wxT("1600"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_mode->Add(m_rb1600, 0, wxALIGN_LEFT|wxALL, 1);
    m_hiddenMode1 = new wxRadioButton( modeBox, wxID_ANY, wxT("hidden2"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_mode->Add(m_hiddenMode1, 0, wxALIGN_LEFT|wxALL, 1);
    m_hiddenMode1->Show(false);
    
    m_collpane = new wxCollapsiblePane(modeBox, ID_MODE_COLLAPSE, "Others:");
    sbSizer_mode->Add(m_collpane, 0, wxGROW|wxALL, 5);
    
    wxWindow *otherModeWin = m_collpane->GetPane();
    wxSizer *otherModeSizer = new wxBoxSizer(wxVERTICAL);
    
    m_rb700c = new wxRadioButton( otherModeWin, wxID_ANY, wxT("700C"), wxDefaultPosition, wxDefaultSize,  0);
    otherModeSizer->Add(m_rb700c, 1, wxALIGN_LEFT|wxALL|wxEXPAND, 1);
    m_rb800xa = new wxRadioButton( otherModeWin, wxID_ANY, wxT("800XA"), wxDefaultPosition, wxDefaultSize, 0);
    otherModeSizer->Add(m_rb800xa, 1, wxALIGN_LEFT|wxALL|wxEXPAND, 1);
    m_rb2400b = new wxRadioButton( otherModeWin, wxID_ANY, wxT("2400B"), wxDefaultPosition, wxDefaultSize, 0);
    otherModeSizer->Add(m_rb2400b, 1, wxALIGN_LEFT|wxALL|wxEXPAND, 1);
    m_rb2020 = new wxRadioButton( otherModeWin, wxID_ANY, wxT("2020"), wxDefaultPosition, wxDefaultSize,  0);
    otherModeSizer->Add(m_rb2020, 1, wxALIGN_LEFT|wxALL|wxEXPAND, 1);
#if defined(FREEDV_MODE_2020B)
    m_rb2020b = new wxRadioButton( otherModeWin, wxID_ANY, wxT("2020B"), wxDefaultPosition, wxDefaultSize,  0);
    otherModeSizer->Add(m_rb2020b, 1, wxALIGN_LEFT|wxALL|wxEXPAND, 1);
#endif // FREEDV_MODE_2020B
    m_hiddenMode2 = new wxRadioButton( otherModeWin, wxID_ANY, wxT("hidden2"), wxDefaultPosition, wxDefaultSize, 0);
    otherModeSizer->Add(m_hiddenMode2, 0, wxALIGN_LEFT|wxALL, 1);
    m_hiddenMode2->Show(false);

    m_hiddenMode1->SetValue(true);
    m_hiddenMode2->SetValue(true);

    sbSizer_mode->SetMinSize(wxSize(175,240));
    otherModeWin->SetSizer(otherModeSizer);
    otherModeSizer->SetSizeHints(otherModeWin);

    rightSizer->Add(sbSizer_mode,0, wxEXPAND, 3);

    //=====================================================
    // Control Toggles box
    //=====================================================
    wxStaticBoxSizer* sbSizer5;
    wxStaticBox* controlBox = new wxStaticBox(m_panel, wxID_ANY, _("Control"), wxDefaultPosition, wxSize(150,-1));
    sbSizer5 = new wxStaticBoxSizer(controlBox, wxVERTICAL);
    wxBoxSizer* bSizer1511;
    bSizer1511 = new wxBoxSizer(wxVERTICAL);

    //-------------------------------
    // Stop/Stop signal processing (rx and tx)
    //-------------------------------
    m_togBtnOnOff = new wxToggleButton(controlBox, wxID_ANY, _("&Start"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnOnOff->SetToolTip(_("Begin/End receiving data."));
    bSizer1511->Add(m_togBtnOnOff, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    sbSizer5->Add(bSizer1511, 0, wxEXPAND, 1);

    wxBoxSizer* bSizer13;
    bSizer13 = new wxBoxSizer(wxVERTICAL);

    //------------------------------
    // Analog Passthrough Toggle
    //------------------------------
    m_togBtnAnalog = new wxToggleButton(controlBox, wxID_ANY, _("A&nalog"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnAnalog->SetToolTip(_("Toggle analog/digital operation."));
    bSizer13->Add(m_togBtnAnalog, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    sbSizer5->Add(bSizer13, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);

    //------------------------------
    // Voice Keyer Toggle
    //------------------------------
    m_togBtnVoiceKeyer = new wxToggleButton(controlBox, wxID_ANY, _("Voice &Keyer"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnVoiceKeyer->SetToolTip(_("Toggle Voice Keyer"));
    wxBoxSizer* bSizer13a = new wxBoxSizer(wxVERTICAL);
    bSizer13a->Add(m_togBtnVoiceKeyer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    sbSizer5->Add(bSizer13a, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);

    //------------------------------
    // PTT button: Toggle Transmit/Receive mode
    //------------------------------
    wxBoxSizer* bSizer11;
    bSizer11 = new wxBoxSizer(wxVERTICAL);
    m_btnTogPTT = new wxToggleButton(controlBox, wxID_ANY, _("&PTT"), wxDefaultPosition, wxDefaultSize, 0);
    m_btnTogPTT->SetToolTip(_("Push to Talk - Switch between Receive and Transmit"));
    bSizer11->Add(m_btnTogPTT, 1, wxALIGN_CENTER|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    sbSizer5->Add(bSizer11, 2, wxEXPAND, 1);
    rightSizer->Add(sbSizer5, 2, wxALL|wxEXPAND, 1);
    
    // Frequency text field (PSK Reporter)
    m_freqBox = new wxStaticBox(m_panel, wxID_ANY, _("Report Frequency"));
    wxBoxSizer* reportFrequencySizer = new wxStaticBoxSizer(m_freqBox, wxHORIZONTAL);
    
    wxStaticText* reportFrequencyUnits = new wxStaticText(m_freqBox, wxID_ANY, wxT(" MHz"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    wxBoxSizer* txtReportFreqSizer = new wxBoxSizer(wxVERTICAL);
    wxString choices[] = {
        _("3.6250"),
        _("3.6430"),
        _("3.6930"),
        _("3.6970"),
        _("5.4035"),
        _("5.3665"),
        _("7.1770"),
        _("14.2360"),
        _("14.2400"),
        _("18.1180"),
        _("21.3130"),
        _("24.9330"),
        _("28.3300"),
        _("28.7200"),
        _("10489.6400"),
    };
    int numChoices = sizeof(choices) / sizeof(wxString);
    m_cboReportFrequency = new wxComboBox(m_freqBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, numChoices, choices, wxCB_DROPDOWN | wxTE_PROCESS_ENTER);
    m_cboReportFrequency->SetMinSize(wxSize(150,-1));
    txtReportFreqSizer->Add(m_cboReportFrequency, 1, 0, 1);
    reportFrequencySizer->Add(txtReportFreqSizer, 1, wxEXPAND, 1);
    reportFrequencySizer->Add(reportFrequencyUnits, 0, wxALIGN_CENTER_VERTICAL, 1);
    
    rightSizer->Add(reportFrequencySizer, 0, wxALL, 1);
    
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
    m_rb700d->MoveBeforeInTabOrder(m_rb700e);
    m_rb700e->MoveBeforeInTabOrder(m_rb1600);
    
    m_rb700c->MoveBeforeInTabOrder(m_rb800xa);
    m_rb800xa->MoveBeforeInTabOrder(m_rb2400b);
    m_rb2400b->MoveBeforeInTabOrder(m_rb2020);
#if defined(FREEDV_MODE_2020B)
    m_rb2020->MoveBeforeInTabOrder(m_rb2020b);
#endif // FREEDV_MODE_2020B
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

    this->Connect(m_menuItemExit->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnExit));
    this->Connect(m_menuItemOnTop->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnTop));

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

    this->Connect(m_menuItemPlayFileToMicIn->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnPlayFileToMicIn));
    this->Connect(m_menuItemRecFileFromRadio->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnRecFileFromRadio));
    this->Connect(m_menuItemRecFileFromModulator->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnRecFileFromModulator));
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
    m_btnTogPTT->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnPTT), NULL, this);

    m_BtnCallSignReset->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnCallSignReset), NULL, this);
    m_BtnBerReset->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnBerReset), NULL, this);
    m_BtnReSync->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnReSync), NULL, this);
    
    m_rb700c->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb700d->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb700e->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb800xa->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb1600->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb2400b->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb2020->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
#if defined(FREEDV_MODE_2020B)
    m_rb2020b->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
#endif // FREEDV_MODE_2020B
    
    m_collpane->Connect(wxEVT_COLLAPSIBLEPANE_CHANGED, wxCollapsiblePaneEventHandler(TopFrame::OnChangeCollapseState), NULL, this);
    
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
    
    m_cboReportFrequency->Connect(wxEVT_TEXT_ENTER, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_TEXT, wxCommandEventHandler(TopFrame::OnChangeReportFrequencyVerify), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_COMBOBOX, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencySetFocus), NULL, this);
    m_cboReportFrequency->Connect(wxEVT_KILL_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencyKillFocus), NULL, this);
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

    this->Disconnect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnPlayFileToMicIn));
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
    m_btnTogPTT->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnPTT), NULL, this);

    m_audioRecord->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnRecord), NULL, this);

    m_rb700c->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb700d->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb700e->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb800xa->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb1600->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb2400b->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb2020->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
#if defined(FREEDV_MODE_2020B)
    m_rb2020b->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
#endif // FREEDV_MODE_2020B
    
    m_collpane->Disconnect(wxEVT_COLLAPSIBLEPANE_CHANGED, wxCollapsiblePaneEventHandler(TopFrame::OnChangeCollapseState), NULL, this);
    
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    
    m_cboReportFrequency->Disconnect(wxEVT_TEXT_ENTER, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    m_cboReportFrequency->Disconnect(wxEVT_TEXT, wxCommandEventHandler(TopFrame::OnChangeReportFrequencyVerify), NULL, this);
    m_cboReportFrequency->Disconnect(wxEVT_COMBOBOX, wxCommandEventHandler(TopFrame::OnChangeReportFrequency), NULL, this);
    
    m_cboReportFrequency->Disconnect(wxEVT_SET_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencySetFocus), NULL, this);
    m_cboReportFrequency->Disconnect(wxEVT_KILL_FOCUS, wxFocusEventHandler(TopFrame::OnReportFrequencyKillFocus), NULL, this);
}

void TopFrame::OnChangeCollapseState(wxCollapsiblePaneEvent& event)
{
    auto bestSize = modeBox->GetBestSize();
    auto curSize = modeBox->GetSize();

    modeBox->SetSize(wxSize(curSize.GetWidth(), bestSize.GetHeight()));
    rightSizer->Layout();
}
