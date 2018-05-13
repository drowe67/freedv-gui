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
extern struct freedv      *g_pfreedv;

// PortAudio over/underflow counters

extern int                 g_infifo1_full;
extern int                 g_outfifo1_empty;
extern int                 g_infifo2_full;
extern int                 g_outfifo2_empty;
extern int                 g_PAstatus1[4];
extern int                 g_PAstatus2[4];

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class OptionsDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
OptionsDlg::OptionsDlg(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxDialog(parent, id, title, pos, size, style)
{

    this->SetSizeHints(wxDefaultSize, wxDefaultSize);

    wxBoxSizer* bSizer30;
    bSizer30 = new wxBoxSizer(wxVERTICAL);

    //------------------------------
    // Txt Msg Text Box
    //------------------------------

    wxStaticBoxSizer* sbSizer_callSign;
    wxStaticBox *sb_textMsg = new wxStaticBox(this, wxID_ANY, _("Txt Msg"));
    sbSizer_callSign = new wxStaticBoxSizer(sb_textMsg, wxVERTICAL);

    m_txtCtrlCallSign = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    m_txtCtrlCallSign->SetToolTip(_("Txt Msg you can send along with Voice"));
    sbSizer_callSign->Add(m_txtCtrlCallSign, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL|wxEXPAND, 3);

    bSizer30->Add(sbSizer_callSign,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);
 
    //----------------------------------------------------------------------
    // Voice Keyer 
    //----------------------------------------------------------------------

    wxStaticBoxSizer* staticBoxSizer28a = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("Voice Keyer")), wxHORIZONTAL);

    wxStaticText *m_staticText28b = new wxStaticText(this, wxID_ANY, _("Wave File: "), wxDefaultPosition, wxDefaultSize, 0);
    staticBoxSizer28a->Add(m_staticText28b, 0, wxALIGN_CENTER_VERTICAL, 5);    
    m_txtCtrlVoiceKeyerWaveFile = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300,-1), 0);
    m_txtCtrlVoiceKeyerWaveFile->SetToolTip(_("Wave file to play for Voice Keyer"));
    staticBoxSizer28a->Add(m_txtCtrlVoiceKeyerWaveFile, 0, 0, 5);

    m_buttonChooseVoiceKeyerWaveFile = new wxButton(this, wxID_APPLY, _("Choose"), wxDefaultPosition, wxSize(-1,-1), 0);
    staticBoxSizer28a->Add(m_buttonChooseVoiceKeyerWaveFile, 0, wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText *m_staticText28c = new wxStaticText(this, wxID_ANY, _("   Rx Pause: "), wxDefaultPosition, wxDefaultSize, 0);
    staticBoxSizer28a->Add(m_staticText28c, 0, wxALIGN_CENTER_VERTICAL , 5);
    m_txtCtrlVoiceKeyerRxPause = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(40,-1), 0);
    m_txtCtrlVoiceKeyerRxPause->SetToolTip(_("How long to wait in Rx mode before repeat"));
    staticBoxSizer28a->Add(m_txtCtrlVoiceKeyerRxPause, 0, 0, 5);

    wxStaticText *m_staticText28d = new wxStaticText(this, wxID_ANY, _("   Repeats: "), wxDefaultPosition, wxDefaultSize, 0);
    staticBoxSizer28a->Add(m_staticText28d, 0, wxALIGN_CENTER_VERTICAL, 5);
    m_txtCtrlVoiceKeyerRepeats = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(40,-1), 0);
    m_txtCtrlVoiceKeyerRepeats->SetToolTip(_("How long to wait in Rx mode before repeat"));
    staticBoxSizer28a->Add(m_txtCtrlVoiceKeyerRepeats, 0, 0, 5);

    bSizer30->Add(staticBoxSizer28a,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);

    //------------------------------
    // FreeDV 700 Options
    //------------------------------

    wxStaticBoxSizer* sbSizer_freedv700;
    wxStaticBox *sb_freedv700 = new wxStaticBox(this, wxID_ANY, _("FreeDV 700 Options"));
    sbSizer_freedv700 = new wxStaticBoxSizer(sb_freedv700, wxHORIZONTAL);

    m_ckboxFreeDV700txClip = new wxCheckBox(this, wxID_ANY, _("Clipping"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_freedv700->Add(m_ckboxFreeDV700txClip, 0, wxALIGN_LEFT, 0);
    m_ckboxFreeDV700Combine = new wxCheckBox(this, wxID_ANY, _("700C Diversity Combine for plots   700D Interleaver:"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_freedv700->Add(m_ckboxFreeDV700Combine, 0, wxALIGN_LEFT, 0);

    //wxStaticText *m_staticTexttb = new wxStaticText(this, wxID_ANY, _("   700D Interleave: "), wxDefaultPosition, wxDefaultSize, 0);
    //sbSizer_freedv700->Add(m_staticTexttb, 0, wxALIGN_CENTRE_VERTICAL, 5);    
    m_txtInterleave = new wxTextCtrl(this, wxID_ANY,  wxString("1"), wxDefaultPosition, wxSize(30,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_freedv700->Add(m_txtInterleave, 0, wxALIGN_CENTRE_VERTICAL, 0);

    m_ckboxFreeDV700ManualUnSync = new wxCheckBox(this, wxID_ANY, _("700D Manual UnSync"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_freedv700->Add(m_ckboxFreeDV700ManualUnSync, 0, wxALIGN_LEFT, 0);

    bSizer30->Add(sbSizer_freedv700, 0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);

    //------------------------------
    // Half/Full duplex selection
    //------------------------------

    wxStaticBox *sb_duplex = new wxStaticBox(this, wxID_ANY, _("Half/Full Duplex Operation"));
    wxStaticBoxSizer* sbSizer_duplex = new wxStaticBoxSizer(sb_duplex, wxHORIZONTAL);
    m_ckHalfDuplex = new wxCheckBox(this, wxID_ANY, _("Half Duplex"), wxDefaultPosition, wxSize(-1,-1), 0);
    sbSizer_duplex->Add(m_ckHalfDuplex, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
    bSizer30->Add(sbSizer_duplex,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);

    //------------------------------
    // Test Frames/Channel simulation check box
    //------------------------------

    wxStaticBoxSizer* sbSizer_testFrames;
    wxStaticBox *sb_testFrames = new wxStaticBox(this, wxID_ANY, _("Testing and Channel Simulation"));
    sbSizer_testFrames = new wxStaticBoxSizer(sb_testFrames, wxHORIZONTAL);

    m_ckboxTestFrame = new wxCheckBox(this, wxID_ANY, _("Test Frames"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_testFrames->Add(m_ckboxTestFrame, 0, wxALIGN_LEFT, 0);

    m_ckboxChannelNoise = new wxCheckBox(this, wxID_ANY, _("Channel Noise   SNR (dB):"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_testFrames->Add(m_ckboxChannelNoise, 0, wxALIGN_LEFT, 0);
    m_txtNoiseSNR = new wxTextCtrl(this, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(30,-1), 0, wxTextValidator(wxFILTER_NUMERIC));
    sbSizer_testFrames->Add(m_txtNoiseSNR, 0, wxALIGN_LEFT, 0);

    m_ckboxAttnCarrierEn = new wxCheckBox(this, wxID_ANY, _("Attn Carrier  Carrier:"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_testFrames->Add(m_ckboxAttnCarrierEn, 0, wxALIGN_LEFT, 0);
    m_txtAttnCarrier = new wxTextCtrl(this, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(30,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_testFrames->Add(m_txtAttnCarrier, 0, wxALIGN_LEFT, 0);

    bSizer30->Add(sbSizer_testFrames,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);

    //------------------------------
    // Interfering tone
    //------------------------------

    wxStaticBoxSizer* sbSizer_tone;
    wxStaticBox *sb_tone = new wxStaticBox(this, wxID_ANY, _("Simulated Interference Tone"));
    sbSizer_tone = new wxStaticBoxSizer(sb_tone, wxHORIZONTAL);

    m_ckboxTone = new wxCheckBox(this, wxID_ANY, _("Tone   Freq (Hz):"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_tone->Add(m_ckboxTone, 0, wxALIGN_LEFT, 0);
    m_txtToneFreqHz = new wxTextCtrl(this, wxID_ANY,  "1000", wxDefaultPosition, wxSize(60,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_tone->Add(m_txtToneFreqHz, 0, wxALIGN_LEFT, 0);
    wxStaticText *m_staticTextta = new wxStaticText(this, wxID_ANY, _(" Amplitude (pk): "), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_tone->Add(m_staticTextta, 0, wxALIGN_CENTER_VERTICAL, 5);    
    m_txtToneAmplitude = new wxTextCtrl(this, wxID_ANY,  "1000", wxDefaultPosition, wxSize(60,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_tone->Add(m_txtToneAmplitude, 0, wxALIGN_LEFT, 0);

    bSizer30->Add(sbSizer_tone,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);

#ifdef __EXPERIMENTAL_UDP__
    //------------------------------
    // Txt Encoding 
    //------------------------------

    wxStaticBoxSizer* sbSizer_encoding = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Text Encoding")), wxHORIZONTAL);

#ifdef SHORT_VARICODE
    m_rb_textEncoding1 = new wxRadioButton( this, wxID_ANY, wxT("Long varicode"), wxDefaultPosition, wxDefaultSize, 0);
    m_rb_textEncoding1->SetValue(true);
    sbSizer_encoding->Add(m_rb_textEncoding1, 0, wxALIGN_LEFT|wxALL, 1);
    m_rb_textEncoding2 = new wxRadioButton( this, wxID_ANY, wxT("Short Varicode"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_encoding->Add(m_rb_textEncoding2, 0, wxALIGN_LEFT|wxALL, 1);
#endif

    m_ckboxEnableChecksum = new wxCheckBox(this, wxID_ANY, _("Use Checksum on Rx"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_encoding->Add(m_ckboxEnableChecksum, 0, wxALIGN_LEFT, 0);

    bSizer30->Add(sbSizer_encoding,0, wxALL|wxEXPAND, 3);
 
    //------------------------------
    // Event processing
    //------------------------------

    wxStaticBoxSizer* sbSizer_events;
    wxStaticBox *sb_events = new wxStaticBox(this, wxID_ANY, _("Event Processing"));
    sbSizer_events = new wxStaticBoxSizer(sb_events, wxVERTICAL);

    // event processing enable and spam timer

    wxStaticBoxSizer* sbSizer_events_top;
    wxStaticBox* sb_events1 = new wxStaticBox(this, wxID_ANY, _(""));    
    sbSizer_events_top = new wxStaticBoxSizer(sb_events1, wxHORIZONTAL);

    m_ckbox_events = new wxCheckBox(this, wxID_ANY, _("Enable System Calls    Syscall Spam Timer"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sb_events->SetToolTip(_("Enable processing of events and generation of system calls"));
    sbSizer_events_top->Add(m_ckbox_events, 0, 0, 5);
    m_txt_spam_timer = new wxTextCtrl(this, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(40,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    m_txt_spam_timer->SetToolTip(_("Many matching events can cause a flood of syscalls. Set minimum time (seconds) between syscalls for each event here"));
    sbSizer_events_top->Add(m_txt_spam_timer, 0, 0, 5);
    m_rb_spam_timer = new wxRadioButton( this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_rb_spam_timer->SetForegroundColour( wxColour(0, 255, 0 ) );
    sbSizer_events_top->Add(m_rb_spam_timer, 0, 0, 10);
    sbSizer_events->Add(sbSizer_events_top, 0, 0, 5);

    // list of regexps

    wxStaticBoxSizer* sbSizer_regexp = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Regular Expressions to Process Events")), wxHORIZONTAL);
    m_txt_events_regexp_match = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200,100), wxTE_MULTILINE);
    m_txt_events_regexp_match->SetToolTip(_("Enter regular expressions to match events"));
    sbSizer_regexp->Add(m_txt_events_regexp_match, 1, wxEXPAND, 5);
    m_txt_events_regexp_replace = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200,100), wxTE_MULTILINE);
    m_txt_events_regexp_replace->SetToolTip(_("Enter regular expressions to replace events"));
    sbSizer_regexp->Add(m_txt_events_regexp_replace, 1, wxEXPAND, 5);
    sbSizer_events->Add(sbSizer_regexp, 1, wxEXPAND, 5);

    // log of events and responses

    wxStaticBoxSizer* sbSizer_event_log = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Log of Events and Responses")), wxVERTICAL);
    wxBoxSizer* bSizer33 = new wxBoxSizer(wxHORIZONTAL);
    m_txt_events_in = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200,50), wxTE_MULTILINE | wxTE_READONLY);
    bSizer33->Add(m_txt_events_in, 1, wxEXPAND, 5);
    m_txt_events_out = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200,50), wxTE_MULTILINE | wxTE_READONLY);
    bSizer33->Add(m_txt_events_out, 1, wxEXPAND, 5);
    sbSizer_event_log->Add(bSizer33, 1, wxEXPAND, 5);
    sbSizer_events->Add(sbSizer_event_log, 1, wxEXPAND, 5);

    bSizer30->Add(sbSizer_events,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);

    //------------------------------
    // UDP control port
    //------------------------------

    wxStaticBoxSizer* sbSizer_udp;
    wxStaticBox* sb_udp = new wxStaticBox(this, wxID_ANY, _("UDP Control Port"));
    sbSizer_udp = new wxStaticBoxSizer(sb_udp, wxHORIZONTAL);
    m_ckbox_udp_enable = new wxCheckBox(this, wxID_ANY, _("Enable UDP Control Port    UDP Port Number:"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sb_udp->SetToolTip(_("Enable control of FreeDV via UDP port"));
    sbSizer_udp->Add(m_ckbox_udp_enable, 0,  wxALIGN_CENTER_HORIZONTAL, 5);
    m_txt_udp_port = new wxTextCtrl(this, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(50,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_udp->Add(m_txt_udp_port, 0, wxALIGN_CENTER_HORIZONTAL, 5);

    bSizer30->Add(sbSizer_udp,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);
#endif

#ifdef __WXMSW__
    //------------------------------
    // debug console, for WIndows build make console pop up for debug messages
    //------------------------------

    wxStaticBoxSizer* sbSizer_console;
    wxStaticBox *sb_console = new wxStaticBox(this, wxID_ANY, _("Debug: Windows"));
    sbSizer_console = new wxStaticBoxSizer(sb_console, wxHORIZONTAL);

    m_ckboxDebugConsole = new wxCheckBox(this, wxID_ANY, _("Show Console"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_console->Add(m_ckboxDebugConsole, 0, wxALIGN_LEFT, 0);

    bSizer30->Add(sbSizer_console,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);
#endif

    //----------------------------------------------------------
    // FIFO and PortAudio under/overflow counters used for debug
    //----------------------------------------------------------

    wxStaticBoxSizer* sbSizer_fifo;
    wxStaticBox* sb_fifo = new wxStaticBox(this, wxID_ANY, _("Debug: FIFO and PortAudio Under/Over Flow Counters"));
    sbSizer_fifo = new wxStaticBoxSizer(sb_fifo, wxVERTICAL);

    m_BtnFifoReset = new wxButton(this, wxID_ANY, _("Reset"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_fifo->Add(m_BtnFifoReset, 0,  wxALIGN_LEFT, 5);

    m_textFifos = new wxStaticText(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_fifo->Add(m_textFifos, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 1);

    m_textPA1 = new wxStaticText(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_fifo->Add(m_textPA1, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 1);
    m_textPA2 = new wxStaticText(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sbSizer_fifo->Add(m_textPA2, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 1);

    bSizer30->Add(sbSizer_fifo,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);

    //------------------------------
    // OK - Cancel - Apply Buttons 
    //------------------------------

    wxBoxSizer* bSizer31 = new wxBoxSizer(wxHORIZONTAL);

    m_sdbSizer5OK = new wxButton(this, wxID_OK);
    bSizer31->Add(m_sdbSizer5OK, 0, wxALL, 2);

    m_sdbSizer5Cancel = new wxButton(this, wxID_CANCEL);
    bSizer31->Add(m_sdbSizer5Cancel, 0, wxALL, 2);

    m_sdbSizer5Apply = new wxButton(this, wxID_APPLY);
    bSizer31->Add(m_sdbSizer5Apply, 0, wxALL, 2);

    bSizer30->Add(bSizer31, 0, wxALIGN_CENTER, 0);

    this->SetSizer(bSizer30);
    if ( GetSizer() ) 
    {
         GetSizer()->Fit(this);
    }
     this->Layout();

    this->Centre(wxBOTH);
 
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

#ifdef __WXMSW__
    m_ckboxDebugConsole->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxScrollEventHandler(OptionsDlg::OnDebugConsole), NULL, this);
#endif
}


//-------------------------------------------------------------------------
// ExchangeData()
//-------------------------------------------------------------------------
void OptionsDlg::ExchangeData(int inout, bool storePersistent)
{
    wxConfigBase *pConfig = wxConfigBase::Get();

    if(inout == EXCHANGE_DATA_IN)
    {
        m_txtCtrlCallSign->SetValue(wxGetApp().m_callSign);

        /* Voice Keyer */

        m_txtCtrlVoiceKeyerWaveFile->SetValue(wxGetApp().m_txtVoiceKeyerWaveFile);
        m_txtCtrlVoiceKeyerRxPause->SetValue(wxString::Format(wxT("%i"), wxGetApp().m_intVoiceKeyerRxPause));
        m_txtCtrlVoiceKeyerRepeats->SetValue(wxString::Format(wxT("%i"), wxGetApp().m_intVoiceKeyerRepeats));

        m_ckHalfDuplex->SetValue(wxGetApp().m_boolHalfDuplex);

        m_ckboxTestFrame->SetValue(wxGetApp().m_testFrames);

        m_ckboxChannelNoise->SetValue(wxGetApp().m_channel_noise);
        m_txtNoiseSNR->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_noise_snr));

        m_ckboxTone->SetValue(wxGetApp().m_tone);
        m_txtToneFreqHz->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_tone_freq_hz));
        m_txtToneAmplitude->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_tone_amplitude));

        m_ckboxAttnCarrierEn->SetValue(wxGetApp().m_attn_carrier_en);
        m_txtAttnCarrier->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_attn_carrier));

#ifdef __EXPERIMENTAL_UDP__
        m_ckbox_events->SetValue(wxGetApp().m_events);
        m_txt_spam_timer->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_events_spam_timer));

        m_txt_events_regexp_match->SetValue(wxGetApp().m_events_regexp_match);
        m_txt_events_regexp_replace->SetValue(wxGetApp().m_events_regexp_replace);
        
        m_ckbox_udp_enable->SetValue(wxGetApp().m_udp_enable);
        m_txt_udp_port->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_udp_port));

#ifdef SHORT_VARICODE
        if (wxGetApp().m_textEncoding == 1)
            m_rb_textEncoding1->SetValue(true);
        if (wxGetApp().m_textEncoding == 2)
            m_rb_textEncoding2->SetValue(true);
#endif
        m_ckboxEnableChecksum->SetValue(wxGetApp().m_enable_checksum);
#endif

        m_ckboxFreeDV700txClip->SetValue(wxGetApp().m_FreeDV700txClip);
        m_ckboxFreeDV700Combine->SetValue(wxGetApp().m_FreeDV700Combine);
        m_txtInterleave->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_FreeDV700Interleave));
        m_ckboxFreeDV700ManualUnSync->SetValue(wxGetApp().m_FreeDV700ManualUnSync);

#ifdef __WXMSW__
        m_ckboxDebugConsole->SetValue(wxGetApp().m_debug_console);
#endif
    }

    if(inout == EXCHANGE_DATA_OUT)
    {
        wxGetApp().m_callSign = m_txtCtrlCallSign->GetValue();

        wxGetApp().m_boolHalfDuplex = m_ckHalfDuplex->GetValue();
        pConfig->Write(wxT("/Rig/HalfDuplex"), wxGetApp().m_boolHalfDuplex);

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

#ifdef __EXPERIMENTAL_UDP__
        wxGetApp().m_events        = m_ckbox_events->GetValue();
        long spam_timer;
        m_txt_spam_timer->GetValue().ToLong(&spam_timer);
        wxGetApp().m_events_spam_timer = (int)spam_timer;

        // make sure regexp lists are terminated by a \n

        if (m_txt_events_regexp_match->GetValue().Last() != '\n') {
            m_txt_events_regexp_match->SetValue(m_txt_events_regexp_match->GetValue()+'\n');
        }
        if (m_txt_events_regexp_replace->GetValue().Last() != '\n') {
            m_txt_events_regexp_replace->SetValue(m_txt_events_regexp_replace->GetValue()+'\n');
        }
        wxGetApp().m_events_regexp_match = m_txt_events_regexp_match->GetValue();
        wxGetApp().m_events_regexp_replace = m_txt_events_regexp_replace->GetValue();
 
        wxGetApp().m_udp_enable     = m_ckbox_udp_enable->GetValue();
        long port;
        m_txt_udp_port->GetValue().ToLong(&port);
        wxGetApp().m_udp_port       = (int)port;

#ifdef SHORT_VARICODE
        if (m_rb_textEncoding1->GetValue())
            wxGetApp().m_textEncoding = 1;
        if (m_rb_textEncoding2->GetValue())
            wxGetApp().m_textEncoding = 2;
#endif
        wxGetApp().m_enable_checksum = m_ckboxEnableChecksum->GetValue();
#endif

        wxGetApp().m_FreeDV700txClip = m_ckboxFreeDV700txClip->GetValue();
        wxGetApp().m_FreeDV700Combine = m_ckboxFreeDV700Combine->GetValue();
        long interleave;
        m_txtInterleave->GetValue().ToLong(&interleave);
        if (interleave < 1) {
            interleave = 1;
            m_txtInterleave->SetValue(wxString("1"));
        }
        if (interleave > 16) {
            interleave = 16;
            m_txtInterleave->SetValue(wxString("16"));
        }
        wxGetApp().m_FreeDV700Interleave = (int)interleave;
        wxGetApp().m_FreeDV700ManualUnSync = m_ckboxFreeDV700ManualUnSync->GetValue();

#ifdef __WXMSW__
        wxGetApp().m_debug_console = m_ckboxDebugConsole->GetValue();
#endif

        if (storePersistent) {
            pConfig->Write(wxT("/Data/CallSign"), wxGetApp().m_callSign);
#ifdef SHORT_VARICODE
            pConfig->Write(wxT("/Data/TextEncoding"), wxGetApp().m_textEncoding);
#endif
            pConfig->Write(wxT("/Data/EnableChecksumOnMsgRx"), wxGetApp().m_enable_checksum);

            pConfig->Write(wxT("/Events/enable"), wxGetApp().m_events);
            pConfig->Write(wxT("/Events/spam_timer"), wxGetApp().m_events_spam_timer);
            pConfig->Write(wxT("/Events/regexp_match"), wxGetApp().m_events_regexp_match);
            pConfig->Write(wxT("/Events/regexp_replace"), wxGetApp().m_events_regexp_replace);
            
            pConfig->Write(wxT("/UDP/enable"), wxGetApp().m_udp_enable);
            pConfig->Write(wxT("/UDP/port"),  wxGetApp().m_udp_port);

            pConfig->Write(wxT("/Events/spam_timer"), wxGetApp().m_events_spam_timer);

            pConfig->Write(wxT("/FreeDV700/txClip"), wxGetApp().m_FreeDV700txClip);
            pConfig->Write(wxT("/FreeDV700/interleave"), wxGetApp().m_FreeDV700Interleave);
            pConfig->Write(wxT("/FreeDV700/manualUnSync"), wxGetApp().m_FreeDV700ManualUnSync);

            pConfig->Write(wxT("/Noise/noise_snr"),  wxGetApp().m_noise_snr);

#ifdef __WXMSW__
            pConfig->Write(wxT("/Debug/console"), wxGetApp().m_debug_console);
#endif

            pConfig->Flush();
        }
    }
    delete wxConfigBase::Set((wxConfigBase *) NULL);
}

//-------------------------------------------------------------------------
// OnOK()
//-------------------------------------------------------------------------
void OptionsDlg::OnOK(wxCommandEvent& event)
{
    ExchangeData(EXCHANGE_DATA_OUT, true);
    //this->EndModal(wxID_OK);
    g_modal = false;
    this->Show(false);
}

//-------------------------------------------------------------------------
// OnCancel()
//-------------------------------------------------------------------------
void OptionsDlg::OnCancel(wxCommandEvent& event)
{
    //this->EndModal(wxID_CANCEL);
    g_modal = false;
    this->Show(false);
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
    long attn_carrier;
    m_txtAttnCarrier->GetValue().ToLong(&attn_carrier);
    wxGetApp().m_attn_carrier = (int)attn_carrier;

    /* uncheck -> checked, attenuate selected carrier */

    if (m_ckboxAttnCarrierEn->GetValue() && !wxGetApp().m_attn_carrier_en) {
        if (freedv_get_mode(g_pfreedv) == FREEDV_MODE_700C) {
            freedv_set_carrier_ampl(g_pfreedv, wxGetApp().m_attn_carrier, 0.25);
        } else {
            wxMessageBox("Carrier attenuation feature only works on 700C", wxT("Warning"), wxOK | wxICON_WARNING, this);
        }
    }

    /* checked -> unchecked, reset selected carrier */

    if (!m_ckboxAttnCarrierEn->GetValue() && wxGetApp().m_attn_carrier_en) {
        if (freedv_get_mode(g_pfreedv) == FREEDV_MODE_700C) {
            freedv_set_carrier_ampl(g_pfreedv, wxGetApp().m_attn_carrier, 1.0);
        }
    }
        
    wxGetApp().m_attn_carrier_en = m_ckboxAttnCarrierEn->GetValue();    
}

void OptionsDlg::OnFreeDV700txClip(wxScrollEvent& event) {
    wxGetApp().m_FreeDV700txClip = m_ckboxFreeDV700txClip->GetValue();
}

void OptionsDlg::OnFreeDV700Combine(wxScrollEvent& event) {
    wxGetApp().m_FreeDV700Combine = m_ckboxFreeDV700Combine->GetValue();
}

void OptionsDlg::updateEventLog(wxString event_in, wxString event_out) {
    wxString event_in_with_serial, event_out_with_serial; 
    event_in_with_serial.Printf(_T("[%d] %s"), event_in_serial++, event_in);
    event_out_with_serial.Printf(_T("[%d] %s"), event_out_serial++, event_out);

    m_txt_events_in->AppendText(event_in_with_serial+"\n");
    m_txt_events_out->AppendText(event_out_with_serial+"\n");
}


void OptionsDlg::OnDebugConsole(wxScrollEvent& event) {
    wxGetApp().m_debug_console = m_ckboxDebugConsole->GetValue();
#ifdef __WXMSW__
    // somewhere to send printfs while developing, causes conmsole to pop up on Windows
    if (wxGetApp().m_debug_console) {
        int ret = AllocConsole();
        freopen("CONOUT$", "w", stdout); 
        freopen("CONOUT$", "w", stderr); 
        fprintf(stderr, "AllocConsole: %d m_debug_console: %d\n", ret, wxGetApp().m_debug_console);
    } 
#endif
}


void OptionsDlg::OnFifoReset(wxCommandEvent& event)
{
    g_infifo1_full = g_outfifo1_empty = g_infifo2_full = g_outfifo2_empty = 0;
    for (int i=0; i<4; i++) {
        g_PAstatus1[i] = g_PAstatus2[i] = 0;
    }
}


void OptionsDlg::DisplayFifoPACounters() {
    char fifo_counters[80];

    sprintf(fifo_counters, "fifos: infull1: %d ooutempty1: %d infull2: %d outempty2: %d", g_infifo1_full, g_outfifo1_empty, g_infifo2_full, g_outfifo2_empty);
    wxString fifo_counters_string(fifo_counters);
    m_textFifos->SetLabel(fifo_counters_string);

    char pa_counters1[80];

    // input: underflow overflow output: underflow overflow
    sprintf(pa_counters1, "PA1: inUnderflow: %d inOverflow: %d outUnderflow %d outOverflow %d", g_PAstatus1[0], g_PAstatus1[1], g_PAstatus1[2], g_PAstatus1[3]);
    wxString pa_counters1_string(pa_counters1); m_textPA1->SetLabel(pa_counters1_string);

    char pa_counters2[80];

    // input: underflow overflow output: underflow overflow
    sprintf(pa_counters2, "PA2: inUnderflow: %d inOverflow: %d outUnderflow %d outOverflow %d", g_PAstatus2[0], g_PAstatus2[1], g_PAstatus2[2], g_PAstatus2[3]);
    wxString pa_counters2_string(pa_counters2);
    m_textPA2->SetLabel(pa_counters2_string);
}
