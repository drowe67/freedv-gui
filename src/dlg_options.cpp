//==========================================================================
// Name:            dlg_options.cpp
// Purpose:         Dialog for controlling misc FreeDV options
// Date:            May 24 2013
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

#include "dlg_options.h"

extern bool                g_modal;
extern FreeDVInterface freedvInterface;

// PortAudio over/underflow counters

extern int                 g_infifo1_full;
extern int                 g_outfifo1_empty;
extern int                 g_infifo2_full;
extern int                 g_outfifo2_empty;
extern int                 g_AEstatus1[4];
extern int                 g_AEstatus2[4];
extern wxDatagramSocket    *g_sock;
extern int                 g_dump_timing;
extern int                 g_dump_fifo_state;
extern int                 g_freedv_verbose;
extern wxConfigBase *pConfig;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class OptionsDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
OptionsDlg::OptionsDlg(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxDialog(parent, id, title, pos, size, style)
{
    sessionActive_ = false;
    
    wxPanel* panel = new wxPanel(this);
    panel->SetMinSize(wxSize(700, -1));
    
    wxBoxSizer* bSizer30;
    bSizer30 = new wxBoxSizer(wxVERTICAL);
    
    // Create notebook and tabs.
    m_notebook = new wxNotebook(panel, wxID_ANY);
    m_reportingTab = new wxPanel(m_notebook, wxID_ANY);
    m_displayTab = new wxPanel(m_notebook, wxID_ANY);
    m_keyerTab = new wxPanel(m_notebook, wxID_ANY);
    m_modemTab = new wxPanel(m_notebook, wxID_ANY);
    m_simulationTab = new wxPanel(m_notebook, wxID_ANY);
    m_interfacingTab = new wxPanel(m_notebook, wxID_ANY);
    m_debugTab = new wxPanel(m_notebook, wxID_ANY);
    
    m_notebook->AddPage(m_reportingTab, _("Reporting"));
    m_notebook->AddPage(m_displayTab, _("Display"));
    m_notebook->AddPage(m_keyerTab, _("Voice Keyer"));
    m_notebook->AddPage(m_modemTab, _("Modem"));
    m_notebook->AddPage(m_simulationTab, _("Simulation"));
    m_notebook->AddPage(m_interfacingTab, _("UDP"));
    m_notebook->AddPage(m_debugTab, _("Debugging"));
    
    bSizer30->Add(m_notebook, 0, wxALL | wxEXPAND, 3);
    
    // Reporting tab
    wxBoxSizer* sizerReporting = new wxBoxSizer(wxVERTICAL);
    
    //------------------------------
    // Txt Msg Text Box
    //------------------------------

    wxStaticBoxSizer* sbSizer_callSign;
    wxStaticBox *sb_textMsg = new wxStaticBox(m_reportingTab, wxID_ANY, _("Txt Msg"));
    sbSizer_callSign = new wxStaticBoxSizer(sb_textMsg, wxVERTICAL);

    m_txtCtrlCallSign = new wxTextCtrl(m_reportingTab, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    m_txtCtrlCallSign->SetToolTip(_("Txt Msg you can send along with Voice"));
    sbSizer_callSign->Add(m_txtCtrlCallSign, 0, wxALL | wxEXPAND, 5);

    sizerReporting->Add(sbSizer_callSign,0, wxALL | wxEXPAND, 5);
 
    //----------------------------------------------------------
    // PSK Reporter 
    //----------------------------------------------------------

    wxStaticBoxSizer* sbSizer_psk;
    wxStaticBox* sb_psk = new wxStaticBox(m_reportingTab, wxID_ANY, _("PSK Reporter"));
    sbSizer_psk = new wxStaticBoxSizer(sb_psk, wxHORIZONTAL);
    m_ckbox_psk_enable = new wxCheckBox(m_reportingTab, wxID_ANY, _("Enable Reporting"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_psk->Add(m_ckbox_psk_enable, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelPskCallsign = new wxStaticText(m_reportingTab, wxID_ANY, wxT("Callsign:"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_psk->Add(labelPskCallsign, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_txt_callsign = new wxTextCtrl(m_reportingTab, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(90,-1), 0, wxTextValidator(wxFILTER_ALPHANUMERIC));
    sbSizer_psk->Add(m_txt_callsign, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelPskGridSquare = new wxStaticText(m_reportingTab, wxID_ANY, wxT("Grid Square:"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_psk->Add(labelPskGridSquare, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_txt_grid_square = new wxTextCtrl(m_reportingTab, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(70,-1), 0, wxTextValidator(wxFILTER_ALPHANUMERIC));
    sbSizer_psk->Add(m_txt_grid_square, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    sizerReporting->Add(sbSizer_psk,0, wxALL | wxEXPAND, 5);
    m_reportingTab->SetSizer(sizerReporting);
        
    // Display tab
    wxBoxSizer* sizerDisplay = new wxBoxSizer(wxVERTICAL);
    
    //----------------------------------------------------------
    // Waterfall color 
    //----------------------------------------------------------
    wxStaticBox* sb_waterfall = new wxStaticBox(m_displayTab, wxID_ANY, _("Waterfall Style"));
    wxStaticBoxSizer* sbSizer_waterfallColor =  new wxStaticBoxSizer(sb_waterfall, wxHORIZONTAL);
    
    m_waterfallColorScheme1 = new wxRadioButton(m_displayTab, wxID_ANY, _("Multicolor"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    sbSizer_waterfallColor->Add(m_waterfallColorScheme1, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    m_waterfallColorScheme2 = new wxRadioButton(m_displayTab, wxID_ANY, _("Black && White"), wxDefaultPosition, wxDefaultSize);
    sbSizer_waterfallColor->Add(m_waterfallColorScheme2, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    m_waterfallColorScheme3 = new wxRadioButton(m_displayTab, wxID_ANY, _("Blue Tint"), wxDefaultPosition, wxDefaultSize);
    sbSizer_waterfallColor->Add(m_waterfallColorScheme3, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    sizerDisplay->Add(sbSizer_waterfallColor, 0, wxALL | wxEXPAND, 5);
    
    m_displayTab->SetSizer(sizerDisplay);
    
    // Voice Keyer tab
    wxBoxSizer* sizerKeyer = new wxBoxSizer(wxVERTICAL);
    
    //----------------------------------------------------------------------
    // Voice Keyer 
    //----------------------------------------------------------------------

    wxStaticBoxSizer* staticBoxSizer28a = new wxStaticBoxSizer( new wxStaticBox(m_keyerTab, wxID_ANY, _("Voice Keyer")), wxVERTICAL);

    wxBoxSizer* voiceKeyerSizer1 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* voiceKeyerSizer2 = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText *m_staticText28b = new wxStaticText(m_keyerTab, wxID_ANY, _("Wave File: "), wxDefaultPosition, wxDefaultSize, 0);
    voiceKeyerSizer1->Add(m_staticText28b, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtCtrlVoiceKeyerWaveFile = new wxTextCtrl(m_keyerTab, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300,-1), 0);
    m_txtCtrlVoiceKeyerWaveFile->SetToolTip(_("Wave file to play for Voice Keyer"));
    voiceKeyerSizer1->Add(m_txtCtrlVoiceKeyerWaveFile, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_buttonChooseVoiceKeyerWaveFile = new wxButton(m_keyerTab, wxID_APPLY, _("Choose"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_buttonChooseVoiceKeyerWaveFile->SetMinSize(wxSize(120, -1));
    voiceKeyerSizer1->Add(m_buttonChooseVoiceKeyerWaveFile, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *m_staticText28c = new wxStaticText(m_keyerTab, wxID_ANY, _("Rx Pause:"), wxDefaultPosition, wxDefaultSize, 0);
    voiceKeyerSizer2->Add(m_staticText28c, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtCtrlVoiceKeyerRxPause = new wxTextCtrl(m_keyerTab, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(40,-1), 0);
    m_txtCtrlVoiceKeyerRxPause->SetToolTip(_("How long to wait in Rx mode before repeat"));
    voiceKeyerSizer2->Add(m_txtCtrlVoiceKeyerRxPause, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *m_staticText28d = new wxStaticText(m_keyerTab, wxID_ANY, _("Repeats:"), wxDefaultPosition, wxDefaultSize, 0);
    voiceKeyerSizer2->Add(m_staticText28d, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtCtrlVoiceKeyerRepeats = new wxTextCtrl(m_keyerTab, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(40,-1), 0);
    m_txtCtrlVoiceKeyerRepeats->SetToolTip(_("How long to wait in Rx mode before repeat"));
    voiceKeyerSizer2->Add(m_txtCtrlVoiceKeyerRepeats, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    staticBoxSizer28a->Add(voiceKeyerSizer1);
    staticBoxSizer28a->Add(voiceKeyerSizer2);

    sizerKeyer->Add(staticBoxSizer28a,0, wxALL | wxEXPAND, 5);
    
    m_keyerTab->SetSizer(sizerKeyer);
    
    // Modem tab
    wxBoxSizer* sizerModem = new wxBoxSizer(wxVERTICAL);
    
    //------------------------------
    // FreeDV 700 Options
    //------------------------------

    wxStaticBoxSizer* sbSizer_freedv700;
    wxStaticBox *sb_freedv700 = new wxStaticBox(m_modemTab, wxID_ANY, _("Modem Options"));
    sbSizer_freedv700 = new wxStaticBoxSizer(sb_freedv700, wxHORIZONTAL);

    wxBoxSizer* optionsCol1 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* optionsCol2 = new wxBoxSizer(wxVERTICAL);

    m_ckboxFreeDV700txClip = new wxCheckBox(m_modemTab, wxID_ANY, _("Clipping"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    optionsCol1->Add(m_ckboxFreeDV700txClip, 0, wxALL | wxALIGN_LEFT, 5);

    m_ckboxFreeDV700Combine = new wxCheckBox(m_modemTab, wxID_ANY, _("700C Diversity Combine"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    optionsCol1->Add(m_ckboxFreeDV700Combine, 0, wxALL | wxALIGN_LEFT, 5);

    m_ckboxFreeDV700txBPF = new wxCheckBox(m_modemTab, wxID_ANY, _(" 700D/E TX Band Pass Filter"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    optionsCol2->Add(m_ckboxFreeDV700txBPF, 0, wxALL | wxALIGN_LEFT, 5);

    sbSizer_freedv700->Add(optionsCol1);
    sbSizer_freedv700->Add(optionsCol2);

    sizerModem->Add(sbSizer_freedv700, 0, wxALL|wxEXPAND, 5);

    //------------------------------
    // Half/Full duplex selection
    //------------------------------

    wxStaticBox *sb_duplex = new wxStaticBox(m_modemTab, wxID_ANY, _("Half/Full Duplex Operation"));
    wxStaticBoxSizer* sbSizer_duplex = new wxStaticBoxSizer(sb_duplex, wxHORIZONTAL);

    m_ckHalfDuplex = new wxCheckBox(m_modemTab, wxID_ANY, _("Half Duplex"), wxDefaultPosition, wxSize(-1,-1), 0);
    sbSizer_duplex->Add(m_ckHalfDuplex, 0, wxALL | wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);

    sizerModem->Add(sbSizer_duplex,0, wxALL | wxEXPAND, 5);

    //------------------------------
    // Multiple RX selection
    //------------------------------
    wxStaticBox *sb_multirx = new wxStaticBox(m_modemTab, wxID_ANY, _("Multiple RX Operation"));
    wxStaticBoxSizer* sbSizer_multirx = new wxStaticBoxSizer(sb_multirx, wxVERTICAL);

    wxBoxSizer* sbSizer_simultaneousDecode = new wxBoxSizer(wxHORIZONTAL);
    m_ckboxMultipleRx = new wxCheckBox(m_modemTab, wxID_ANY, _("Simultaneously Decode All HF Modes"), wxDefaultPosition, wxSize(-1,-1), 0);
    sbSizer_simultaneousDecode->Add(m_ckboxMultipleRx, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    sbSizer_multirx->Add(sbSizer_simultaneousDecode, 0, wxALIGN_LEFT, 0);
    
    wxBoxSizer* sbSizer_singleThread = new wxBoxSizer(wxHORIZONTAL);
    m_ckboxSingleRxThread = new wxCheckBox(m_modemTab, wxID_ANY, _("Use single thread for multiple RX operation"), wxDefaultPosition, wxSize(-1,-1), 0);
    sbSizer_singleThread->Add(m_ckboxSingleRxThread, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    sbSizer_multirx->Add(sbSizer_singleThread, 0, wxALIGN_LEFT, 0);
    
    sizerModem->Add(sbSizer_multirx,0, wxALL|wxEXPAND, 5);
    
    wxStaticBox *sb_modemstats = new wxStaticBox(m_modemTab, wxID_ANY, _("Modem Statistics"));
    wxStaticBoxSizer* sbSizer_modemstats = new wxStaticBoxSizer(sb_modemstats, wxVERTICAL);

    wxBoxSizer* sbSizer_statsResetTime = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText *m_staticTextResetTime = new wxStaticText(m_modemTab, wxID_ANY, _("Time before resetting stats (sec):"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_statsResetTime->Add(m_staticTextResetTime, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_statsResetTime = new wxTextCtrl(m_modemTab, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(50,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_statsResetTime->Add(m_statsResetTime, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    sbSizer_modemstats->Add(sbSizer_statsResetTime, 0, wxALIGN_LEFT, 0);
    
    sizerModem->Add(sbSizer_modemstats,0, wxALL | wxEXPAND, 5);
        
    m_modemTab->SetSizer(sizerModem);
    
    // Simulation tab
    wxBoxSizer* sizerSimulation = new wxBoxSizer(wxVERTICAL);
    
    //------------------------------
    // Test Frames/Channel simulation check box
    //------------------------------

    wxStaticBoxSizer* sbSizer_testFrames;
    wxStaticBox *sb_testFrames = new wxStaticBox(m_simulationTab, wxID_ANY, _("Testing and Channel Simulation"));
    sbSizer_testFrames = new wxStaticBoxSizer(sb_testFrames, wxVERTICAL);

    m_ckboxTestFrame = new wxCheckBox(m_simulationTab, wxID_ANY, _("Test Frames"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_testFrames->Add(m_ckboxTestFrame, 0, wxALL | wxALIGN_LEFT, 5);

    wxBoxSizer* channelNoiseSizer = new wxBoxSizer(wxHORIZONTAL);

    m_ckboxChannelNoise = new wxCheckBox(m_simulationTab, wxID_ANY, _("Channel Noise"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    channelNoiseSizer->Add(m_ckboxChannelNoise, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *channelNoiseDbLabel = new wxStaticText(m_simulationTab, wxID_ANY, _("SNR (dB):"), wxDefaultPosition, wxDefaultSize, 0);
    channelNoiseSizer->Add(channelNoiseDbLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtNoiseSNR = new wxTextCtrl(m_simulationTab, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(30,-1), 0, wxTextValidator(wxFILTER_NUMERIC));
    channelNoiseSizer->Add(m_txtNoiseSNR, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    sbSizer_testFrames->Add(channelNoiseSizer);

    wxBoxSizer* attnCarrierSizer = new wxBoxSizer(wxHORIZONTAL);

    m_ckboxAttnCarrierEn = new wxCheckBox(m_simulationTab, wxID_ANY, _("Attn Carrier"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    attnCarrierSizer->Add(m_ckboxAttnCarrierEn, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *carrierLabel = new wxStaticText(m_simulationTab, wxID_ANY, _("Carrier:"), wxDefaultPosition, wxDefaultSize, 0);
    attnCarrierSizer->Add(carrierLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_txtAttnCarrier = new wxTextCtrl(m_simulationTab, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(30,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    attnCarrierSizer->Add(m_txtAttnCarrier, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    sbSizer_testFrames->Add(attnCarrierSizer);

    sizerSimulation->Add(sbSizer_testFrames,0, wxALL|wxEXPAND, 5);

    //------------------------------
    // Interfering tone
    //------------------------------

    wxStaticBoxSizer* sbSizer_tone;
    wxStaticBox *sb_tone = new wxStaticBox(m_simulationTab, wxID_ANY, _("Simulated Interference Tone"));
    sbSizer_tone = new wxStaticBoxSizer(sb_tone, wxHORIZONTAL);

    m_ckboxTone = new wxCheckBox(m_simulationTab, wxID_ANY, _("Tone"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_tone->Add(m_ckboxTone, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *toneFreqLabel = new wxStaticText(m_simulationTab, wxID_ANY, _("Freq (Hz):"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_tone->Add(toneFreqLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtToneFreqHz = new wxTextCtrl(m_simulationTab, wxID_ANY,  "1000", wxDefaultPosition, wxSize(60,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_tone->Add(m_txtToneFreqHz, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *m_staticTextta = new wxStaticText(m_simulationTab, wxID_ANY, _("Amplitude (pk): "), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_tone->Add(m_staticTextta, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtToneAmplitude = new wxTextCtrl(m_simulationTab, wxID_ANY,  "1000", wxDefaultPosition, wxSize(60,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_tone->Add(m_txtToneAmplitude, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    sizerSimulation->Add(sbSizer_tone,0, wxALL|wxEXPAND, 5);

    m_simulationTab->SetSizer(sizerSimulation);
    
    // Interfacing tab
    wxBoxSizer* sizerInterfacing = new wxBoxSizer(wxVERTICAL);
    
    //----------------------------------------------------------
    // UDP Send Messages on Events
    //----------------------------------------------------------

    wxStaticBoxSizer* sbSizer_udp;
    wxStaticBox* sb_udp = new wxStaticBox(m_interfacingTab, wxID_ANY, _("UDP Messages"));
    sbSizer_udp = new wxStaticBoxSizer(sb_udp, wxVERTICAL);
    m_ckbox_udp_enable = new wxCheckBox(m_interfacingTab, wxID_ANY, _("Enable UDP Messages"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_udp->Add(m_ckbox_udp_enable, 0, wxALL, 5);

    wxBoxSizer* portNumberSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* udpPortNumberLabel = new wxStaticText(m_interfacingTab, wxID_ANY, _("UDP Port Number:"), wxDefaultPosition, wxDefaultSize, 0);
    portNumberSizer->Add(udpPortNumberLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txt_udp_port = new wxTextCtrl(m_interfacingTab, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(70,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    portNumberSizer->Add(m_txt_udp_port, 0, wxALL, 5);
    m_btn_udp_test = new wxButton(m_interfacingTab, wxID_ANY, _("Test"), wxDefaultPosition, wxDefaultSize, 0);
    portNumberSizer->Add(m_btn_udp_test, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    sbSizer_udp->Add(portNumberSizer, 0, wxALL, 5);
    sizerInterfacing->Add(sbSizer_udp, 0, wxALL | wxEXPAND, 5);

    m_interfacingTab->SetSizer(sizerInterfacing);
        
    // Debug tab
    wxBoxSizer* sizerDebug = new wxBoxSizer(wxVERTICAL);
    
#ifdef __WXMSW__
    //------------------------------
    // debug console, for WIndows build make console pop up for debug messages
    //------------------------------

    wxStaticBoxSizer* sbSizer_console;
    wxStaticBox *sb_console = new wxStaticBox(m_debugTab, wxID_ANY, _("Debug: Windows"));
    sbSizer_console = new wxStaticBoxSizer(sb_console, wxHORIZONTAL);

    m_ckboxDebugConsole = new wxCheckBox(m_debugTab, wxID_ANY, _("Show Console"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_console->Add(m_ckboxDebugConsole, 0, wxALIGN_LEFT, 5);

    sizerDebug->Add(sbSizer_console,0, wxALL|wxEXPAND, 5);
#endif // __WXMSW__
    
    //----------------------------------------------------------
    // FIFO and under/overflow counters used for debug
    //----------------------------------------------------------

    wxStaticBox* sb_fifo = new wxStaticBox(m_debugTab, wxID_ANY, _("Debug: FIFO and Under/Over Flow Counters"));
    wxStaticBoxSizer* sbSizer_fifo = new wxStaticBoxSizer(sb_fifo, wxVERTICAL);

    wxBoxSizer* sbSizer_fifo1 = new wxBoxSizer(wxHORIZONTAL);

    // FIFO size in ms

    wxStaticText *m_staticTextFifo1 = new wxStaticText(m_debugTab, wxID_ANY, _("Fifo Size (ms):"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_fifo1->Add(m_staticTextFifo1, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    m_txtCtrlFifoSize = new wxTextCtrl(m_debugTab, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(50,-1), 0);
    sbSizer_fifo1->Add(m_txtCtrlFifoSize, 0, wxALL, 5);

    // Reset stats button
    
    m_BtnFifoReset = new wxButton(m_debugTab, wxID_ANY, _("Reset"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_fifo1->Add(m_BtnFifoReset, 0,  wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    sbSizer_fifo->Add(sbSizer_fifo1);

    // text lines with fifo counters
    
    m_textPA1 = new wxStaticText(m_debugTab, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_fifo->Add(m_textPA1, 0, wxALIGN_LEFT, 1);
    m_textPA2 = new wxStaticText(m_debugTab, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_fifo->Add(m_textPA2, 0, wxALIGN_LEFT, 1);

    m_textFifos = new wxStaticText(m_debugTab, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_fifo->Add(m_textFifos, 0, wxALIGN_LEFT, 1);

    // 2nd line
    
    wxStaticBox* sb_fifo2 = new wxStaticBox(m_debugTab, wxID_ANY, _("Debug: Application Options"));
    wxStaticBoxSizer* sbSizer_fifo2 = new wxStaticBoxSizer(sb_fifo2, wxVERTICAL);

    wxBoxSizer* sbDebugOptionsSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sbDebugOptionsSizer2 = new wxBoxSizer(wxHORIZONTAL);

    m_ckboxVerbose = new wxCheckBox(m_debugTab, wxID_ANY, _("Verbose"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer->Add(m_ckboxVerbose, 0, wxALL | wxALIGN_LEFT, 5);   
    m_ckboxTxRxThreadPriority = new wxCheckBox(m_debugTab, wxID_ANY, _("txRxThreadPriority"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer->Add(m_ckboxTxRxThreadPriority, 0, wxALL | wxALIGN_LEFT, 5);
    m_ckboxTxRxDumpTiming = new wxCheckBox(m_debugTab, wxID_ANY, _("txRxDumpTiming"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer->Add(m_ckboxTxRxDumpTiming, 0, wxALL | wxALIGN_LEFT, 5);
    m_ckboxTxRxDumpFifoState = new wxCheckBox(m_debugTab, wxID_ANY, _("txRxDumpFifoState"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer2->Add(m_ckboxTxRxDumpFifoState, 0, wxALL | wxALIGN_LEFT, 5);   
    m_ckboxFreeDVAPIVerbose = new wxCheckBox(m_debugTab, wxID_ANY, _("APiVerbose"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer2->Add(m_ckboxFreeDVAPIVerbose, 0, wxALL | wxALIGN_LEFT, 5);   

    sbSizer_fifo2->Add(sbDebugOptionsSizer, 0, wxALL | wxEXPAND | 0);
    sbSizer_fifo2->Add(sbDebugOptionsSizer2, 0, wxALL | wxEXPAND | 0);

    sizerDebug->Add(sbSizer_fifo,0, wxALL|wxEXPAND, 3);
    sizerDebug->Add(sbSizer_fifo2,0, wxALL|wxEXPAND, 3);

    m_debugTab->SetSizer(sizerDebug);
    
    //------------------------------
    // OK - Cancel - Apply Buttons 
    //------------------------------

    wxBoxSizer* bSizer31 = new wxBoxSizer(wxHORIZONTAL);

    m_sdbSizer5OK = new wxButton(panel, wxID_OK);
    bSizer31->Add(m_sdbSizer5OK, 0, wxALL, 2);

    m_sdbSizer5Cancel = new wxButton(panel, wxID_CANCEL);
    bSizer31->Add(m_sdbSizer5Cancel, 0, wxALL, 2);

    m_sdbSizer5Apply = new wxButton(panel, wxID_APPLY);
    bSizer31->Add(m_sdbSizer5Apply, 0, wxALL, 2);

    bSizer30->Add(bSizer31, 0, wxALL | wxALIGN_CENTER, 5);

    panel->SetSizer(bSizer30);
    
    wxBoxSizer* winSizer = new wxBoxSizer(wxVERTICAL);
    winSizer->Add(panel, 0, wxEXPAND);
    
    this->SetSizerAndFit(winSizer);
    this->Layout();
    this->Centre(wxBOTH);
 
    //-------------------
    // Tab ordering for accessibility
    //-------------------
    m_txtCtrlCallSign->MoveBeforeInTabOrder(m_ckbox_psk_enable);
    m_ckbox_psk_enable->MoveBeforeInTabOrder(m_txt_callsign);
    m_txt_callsign->MoveBeforeInTabOrder(m_txt_grid_square);
    
    m_waterfallColorScheme1->MoveBeforeInTabOrder(m_waterfallColorScheme2);
    m_waterfallColorScheme2->MoveBeforeInTabOrder(m_waterfallColorScheme3);
    
    m_txtCtrlVoiceKeyerWaveFile->MoveBeforeInTabOrder(m_buttonChooseVoiceKeyerWaveFile);
    m_buttonChooseVoiceKeyerWaveFile->MoveBeforeInTabOrder(m_txtCtrlVoiceKeyerRxPause);
    m_txtCtrlVoiceKeyerRxPause->MoveBeforeInTabOrder(m_txtCtrlVoiceKeyerRepeats);
    
    m_ckboxFreeDV700txClip->MoveBeforeInTabOrder(m_ckboxFreeDV700Combine);
    m_ckboxFreeDV700Combine->MoveBeforeInTabOrder(m_ckboxFreeDV700txBPF);
    m_ckboxFreeDV700txBPF->MoveBeforeInTabOrder(m_ckHalfDuplex);
    m_ckHalfDuplex->MoveBeforeInTabOrder(m_ckboxMultipleRx);
    m_ckboxMultipleRx->MoveBeforeInTabOrder(m_ckboxSingleRxThread);
    m_ckboxSingleRxThread->MoveBeforeInTabOrder(m_statsResetTime);
    
    m_ckboxTestFrame->MoveBeforeInTabOrder(m_ckboxChannelNoise);
    m_ckboxChannelNoise->MoveBeforeInTabOrder(m_txtNoiseSNR);
    m_txtNoiseSNR->MoveBeforeInTabOrder(m_ckboxAttnCarrierEn);
    m_ckboxAttnCarrierEn->MoveBeforeInTabOrder(m_txtAttnCarrier);
    m_txtAttnCarrier->MoveBeforeInTabOrder(m_ckboxTone);
    m_ckboxTone->MoveBeforeInTabOrder(m_txtToneFreqHz);
    m_txtToneFreqHz->MoveBeforeInTabOrder(m_txtToneAmplitude);
    
    m_ckbox_udp_enable->MoveBeforeInTabOrder(m_txt_udp_port);
    m_txt_udp_port->MoveBeforeInTabOrder(m_btn_udp_test);
    
    m_txtCtrlFifoSize->MoveBeforeInTabOrder(m_ckboxVerbose);
    m_ckboxVerbose->MoveBeforeInTabOrder(m_ckboxTxRxThreadPriority);
    m_ckboxTxRxThreadPriority->MoveBeforeInTabOrder(m_ckboxTxRxDumpTiming);
    m_ckboxTxRxDumpTiming->MoveBeforeInTabOrder(m_ckboxTxRxDumpFifoState);
    m_ckboxTxRxDumpFifoState->MoveBeforeInTabOrder(m_ckboxFreeDVAPIVerbose);
    m_ckboxFreeDVAPIVerbose->MoveBeforeInTabOrder(m_BtnFifoReset);
    
    m_reportingTab->MoveBeforeInTabOrder(m_displayTab);    
    m_displayTab->MoveBeforeInTabOrder(m_keyerTab);
    m_keyerTab->MoveBeforeInTabOrder(m_modemTab);
    m_modemTab->MoveBeforeInTabOrder(m_simulationTab);
    m_simulationTab->MoveBeforeInTabOrder(m_interfacingTab);
    m_interfacingTab->MoveBeforeInTabOrder(m_debugTab);
    
    m_notebook->MoveBeforeInTabOrder(m_sdbSizer5OK);
    m_sdbSizer5OK->MoveBeforeInTabOrder(m_sdbSizer5Cancel);
    m_sdbSizer5Cancel->MoveBeforeInTabOrder(m_sdbSizer5Apply);
    
    // Connect Events -------------------------------------------------------

    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(OptionsDlg::OnInitDialog));

    m_sdbSizer5OK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnOK), NULL, this);
    m_sdbSizer5Cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnCancel), NULL, this);
    m_sdbSizer5Apply->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnApply), NULL, this);

    m_ckboxTestFrame->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnTestFrame), NULL, this);
    m_ckboxChannelNoise->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnChannelNoise), NULL, this);
    m_ckboxAttnCarrierEn->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnAttnCarrierEn), NULL, this);

    m_ckboxFreeDV700txClip->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnFreeDV700txClip), NULL, this);
    m_ckboxFreeDV700Combine->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnFreeDV700Combine), NULL, this);

#ifdef __WXMSW__
    m_ckboxDebugConsole->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnDebugConsole), NULL, this);
#endif

    m_buttonChooseVoiceKeyerWaveFile->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnChooseVoiceKeyerWaveFile), NULL, this);

    m_BtnFifoReset->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnFifoReset), NULL, this);
    m_btn_udp_test->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnUDPTest), NULL, this);

    m_ckbox_psk_enable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnPSKReporterEnable), NULL, this);
    m_ckboxTone->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnToneStateEnable), NULL, this);
    m_ckbox_udp_enable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnUDPStateEnable), NULL, this);
    
    m_ckboxMultipleRx->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnMultipleRxEnable), NULL, this);
    
    event_in_serial = 0;
    event_out_serial = 0;
}

//-------------------------------------------------------------------------
// ~OptionsDlg()
//-------------------------------------------------------------------------
OptionsDlg::~OptionsDlg()
{

    // Disconnect Events

    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(OptionsDlg::OnInitDialog));

    m_sdbSizer5OK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnOK), NULL, this);
    m_sdbSizer5Cancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnCancel), NULL, this);
    m_sdbSizer5Apply->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnApply), NULL, this);

    m_ckboxTestFrame->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnTestFrame), NULL, this);
    m_ckboxChannelNoise->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnChannelNoise), NULL, this);
    m_ckboxAttnCarrierEn->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnAttnCarrierEn), NULL, this);

    m_ckboxFreeDV700txClip->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnFreeDV700txClip), NULL, this);
    m_ckboxFreeDV700Combine->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnFreeDV700Combine), NULL, this);
    m_buttonChooseVoiceKeyerWaveFile->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnChooseVoiceKeyerWaveFile), NULL, this);

    m_BtnFifoReset->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnFifoReset), NULL, this);
    m_btn_udp_test->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnUDPTest), NULL, this);

#ifdef __WXMSW__
    m_ckboxDebugConsole->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnDebugConsole), NULL, this);
#endif
    
    m_ckbox_psk_enable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnPSKReporterEnable), NULL, this);
    m_ckboxTone->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnToneStateEnable), NULL, this);
    m_ckbox_udp_enable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnUDPStateEnable), NULL, this);
    
    m_ckboxMultipleRx->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnMultipleRxEnable), NULL, this);
}


//-------------------------------------------------------------------------
// ExchangeData()
//-------------------------------------------------------------------------
void OptionsDlg::ExchangeData(int inout, bool storePersistent)
{
    if(inout == EXCHANGE_DATA_IN)
    {
        m_txtCtrlCallSign->SetValue(wxGetApp().m_callSign);

        /* Voice Keyer */

        m_txtCtrlVoiceKeyerWaveFile->SetValue(wxGetApp().m_txtVoiceKeyerWaveFile);
        m_txtCtrlVoiceKeyerRxPause->SetValue(wxString::Format(wxT("%i"), wxGetApp().m_intVoiceKeyerRxPause));
        m_txtCtrlVoiceKeyerRepeats->SetValue(wxString::Format(wxT("%i"), wxGetApp().m_intVoiceKeyerRepeats));

        m_ckHalfDuplex->SetValue(wxGetApp().m_boolHalfDuplex);

        m_ckboxMultipleRx->SetValue(wxGetApp().m_boolMultipleRx);
        m_ckboxSingleRxThread->SetValue(wxGetApp().m_boolSingleRxThread);
        
        m_ckboxTestFrame->SetValue(wxGetApp().m_testFrames);

        m_ckboxChannelNoise->SetValue(wxGetApp().m_channel_noise);
        m_txtNoiseSNR->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_noise_snr));

        m_ckboxTone->SetValue(wxGetApp().m_tone);
        m_txtToneFreqHz->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_tone_freq_hz));
        m_txtToneAmplitude->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_tone_amplitude));

        m_ckboxAttnCarrierEn->SetValue(wxGetApp().m_attn_carrier_en);
        m_txtAttnCarrier->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_attn_carrier));

        m_ckbox_udp_enable->SetValue(wxGetApp().m_udp_enable);
        m_txt_udp_port->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_udp_port));

        m_txtCtrlFifoSize->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_fifoSize_ms));

        m_ckboxTxRxThreadPriority->SetValue(wxGetApp().m_txRxThreadHighPriority);
        m_ckboxTxRxDumpTiming->SetValue(g_dump_timing);
        m_ckboxTxRxDumpFifoState->SetValue(g_dump_fifo_state);
        m_ckboxVerbose->SetValue(g_verbose);
        m_ckboxFreeDVAPIVerbose->SetValue(g_freedv_verbose);
       
        m_ckboxFreeDV700txClip->SetValue(wxGetApp().m_FreeDV700txClip);
        m_ckboxFreeDV700txBPF->SetValue(wxGetApp().m_FreeDV700txBPF);
        m_ckboxFreeDV700Combine->SetValue(wxGetApp().m_FreeDV700Combine);

#ifdef __WXMSW__
        m_ckboxDebugConsole->SetValue(wxGetApp().m_debug_console);
#endif
        
        // PSK Reporter config
        m_ckbox_psk_enable->SetValue(wxGetApp().m_psk_enable);
        m_txt_callsign->SetValue(wxGetApp().m_psk_callsign);
        m_txt_grid_square->SetValue(wxGetApp().m_psk_grid_square);
        
        // Stats reset time
        m_statsResetTime->SetValue(wxString::Format(wxT("%i"), wxGetApp().m_statsResetTimeSec));
        
        // Waterfall color
        switch (wxGetApp().m_waterfallColor)
        {
            case 1:
                m_waterfallColorScheme1->SetValue(false);
                m_waterfallColorScheme2->SetValue(true);
                m_waterfallColorScheme3->SetValue(false);
                break;
            case 2:
                m_waterfallColorScheme1->SetValue(false);
                m_waterfallColorScheme2->SetValue(false);
                m_waterfallColorScheme3->SetValue(true);
                break;
            case 0:
            default:
                m_waterfallColorScheme1->SetValue(true);
                m_waterfallColorScheme2->SetValue(false);
                m_waterfallColorScheme3->SetValue(false);
                break;
        };
        
        if (m_waterfallColorScheme1->GetValue())
        {
            wxGetApp().m_waterfallColor = 0;
        }
        else if (m_waterfallColorScheme2->GetValue())
        {
            wxGetApp().m_waterfallColor = 1;
        }
        else if (m_waterfallColorScheme3->GetValue())
        {
            wxGetApp().m_waterfallColor = 2;
        }
        
        // Update control state based on checkbox state.
        updatePSKReporterState();
        updateChannelNoiseState();
        updateAttnCarrierState();
        updateToneState();
        updateUDPState();
        updateMultipleRxState();
    }

    if(inout == EXCHANGE_DATA_OUT)
    {
        wxGetApp().m_callSign = m_txtCtrlCallSign->GetValue();

        wxGetApp().m_boolHalfDuplex = m_ckHalfDuplex->GetValue();
        pConfig->Write(wxT("/Rig/HalfDuplex"), wxGetApp().m_boolHalfDuplex);

        wxGetApp().m_boolMultipleRx = m_ckboxMultipleRx->GetValue();
        pConfig->Write(wxT("/Rig/MultipleRx"), wxGetApp().m_boolMultipleRx);
        
        wxGetApp().m_boolSingleRxThread = m_ckboxSingleRxThread->GetValue();
        pConfig->Write(wxT("/Rig/SingleRxThread"), wxGetApp().m_boolSingleRxThread);
        
        /* Voice Keyer */

        wxGetApp().m_txtVoiceKeyerWaveFile = m_txtCtrlVoiceKeyerWaveFile->GetValue();
        pConfig->Write(wxT("/VoiceKeyer/WaveFile"), wxGetApp().m_txtVoiceKeyerWaveFile);
        long tmp;
        m_txtCtrlVoiceKeyerRxPause->GetValue().ToLong(&tmp); if (tmp < 0) tmp = 0; wxGetApp().m_intVoiceKeyerRxPause = (int)tmp;
        pConfig->Write(wxT("/VoiceKeyer/RxPause"), wxGetApp().m_intVoiceKeyerRxPause);
        m_txtCtrlVoiceKeyerRepeats->GetValue().ToLong(&tmp);
        if (tmp < 0) {tmp = 0;} if (tmp > 100) {tmp = 100;}
        wxGetApp().m_intVoiceKeyerRepeats = (int)tmp;
        pConfig->Write(wxT("/VoiceKeyer/Repeats"), wxGetApp().m_intVoiceKeyerRepeats);

        wxGetApp().m_testFrames    = m_ckboxTestFrame->GetValue();

        wxGetApp().m_channel_noise = m_ckboxChannelNoise->GetValue();
        long noise_snr;
        m_txtNoiseSNR->GetValue().ToLong(&noise_snr);
        wxGetApp().m_noise_snr = (int)noise_snr;
        //fprintf(stderr, "noise_snr: %d\n", (int)noise_snr);
        
        wxGetApp().m_tone    = m_ckboxTone->GetValue();
        long tone_freq_hz, tone_amplitude;
        m_txtToneFreqHz->GetValue().ToLong(&tone_freq_hz);
        wxGetApp().m_tone_freq_hz = (int)tone_freq_hz;
        m_txtToneAmplitude->GetValue().ToLong(&tone_amplitude);
        wxGetApp().m_tone_amplitude = (int)tone_amplitude;

        wxGetApp().m_attn_carrier_en = m_ckboxAttnCarrierEn->GetValue();
        long attn_carrier;
        m_txtAttnCarrier->GetValue().ToLong(&attn_carrier);
        wxGetApp().m_attn_carrier = (int)attn_carrier;

        long FifoSize_ms;
        m_txtCtrlFifoSize->GetValue().ToLong(&FifoSize_ms);
        wxGetApp().m_fifoSize_ms = (int)FifoSize_ms;

        wxGetApp().m_txRxThreadHighPriority = m_ckboxTxRxThreadPriority->GetValue();
        g_dump_timing = m_ckboxTxRxDumpTiming->GetValue();
        g_dump_fifo_state = m_ckboxTxRxDumpFifoState->GetValue();
        g_verbose = m_ckboxVerbose->GetValue();
        g_freedv_verbose = m_ckboxFreeDVAPIVerbose->GetValue();

        wxGetApp().m_udp_enable     = m_ckbox_udp_enable->GetValue();
        long port;
        m_txt_udp_port->GetValue().ToLong(&port);
        wxGetApp().m_udp_port       = (int)port;

        wxGetApp().m_FreeDV700txClip = m_ckboxFreeDV700txClip->GetValue();
        wxGetApp().m_FreeDV700txBPF = m_ckboxFreeDV700txBPF->GetValue();
        wxGetApp().m_FreeDV700Combine = m_ckboxFreeDV700Combine->GetValue();

#ifdef __WXMSW__
        wxGetApp().m_debug_console = m_ckboxDebugConsole->GetValue();
#endif

        // PSK Reporter config
        wxGetApp().m_psk_enable = m_ckbox_psk_enable->GetValue();
        wxGetApp().m_psk_callsign = m_txt_callsign->GetValue();
        wxGetApp().m_psk_grid_square = m_txt_grid_square->GetValue();
        
        // Waterfall color
        if (m_waterfallColorScheme1->GetValue())
        {
            wxGetApp().m_waterfallColor = 0;
        }
        else if (m_waterfallColorScheme2->GetValue())
        {
            wxGetApp().m_waterfallColor = 1;
        }
        else if (m_waterfallColorScheme3->GetValue())
        {
            wxGetApp().m_waterfallColor = 2;
        }
        
        // Stats reset time
        long resetTime;
        m_statsResetTime->GetValue().ToLong(&resetTime);
        wxGetApp().m_statsResetTimeSec = resetTime;
        
        if (storePersistent) {
            pConfig->Write(wxT("/Data/CallSign"), wxGetApp().m_callSign);
#ifdef SHORT_VARICODE
            pConfig->Write(wxT("/Data/TextEncoding"), wxGetApp().m_textEncoding);
#endif

            pConfig->Write(wxT("/UDP/enable"), wxGetApp().m_udp_enable);
            pConfig->Write(wxT("/UDP/port"),  wxGetApp().m_udp_port);

            pConfig->Write(wxT("/FreeDV700/txClip"), wxGetApp().m_FreeDV700txClip);
            pConfig->Write(wxT("/FreeDV700/txBPF"), wxGetApp().m_FreeDV700txBPF);

            pConfig->Write(wxT("/Noise/noise_snr"),  wxGetApp().m_noise_snr);

#ifdef __WXMSW__
            pConfig->Write(wxT("/Debug/console"), wxGetApp().m_debug_console);
#endif
            pConfig->Write(wxT("/Debug/verbose"), g_verbose);
            pConfig->Write(wxT("/Debug/APIverbose"), g_freedv_verbose);

            pConfig->Write(wxT("/PSKReporter/Enable"), wxGetApp().m_psk_enable);
            pConfig->Write(wxT("/PSKReporter/Callsign"), wxGetApp().m_psk_callsign);
            pConfig->Write(wxT("/PSKReporter/GridSquare"), wxGetApp().m_psk_grid_square);
            
            pConfig->Write(wxT("/Stats/ResetTime"), wxGetApp().m_statsResetTimeSec);
            
            // Waterfall configuration
            pConfig->Write(wxT("/Waterfall/Color"), wxGetApp().m_waterfallColor);
            
            pConfig->Flush();
        }
    }
}

//-------------------------------------------------------------------------
// OnOK()
//-------------------------------------------------------------------------
void OptionsDlg::OnOK(wxCommandEvent& event)
{
    ExchangeData(EXCHANGE_DATA_OUT, true);
    //this->EndModal(wxID_OK);
    g_modal = false;
    EndModal(wxOK);
}

//-------------------------------------------------------------------------
// OnCancel()
//-------------------------------------------------------------------------
void OptionsDlg::OnCancel(wxCommandEvent& event)
{
    //this->EndModal(wxID_CANCEL);
    g_modal = false;
    EndModal(wxCANCEL);
}

//-------------------------------------------------------------------------
// OnApply()
//-------------------------------------------------------------------------
void OptionsDlg::OnApply(wxCommandEvent& event)
{
    ExchangeData(EXCHANGE_DATA_OUT, true);
}

//-------------------------------------------------------------------------
// OnInitDialog()
//-------------------------------------------------------------------------
void OptionsDlg::OnInitDialog(wxInitDialogEvent& event)
{
    ExchangeData(EXCHANGE_DATA_IN, false);
}

// immediately change flags rather using ExchangeData() so we can switch on and off at run time

void OptionsDlg::OnTestFrame(wxScrollEvent& event) {
    wxGetApp().m_testFrames    = m_ckboxTestFrame->GetValue();
}

void OptionsDlg::OnChannelNoise(wxScrollEvent& event) {
    wxGetApp().m_channel_noise = m_ckboxChannelNoise->GetValue();
    updateChannelNoiseState();
}


void OptionsDlg::OnChooseVoiceKeyerWaveFile(wxCommandEvent& event) {
     wxFileDialog openFileDialog(
                                 this,
                                 wxT("Voice Keyer wave file"),
                                 wxGetApp().m_txtVoiceKeyerWaveFilePath,
                                 wxEmptyString,
                                 wxT("WAV files (*.wav)|*.wav"),
                                 wxFD_OPEN
                                 );
     if(openFileDialog.ShowModal() == wxID_CANCEL) {
         return;     // the user changed their mind...
     }

     wxString fileName, extension;
     wxGetApp().m_txtVoiceKeyerWaveFile = openFileDialog.GetPath();
     wxFileName::SplitPath(wxGetApp().m_txtVoiceKeyerWaveFile, &wxGetApp().m_txtVoiceKeyerWaveFilePath, &fileName, &extension);
     m_txtCtrlVoiceKeyerWaveFile->SetValue(wxGetApp().m_txtVoiceKeyerWaveFile);
}

//  Run time update of carrier amplitude attenuation

void OptionsDlg::OnAttnCarrierEn(wxScrollEvent& event) {
    if (freedvInterface.isRunning())
    {
        long attn_carrier;
        m_txtAttnCarrier->GetValue().ToLong(&attn_carrier);
        wxGetApp().m_attn_carrier = (int)attn_carrier;

        /* uncheck -> checked, attenuate selected carrier */

        if (m_ckboxAttnCarrierEn->GetValue() && !wxGetApp().m_attn_carrier_en) {
            if (freedvInterface.isModeActive(FREEDV_MODE_700C)) {
                freedvInterface.setCarrierAmplitude(wxGetApp().m_attn_carrier, 0.25);
            } else {
                wxMessageBox("Carrier attenuation feature only works on 700C", wxT("Warning"), wxOK | wxICON_WARNING, this);
            }
        }

        /* checked -> unchecked, reset selected carrier */

        if (!m_ckboxAttnCarrierEn->GetValue() && wxGetApp().m_attn_carrier_en) {
            if (freedvInterface.isModeActive(FREEDV_MODE_700C)) {
                freedvInterface.setCarrierAmplitude(wxGetApp().m_attn_carrier, 1.0);
            }
        }
    }
       
    wxGetApp().m_attn_carrier_en = m_ckboxAttnCarrierEn->GetValue();
    updateAttnCarrierState();
}

void OptionsDlg::OnFreeDV700txClip(wxScrollEvent& event) {
    wxGetApp().m_FreeDV700txClip = m_ckboxFreeDV700txClip->GetValue();
}

void OptionsDlg::OnFreeDV700Combine(wxScrollEvent& event) {
    wxGetApp().m_FreeDV700Combine = m_ckboxFreeDV700Combine->GetValue();
}

void OptionsDlg::OnDebugConsole(wxScrollEvent& event) {
    wxGetApp().m_debug_console = m_ckboxDebugConsole->GetValue();
#ifdef __WXMSW__
    // somewhere to send printfs while developing, causes conmsole to pop up on Windows
    if (wxGetApp().m_debug_console) {
        int ret = AllocConsole();
        freopen("CONOUT$", "w", stdout); 
        freopen("CONOUT$", "w", stderr); 
        if (g_verbose) fprintf(stderr, "AllocConsole: %d m_debug_console: %d\n", ret, wxGetApp().m_debug_console);
    } 
#endif
}


void OptionsDlg::OnFifoReset(wxCommandEvent& event)
{
    g_infifo1_full = g_outfifo1_empty = g_infifo2_full = g_outfifo2_empty = 0;
    for (int i=0; i<4; i++) {
        g_AEstatus1[i] = g_AEstatus2[i] = 0;
    }
}

void OptionsDlg::OnUDPTest(wxCommandEvent& event)
{
    char s[80];
    snprintf(s, 80, "hello from FreeDV!");
    UDPSend(wxGetApp().m_udp_port, s, strlen(s)+1);
}

void OptionsDlg::updatePSKReporterState()
{
    if (!sessionActive_)
    {
        m_ckbox_psk_enable->Enable(true);
        if (m_ckbox_psk_enable->GetValue())
        {
            m_txtCtrlCallSign->Enable(false);
            m_txt_callsign->Enable(true);
            m_txt_grid_square->Enable(true);
        }
        else
        {
            m_txtCtrlCallSign->Enable(true);
            m_txt_callsign->Enable(false);
            m_txt_grid_square->Enable(false);
        }    
    }
    else
    {
        // Txt Msg/PSK Reporter options cannot be modified during a session.
        m_ckbox_psk_enable->Enable(false);
        m_txtCtrlCallSign->Enable(false);
        m_txt_callsign->Enable(false);
        m_txt_grid_square->Enable(false);
    }
}

void OptionsDlg::updateChannelNoiseState()
{
    m_txtNoiseSNR->Enable(m_ckboxChannelNoise->GetValue());
}

void OptionsDlg::updateAttnCarrierState()
{
    m_txtAttnCarrier->Enable(m_ckboxAttnCarrierEn->GetValue());
}

void OptionsDlg::updateToneState()
{
    m_txtToneFreqHz->Enable(m_ckboxTone->GetValue());
    m_txtToneAmplitude->Enable(m_ckboxTone->GetValue());
}

void OptionsDlg::updateUDPState()
{
    m_txt_udp_port->Enable(m_ckbox_udp_enable->GetValue());
    m_btn_udp_test->Enable(m_ckbox_udp_enable->GetValue());
}

void OptionsDlg::updateMultipleRxState()
{
    if (!sessionActive_)
    {
        m_ckboxMultipleRx->Enable(true);
        m_ckboxSingleRxThread->Enable(m_ckboxMultipleRx->GetValue());
    }
    else
    {
        // Multi-RX settings cannot be updated during a session.
        m_ckboxMultipleRx->Enable(false);
        m_ckboxSingleRxThread->Enable(false);
    }
}

void OptionsDlg::OnPSKReporterEnable(wxCommandEvent& event)
{
    updatePSKReporterState();
}

void OptionsDlg::OnToneStateEnable(wxCommandEvent& event)
{
    updateToneState();
}

void OptionsDlg::OnUDPStateEnable(wxCommandEvent& event)
{
    updateUDPState();
}

void OptionsDlg::OnMultipleRxEnable(wxCommandEvent& event)
{
    updateMultipleRxState();
}

void OptionsDlg::DisplayFifoPACounters() {
    const int STR_LENGTH = 256;
    
    char fifo_counters[STR_LENGTH];

    snprintf(fifo_counters, STR_LENGTH, "Fifos: infull1: %d outempty1: %d infull2: %d outempty2: %d", g_infifo1_full, g_outfifo1_empty, g_infifo2_full, g_outfifo2_empty);
    wxString fifo_counters_string(fifo_counters);
    m_textFifos->SetLabel(fifo_counters_string);

    char pa_counters1[STR_LENGTH];

    // input: underflow overflow output: underflow overflow
    snprintf(pa_counters1, STR_LENGTH, "Audio1: inUnderflow: %d inOverflow: %d outUnderflow %d outOverflow %d", g_AEstatus1[0], g_AEstatus1[1], g_AEstatus1[2], g_AEstatus1[3]);
    wxString pa_counters1_string(pa_counters1); m_textPA1->SetLabel(pa_counters1_string);

    char pa_counters2[STR_LENGTH];

    // input: underflow overflow output: underflow overflow
    snprintf(pa_counters2, STR_LENGTH, "Audio2: inUnderflow: %d inOverflow: %d outUnderflow %d outOverflow %d", g_AEstatus2[0], g_AEstatus2[1], g_AEstatus2[2], g_AEstatus2[3]);
    wxString pa_counters2_string(pa_counters2);
    m_textPA2->SetLabel(pa_counters2_string);
}
