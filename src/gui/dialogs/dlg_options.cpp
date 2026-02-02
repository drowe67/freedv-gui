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

#include <wx/gbsizer.h>
#include <wx/numformatter.h>
#include "dlg_options.h"

extern FreeDVInterface freedvInterface;

// PortAudio over/underflow counters

extern std::atomic<int>    g_infifo1_full;
extern std::atomic<int>    g_outfifo1_empty;
extern std::atomic<int>    g_infifo2_full;
extern std::atomic<int>    g_outfifo2_empty;
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
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
    
    if (wxGetApp().customConfigFileName != "")
    {
        SetTitle(wxString::Format("%s (%s)", title, wxGetApp().customConfigFileName));
    }
    
    sessionActive_ = false;
    
    wxPanel* panel = new wxPanel(this);
    
    wxBoxSizer* bSizer30;
    bSizer30 = new wxBoxSizer(wxVERTICAL);
    
    // Create notebook and tabs.
    m_notebook = new wxNotebook(panel, wxID_ANY);
    m_reportingTab = new wxPanel(m_notebook, wxID_ANY);
    m_rigControlTab = new wxPanel(m_notebook, wxID_ANY);
    m_displayTab = new wxPanel(m_notebook, wxID_ANY);
    m_keyerTab = new wxPanel(m_notebook, wxID_ANY);
    m_modemTab = new wxPanel(m_notebook, wxID_ANY);
    m_simulationTab = new wxPanel(m_notebook, wxID_ANY);
    m_debugTab = new wxPanel(m_notebook, wxID_ANY);
    
    m_notebook->AddPage(m_reportingTab, _("Reporting"));
    m_notebook->AddPage(m_rigControlTab, _("Rig Control"));
    m_notebook->AddPage(m_displayTab, _("Display"));
    m_notebook->AddPage(m_keyerTab, _("Audio"));
    m_notebook->AddPage(m_modemTab, _("Modem"));
    m_notebook->AddPage(m_simulationTab, _("Simulation"));
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

    m_txtCtrlCallSign = new wxTextCtrl(sb_textMsg, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    m_txtCtrlCallSign->SetToolTip(_("Text message that you can send along with your voice. Note that this does not have error correction and thus is not guaranteed to arrive at the receiving station."));
    sbSizer_callSign->Add(m_txtCtrlCallSign, 0, wxALL | wxEXPAND, 5);

    sizerReporting->Add(sbSizer_callSign,0, wxALL | wxEXPAND, 5);
 
    //----------------------------------------------------------
    // Reporting Options 
    //----------------------------------------------------------

    wxStaticBoxSizer* sbSizerReportingRows;
    wxBoxSizer* sbSizerReportingGeneral = new wxBoxSizer(wxHORIZONTAL);
    
    wxStaticBox* sbReporting = new wxStaticBox(m_reportingTab, wxID_ANY, _("Reporting"));
    
    sbSizerReportingRows = new wxStaticBoxSizer(sbReporting, wxVERTICAL);
    m_ckboxReportingEnable = new wxCheckBox(sbReporting, wxID_ANY, _("Enable Reporting"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizerReportingGeneral->Add(m_ckboxReportingEnable, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelPskCallsign = new wxStaticText(sbReporting, wxID_ANY, wxT("Callsign:"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizerReportingGeneral->Add(labelPskCallsign, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_txt_callsign = new wxTextCtrl(sbReporting, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(180,-1), 0, wxTextValidator(wxFILTER_ALPHANUMERIC));
    sbSizerReportingGeneral->Add(m_txt_callsign, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelPskGridSquare = new wxStaticText(sbReporting, wxID_ANY, wxT("Grid Square/Locator:"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizerReportingGeneral->Add(labelPskGridSquare, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_txt_grid_square = new wxTextCtrl(sbReporting, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(180,-1), 0, wxTextValidator(wxFILTER_ALPHANUMERIC));
    sbSizerReportingGeneral->Add(m_txt_grid_square, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    sbSizerReportingRows->Add(sbSizerReportingGeneral, 0, wxALL | wxEXPAND, 5);
    
    wxBoxSizer* sbSizerReportingManualFrequency = new wxBoxSizer(wxHORIZONTAL);
    m_ckboxManualFrequencyReporting = new wxCheckBox(sbReporting, wxID_ANY, _("Manual Frequency Reporting"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizerReportingManualFrequency->Add(m_ckboxManualFrequencyReporting, 0, wxALL | wxEXPAND, 5);
    sbSizerReportingRows->Add(sbSizerReportingManualFrequency, 0, wxALL | wxEXPAND, 5);
    
    // PSK Reporter options
    wxBoxSizer* sbSizerReportingPSK = new wxBoxSizer(wxHORIZONTAL);
    m_ckboxPskReporterEnable = new wxCheckBox(sbReporting, wxID_ANY, _("Report to PSK Reporter"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizerReportingPSK->Add(m_ckboxPskReporterEnable, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    sbSizerReportingRows->Add(sbSizerReportingPSK, 0, wxALL | wxEXPAND, 5);
    
    // FreeDV Reporter options
    wxBoxSizer* sbSizerReportingFreeDV = new wxBoxSizer(wxHORIZONTAL);
    m_ckboxFreeDVReporterEnable = new wxCheckBox(sbReporting, wxID_ANY, _("Report to FreeDV Reporter"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizerReportingFreeDV->Add(m_ckboxFreeDVReporterEnable, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);
    sbSizerReportingRows->Add(sbSizerReportingFreeDV, 0, wxALL | wxEXPAND, 5);
    
    // UDP reporting options
    wxBoxSizer* sbSizerReportingUDP = new wxBoxSizer(wxHORIZONTAL);
    m_ckboxUDPReportingEnable = new wxCheckBox(sbReporting, wxID_ANY, _("Enable QSO Logging"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    m_ckboxUDPReportingEnable->SetToolTip(_("Enables QSO logging using the WSJT-X support in your preferred logging program."));
    sbSizerReportingUDP->Add(m_ckboxUDPReportingEnable, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    wxStaticText* labelUDPHostName = new wxStaticText(sbReporting, wxID_ANY, wxT("IP Address:"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizerReportingUDP->Add(labelUDPHostName, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    m_udpHostname = new wxTextCtrl(sbReporting, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(150,-1), 0);
    sbSizerReportingUDP->Add(m_udpHostname, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    wxStaticText* labelUDPPort = new wxStaticText(sbReporting, wxID_ANY, wxT("Port:"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizerReportingUDP->Add(labelUDPPort, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    m_udpPort = new wxTextCtrl(sbReporting, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(60,-1), 0);
    sbSizerReportingUDP->Add(m_udpPort, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    sbSizerReportingRows->Add(sbSizerReportingUDP, 0, wxALL | wxEXPAND, 5);
    
    sizerReporting->Add(sbSizerReportingRows, 0, wxALL | wxEXPAND, 5);
    
    // FreeDV Reporter options that don't depend on Reporting checkboxes
    wxStaticBox* sbReportingFreeDV = new wxStaticBox(m_reportingTab, wxID_ANY, _("FreeDV Reporter"));
    wxStaticBoxSizer* sbSizerReportingFreeDVNoCall = new wxStaticBoxSizer(sbReportingFreeDV, wxHORIZONTAL);

    wxStaticText* labelFreeDVHostName = new wxStaticText(sbReportingFreeDV, wxID_ANY, wxT("Hostname:"), wxDefaultPosition, wxDefaultSize, 0);
    m_freedvReporterHostname = new wxTextCtrl(sbReportingFreeDV, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(250, -1), 0);
    sbSizerReportingFreeDVNoCall->Add(labelFreeDVHostName, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);
    sbSizerReportingFreeDVNoCall->Add(m_freedvReporterHostname, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_useMetricDistances = new wxCheckBox(sbReportingFreeDV, wxID_ANY, _("Distances in km"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizerReportingFreeDVNoCall->Add(m_useMetricDistances, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_useCardinalDirections = new wxCheckBox(sbReportingFreeDV, wxID_ANY, _("Show cardinal directions"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizerReportingFreeDVNoCall->Add(m_useCardinalDirections, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_ckboxFreeDVReporterForceReceiveOnly = new wxCheckBox(sbReportingFreeDV, wxID_ANY, _("Force RX Only reporting"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizerReportingFreeDVNoCall->Add(m_ckboxFreeDVReporterForceReceiveOnly, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    sizerReporting->Add(sbSizerReportingFreeDVNoCall, 0, wxALL | wxEXPAND, 5);

    // Callsign list settings
    wxStaticBoxSizer* sbSizer_callsign_list;
    wxStaticBox* sb_callsignList = new wxStaticBox(m_reportingTab, wxID_ANY, _("Callsign List"));
    sbSizer_callsign_list = new wxStaticBoxSizer(sb_callsignList, wxHORIZONTAL);
    m_ckbox_use_utc_time = new wxCheckBox(sb_callsignList, wxID_ANY, _("Use UTC Time"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_callsign_list->Add(m_ckbox_use_utc_time, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    sizerReporting->Add(sbSizer_callsign_list,0, wxALL | wxEXPAND, 5);
    
    m_reportingTab->SetSizer(sizerReporting);
    
    // Rig Control tab
    wxBoxSizer* sizerRigControl = new wxBoxSizer(wxVERTICAL);
    
    //------------------------------
    // Rig Control options
    //------------------------------
    
    wxStaticBoxSizer* sbSizer_ptt;
    wxStaticBox *sb_ptt = new wxStaticBox(m_rigControlTab, wxID_ANY, _("PTT Options"));
    sbSizer_ptt = new wxStaticBoxSizer(sb_ptt, wxVERTICAL);
    
    m_ckboxEnableSpacebarForPTT = new wxCheckBox(sb_ptt, wxID_ANY, _("Enable Space key for PTT"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_ptt->Add(m_ckboxEnableSpacebarForPTT, 0, wxALL | wxALIGN_LEFT, 5);

    wxSizer* txRxDelaySizer = new wxBoxSizer(wxHORIZONTAL);
    
    auto txRxDelayLabel = new wxStaticText(sb_ptt, wxID_ANY, _("TX/RX Delay (milliseconds): "));
    txRxDelaySizer->Add(txRxDelayLabel, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    m_txtTxRxDelayMilliseconds = new wxTextCtrl(sb_ptt, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    m_txtTxRxDelayMilliseconds->SetToolTip(_("The amount of time to wait between toggling PTT and stopping/starting TX audio in milliseconds."));
    txRxDelaySizer->Add(m_txtTxRxDelayMilliseconds, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    sbSizer_ptt->Add(txRxDelaySizer, 0, wxALL, 0);
    sizerRigControl->Add(sbSizer_ptt,0, wxALL | wxEXPAND, 5);
    
    wxStaticBoxSizer* sbSizer_hamlib;
    wxStaticBox *sb_hamlib = new wxStaticBox(m_rigControlTab, wxID_ANY, _("Frequency/Mode Control Options"));
    sbSizer_hamlib = new wxStaticBoxSizer(sb_hamlib, wxVERTICAL);
    
    wxSizer* freqModeSizer = new wxBoxSizer(wxHORIZONTAL);
    m_ckboxEnableFreqModeChanges = new wxRadioButton(sb_hamlib, wxID_ANY, _("Enable frequency and mode changes"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    freqModeSizer->Add(m_ckboxEnableFreqModeChanges, 0, wxALL | wxALIGN_LEFT, 5);
    
    m_ckboxEnableFreqChangesOnly = new wxRadioButton(sb_hamlib, wxID_ANY, _("Enable frequency changes only"), wxDefaultPosition, wxDefaultSize);
    freqModeSizer->Add(m_ckboxEnableFreqChangesOnly, 0, wxALL | wxALIGN_LEFT, 5);
    
    m_ckboxNoFreqModeChanges = new wxRadioButton(sb_hamlib, wxID_ANY, _("No frequency or mode changes"), wxDefaultPosition, wxDefaultSize);
    freqModeSizer->Add(m_ckboxNoFreqModeChanges, 0, wxALL | wxALIGN_LEFT, 5);
    
    sbSizer_hamlib->Add(freqModeSizer, 0, wxALL | wxALIGN_LEFT, 5);
    
    m_ckboxUseAnalogModes = new wxCheckBox(sb_hamlib, wxID_ANY, _("Use USB/LSB instead of DIGU/DIGL"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_hamlib->Add(m_ckboxUseAnalogModes, 0, wxALL | wxALIGN_LEFT, 5);
    
    m_ckboxFrequencyEntryAsKHz = new wxCheckBox(sb_hamlib, wxID_ANY, _("Frequency entry in kHz"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_hamlib->Add(m_ckboxFrequencyEntryAsKHz, 0, wxALL | wxALIGN_LEFT, 5);

    sizerRigControl->Add(sbSizer_hamlib,0, wxALL | wxEXPAND, 5);
    
    wxStaticBoxSizer* sbSizer_freqList;
    wxStaticBox *sb_freqList = new wxStaticBox(m_rigControlTab, wxID_ANY, _("Predefined Frequencies"));
    sbSizer_freqList = new wxStaticBoxSizer(sb_freqList, wxVERTICAL);
    
    wxGridBagSizer* gridSizer = new wxGridBagSizer(5, 5);
    
    m_freqList = new wxListBox(sb_freqList, wxID_ANY, wxDefaultPosition, wxSize(350,150), 0, NULL, wxLB_SINGLE | wxLB_NEEDED_SB);
    gridSizer->Add(m_freqList, wxGBPosition(0, 0), wxGBSpan(5, 2), wxEXPAND);

    const int FREQ_LIST_BUTTON_WIDTH = 100; 
    const int FREQ_LIST_BUTTON_HEIGHT = -1;
    m_freqListAdd = new wxButton(sb_freqList, wxID_ANY, _("Add"), wxDefaultPosition, wxSize(FREQ_LIST_BUTTON_WIDTH,FREQ_LIST_BUTTON_HEIGHT), 0);
    gridSizer->Add(m_freqListAdd, wxGBPosition(0, 2), wxDefaultSpan, wxEXPAND);
    m_freqListRemove = new wxButton(sb_freqList, wxID_ANY, _("Remove"), wxDefaultPosition, wxSize(FREQ_LIST_BUTTON_WIDTH,FREQ_LIST_BUTTON_HEIGHT), 0);
    gridSizer->Add(m_freqListRemove, wxGBPosition(1, 2), wxDefaultSpan, wxEXPAND);
    m_freqListMoveUp = new wxButton(sb_freqList, wxID_ANY, _("Move Up"), wxDefaultPosition, wxSize(FREQ_LIST_BUTTON_WIDTH,FREQ_LIST_BUTTON_HEIGHT), 0);
    gridSizer->Add(m_freqListMoveUp, wxGBPosition(2, 2), wxDefaultSpan, wxEXPAND);
    m_freqListMoveDown = new wxButton(sb_freqList, wxID_ANY, _("Move Down"), wxDefaultPosition, wxSize(FREQ_LIST_BUTTON_WIDTH,FREQ_LIST_BUTTON_HEIGHT), 0);
    gridSizer->Add(m_freqListMoveDown, wxGBPosition(3, 2), wxDefaultSpan, wxEXPAND);
    
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
    {
        m_labelEnterFreq = new wxStaticText(sb_freqList, wxID_ANY, wxT("Enter frequency (kHz):"), wxDefaultPosition, wxDefaultSize, 0);
    }
    else
    {
        m_labelEnterFreq = new wxStaticText(sb_freqList, wxID_ANY, wxT("Enter frequency (MHz):"), wxDefaultPosition, wxDefaultSize, 0);
    }
    gridSizer->Add(m_labelEnterFreq, wxGBPosition(5, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
    
    m_txtCtrlNewFrequency = new wxTextCtrl(sb_freqList, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    gridSizer->Add(m_txtCtrlNewFrequency, wxGBPosition(5, 1), wxGBSpan(1, 2), wxEXPAND);
    
    sbSizer_freqList->Add(gridSizer, 0, wxALL, 5);
    
    sizerRigControl->Add(sbSizer_freqList,0, wxALL | wxEXPAND, 5);
    
    m_rigControlTab->SetSizer(sizerRigControl);
        
    // Display tab
    wxBoxSizer* sizerDisplay = new wxBoxSizer(wxVERTICAL);
    
    //----------------------------------------------------------
    // Waterfall color 
    //----------------------------------------------------------
    wxStaticBox* sb_waterfall = new wxStaticBox(m_displayTab, wxID_ANY, _("Waterfall Style"));
    wxStaticBoxSizer* sbSizer_waterfallColor =  new wxStaticBoxSizer(sb_waterfall, wxHORIZONTAL);
    
    m_waterfallColorScheme1 = new wxRadioButton(sb_waterfall, wxID_ANY, _("Multicolor"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    sbSizer_waterfallColor->Add(m_waterfallColorScheme1, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    m_waterfallColorScheme2 = new wxRadioButton(sb_waterfall, wxID_ANY, _("Black && White"), wxDefaultPosition, wxDefaultSize);
    sbSizer_waterfallColor->Add(m_waterfallColorScheme2, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    m_waterfallColorScheme3 = new wxRadioButton(sb_waterfall, wxID_ANY, _("Blue Tint"), wxDefaultPosition, wxDefaultSize);
    sbSizer_waterfallColor->Add(m_waterfallColorScheme3, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    sizerDisplay->Add(sbSizer_waterfallColor, 0, wxALL | wxEXPAND, 5);

    //----------------------------------------------------------
    // FreeDV Reporter colors
    //----------------------------------------------------------
    wxStaticBox* sb_reporterColor = new wxStaticBox(m_displayTab, wxID_ANY, _("FreeDV Reporter colors"));
    wxStaticBoxSizer* sbSizer_reporterColor =  new wxStaticBoxSizer(sb_reporterColor, wxVERTICAL);

    wxFlexGridSizer* reporterColorSizer = new wxFlexGridSizer(5, wxSize(5, 5));

    // TX colors
    wxStaticText* labelReporterTxStation = new wxStaticText(sb_reporterColor, wxID_ANY, wxT("TX Stations:"), wxDefaultPosition, wxDefaultSize, 0);
    reporterColorSizer->Add(labelReporterTxStation, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* labelReporterTxBackgroundColor = new wxStaticText(sb_reporterColor, wxID_ANY, wxT("Background"), wxDefaultPosition, wxDefaultSize, 0);
    reporterColorSizer->Add(labelReporterTxBackgroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_freedvReporterTxBackgroundColor = new wxColourPickerCtrl(sb_reporterColor, wxID_ANY);
    reporterColorSizer->Add(m_freedvReporterTxBackgroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelReporterTxForegroundColor = new wxStaticText(sb_reporterColor, wxID_ANY, wxT("Foreground"), wxDefaultPosition, wxDefaultSize, 0);
    reporterColorSizer->Add(labelReporterTxForegroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_freedvReporterTxForegroundColor = new wxColourPickerCtrl(sb_reporterColor, wxID_ANY);
    reporterColorSizer->Add(m_freedvReporterTxForegroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    // RX colors
    wxStaticText* labelReporterRxStation = new wxStaticText(sb_reporterColor, wxID_ANY, wxT("RX Stations:"), wxDefaultPosition, wxDefaultSize, 0);
    reporterColorSizer->Add(labelReporterRxStation, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* labelReporterRxBackgroundColor = new wxStaticText(sb_reporterColor, wxID_ANY, wxT("Background"), wxDefaultPosition, wxDefaultSize, 0);
    reporterColorSizer->Add(labelReporterRxBackgroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_freedvReporterRxBackgroundColor = new wxColourPickerCtrl(sb_reporterColor, wxID_ANY);
    reporterColorSizer->Add(m_freedvReporterRxBackgroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelReporterRxForegroundColor = new wxStaticText(sb_reporterColor, wxID_ANY, wxT("Foreground"), wxDefaultPosition, wxDefaultSize, 0);
    reporterColorSizer->Add(labelReporterRxForegroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_freedvReporterRxForegroundColor = new wxColourPickerCtrl(sb_reporterColor, wxID_ANY);
    reporterColorSizer->Add(m_freedvReporterRxForegroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    // Message colors
    wxStaticText* labelReporterMsgStation = new wxStaticText(sb_reporterColor, wxID_ANY, wxT("Message updates:"), wxDefaultPosition, wxDefaultSize, 0);
    reporterColorSizer->Add(labelReporterMsgStation, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* labelReporterMsgBackgroundColor = new wxStaticText(sb_reporterColor, wxID_ANY, wxT("Background"), wxDefaultPosition, wxDefaultSize, 0);
    reporterColorSizer->Add(labelReporterMsgBackgroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_freedvReporterMsgBackgroundColor = new wxColourPickerCtrl(sb_reporterColor, wxID_ANY);
    reporterColorSizer->Add(m_freedvReporterMsgBackgroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelReporterMsgForegroundColor = new wxStaticText(sb_reporterColor, wxID_ANY, wxT("Foreground"), wxDefaultPosition, wxDefaultSize, 0);
    reporterColorSizer->Add(labelReporterMsgForegroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_freedvReporterMsgForegroundColor = new wxColourPickerCtrl(sb_reporterColor, wxID_ANY);
    reporterColorSizer->Add(m_freedvReporterMsgForegroundColor, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    sbSizer_reporterColor->Add(reporterColorSizer, 0, wxALL, 5);

    sizerDisplay->Add(sbSizer_reporterColor, 0, wxALL | wxEXPAND, 5);
    
    // Plot settings
    wxStaticBox* sb_PlotSettings = new wxStaticBox(m_displayTab, wxID_ANY, _("Plot settings"));
    wxStaticBoxSizer* sbSizer_PlotSettings =  new wxStaticBoxSizer(sb_PlotSettings, wxVERTICAL);

    wxBoxSizer* spectrumPanelControlSizer = new wxBoxSizer(wxHORIZONTAL);
    
    wxStaticText* labelAveraging = new wxStaticText(sb_PlotSettings, wxID_ANY, wxT("Average spectrum plot across"), wxDefaultPosition, wxDefaultSize, 0);
    spectrumPanelControlSizer->Add(labelAveraging, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxString samplingChoices[] = {
        "1",
        "2",
        "3"
    };
    m_cbxNumSpectrumAveraging = new wxComboBox(sb_PlotSettings, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 3, samplingChoices, wxCB_DROPDOWN | wxCB_READONLY);
    m_cbxNumSpectrumAveraging->SetSelection(wxGetApp().appConfiguration.currentSpectrumAveraging);
    spectrumPanelControlSizer->Add(m_cbxNumSpectrumAveraging, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelSamples = new wxStaticText(sb_PlotSettings, wxID_ANY, wxT("sample(s)"), wxDefaultPosition, wxDefaultSize, 0);
    spectrumPanelControlSizer->Add(labelSamples, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    sbSizer_PlotSettings->Add(spectrumPanelControlSizer, 0, wxALL | wxEXPAND, 5);
    sizerDisplay->Add(sbSizer_PlotSettings, 0, wxALL | wxEXPAND, 5);
    
    m_displayTab->SetSizer(sizerDisplay);
    
    // Voice Keyer tab
    wxBoxSizer* sizerKeyer = new wxBoxSizer(wxVERTICAL);
    
    //----------------------------------------------------------------------
    // Voice Keyer 
    //----------------------------------------------------------------------

    wxStaticBox* voiceKeyerBox = new wxStaticBox(m_keyerTab, wxID_ANY, _("Voice Keyer"));
    wxStaticBoxSizer* staticBoxSizer28a = new wxStaticBoxSizer(voiceKeyerBox, wxVERTICAL);

    wxBoxSizer* voiceKeyerSizer1 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* voiceKeyerSizer2 = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText *m_staticText28b = new wxStaticText(voiceKeyerBox, wxID_ANY, _("File location: "), wxDefaultPosition, wxDefaultSize, 0);
    voiceKeyerSizer1->Add(m_staticText28b, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtCtrlVoiceKeyerWaveFilePath = new wxTextCtrl(voiceKeyerBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(450,-1), 0);
    m_txtCtrlVoiceKeyerWaveFilePath->SetToolTip(_("Path to Voice Keyer audio files"));
    voiceKeyerSizer1->Add(m_txtCtrlVoiceKeyerWaveFilePath, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_buttonChooseVoiceKeyerWaveFilePath = new wxButton(voiceKeyerBox, wxID_APPLY, _("Choose"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_buttonChooseVoiceKeyerWaveFilePath->SetMinSize(wxSize(120, -1));
    voiceKeyerSizer1->Add(m_buttonChooseVoiceKeyerWaveFilePath, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *m_staticText28c = new wxStaticText(voiceKeyerBox, wxID_ANY, _("Rx Pause:"), wxDefaultPosition, wxDefaultSize, 0);
    voiceKeyerSizer2->Add(m_staticText28c, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtCtrlVoiceKeyerRxPause = new wxTextCtrl(voiceKeyerBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(50,-1), 0);
    m_txtCtrlVoiceKeyerRxPause->SetToolTip(_("How long to wait in Rx mode before repeat"));
    voiceKeyerSizer2->Add(m_txtCtrlVoiceKeyerRxPause, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *m_staticText28d = new wxStaticText(voiceKeyerBox, wxID_ANY, _("Repeats:"), wxDefaultPosition, wxDefaultSize, 0);
    voiceKeyerSizer2->Add(m_staticText28d, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtCtrlVoiceKeyerRepeats = new wxTextCtrl(voiceKeyerBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(50,-1), 0);
    m_txtCtrlVoiceKeyerRepeats->SetToolTip(_("How long to wait in Rx mode before repeat"));
    voiceKeyerSizer2->Add(m_txtCtrlVoiceKeyerRepeats, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    staticBoxSizer28a->Add(voiceKeyerSizer1);
    staticBoxSizer28a->Add(voiceKeyerSizer2);

    sizerKeyer->Add(staticBoxSizer28a,0, wxALL | wxEXPAND, 5);
    
    m_keyerTab->SetSizer(sizerKeyer);
    
    //------------------------------
    // Quick Record
    //------------------------------
    
    wxStaticBox* quickRecordBox = new wxStaticBox(m_keyerTab, wxID_ANY, _("Quick Record"));
    wxStaticBoxSizer* sbsQuickRecord = new wxStaticBoxSizer(quickRecordBox, wxVERTICAL);

    wxFlexGridSizer* quickRecordSizer = new wxFlexGridSizer(2, 3, 5, 5);
    quickRecordSizer->AddGrowableCol(1);

    wxStaticText *staticTextQRPath = new wxStaticText(quickRecordBox, wxID_ANY, _("Location to save raw recordings: "), wxDefaultPosition, wxDefaultSize, 0);
    quickRecordSizer->Add(staticTextQRPath, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtCtrlQuickRecordRawPath = new wxTextCtrl(quickRecordBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(450,-1), 0);
    m_txtCtrlQuickRecordRawPath->SetToolTip(_("Location which to save raw recordings started via the Record button in the main window."));
    quickRecordSizer->Add(m_txtCtrlQuickRecordRawPath, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_buttonChooseQuickRecordRawPath = new wxButton(quickRecordBox, wxID_APPLY, _("Choose"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_buttonChooseQuickRecordRawPath->SetMinSize(wxSize(120, -1));
    quickRecordSizer->Add(m_buttonChooseQuickRecordRawPath, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    staticTextQRPath = new wxStaticText(quickRecordBox, wxID_ANY, _("Location to save decoded recordings: "), wxDefaultPosition, wxDefaultSize, 0);
    quickRecordSizer->Add(staticTextQRPath, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtCtrlQuickRecordDecodedPath = new wxTextCtrl(quickRecordBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(450,-1), 0);
    m_txtCtrlQuickRecordDecodedPath->SetToolTip(_("Location which to save decoded recordings started via the Record button in the main window."));
    quickRecordSizer->Add(m_txtCtrlQuickRecordDecodedPath, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_buttonChooseQuickRecordDecodedPath = new wxButton(quickRecordBox, wxID_APPLY, _("Choose"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_buttonChooseQuickRecordDecodedPath->SetMinSize(wxSize(120, -1));
    quickRecordSizer->Add(m_buttonChooseQuickRecordDecodedPath, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    sbsQuickRecord->Add(quickRecordSizer);
    
    sizerKeyer->Add(sbsQuickRecord,0, wxALL | wxEXPAND, 5);
    
    // Modem tab
    wxBoxSizer* sizerModem = new wxBoxSizer(wxVERTICAL);
    
    //------------------------------
    // FreeDV 700 Options
    //------------------------------

    wxStaticBoxSizer* sbSizer_freedv700;
    wxStaticBox *sb_freedv700 = new wxStaticBox(m_modemTab, wxID_ANY, _("Modem Options"));
    sbSizer_freedv700 = new wxStaticBoxSizer(sb_freedv700, wxHORIZONTAL);

    m_ckboxFreeDV700txClip = new wxCheckBox(sb_freedv700, wxID_ANY, _("Clipping"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_freedv700->Add(m_ckboxFreeDV700txClip, 0, wxALL | wxALIGN_LEFT, 5);

    m_ckboxFreeDV700txBPF = new wxCheckBox(sb_freedv700, wxID_ANY, _("TX Band Pass Filter"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_freedv700->Add(m_ckboxFreeDV700txBPF, 0, wxALL | wxALIGN_LEFT, 5);

    m_ckboxEnableLegacyModes = new wxCheckBox(sb_freedv700, wxID_ANY, _("Enable Legacy Modes"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_freedv700->Add(m_ckboxEnableLegacyModes, 0, wxALL | wxALIGN_LEFT, 5);
    
    sizerModem->Add(sbSizer_freedv700, 0, wxALL|wxEXPAND, 5);

    //------------------------------
    // Half/Full duplex selection
    //------------------------------

    wxStaticBox *sb_duplex = new wxStaticBox(m_modemTab, wxID_ANY, _("Half/Full Duplex Operation"));
    wxStaticBoxSizer* sbSizer_duplex = new wxStaticBoxSizer(sb_duplex, wxHORIZONTAL);

    m_ckHalfDuplex = new wxCheckBox(sb_duplex, wxID_ANY, _("Half Duplex"), wxDefaultPosition, wxSize(-1,-1), 0);
    sbSizer_duplex->Add(m_ckHalfDuplex, 0, wxALL | wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);

    sizerModem->Add(sbSizer_duplex,0, wxALL | wxEXPAND, 5);

    //------------------------------
    // Multiple RX selection
    //------------------------------
    wxStaticBox *sb_multirx = new wxStaticBox(m_modemTab, wxID_ANY, _("Multiple RX Operation"));
    wxStaticBoxSizer* sbSizer_multirx = new wxStaticBoxSizer(sb_multirx, wxVERTICAL);

    wxBoxSizer* sbSizer_simultaneousDecode = new wxBoxSizer(wxHORIZONTAL);
    m_ckboxMultipleRx = new wxCheckBox(sb_multirx, wxID_ANY, _("Simultaneously Decode All HF Modes"), wxDefaultPosition, wxSize(-1,-1), 0);
    sbSizer_simultaneousDecode->Add(m_ckboxMultipleRx, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    sbSizer_multirx->Add(sbSizer_simultaneousDecode, 0, wxALIGN_LEFT, 0);
    
    wxBoxSizer* sbSizer_singleThread = new wxBoxSizer(wxHORIZONTAL);
    m_ckboxSingleRxThread = new wxCheckBox(sb_multirx, wxID_ANY, _("Use single thread for multiple RX operation"), wxDefaultPosition, wxSize(-1,-1), 0);
    sbSizer_singleThread->Add(m_ckboxSingleRxThread, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    sbSizer_multirx->Add(sbSizer_singleThread, 0, wxALIGN_LEFT, 0);
    
    sizerModem->Add(sbSizer_multirx,0, wxALL|wxEXPAND, 5);
    
    wxStaticBox *sb_modemstats = new wxStaticBox(m_modemTab, wxID_ANY, _("Modem Statistics"));
    wxStaticBoxSizer* sbSizer_modemstats = new wxStaticBoxSizer(sb_modemstats, wxVERTICAL);

    wxBoxSizer* sbSizer_statsResetTime = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText *m_staticTextResetTime = new wxStaticText(sb_modemstats, wxID_ANY, _("Time before resetting stats (in seconds):"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_statsResetTime->Add(m_staticTextResetTime, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_statsResetTime = new wxTextCtrl(sb_modemstats, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(50,-1), 0, wxTextValidator(wxFILTER_DIGITS));
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

    m_ckboxTestFrame = new wxCheckBox(sb_testFrames, wxID_ANY, _("Test Frames"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_testFrames->Add(m_ckboxTestFrame, 0, wxALL | wxALIGN_LEFT, 5);

    wxBoxSizer* channelNoiseSizer = new wxBoxSizer(wxHORIZONTAL);

    m_ckboxChannelNoise = new wxCheckBox(sb_testFrames, wxID_ANY, _("Channel Noise"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    channelNoiseSizer->Add(m_ckboxChannelNoise, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *channelNoiseDbLabel = new wxStaticText(sb_testFrames, wxID_ANY, _("SNR (dB):"), wxDefaultPosition, wxDefaultSize, 0);
    channelNoiseSizer->Add(channelNoiseDbLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtNoiseSNR = new wxTextCtrl(sb_testFrames, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(60,-1), 0, wxTextValidator(wxFILTER_NUMERIC));
    channelNoiseSizer->Add(m_txtNoiseSNR, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    sbSizer_testFrames->Add(channelNoiseSizer);

    wxBoxSizer* attnCarrierSizer = new wxBoxSizer(wxHORIZONTAL);

    m_ckboxAttnCarrierEn = new wxCheckBox(sb_testFrames, wxID_ANY, _("Attn Carrier"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    attnCarrierSizer->Add(m_ckboxAttnCarrierEn, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *carrierLabel = new wxStaticText(sb_testFrames, wxID_ANY, _("Carrier:"), wxDefaultPosition, wxDefaultSize, 0);
    attnCarrierSizer->Add(carrierLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_txtAttnCarrier = new wxTextCtrl(sb_testFrames, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(60,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    attnCarrierSizer->Add(m_txtAttnCarrier, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    sbSizer_testFrames->Add(attnCarrierSizer);

    sizerSimulation->Add(sbSizer_testFrames,0, wxALL|wxEXPAND, 5);

    //------------------------------
    // Interfering tone
    //------------------------------

    wxStaticBoxSizer* sbSizer_tone;
    wxStaticBox *sb_tone = new wxStaticBox(m_simulationTab, wxID_ANY, _("Simulated Interference Tone"));
    sbSizer_tone = new wxStaticBoxSizer(sb_tone, wxHORIZONTAL);

    m_ckboxTone = new wxCheckBox(sb_tone, wxID_ANY, _("Tone"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_tone->Add(m_ckboxTone, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *toneFreqLabel = new wxStaticText(sb_tone, wxID_ANY, _("Freq (Hz):"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_tone->Add(toneFreqLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtToneFreqHz = new wxTextCtrl(sb_tone, wxID_ANY,  "1000", wxDefaultPosition, wxSize(90,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_tone->Add(m_txtToneFreqHz, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *m_staticTextta = new wxStaticText(sb_tone, wxID_ANY, _("Amplitude (pk): "), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_tone->Add(m_staticTextta, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_txtToneAmplitude = new wxTextCtrl(sb_tone, wxID_ANY,  "1000", wxDefaultPosition, wxSize(90,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_tone->Add(m_txtToneAmplitude, 0, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);

    sizerSimulation->Add(sbSizer_tone,0, wxALL|wxEXPAND, 5);

    m_simulationTab->SetSizer(sizerSimulation);
        
    // Debug tab
    wxBoxSizer* sizerDebug = new wxBoxSizer(wxVERTICAL);
    
#ifdef __WXMSW__
    //------------------------------
    // debug console, for WIndows build make console pop up for debug messages
    //------------------------------

    wxStaticBoxSizer* sbSizer_console;
    wxStaticBox *sb_console = new wxStaticBox(m_debugTab, wxID_ANY, _("Debug: Windows"));
    sbSizer_console = new wxStaticBoxSizer(sb_console, wxHORIZONTAL);

    m_ckboxDebugConsole = new wxCheckBox(sb_console, wxID_ANY, _("Show Console"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
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

    wxStaticText *m_staticTextFifo1 = new wxStaticText(sb_fifo, wxID_ANY, _("Fifo Size (ms):"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_fifo1->Add(m_staticTextFifo1, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    m_txtCtrlFifoSize = new wxTextCtrl(sb_fifo, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80,-1), 0);
    sbSizer_fifo1->Add(m_txtCtrlFifoSize, 0, wxALL, 5);

    // Reset stats button
    
    m_BtnFifoReset = new wxButton(sb_fifo, wxID_ANY, _("Reset"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_fifo1->Add(m_BtnFifoReset, 0,  wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
    sbSizer_fifo->Add(sbSizer_fifo1);

    // text lines with fifo counters
    
    m_textPA1 = new wxStaticText(sb_fifo, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_fifo->Add(m_textPA1, 0, wxALIGN_LEFT, 1);
    m_textPA2 = new wxStaticText(sb_fifo, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_fifo->Add(m_textPA2, 0, wxALIGN_LEFT, 1);

    m_textFifos = new wxStaticText(sb_fifo, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_fifo->Add(m_textFifos, 0, wxALIGN_LEFT, 1);

    // 2nd line
    
    wxStaticBox* sb_fifo2 = new wxStaticBox(m_debugTab, wxID_ANY, _("Debug: Application Options"));
    wxStaticBoxSizer* sbSizer_fifo2 = new wxStaticBoxSizer(sb_fifo2, wxVERTICAL);

    wxBoxSizer* sbDebugOptionsSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sbDebugOptionsSizer2 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sbDebugOptionsSizer3 = new wxBoxSizer(wxHORIZONTAL);

    m_ckboxVerbose = new wxCheckBox(sb_fifo2, wxID_ANY, _("Verbose"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer->Add(m_ckboxVerbose, 0, wxALL | wxALIGN_LEFT, 5);   
    m_ckboxTxRxThreadPriority = new wxCheckBox(sb_fifo2, wxID_ANY, _("txRxThreadPriority"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer->Add(m_ckboxTxRxThreadPriority, 0, wxALL | wxALIGN_LEFT, 5);
    m_ckboxTxRxDumpTiming = new wxCheckBox(sb_fifo2, wxID_ANY, _("txRxDumpTiming"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer->Add(m_ckboxTxRxDumpTiming, 0, wxALL | wxALIGN_LEFT, 5);
    
    m_ckboxTxRxDumpFifoState = new wxCheckBox(sb_fifo2, wxID_ANY, _("txRxDumpFifoState"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer2->Add(m_ckboxTxRxDumpFifoState, 0, wxALL | wxALIGN_LEFT, 5);   
    m_ckboxFreeDVAPIVerbose = new wxCheckBox(sb_fifo2, wxID_ANY, _("APiVerbose"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer2->Add(m_ckboxFreeDVAPIVerbose, 0, wxALL | wxALIGN_LEFT, 5);   
    m_showDecodeStats = new wxCheckBox(sb_fifo2, wxID_ANY, _("Show Decode Stats"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer2->Add(m_showDecodeStats, 0, wxALL | wxALIGN_LEFT, 5); 
    
    m_experimentalFeatures = new wxCheckBox(sb_fifo2, wxID_ANY, _("Enable Experimental Features"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbDebugOptionsSizer3->Add(m_experimentalFeatures, 0, wxALL | wxALIGN_LEFT, 5);   

    sbSizer_fifo2->Add(sbDebugOptionsSizer, 0, wxALL | wxEXPAND | 0);
    sbSizer_fifo2->Add(sbDebugOptionsSizer2, 0, wxALL | wxEXPAND | 0);
    sbSizer_fifo2->Add(sbDebugOptionsSizer3, 0, wxALL | wxEXPAND | 0);

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
#if 0
    m_txtCtrlCallSign->MoveBeforeInTabOrder(m_ckboxReportingEnable);
    m_ckboxReportingEnable->MoveBeforeInTabOrder(m_txt_callsign);
    m_txt_callsign->MoveBeforeInTabOrder(m_txt_grid_square);
    
    m_txt_grid_square->MoveBeforeInTabOrder(m_ckboxManualFrequencyReporting);
    m_ckboxManualFrequencyReporting->MoveBeforeInTabOrder(m_ckboxPskReporterEnable);
    m_ckboxPskReporterEnable->MoveBeforeInTabOrder(m_ckboxFreeDVReporterEnable);
    m_ckboxFreeDVReporterEnable->MoveBeforeInTabOrder(m_freedvReporterHostname);
    m_freedvReporterHostname->MoveBeforeInTabOrder(m_useMetricDistances);
    
    m_waterfallColorScheme1->MoveBeforeInTabOrder(m_waterfallColorScheme2);
    m_waterfallColorScheme2->MoveBeforeInTabOrder(m_waterfallColorScheme3);
    
    m_txtCtrlVoiceKeyerWaveFilePath->MoveBeforeInTabOrder(m_buttonChooseVoiceKeyerWaveFilePath);
    m_buttonChooseVoiceKeyerWaveFilePath->MoveBeforeInTabOrder(m_txtCtrlVoiceKeyerRxPause);
    m_txtCtrlVoiceKeyerRxPause->MoveBeforeInTabOrder(m_txtCtrlVoiceKeyerRepeats);
    
    m_ckboxFreeDV700txClip->MoveBeforeInTabOrder(m_ckboxFreeDV700txBPF);
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
    
    // Tab ordering for Debug tab.    
#ifdef __WXMSW__
    sb_console->MoveBeforeInTabOrder(sb_fifo);
#endif // __WXMSW__
    sb_fifo->MoveBeforeInTabOrder(sb_fifo2);

    m_txtCtrlFifoSize->MoveBeforeInTabOrder(m_BtnFifoReset);
    m_BtnFifoReset->MoveBeforeInTabOrder(m_ckboxVerbose);

    m_ckboxVerbose->MoveBeforeInTabOrder(m_ckboxTxRxThreadPriority);
    m_ckboxTxRxThreadPriority->MoveBeforeInTabOrder(m_ckboxTxRxDumpTiming);
    m_ckboxTxRxDumpTiming->MoveBeforeInTabOrder(m_ckboxTxRxDumpFifoState);
    m_ckboxTxRxDumpFifoState->MoveBeforeInTabOrder(m_ckboxFreeDVAPIVerbose);

#ifdef __WXMSW__
    m_ckboxDebugConsole->MoveBeforeInTabOrder(m_txtCtrlFifoSize);
#endif // __WXMSW__
    
    m_reportingTab->MoveBeforeInTabOrder(m_displayTab);    
    m_displayTab->MoveBeforeInTabOrder(m_keyerTab);
    m_keyerTab->MoveBeforeInTabOrder(m_modemTab);
    m_modemTab->MoveBeforeInTabOrder(m_simulationTab);
    m_simulationTab->MoveBeforeInTabOrder(m_debugTab);
    
    m_notebook->MoveBeforeInTabOrder(m_sdbSizer5OK);
    m_sdbSizer5OK->MoveBeforeInTabOrder(m_sdbSizer5Cancel);
    m_sdbSizer5Cancel->MoveBeforeInTabOrder(m_sdbSizer5Apply);
#endif
    
    // Connect Events -------------------------------------------------------

    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(OptionsDlg::OnInitDialog));

    m_sdbSizer5OK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnOK), NULL, this);
    m_sdbSizer5Cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnCancel), NULL, this);
    m_sdbSizer5Apply->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnApply), NULL, this);

    m_ckboxTestFrame->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnTestFrame), NULL, this);
    m_ckboxChannelNoise->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnChannelNoise), NULL, this);

    m_ckboxFreeDV700txClip->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnFreeDV700txClip), NULL, this);

#ifdef __WXMSW__
    m_ckboxDebugConsole->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnDebugConsole), NULL, this);
#endif

    m_buttonChooseVoiceKeyerWaveFilePath->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnChooseVoiceKeyerWaveFilePath), NULL, this);

    m_buttonChooseQuickRecordRawPath->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnChooseQuickRecordPath), NULL, this);
    m_buttonChooseQuickRecordDecodedPath->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnChooseQuickRecordPath), NULL, this);

    m_BtnFifoReset->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnFifoReset), NULL, this);

    m_ckboxReportingEnable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingEnable), NULL, this);
    m_ckboxFreeDVReporterEnable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingEnable), NULL, this);
    m_ckboxUDPReportingEnable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingEnable), NULL, this);
    m_ckboxTone->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnToneStateEnable), NULL, this);
    
    m_ckboxMultipleRx->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnMultipleRxEnable), NULL, this);
    
    m_ckboxEnableFreqModeChanges->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(OptionsDlg::OnFreqModeChangeEnable), NULL, this);
    m_ckboxEnableFreqChangesOnly->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(OptionsDlg::OnFreqModeChangeEnable), NULL, this);
    m_ckboxNoFreqModeChanges->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(OptionsDlg::OnFreqModeChangeEnable), NULL, this);
    
    m_freqList->Connect(wxEVT_LISTBOX, wxCommandEventHandler(OptionsDlg::OnReportingFreqSelectionChange), NULL, this);
    m_txtCtrlNewFrequency->Connect(wxEVT_TEXT, wxCommandEventHandler(OptionsDlg::OnReportingFreqTextChange), NULL, this);
    m_freqListAdd->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingFreqAdd), NULL, this);
    m_freqListRemove->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingFreqRemove), NULL, this);
    m_freqListMoveUp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingFreqMoveUp), NULL, this);
    m_freqListMoveDown->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingFreqMoveDown), NULL, this);
    
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

    m_ckboxFreeDV700txClip->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnFreeDV700txClip), NULL, this);
    m_buttonChooseVoiceKeyerWaveFilePath->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnChooseVoiceKeyerWaveFilePath), NULL, this);
    m_buttonChooseQuickRecordRawPath->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnChooseQuickRecordPath), NULL, this);
    m_buttonChooseQuickRecordDecodedPath->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnChooseQuickRecordPath), NULL, this);

    m_BtnFifoReset->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnFifoReset), NULL, this);

#ifdef __WXMSW__
    m_ckboxDebugConsole->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnDebugConsole), NULL, this);
#endif
    
    m_ckboxReportingEnable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingEnable), NULL, this);
    m_ckboxFreeDVReporterEnable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingEnable), NULL, this);
    m_ckboxUDPReportingEnable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingEnable), NULL, this);
    m_ckboxTone->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnToneStateEnable), NULL, this);
    
    m_ckboxMultipleRx->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(OptionsDlg::OnMultipleRxEnable), NULL, this);
    
    m_ckboxEnableFreqModeChanges->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(OptionsDlg::OnFreqModeChangeEnable), NULL, this);
    m_ckboxEnableFreqChangesOnly->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(OptionsDlg::OnFreqModeChangeEnable), NULL, this);
    m_ckboxNoFreqModeChanges->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(OptionsDlg::OnFreqModeChangeEnable), NULL, this);
    
    m_freqList->Disconnect(wxEVT_LISTBOX, wxCommandEventHandler(OptionsDlg::OnReportingFreqSelectionChange), NULL, this);
    m_txtCtrlNewFrequency->Disconnect(wxEVT_TEXT, wxCommandEventHandler(OptionsDlg::OnReportingFreqTextChange), NULL, this);
    m_freqListAdd->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingFreqAdd), NULL, this);
    m_freqListRemove->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingFreqRemove), NULL, this);
    m_freqListMoveUp->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingFreqMoveUp), NULL, this);
    m_freqListMoveDown->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OptionsDlg::OnReportingFreqMoveDown), NULL, this);
}


//-------------------------------------------------------------------------
// ExchangeData()
//-------------------------------------------------------------------------
void OptionsDlg::ExchangeData(int inout, bool storePersistent)
{
    if(inout == EXCHANGE_DATA_IN)
    {
        // Populate FreeDV Reporter color settings
        wxColour rxBackgroundColor(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowBackgroundColor);
        wxColour rxForegroundColor(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowForegroundColor);
        wxColour txBackgroundColor(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowBackgroundColor);
        wxColour txForegroundColor(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowForegroundColor);
        wxColour msgBackgroundColor(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMsgRowBackgroundColor);
        wxColour msgForegroundColor(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMsgRowForegroundColor);

        m_freedvReporterRxBackgroundColor->SetColour(rxBackgroundColor);
        m_freedvReporterRxForegroundColor->SetColour(rxForegroundColor);
        m_freedvReporterTxBackgroundColor->SetColour(txBackgroundColor);
        m_freedvReporterTxForegroundColor->SetColour(txForegroundColor);
        m_freedvReporterMsgBackgroundColor->SetColour(msgBackgroundColor);
        m_freedvReporterMsgForegroundColor->SetColour(msgForegroundColor);

        // Populate reporting frequency list.
        for (auto& item : wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyList.get())
        {
            m_freqList->Append(item);
        }
        
        m_txtCtrlCallSign->SetValue(wxGetApp().appConfiguration.reportingConfiguration.reportingFreeTextString);

        m_ckboxEnableSpacebarForPTT->SetValue(wxGetApp().appConfiguration.enableSpaceBarForPTT);
        m_txtTxRxDelayMilliseconds->SetValue(wxString::Format("%d", wxGetApp().appConfiguration.txRxDelayMilliseconds.get()));
        m_ckboxUseAnalogModes->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseAnalogModes);
        m_ckboxEnableFreqModeChanges->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges);
        m_ckboxEnableFreqChangesOnly->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly);
        m_ckboxNoFreqModeChanges->SetValue(!wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges && !wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly);
        m_ckboxFrequencyEntryAsKHz->SetValue(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz);
        
        /* Plot settings */
        m_cbxNumSpectrumAveraging->SetSelection(wxGetApp().appConfiguration.currentSpectrumAveraging);
         
        /* Voice Keyer */

        m_txtCtrlVoiceKeyerWaveFilePath->SetValue(wxGetApp().appConfiguration.voiceKeyerWaveFilePath);
        m_txtCtrlVoiceKeyerRxPause->SetValue(wxString::Format(wxT("%i"), wxGetApp().appConfiguration.voiceKeyerRxPause.get()));
        m_txtCtrlVoiceKeyerRepeats->SetValue(wxString::Format(wxT("%i"), wxGetApp().appConfiguration.voiceKeyerRepeats.get()));

        m_txtCtrlQuickRecordRawPath->SetValue(wxGetApp().appConfiguration.quickRecordRawPath);
        m_txtCtrlQuickRecordDecodedPath->SetValue(wxGetApp().appConfiguration.quickRecordDecodedPath);
        
        m_ckHalfDuplex->SetValue(wxGetApp().appConfiguration.halfDuplexMode);

        m_ckboxMultipleRx->SetValue(wxGetApp().appConfiguration.multipleReceiveEnabled);
        m_ckboxSingleRxThread->SetValue(wxGetApp().appConfiguration.multipleReceiveOnSingleThread);
        
        m_ckboxTestFrame->SetValue(wxGetApp().m_testFrames);

        m_ckboxChannelNoise->SetValue(wxGetApp().m_channel_noise);
        m_txtNoiseSNR->SetValue(wxString::Format(wxT("%i"),wxGetApp().appConfiguration.noiseSNR.get()));

        m_ckboxTone->SetValue(wxGetApp().m_tone);
        m_txtToneFreqHz->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_tone_freq_hz));
        m_txtToneAmplitude->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_tone_amplitude));

        m_ckboxAttnCarrierEn->SetValue(wxGetApp().m_attn_carrier_en);
        m_txtAttnCarrier->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_attn_carrier));

        m_txtCtrlFifoSize->SetValue(wxString::Format(wxT("%i"),wxGetApp().appConfiguration.fifoSizeMs.get()));

        m_ckboxTxRxThreadPriority->SetValue(wxGetApp().m_txRxThreadHighPriority);
        m_ckboxTxRxDumpTiming->SetValue(g_dump_timing);
        m_ckboxTxRxDumpFifoState->SetValue(g_dump_fifo_state);
        m_ckboxVerbose->SetValue(wxGetApp().appConfiguration.debugVerbose);
        m_ckboxFreeDVAPIVerbose->SetValue(g_freedv_verbose);
        m_showDecodeStats->SetValue(wxGetApp().appConfiguration.showDecodeStats);
        
        m_experimentalFeatures->SetValue(wxGetApp().appConfiguration.experimentalFeatures);
       
        m_ckboxFreeDV700txClip->SetValue(wxGetApp().appConfiguration.freedv700Clip);
        m_ckboxFreeDV700txBPF->SetValue(wxGetApp().appConfiguration.freedv700TxBPF);
        m_ckboxEnableLegacyModes->SetValue(wxGetApp().appConfiguration.enableLegacyModes);
        
#ifdef __WXMSW__
        m_ckboxDebugConsole->SetValue(wxGetApp().appConfiguration.debugConsoleEnabled);
#endif
        
        // General reporting config
        m_ckboxReportingEnable->SetValue(wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled);
        m_txt_callsign->SetValue(wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign);
        m_txt_grid_square->SetValue(wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare);
        m_ckboxManualFrequencyReporting->SetValue(wxGetApp().appConfiguration.reportingConfiguration.manualFrequencyReporting);
        
        // PSK Reporter options
        m_ckboxPskReporterEnable->SetValue(wxGetApp().appConfiguration.reportingConfiguration.pskReporterEnabled);
        
        // FreeDV Reporter options
        m_ckboxFreeDVReporterEnable->SetValue(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterEnabled);
        m_freedvReporterHostname->SetValue(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterHostname);
        m_useMetricDistances->SetValue(wxGetApp().appConfiguration.reportingConfiguration.useMetricDistances);
        m_useCardinalDirections->SetValue(wxGetApp().appConfiguration.reportingConfiguration.reportingDirectionAsCardinal);
        m_ckboxFreeDVReporterForceReceiveOnly->SetValue(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterForceReceiveOnly);
        
        // UDP reporting options
        m_ckboxUDPReportingEnable->SetValue(wxGetApp().appConfiguration.reportingConfiguration.udpReportingEnabled);
        m_udpHostname->SetValue(wxGetApp().appConfiguration.reportingConfiguration.udpReportingHostname);
        m_udpPort->SetValue(wxString::Format(wxT("%i"), wxGetApp().appConfiguration.reportingConfiguration.udpReportingPort.get()));
        
        // Callsign list config
        m_ckbox_use_utc_time->SetValue(wxGetApp().appConfiguration.reportingConfiguration.useUTCForReporting);
        
        // Stats reset time
        m_statsResetTime->SetValue(wxString::Format(wxT("%i"), wxGetApp().appConfiguration.statsResetTimeSecs.get()));
        
        // Waterfall color
        switch (wxGetApp().appConfiguration.waterfallColor)
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
            wxGetApp().appConfiguration.waterfallColor = 0;
        }
        else if (m_waterfallColorScheme2->GetValue())
        {
            wxGetApp().appConfiguration.waterfallColor = 1;
        }
        else if (m_waterfallColorScheme3->GetValue())
        {
            wxGetApp().appConfiguration.waterfallColor = 2;
        }
        
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            m_labelEnterFreq->SetLabel(wxT("Enter frequency (kHz):"));
        }
        else
        {
            m_labelEnterFreq->SetLabel(wxT("Enter frequency (MHz):"));
        }
        
        // Update control state based on checkbox state.
        updateReportingState();
        updateChannelNoiseState();
        updateAttnCarrierState();
        updateToneState();
        updateMultipleRxState();
        updateRigControlState();

        wxCommandEvent tmpEvent;
        OnReportingFreqTextChange(tmpEvent);
    }

    if(inout == EXCHANGE_DATA_OUT)
    {
        // Populate FreeDV Reporter color settings
        wxColour rxBackgroundColor = m_freedvReporterRxBackgroundColor->GetColour();
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowBackgroundColor = rxBackgroundColor.GetAsString(wxC2S_HTML_SYNTAX);

        wxColour rxForegroundColor = m_freedvReporterRxForegroundColor->GetColour();
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterRxRowForegroundColor = rxForegroundColor.GetAsString(wxC2S_HTML_SYNTAX);

        wxColour txBackgroundColor = m_freedvReporterTxBackgroundColor->GetColour();
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowBackgroundColor = txBackgroundColor.GetAsString(wxC2S_HTML_SYNTAX);

        wxColour txForegroundColor = m_freedvReporterTxForegroundColor->GetColour();
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterTxRowForegroundColor = txForegroundColor.GetAsString(wxC2S_HTML_SYNTAX);

        wxColour msgBackgroundColor = m_freedvReporterMsgBackgroundColor->GetColour();
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMsgRowBackgroundColor = msgBackgroundColor.GetAsString(wxC2S_HTML_SYNTAX);

        wxColour msgForegroundColor = m_freedvReporterMsgForegroundColor->GetColour();
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterMsgRowForegroundColor = msgForegroundColor.GetAsString(wxC2S_HTML_SYNTAX);

        // Save new reporting frequency list.
        std::vector<wxString> tmpList;
        tmpList.reserve(m_freqList->GetCount());
        for (unsigned int index = 0; index < m_freqList->GetCount(); index++)
        {
            tmpList.push_back(m_freqList->GetString(index));
        }
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyList = tmpList;
        
        wxGetApp().appConfiguration.enableSpaceBarForPTT = m_ckboxEnableSpacebarForPTT->GetValue();

        wxGetApp().appConfiguration.txRxDelayMilliseconds = wxAtoi(m_txtTxRxDelayMilliseconds->GetValue());
        
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseAnalogModes = m_ckboxUseAnalogModes->GetValue();
        
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges = m_ckboxEnableFreqModeChanges->GetValue();
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly = m_ckboxEnableFreqChangesOnly->GetValue();
        
        wxGetApp().appConfiguration.reportingConfiguration.reportingFreeTextString = m_txtCtrlCallSign->GetValue();

        wxGetApp().appConfiguration.halfDuplexMode = m_ckHalfDuplex->GetValue();
        wxGetApp().appConfiguration.multipleReceiveEnabled = m_ckboxMultipleRx->GetValue();
        wxGetApp().appConfiguration.multipleReceiveOnSingleThread = m_ckboxSingleRxThread->GetValue();
        
        /* Plot settings */
        wxGetApp().appConfiguration.currentSpectrumAveraging = m_cbxNumSpectrumAveraging->GetSelection();
        
        /* Voice Keyer */

        wxGetApp().appConfiguration.voiceKeyerWaveFilePath = m_txtCtrlVoiceKeyerWaveFilePath->GetValue();
        
        long tmp;
        m_txtCtrlVoiceKeyerRxPause->GetValue().ToLong(&tmp); if (tmp < 0) tmp = 0; wxGetApp().appConfiguration.voiceKeyerRxPause = (int)tmp;
        m_txtCtrlVoiceKeyerRepeats->GetValue().ToLong(&tmp);
        if (tmp < 0) {tmp = 0;} if (tmp > 100) {tmp = 100;}
        wxGetApp().appConfiguration.voiceKeyerRepeats = (int)tmp;
        
        wxGetApp().appConfiguration.quickRecordRawPath = m_txtCtrlQuickRecordRawPath->GetValue();
        wxGetApp().appConfiguration.quickRecordDecodedPath = m_txtCtrlQuickRecordDecodedPath->GetValue();
        
        wxGetApp().m_testFrames    = m_ckboxTestFrame->GetValue();

        wxGetApp().m_channel_noise = m_ckboxChannelNoise->GetValue();
        long noise_snr;
        m_txtNoiseSNR->GetValue().ToLong(&noise_snr);
        wxGetApp().appConfiguration.noiseSNR = (int)noise_snr;
        
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
        wxGetApp().appConfiguration.fifoSizeMs = (int)FifoSize_ms;

        wxGetApp().m_txRxThreadHighPriority = m_ckboxTxRxThreadPriority->GetValue();
        g_dump_timing = m_ckboxTxRxDumpTiming->GetValue();
        g_dump_fifo_state = m_ckboxTxRxDumpFifoState->GetValue();
        wxGetApp().appConfiguration.debugVerbose = m_ckboxVerbose->GetValue();
        if (wxGetApp().appConfiguration.debugVerbose)
        {
            ulog_set_level(LOG_TRACE);
        }
        else
        {
            ulog_set_level(LOG_INFO);
        }
        g_freedv_verbose = m_ckboxFreeDVAPIVerbose->GetValue();

        wxGetApp().appConfiguration.showDecodeStats = m_showDecodeStats->GetValue();
        wxGetApp().appConfiguration.freedv700Clip = m_ckboxFreeDV700txClip->GetValue();
        wxGetApp().appConfiguration.freedv700TxBPF = m_ckboxFreeDV700txBPF->GetValue();
        wxGetApp().appConfiguration.enableLegacyModes = m_ckboxEnableLegacyModes->GetValue();
        
#ifdef __WXMSW__
        wxGetApp().appConfiguration.debugConsoleEnabled = m_ckboxDebugConsole->GetValue();
#endif
        
        wxGetApp().appConfiguration.experimentalFeatures = m_experimentalFeatures->GetValue();

        // General reporting config
        wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled = m_ckboxReportingEnable->GetValue();
        wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign = m_txt_callsign->GetValue();
        wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare = m_txt_grid_square->GetValue();
        wxGetApp().appConfiguration.reportingConfiguration.manualFrequencyReporting = m_ckboxManualFrequencyReporting->GetValue();
        
        // PSK Reporter options
        wxGetApp().appConfiguration.reportingConfiguration.pskReporterEnabled = m_ckboxPskReporterEnable->GetValue();
        
        // FreeDV Reporter options
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterEnabled = m_ckboxFreeDVReporterEnable->GetValue();
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterHostname = m_freedvReporterHostname->GetValue();
        wxGetApp().appConfiguration.reportingConfiguration.useMetricDistances = m_useMetricDistances->GetValue();
        wxGetApp().appConfiguration.reportingConfiguration.freedvReporterForceReceiveOnly = m_ckboxFreeDVReporterForceReceiveOnly->GetValue();
        wxGetApp().appConfiguration.reportingConfiguration.reportingDirectionAsCardinal = m_useCardinalDirections->GetValue();
        
        // UDP reporting options
        wxGetApp().appConfiguration.reportingConfiguration.udpReportingEnabled = m_ckboxUDPReportingEnable->GetValue();
        wxGetApp().appConfiguration.reportingConfiguration.udpReportingHostname = m_udpHostname->GetValue();
        
        long udpPort;
        m_udpPort->GetValue().ToLong(&udpPort);
        wxGetApp().appConfiguration.reportingConfiguration.udpReportingPort = (int)udpPort;
            
        // Callsign list config
        wxGetApp().appConfiguration.reportingConfiguration.useUTCForReporting = m_ckbox_use_utc_time->GetValue();
        
        // Waterfall color
        if (m_waterfallColorScheme1->GetValue())
        {
            wxGetApp().appConfiguration.waterfallColor = 0;
        }
        else if (m_waterfallColorScheme2->GetValue())
        {
            wxGetApp().appConfiguration.waterfallColor = 1;
        }
        else if (m_waterfallColorScheme3->GetValue())
        {
            wxGetApp().appConfiguration.waterfallColor = 2;
        }
        
        // Stats reset time
        long resetTime;
        m_statsResetTime->GetValue().ToLong(&resetTime);
        wxGetApp().appConfiguration.statsResetTimeSecs = resetTime;
        
        if (storePersistent) {
            wxGetApp().appConfiguration.apiVerbose = g_freedv_verbose;            
            wxGetApp().appConfiguration.save(pConfig);
            
            // Save reporting frequency units last due to how the frequency list is stored.
            // Then reload the configuration to ensure that the displayed frequencies are correct.
            wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz = m_ckboxFrequencyEntryAsKHz->GetValue();
            wxGetApp().appConfiguration.save(pConfig);
            wxGetApp().appConfiguration.load(pConfig);
        }
    }
}

//-------------------------------------------------------------------------
// OnOK()
//-------------------------------------------------------------------------
void OptionsDlg::OnOK(wxCommandEvent&)
{
    ExchangeData(EXCHANGE_DATA_OUT, true);
    //this->EndModal(wxID_OK);
    EndModal(wxOK);
    
    // Clear frequency list to prevent sizing issues on re-display.
    // EXCHANGE_DATA_IN will repopulate then.
    m_freqList->Clear();
}

//-------------------------------------------------------------------------
// OnCancel()
//-------------------------------------------------------------------------
void OptionsDlg::OnCancel(wxCommandEvent&)
{
    //this->EndModal(wxID_CANCEL);
    EndModal(wxCANCEL);
    
    // Clear frequency list to prevent sizing issues on re-display.
    // EXCHANGE_DATA_IN will repopulate then.
    m_freqList->Clear();
}

//-------------------------------------------------------------------------
// OnApply()
//-------------------------------------------------------------------------
void OptionsDlg::OnApply(wxCommandEvent&)
{
    bool oldFreqAsKHz = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz;
    
    ExchangeData(EXCHANGE_DATA_OUT, true);
    
    bool khzChanged = 
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz != oldFreqAsKHz;
    
    // If there's something in the frequency entry textbox (i.e. if the user pushes Apply),
    // convert to the correct units.
    auto freqString = m_txtCtrlNewFrequency->GetValue();
    if (freqString.Length() > 0 && khzChanged)
    {
        double freqDouble = 0;
        wxNumberFormatter::FromString(freqString, &freqDouble);
        
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            freqDouble *= 1000;
            m_txtCtrlNewFrequency->SetValue(wxNumberFormatter::ToString(freqDouble, 1));
        }
        else
        {
            freqDouble /= 1000.0;
            m_txtCtrlNewFrequency->SetValue(wxNumberFormatter::ToString(freqDouble, 1));
        }
    }
        
    // Reload saved data to ensure the frequency list is properly displayed.
    m_freqList->Clear();
    ExchangeData(EXCHANGE_DATA_IN, false);
}

//-------------------------------------------------------------------------
// OnInitDialog()
//-------------------------------------------------------------------------
void OptionsDlg::OnInitDialog(wxInitDialogEvent&)
{
    ExchangeData(EXCHANGE_DATA_IN, false);
}

// immediately change flags rather using ExchangeData() so we can switch on and off at run time

void OptionsDlg::OnTestFrame(wxScrollEvent&) {
    wxGetApp().m_testFrames    = m_ckboxTestFrame->GetValue();
}

void OptionsDlg::OnChannelNoise(wxScrollEvent&) {
    wxGetApp().m_channel_noise = m_ckboxChannelNoise->GetValue();
    updateChannelNoiseState();
}


void OptionsDlg::OnChooseVoiceKeyerWaveFilePath(wxCommandEvent&) {
    wxDirDialog pathDialog(
                                this,
                                wxT("Voice Keyer file location"),
                                wxGetApp().appConfiguration.voiceKeyerWaveFilePath
                                );
                                
    if(pathDialog.ShowModal() == wxID_CANCEL) {
        return;     // the user changed their mind...
    }
    
    m_txtCtrlVoiceKeyerWaveFilePath->SetValue(pathDialog.GetPath());
}

void OptionsDlg::OnChooseQuickRecordPath(wxCommandEvent& event) {
    wxString defaultLocation = 
        (event.GetEventObject() == m_buttonChooseQuickRecordRawPath) ?
        wxGetApp().appConfiguration.quickRecordRawPath :
        wxGetApp().appConfiguration.quickRecordDecodedPath;
     wxDirDialog pathDialog(
                                 this,
                                 wxT("Choose Quick Record save location"),
                                 defaultLocation
                                 );
     if(pathDialog.ShowModal() == wxID_CANCEL) {
         return;     // the user changed their mind...
     }

     if (event.GetEventObject() == m_buttonChooseQuickRecordRawPath)
     {
        m_txtCtrlQuickRecordRawPath->SetValue(pathDialog.GetPath());
     }
     else
     {
        m_txtCtrlQuickRecordDecodedPath->SetValue(pathDialog.GetPath());
     }
}

void OptionsDlg::OnFreeDV700txClip(wxScrollEvent&) {
    wxGetApp().appConfiguration.freedv700Clip = m_ckboxFreeDV700txClip->GetValue();
}

void OptionsDlg::OnDebugConsole(wxScrollEvent&) {
    wxGetApp().appConfiguration.debugConsoleEnabled = m_ckboxDebugConsole->GetValue();
#ifdef __WXMSW__
    // somewhere to send printfs while developing, causes conmsole to pop up on Windows
    if (wxGetApp().appConfiguration.debugConsoleEnabled) {
        int ret = AllocConsole();
        freopen("CONOUT$", "w", stdout); 
        freopen("CONOUT$", "w", stderr); 
        log_info("AllocConsole: %d m_debug_console: %d", ret, wxGetApp().appConfiguration.debugConsoleEnabled.get());
    } 
#endif
}


void OptionsDlg::OnFifoReset(wxCommandEvent&)
{
    g_infifo1_full.store(0, std::memory_order_release);
    g_outfifo1_empty.store(0, std::memory_order_release);
    g_infifo2_full.store(0, std::memory_order_release);
    g_outfifo2_empty.store(0, std::memory_order_release);
    for (int i=0; i<4; i++) {
        g_AEstatus1[i] = g_AEstatus2[i] = 0;
    }
}

void OptionsDlg::updateReportingState()
{    
    if (!sessionActive_)
    {
        m_ckbox_use_utc_time->Enable(true);
        m_ckboxReportingEnable->Enable(true);
        m_useMetricDistances->Enable(true);
        m_freedvReporterHostname->Enable(true);

        if (m_ckboxReportingEnable->GetValue())
        {
            m_txtCtrlCallSign->Enable(false);
            m_txt_callsign->Enable(true);
            m_txt_grid_square->Enable(true);
            m_ckboxManualFrequencyReporting->Enable(true);
            m_ckboxPskReporterEnable->Enable(true);
            m_ckboxFreeDVReporterEnable->Enable(true);
            m_ckboxFreeDVReporterForceReceiveOnly->Enable(true);
            m_useCardinalDirections->Enable(true);
            m_ckboxUDPReportingEnable->Enable(true);
            
            if (m_ckboxUDPReportingEnable->GetValue())
            {
                m_udpHostname->Enable(true);
                m_udpPort->Enable(true);
            }
            else
            {
                m_udpHostname->Enable(false);
                m_udpPort->Enable(false);
            }
        }
        else
        {
            m_txtCtrlCallSign->Enable(true);
            m_txt_callsign->Enable(false);
            m_txt_grid_square->Enable(false);
            m_ckboxPskReporterEnable->Enable(false);
            m_ckboxFreeDVReporterEnable->Enable(false);
            m_ckboxManualFrequencyReporting->Enable(false);
            m_ckboxFreeDVReporterForceReceiveOnly->Enable(true);
            m_useCardinalDirections->Enable(true);
            m_udpHostname->Enable(false);
            m_udpPort->Enable(false);
            m_ckboxUDPReportingEnable->Enable(false);
        }    
    }
    else
    {
        // Txt Msg/Reporter options cannot be modified during a session.
        m_ckboxReportingEnable->Enable(false);
        m_txtCtrlCallSign->Enable(false);
        m_txt_callsign->Enable(false);
        m_txt_grid_square->Enable(false);
        m_ckboxManualFrequencyReporting->Enable(false);
        m_ckboxPskReporterEnable->Enable(false);
        m_ckboxFreeDVReporterEnable->Enable(false);
        m_useMetricDistances->Enable(false);
        m_freedvReporterHostname->Enable(false);
        m_ckboxFreeDVReporterForceReceiveOnly->Enable(false);
        m_useCardinalDirections->Enable(false);
        m_udpHostname->Enable(false);
        m_udpPort->Enable(false);
        m_ckboxUDPReportingEnable->Enable(false);
        
        m_ckbox_use_utc_time->Enable(false);
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

void OptionsDlg::updateRigControlState()
{
    if (!sessionActive_)
    {
        m_ckboxEnableFreqModeChanges->Enable(true);
        m_ckboxEnableFreqChangesOnly->Enable(true);
        m_ckboxNoFreqModeChanges->Enable(true);
        m_ckboxEnableSpacebarForPTT->Enable(true);
        m_txtTxRxDelayMilliseconds->Enable(true);
        m_ckboxUseAnalogModes->Enable(m_ckboxEnableFreqModeChanges->GetValue());
    }
    else
    {
        // Rig control settings cannot be updated during a session.
        m_ckboxUseAnalogModes->Enable(false);
        m_ckboxEnableFreqModeChanges->Enable(false);
        m_ckboxEnableFreqChangesOnly->Enable(false);
        m_ckboxNoFreqModeChanges->Enable(false);
        m_ckboxEnableSpacebarForPTT->Enable(false);
        m_txtTxRxDelayMilliseconds->Enable(false);
    }
}
    
void OptionsDlg::OnReportingEnable(wxCommandEvent&)
{
    updateReportingState();
}

void OptionsDlg::OnToneStateEnable(wxCommandEvent&)
{
    updateToneState();
}

void OptionsDlg::OnMultipleRxEnable(wxCommandEvent&)
{
    updateMultipleRxState();
}

void OptionsDlg::OnFreqModeChangeEnable(wxCommandEvent&)
{
    updateRigControlState();
}

void OptionsDlg::DisplayFifoPACounters() {
    if (IsShownOnScreen())
    {
        wxString fifo_counters = wxString::Format(wxT("Fifos: infull1: %d outempty1: %d infull2: %d outempty2: %d"), g_infifo1_full.load(std::memory_order_acquire), g_outfifo1_empty.load(std::memory_order_acquire), g_infifo2_full.load(std::memory_order_acquire), g_outfifo2_empty.load(std::memory_order_acquire));
        m_textFifos->SetLabel(fifo_counters);

        // input: underflow overflow output: underflow overflow
        wxString pa_counters_1 = wxString::Format(wxT("Audio1: inUnderflow: %d inOverflow: %d outUnderflow %d outOverflow %d"), g_AEstatus1[0], g_AEstatus1[1], g_AEstatus1[2], g_AEstatus1[3]);
        m_textPA1->SetLabel(pa_counters_1);

        // input: underflow overflow output: underflow overflow
        wxString pa_counters_2 = wxString::Format(wxT("Audio2: inUnderflow: %d inOverflow: %d outUnderflow %d outOverflow %d"), g_AEstatus2[0], g_AEstatus2[1], g_AEstatus2[2], g_AEstatus2[3]);
        m_textPA2->SetLabel(pa_counters_2);
    }
}

void OptionsDlg::OnReportingFreqSelectionChange(wxCommandEvent&)
{
    auto sel = m_freqList->GetSelection();
    if (sel >= 0)
    {
        m_txtCtrlNewFrequency->SetValue(m_freqList->GetString(sel));
    }
    else
    {
        m_txtCtrlNewFrequency->SetValue("");
    }
}

void OptionsDlg::OnReportingFreqTextChange(wxCommandEvent&)
{
    double tmpValue = 0.0;
    bool validNumber = wxNumberFormatter::FromString(m_txtCtrlNewFrequency->GetValue(), &tmpValue);

    auto idx = m_freqList->FindString(m_txtCtrlNewFrequency->GetValue());
    if (idx != wxNOT_FOUND)
    {
        m_freqListAdd->Enable(false);
        m_freqListRemove->Enable(true);
        
        if (idx == 0)
        {
            m_freqListMoveUp->Enable(false);
            m_freqListMoveDown->Enable(true);
        }
        else if ((unsigned)idx == m_freqList->GetCount() - 1)
        {
            m_freqListMoveUp->Enable(true);
            m_freqListMoveDown->Enable(false);
        }
        else
        {
            m_freqListMoveUp->Enable(true);
            m_freqListMoveDown->Enable(true);
        }
    }
    else if (validNumber && tmpValue > 0)
    {
        m_freqListAdd->Enable(true);
        m_freqListRemove->Enable(false);
        m_freqListMoveUp->Enable(false);
        m_freqListMoveDown->Enable(false);
    }
    else
    {
        m_freqListAdd->Enable(false);
        m_freqListRemove->Enable(false);
        m_freqListMoveUp->Enable(false);
        m_freqListMoveDown->Enable(false);
    }
}

void OptionsDlg::OnReportingFreqAdd(wxCommandEvent&)
{
    auto val = m_txtCtrlNewFrequency->GetValue();
    
    double dVal = 0;
    wxNumberFormatter::FromString(val, &dVal);
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
    {
        val = wxNumberFormatter::ToString(dVal, 1);
    }
    else
    {
        val = wxNumberFormatter::ToString(dVal, 4);
    }
    m_freqList->Append(val);
    m_txtCtrlNewFrequency->SetValue("");
}

void OptionsDlg::OnReportingFreqRemove(wxCommandEvent&)
{
    auto idx = m_freqList->FindString(m_txtCtrlNewFrequency->GetValue());
    if (idx >= 0)
    {
        m_freqList->Delete(idx);
    }
    m_txtCtrlNewFrequency->SetValue("");
}

void OptionsDlg::OnReportingFreqMoveUp(wxCommandEvent&)
{
    auto prevStr = m_txtCtrlNewFrequency->GetValue();
    auto idx = m_freqList->FindString(m_txtCtrlNewFrequency->GetValue());
    if (idx != wxNOT_FOUND && idx > 0)
    {
        m_freqList->Delete(idx);
        m_freqList->Insert(prevStr, idx - 1);
        m_freqList->SetSelection(idx - 1);
    }
    
    // Refresh button status
    m_txtCtrlNewFrequency->SetValue(prevStr);
}

void OptionsDlg::OnReportingFreqMoveDown(wxCommandEvent&)
{
    auto prevStr = m_txtCtrlNewFrequency->GetValue();
    auto idx = m_freqList->FindString(m_txtCtrlNewFrequency->GetValue());
    if (idx != wxNOT_FOUND && (unsigned int)idx < m_freqList->GetCount() - 1)
    {
        m_freqList->Delete(idx);
        m_freqList->Insert(prevStr, idx + 1);
        m_freqList->SetSelection(idx + 1);
    }
    
    // Refresh button status
    m_txtCtrlNewFrequency->SetValue(prevStr);
}
