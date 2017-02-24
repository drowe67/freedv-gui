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

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class OptionsDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
OptionsDlg::OptionsDlg(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxDialog(parent, id, title, pos, size, style)
{

    this->SetSizeHints(wxDefaultSize, wxDefaultSize);

    wxBoxSizer* bSizer30;
    bSizer30 = new wxBoxSizer(wxVERTICAL);

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
    m_txtNoiseSNR = new wxTextCtrl(this, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(30,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_testFrames->Add(m_txtNoiseSNR, 0, wxALIGN_LEFT, 0);

    m_ckboxAttnCarrierEn = new wxCheckBox(this, wxID_ANY, _("Attn Carrier  Carrier:"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_testFrames->Add(m_ckboxAttnCarrierEn, 0, wxALIGN_LEFT, 0);
    m_txtAttnCarrier = new wxTextCtrl(this, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(30,-1), 0, wxTextValidator(wxFILTER_DIGITS));
    sbSizer_testFrames->Add(m_txtAttnCarrier, 0, wxALIGN_LEFT, 0);

    bSizer30->Add(sbSizer_testFrames,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);

    //------------------------------
    // FreeDV 700 Options
    //------------------------------

    wxStaticBoxSizer* sbSizer_freedv700;
    wxStaticBox *sb_freedv700 = new wxStaticBox(this, wxID_ANY, _("FreeDV 700 Options"));
    sbSizer_freedv700 = new wxStaticBoxSizer(sb_freedv700, wxHORIZONTAL);

    m_ckboxFreeDV700txClip = new wxCheckBox(this, wxID_ANY, _("Clipping"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_freedv700->Add(m_ckboxFreeDV700txClip, 0, wxALIGN_LEFT, 0);
    m_ckboxFreeDV700Combine = new wxCheckBox(this, wxID_ANY, _("Diversity Combine for plots"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_freedv700->Add(m_ckboxFreeDV700Combine, 0, wxALIGN_LEFT, 0);

    bSizer30->Add(sbSizer_freedv700,0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 3);

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

    bSizer30->Add(bSizer31, 0, wxALIGN_RIGHT|wxALL, 0);

    this->SetSizer(bSizer30);
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
        m_ckboxTestFrame->SetValue(wxGetApp().m_testFrames);

        m_ckboxChannelNoise->SetValue(wxGetApp().m_channel_noise);
        m_txtNoiseSNR->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_noise_snr));

        m_ckboxAttnCarrierEn->SetValue(wxGetApp().m_attn_carrier_en);
        m_txtAttnCarrier->SetValue(wxString::Format(wxT("%i"),wxGetApp().m_attn_carrier));

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

        m_ckboxFreeDV700txClip->SetValue(wxGetApp().m_FreeDV700txClip);
        m_ckboxFreeDV700Combine->SetValue(wxGetApp().m_FreeDV700Combine);
    }

    if(inout == EXCHANGE_DATA_OUT)
    {
        wxGetApp().m_callSign      = m_txtCtrlCallSign->GetValue();

        wxGetApp().m_testFrames    = m_ckboxTestFrame->GetValue();

        wxGetApp().m_channel_noise = m_ckboxChannelNoise->GetValue();
        long noise_snr;
        m_txtNoiseSNR->GetValue().ToLong(&noise_snr);
        wxGetApp().m_noise_snr = (int)noise_snr;

        wxGetApp().m_attn_carrier_en = m_ckboxAttnCarrierEn->GetValue();
        long attn_carrier;
        m_txtAttnCarrier->GetValue().ToLong(&attn_carrier);
        wxGetApp().m_attn_carrier = (int)attn_carrier;

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

        wxGetApp().m_FreeDV700txClip = m_ckboxFreeDV700txClip->GetValue();
        wxGetApp().m_FreeDV700Combine = m_ckboxFreeDV700Combine->GetValue();

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

            pConfig->Write(wxT("/Noise/noise_snr"),  wxGetApp().m_noise_snr);

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

//  Run time update of carrier amplitude attenuation

void OptionsDlg::OnAttnCarrierEn(wxScrollEvent& event) {
    long attn_carrier;
    m_txtAttnCarrier->GetValue().ToLong(&attn_carrier);
    wxGetApp().m_attn_carrier = (int)attn_carrier;

    /* uncheck -> checked, attenuate selected carrier */

    if (m_ckboxAttnCarrierEn->GetValue() && !wxGetApp().m_attn_carrier_en) {
        freedv_set_carrier_ampl(g_pfreedv, wxGetApp().m_attn_carrier, 0.25);
    }

    /* checked -> unchecked, reset selected carrier */

    if (!m_ckboxAttnCarrierEn->GetValue() && wxGetApp().m_attn_carrier_en) {
        freedv_set_carrier_ampl(g_pfreedv, wxGetApp().m_attn_carrier, 1.0);
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

