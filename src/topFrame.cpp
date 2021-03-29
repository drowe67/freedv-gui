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
#include "topFrame.h"

extern int g_playFileToMicInEventId;
extern int g_recFileFromRadioEventId;
extern int g_playFileFromRadioEventId;
extern int g_recFileFromModulatorEventId;
extern int g_txLevel;

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
    m_menuItemOnTop = new wxMenuItem(file, wxID_ANY, wxString(_("On Top")) , _("Always Top Window"), wxITEM_NORMAL);
    file->Append(m_menuItemOnTop);

    wxMenuItem* m_menuItemExit;
    m_menuItemExit = new wxMenuItem(file, ID_EXIT, wxString(_("E&xit")) , _("Exit Program"), wxITEM_NORMAL);
    file->Append(m_menuItemExit);

    m_menubarMain->Append(file, _("&File"));

    tools = new wxMenu();
    wxMenuItem* m_menuItemAudio;
    m_menuItemAudio = new wxMenuItem(tools, wxID_ANY, wxString(_("&Audio Config...")) , wxEmptyString, wxITEM_NORMAL);
    tools->Append(m_menuItemAudio);

    wxMenuItem* m_menuItemRigCtrlCfg;
    m_menuItemRigCtrlCfg = new wxMenuItem(tools, wxID_ANY, wxString(_("&PTT Config...")) , wxEmptyString, wxITEM_NORMAL);
    tools->Append(m_menuItemRigCtrlCfg);

    wxMenuItem* m_menuItemOptions;
    m_menuItemOptions = new wxMenuItem(tools, wxID_ANY, wxString(_("&Options...")) , wxEmptyString, wxITEM_NORMAL);
    tools->Append(m_menuItemOptions);

    wxMenuItem* m_menuItemFilter;
    m_menuItemFilter = new wxMenuItem(tools, wxID_ANY, wxString(_("&Filter...")) , wxEmptyString, wxITEM_NORMAL);
    tools->Append(m_menuItemFilter);

    m_menuItemPlayFileToMicIn = new wxMenuItem(tools, wxID_ANY, wxString(_("Start Play File - Mic In...")) , wxEmptyString, wxITEM_NORMAL);
    g_playFileToMicInEventId = m_menuItemPlayFileToMicIn->GetId();
    tools->Append(m_menuItemPlayFileToMicIn);

    m_menuItemRecFileFromRadio = new wxMenuItem(tools, wxID_ANY, wxString(_("Start Record File - From Radio...")) , wxEmptyString, wxITEM_NORMAL);
    g_recFileFromRadioEventId = m_menuItemRecFileFromRadio->GetId();
    tools->Append(m_menuItemRecFileFromRadio);

    m_menuItemRecFileFromModulator = new wxMenuItem(tools, wxID_ANY, wxString(_("Start Record File - From Modulator...")) , wxEmptyString, wxITEM_NORMAL);
    g_recFileFromModulatorEventId = m_menuItemRecFileFromModulator->GetId();
    tools->Append(m_menuItemRecFileFromModulator);

    m_menuItemPlayFileFromRadio = new wxMenuItem(tools, wxID_ANY, wxString(_("Start Play File - From Radio...")) , wxEmptyString, wxITEM_NORMAL);
    g_playFileFromRadioEventId = m_menuItemPlayFileFromRadio->GetId();
    tools->Append(m_menuItemPlayFileFromRadio);
    m_menubarMain->Append(tools, _("&Tools"));

    help = new wxMenu();
    wxMenuItem* m_menuItemHelpUpdates;
    m_menuItemHelpUpdates = new wxMenuItem(help, wxID_ANY, wxString(_("Check for Updates")) , wxEmptyString, wxITEM_NORMAL);
    help->Append(m_menuItemHelpUpdates);
    m_menuItemHelpUpdates->Enable(false);

    wxMenuItem* m_menuItemAbout;
    m_menuItemAbout = new wxMenuItem(help, ID_ABOUT, wxString(_("&About...")) , _("About this program"), wxITEM_NORMAL);
    help->Append(m_menuItemAbout);

    m_menubarMain->Append(help, _("&Help"));

    this->SetMenuBar(m_menubarMain);

    wxBoxSizer* bSizer1;
    bSizer1 = new wxBoxSizer(wxHORIZONTAL);

    //=====================================================
    // Left side
    //=====================================================
    wxBoxSizer* leftSizer;
    leftSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer* snrSizer;
    snrSizer = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("SNR")), wxVERTICAL);

    //------------------------------
    // S/N ratio Guage (vert. bargraph)
    //------------------------------
    m_gaugeSNR = new wxGauge(this, wxID_ANY, 25, wxDefaultPosition, wxSize(15,135), wxGA_SMOOTH|wxGA_VERTICAL);
    m_gaugeSNR->SetToolTip(_("Displays signal to noise ratio in dB."));
    snrSizer->Add(m_gaugeSNR, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 10);

    //------------------------------
    // Box for S/N ratio (Numeric)
    //------------------------------
    m_textSNR = new wxStaticText(this, wxID_ANY, wxT(" 0.0"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    m_textSNR->SetMinSize(wxSize(40,-1));
    snrSizer->Add(m_textSNR, 0, wxALIGN_CENTER_HORIZONTAL, 1);

    //------------------------------
    // S/N ratio slow Checkbox
    //------------------------------
    m_ckboxSNR = new wxCheckBox(this, wxID_ANY, _("Slow"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    m_ckboxSNR->SetToolTip(_("Smooth but slow SNR estimation"));
    snrSizer->Add(m_ckboxSNR, 0, wxALIGN_CENTER_HORIZONTAL, 5);

    leftSizer->Add(snrSizer, 2, wxEXPAND|wxALL, 1);

    //------------------------------
    // Sync  Indicator box
    //------------------------------
    wxStaticBoxSizer* sbSizer3_33;
    sbSizer3_33 = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Sync")), wxVERTICAL);

    m_textSync = new wxStaticText(this, wxID_ANY, wxT("Modem"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    sbSizer3_33->Add(m_textSync, 0, wxALIGN_CENTER_HORIZONTAL, 1);

    m_textSync->Disable();

    m_BtnReSync = new wxButton(this, wxID_ANY, _("ReSync"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer3_33->Add(m_BtnReSync, 0, wxALIGN_CENTRE , 1);

    leftSizer->Add(sbSizer3_33,0, wxALL|wxEXPAND, 3);

    //------------------------------
    // BER Frames box
    //------------------------------

    wxStaticBoxSizer* sbSizer_ber;
    sbSizer_ber = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Stats")), wxVERTICAL);

    m_BtnBerReset = new wxButton(this, wxID_ANY, _("Reset"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_ber->Add(m_BtnBerReset, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);

    m_textCurrentDecodeMode = new wxStaticText(this, wxID_ANY, wxT("Mode: unk"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textCurrentDecodeMode, 0, wxALIGN_LEFT, 1);
    m_textBits = new wxStaticText(this, wxID_ANY, wxT("Bits: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textBits, 0, wxALIGN_LEFT, 1);
    m_textErrors = new wxStaticText(this, wxID_ANY, wxT("Errs: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textErrors, 0, wxALIGN_LEFT, 1);
    m_textBER = new wxStaticText(this, wxID_ANY, wxT("BER: 0.0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textBER, 0, wxALIGN_LEFT, 1);
    m_textResyncs = new wxStaticText(this, wxID_ANY, wxT("Resyncs: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textResyncs, 0, wxALIGN_LEFT, 1);
    m_textClockOffset = new wxStaticText(this, wxID_ANY, wxT("ClkOff: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_textClockOffset->SetMinSize(wxSize(125,-1));
    sbSizer_ber->Add(m_textClockOffset, 0, wxALIGN_LEFT, 1);
    m_textFreqOffset = new wxStaticText(this, wxID_ANY, wxT("FreqOff: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textFreqOffset, 0, wxALIGN_LEFT, 1);
    m_textSyncMetric = new wxStaticText(this, wxID_ANY, wxT("Sync: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textSyncMetric, 0, wxALIGN_LEFT, 1);
    m_textCodec2Var = new wxStaticText(this, wxID_ANY, wxT("Var: 0"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_ber->Add(m_textCodec2Var, 0, wxALIGN_LEFT, 1);

    leftSizer->Add(sbSizer_ber,0, wxALL|wxEXPAND, 3);

    //------------------------------
    // Signal Level(vert. bargraph)
    //------------------------------
    wxStaticBoxSizer* levelSizer;
    levelSizer = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Level")), wxVERTICAL);
    
    m_textLevel = new wxStaticText(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(60,-1), wxALIGN_CENTRE);
    m_textLevel->SetForegroundColour(wxColour(255,0,0));
    levelSizer->Add(m_textLevel, 0, wxALIGN_LEFT, 1);

    m_gaugeLevel = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(15,135), wxGA_SMOOTH|wxGA_VERTICAL);
    m_gaugeLevel->SetToolTip(_("Peak of From Radio in Rx, or peak of From Mic in Tx mode.  If Red you should reduce your levels"));
    levelSizer->Add(m_gaugeLevel, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 10);

    leftSizer->Add(levelSizer, 2, wxALL|wxEXPAND, 1);

    bSizer1->Add(leftSizer, 0, wxALL|wxEXPAND, 5);

    //=====================================================
    // Center Section
    //=====================================================
    wxBoxSizer* centerSizer;
    centerSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* upperSizer;
    upperSizer = new wxBoxSizer(wxVERTICAL);

    //=====================================================
    // Tabbed Notebook control containing display graphs
    //=====================================================
    //m_auiNbookCtrl = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxAUI_NB_BOTTOM|wxAUI_NB_DEFAULT_STYLE);
    //long style = wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS | wxAUI_NB_CLOSE_ON_ACTIVE_TAB | wxAUI_NB_MIDDLE_CLICK_CLOSE;
    long nb_style = wxAUI_NB_BOTTOM | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS;
    m_auiNbookCtrl = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, nb_style);
    // This line sets the fontsize for the tabs on the notebook control
    m_auiNbookCtrl->SetFont(wxFont(8, 70, 90, 90, false, wxEmptyString));

    upperSizer->Add(m_auiNbookCtrl, 1, wxALIGN_TOP|wxEXPAND, 1);
    centerSizer->Add(upperSizer, 1, wxALIGN_TOP|wxEXPAND, 0);

    // lower middle used for user ID

    wxBoxSizer* lowerSizer;
    lowerSizer = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* modeStatusSizer;
    modeStatusSizer = new wxBoxSizer(wxVERTICAL);
    m_txtModeStatus = new wxStaticText(this, wxID_ANY, wxT("unk"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_txtModeStatus->Enable(false); // enabled only if Hamlib is turned on
    m_txtModeStatus->SetMinSize(wxSize(40,-1));
    modeStatusSizer->Add(m_txtModeStatus, 0, wxALL|wxEXPAND, 1);
    lowerSizer->Add(modeStatusSizer, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);

    m_BtnCallSignReset = new wxButton(this, wxID_ANY, _("Clear"), wxDefaultPosition, wxDefaultSize, 0);
    lowerSizer->Add(m_BtnCallSignReset, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);

    wxBoxSizer* bSizer15;
    bSizer15 = new wxBoxSizer(wxVERTICAL);
    m_txtCtrlCallSign = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    m_txtCtrlCallSign->SetToolTip(_("Call Sign of transmitting station will appear here"));
    bSizer15->Add(m_txtCtrlCallSign, 0, wxALL|wxEXPAND, 5);
    lowerSizer->Add(bSizer15, 1, wxEXPAND, 5);

    centerSizer->Add(lowerSizer, 0, wxEXPAND, 2);

    
    bSizer1->Add(centerSizer, 4, wxALL|wxEXPAND, 1);
    
    //=====================================================
    // Right side
    //=====================================================
    wxBoxSizer* rightSizer;
    rightSizer = new wxBoxSizer(wxVERTICAL);

    //=====================================================
    // Squelch Slider Control
    //=====================================================
    wxStaticBoxSizer* sbSizer3;
    sbSizer3 = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Squelch")), wxVERTICAL);

    m_sliderSQ = new wxSlider(this, wxID_ANY, 0, 0, 40, wxDefaultPosition, wxSize(-1,60), wxSL_AUTOTICKS|wxSL_INVERSE|wxSL_VERTICAL);
    m_sliderSQ->SetToolTip(_("Set Squelch level in dB."));

    sbSizer3->Add(m_sliderSQ, 1, wxALIGN_CENTER_HORIZONTAL, 0);

    //------------------------------
    // Squelch Level static text box
    //------------------------------
    m_textSQ = new wxStaticText(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);

    sbSizer3->Add(m_textSQ, 0, wxALIGN_CENTER_HORIZONTAL, 0);

    //------------------------------
    // Squelch Toggle Checkbox
    //------------------------------
    m_ckboxSQ = new wxCheckBox(this, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);

    sbSizer3->Add(m_ckboxSQ, 0, wxALIGN_CENTER_HORIZONTAL, 0);
    rightSizer->Add(sbSizer3, 2, wxEXPAND, 0);

    // Transmit Level slider
    wxBoxSizer* txLevelSizer = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("TX Attenuation")), wxVERTICAL);
    
    // Sliders are integer values, so we're multiplying min/max by 10 here to allow 1 decimal precision.
    m_sliderTxLevel = new wxSlider(this, wxID_ANY, g_txLevel, -300, 0, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS|wxSL_INVERSE|wxSL_VERTICAL);
    m_sliderTxLevel->SetToolTip(_("Sets TX attenuation (0-30dB))."));
    txLevelSizer->Add(m_sliderTxLevel, 1, wxALIGN_CENTER_HORIZONTAL, 0);
    
    m_txtTxLevelNum = new wxStaticText(this, wxID_ANY, wxT("0 dB"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_txtTxLevelNum->SetMinSize(wxSize(50,-1));
    txLevelSizer->Add(m_txtTxLevelNum, 0, wxALIGN_CENTER_HORIZONTAL, 0);
    
    rightSizer->Add(txLevelSizer, 2, wxEXPAND, 0);
    
    //rightSizer->Add(sbSizer3_33,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);

    /* new --- */

    //------------------------------
    // Mode box
    //------------------------------
    wxStaticBoxSizer* sbSizer_mode;
    sbSizer_mode = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Mode")), wxVERTICAL);

    m_rb700c = new wxRadioButton( this, wxID_ANY, wxT("700C"), wxDefaultPosition, wxDefaultSize,  wxRB_GROUP);
    sbSizer_mode->Add(m_rb700c, 0, wxALIGN_LEFT|wxALL, 1);
    m_rb700d = new wxRadioButton( this, wxID_ANY, wxT("700D"), wxDefaultPosition, wxDefaultSize,  0);
    sbSizer_mode->Add(m_rb700d, 0, wxALIGN_LEFT|wxALL, 1);
    m_rb700e = new wxRadioButton( this, wxID_ANY, wxT("700E"), wxDefaultPosition, wxDefaultSize,  0);
    sbSizer_mode->Add(m_rb700e, 0, wxALIGN_LEFT|wxALL, 1);
    m_rb800xa = new wxRadioButton( this, wxID_ANY, wxT("800XA"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_mode->Add(m_rb800xa, 0, wxALIGN_LEFT|wxALL, 1);
    m_rb1600 = new wxRadioButton( this, wxID_ANY, wxT("1600"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_mode->Add(m_rb1600, 0, wxALIGN_LEFT|wxALL, 1);
    m_rb2400b = new wxRadioButton( this, wxID_ANY, wxT("2400B"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_mode->Add(m_rb2400b, 0, wxALIGN_LEFT|wxALL, 1);
    m_rb2020 = new wxRadioButton( this, wxID_ANY, wxT("2020"), wxDefaultPosition, wxDefaultSize,  0);
    sbSizer_mode->Add(m_rb2020, 0, wxALIGN_LEFT|wxALL, 1);

    m_rb1600->SetValue(true);

    rightSizer->Add(sbSizer_mode,0, wxALL|wxEXPAND, 3);

    #ifdef MOVED_TO_OPTIONS_DIALOG
    /* new --- */

    //------------------------------
    // Test Frames box
    //------------------------------

    wxStaticBoxSizer* sbSizer_testFrames;
    sbSizer_testFrames = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Test Frames")), wxVERTICAL);

    m_ckboxTestFrame = new wxCheckBox(this, wxID_ANY, _("Enable"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_testFrames->Add(m_ckboxTestFrame, 0, wxALIGN_LEFT, 0);

    rightSizer->Add(sbSizer_testFrames,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);
    #endif

    //=====================================================
    // Control Toggles box
    //=====================================================
    wxStaticBoxSizer* sbSizer5;
    sbSizer5 = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Control")), wxVERTICAL);
    wxBoxSizer* bSizer1511;
    bSizer1511 = new wxBoxSizer(wxVERTICAL);

    //-------------------------------
    // Stop/Stop signal processing (rx and tx)
    //-------------------------------
    m_togBtnOnOff = new wxToggleButton(this, wxID_ANY, _("Start"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnOnOff->SetToolTip(_("Begin/End receiving data."));
    bSizer1511->Add(m_togBtnOnOff, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    sbSizer5->Add(bSizer1511, 0, wxEXPAND, 1);

#ifdef UNIMPLEMENTED
    //------------------------------
    // Toggle Loopback button for RX
    //------------------------------
    wxBoxSizer* bSizer15113;
    bSizer15113 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* bSizer15111;
    bSizer15111 = new wxBoxSizer(wxVERTICAL);
    wxSize wxSz = wxSize(44, 30);
    m_togBtnLoopRx = new wxToggleButton(this, wxID_ANY, _("Loop\nRX"), wxDefaultPosition, wxSz, 0);
    m_togBtnLoopRx->SetFont(wxFont(6, 70, 90, 90, false, wxEmptyString));
    m_togBtnLoopRx->SetToolTip(_("Loopback Receive audio data."));

    bSizer15111->Add(m_togBtnLoopRx, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);

    //sbSizer5->Add(bSizer15111, 0, wxEXPAND, 1);
    bSizer15113->Add(bSizer15111, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);

    //------------------------------
    // Toggle Loopback button for Tx
    //------------------------------
    wxBoxSizer* bSizer15112;
    bSizer15112 = new wxBoxSizer(wxVERTICAL);
    m_togBtnLoopTx = new wxToggleButton(this, wxID_ANY, _("Loop\nTX"), wxDefaultPosition, wxSz, 0);
    m_togBtnLoopTx->SetFont(wxFont(6, 70, 90, 90, false, wxEmptyString));
    m_togBtnLoopTx->SetToolTip(_("Loopback Transmit audio data."));

    bSizer15112->Add(m_togBtnLoopTx, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
    bSizer15113->Add(bSizer15112, 0,  wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);

    sbSizer5->Add(bSizer15113, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
#endif

    //------------------------------
    // Split Frequency Mode Toggle
    //------------------------------
    wxBoxSizer* bSizer151;
    bSizer151 = new wxBoxSizer(wxVERTICAL);

    m_togBtnSplit = new wxToggleButton(this, wxID_ANY, _("Split"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnSplit->SetToolTip(_("Toggle split frequency mode."));

    bSizer151->Add(m_togBtnSplit, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    sbSizer5->Add(bSizer151, 0, wxALL|wxEXPAND, 1);
    wxBoxSizer* bSizer13;
    bSizer13 = new wxBoxSizer(wxVERTICAL);

    //------------------------------
    // Analog Passthrough Toggle
    //------------------------------
    m_togBtnAnalog = new wxToggleButton(this, wxID_ANY, _("Analog"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnAnalog->SetToolTip(_("Toggle analog/digital operation."));
    bSizer13->Add(m_togBtnAnalog, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    sbSizer5->Add(bSizer13, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);

    //------------------------------
    // Voice Keyer Toggle
    //------------------------------
    m_togBtnVoiceKeyer = new wxToggleButton(this, wxID_ANY, _("Voice Keyer"), wxDefaultPosition, wxDefaultSize, 0);
    m_togBtnVoiceKeyer->SetToolTip(_("Toggle Voice Keyer"));
    wxBoxSizer* bSizer13a = new wxBoxSizer(wxVERTICAL);
    bSizer13a->Add(m_togBtnVoiceKeyer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    sbSizer5->Add(bSizer13a, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);

    //------------------------------
    // PTT button: Toggle Transmit/Receive mode
    //------------------------------
    wxBoxSizer* bSizer11;
    bSizer11 = new wxBoxSizer(wxVERTICAL);
    m_btnTogPTT = new wxToggleButton(this, wxID_ANY, _("PTT"), wxDefaultPosition, wxDefaultSize, 0);
    m_btnTogPTT->SetToolTip(_("Push to Talk - Switch between Receive and Transmit - you can also use the space bar "));
    bSizer11->Add(m_btnTogPTT, 1, wxALIGN_CENTER|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 1);
    sbSizer5->Add(bSizer11, 2, wxEXPAND, 1);
    rightSizer->Add(sbSizer5, 2, wxALL|wxEXPAND, 3);
    bSizer1->Add(rightSizer, 0, wxALL|wxEXPAND, 3);
    this->SetSizerAndFit(bSizer1);
    this->Layout();
    m_statusBar1 = this->CreateStatusBar(3, wxST_SIZEGRIP, wxID_ANY);
    
    //=====================================================
    // End of layout
    //=====================================================

    //-------------------
    // Connect Events
    //-------------------
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(TopFrame::topFrame_OnClose));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(TopFrame::topFrame_OnPaint));
    this->Connect(wxEVT_SIZE, wxSizeEventHandler(TopFrame::topFrame_OnSize));
    this->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::topFrame_OnUpdateUI));

    this->Connect(m_menuItemExit->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnExit));
    this->Connect(m_menuItemOnTop->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(TopFrame::OnTop));

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
    m_sliderSQ->Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnSliderScrollBottom), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnCmdSliderScrollChanged), NULL, this);
    m_sliderSQ->Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnSliderScrollTop), NULL, this);
    m_ckboxSQ->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(TopFrame::OnCheckSQClick), NULL, this);

    m_ckboxSNR->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(TopFrame::OnCheckSNRClick), NULL, this);

    m_togBtnOnOff->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnOnOff), NULL, this);
    m_togBtnSplit->Connect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnSplitClick), NULL, this);
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
    
    m_sliderTxLevel->Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
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
    m_sliderSQ->Disconnect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnCmdSliderScroll), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnSliderScrollBottom), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnCmdSliderScrollChanged), NULL, this);
    m_sliderSQ->Disconnect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnSliderScrollTop), NULL, this);
    m_ckboxSQ->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(TopFrame::OnCheckSQClick), NULL, this);

    m_togBtnOnOff->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnOnOff), NULL, this);
    m_togBtnSplit->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnSplitClick), NULL, this);
    m_togBtnAnalog->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnAnalogClick), NULL, this);
    m_togBtnVoiceKeyer->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnVoiceKeyerClick), NULL, this);
    m_btnTogPTT->Disconnect(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler(TopFrame::OnTogBtnPTT), NULL, this);

    m_rb700c->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb700d->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb700e->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb800xa->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb1600->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb2400b->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    m_rb2020->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(TopFrame::OnChangeTxMode), NULL, this);
    
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_CHANGED, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
    m_sliderTxLevel->Disconnect(wxEVT_SCROLL_TOP, wxScrollEventHandler(TopFrame::OnChangeTxLevel), NULL, this);
}
