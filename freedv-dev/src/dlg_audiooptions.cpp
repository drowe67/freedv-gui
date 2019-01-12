//=========================================================================
// Name:            AudioOptsDialog.cpp
// Purpose:         Implements an Audio options selection dialog.
//
// Authors:         David Rowe, David Witten
// License:
//
//  All rights reserved.
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
//=========================================================================
#include "fdmdv2_main.h"
#include "dlg_audiooptions.h"

// constants for test waveform plots

#define TEST_WAVEFORM_X          180
#define TEST_WAVEFORM_Y          180
#define TEST_WAVEFORM_PLOT_TIME  2.0
#define TEST_WAVEFORM_PLOT_FS    400
#define TEST_BUF_SIZE           1024
#define TEST_FS                 48000.0
#define TEST_DT                 0.1      // time between plot updates in seconds
#define TEST_WAVEFORM_PLOT_BUF  ((int)(DT*400))

void AudioOptsDialog::Pa_Init(void)
{
    m_isPaInitialized = false;

    if((pa_err = Pa_Initialize()) == paNoError)
    {
        m_isPaInitialized = true;
    }
    else
    {
        wxMessageBox(wxT("Port Audio failed to initialize"), wxT("Pa_Initialize"), wxOK);
        return;
    }
}


void AudioOptsDialog::buildTestControls(PlotScalar **plotScalar, wxButton **btnTest, 
                                        wxPanel *parentPanel, wxBoxSizer *bSizer, wxString buttonLabel)
{
    wxBoxSizer* bSizer1 = new wxBoxSizer(wxVERTICAL);

    wxPanel *panel = new wxPanel(parentPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0);
    *plotScalar = new PlotScalar((wxFrame*) panel, 1, TEST_WAVEFORM_PLOT_TIME, 1.0/TEST_WAVEFORM_PLOT_FS, -1, 1, 1, 0.2, "", 1);
    (*plotScalar)->SetClientSize(wxSize(TEST_WAVEFORM_X,TEST_WAVEFORM_Y));
    bSizer1->Add(panel, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 8);

    *btnTest = new wxButton(parentPanel, wxID_ANY, buttonLabel, wxDefaultPosition, wxDefaultSize);
    bSizer1->Add(*btnTest, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 0);

    bSizer->Add(bSizer1, 0, wxALIGN_CENTER_HORIZONTAL |wxALIGN_CENTER_VERTICAL );
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// AudioOptsDialog()
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
AudioOptsDialog::AudioOptsDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxDialog(parent, id, title, pos, size, style)
{
    //this->SetSizeHints(wxSize(850, 600), wxDefaultSize);
    fprintf(stderr, "pos %d %d\n", pos.x, pos.y);
    Pa_Init();

    wxBoxSizer* mainSizer;
    mainSizer = new wxBoxSizer(wxVERTICAL);
    m_panel1 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    wxBoxSizer* bSizer4;
    bSizer4 = new wxBoxSizer(wxVERTICAL);
    m_notebook1 = new wxNotebook(m_panel1, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_BOTTOM);
    m_panelRx = new wxPanel(m_notebook1, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    wxBoxSizer* bSizer20;
    bSizer20 = new wxBoxSizer(wxVERTICAL);
    wxGridSizer* gSizer4;
    gSizer4 = new wxGridSizer(2, 1, 0, 0);

    // Rx In -----------------------------------------------------------------------

    wxStaticBoxSizer* sbSizer2;
    sbSizer2 = new wxStaticBoxSizer(new wxStaticBox(m_panelRx, wxID_ANY, _("From Radio")), wxHORIZONTAL);

    wxBoxSizer* bSizer811a = new wxBoxSizer(wxVERTICAL);

    m_listCtrlRxInDevices = new wxListCtrl(m_panelRx, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_HRULES|wxLC_REPORT|wxLC_VRULES);
    bSizer811a->Add(m_listCtrlRxInDevices, 1, wxALL|wxEXPAND, 1);

    wxBoxSizer* bSizer811;
    bSizer811 = new wxBoxSizer(wxHORIZONTAL);
    m_staticText51 = new wxStaticText(m_panelRx, wxID_ANY, _("Device:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText51->Wrap(-1);
    bSizer811->Add(m_staticText51, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 5);
    m_textCtrlRxIn = new wxTextCtrl(m_panelRx, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    bSizer811->Add(m_textCtrlRxIn, 1, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    m_staticText6 = new wxStaticText(m_panelRx, wxID_ANY, _("Sample Rate:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText6->Wrap(-1);
    bSizer811->Add(m_staticText6, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 5);
    m_cbSampleRateRxIn = new wxComboBox(m_panelRx, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(90,-1), 0, NULL, wxCB_DROPDOWN);
    bSizer811->Add(m_cbSampleRateRxIn, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);

    bSizer811a->Add(bSizer811, 0, wxEXPAND, 5);

    sbSizer2->Add(bSizer811a, 1, wxEXPAND, 2);
    buildTestControls(&m_plotScalarRxIn, &m_btnRxInTest, m_panelRx, sbSizer2, _("Rec 2s"));

    gSizer4->Add(sbSizer2, 1, wxEXPAND, 5);

    // Rx Out -----------------------------------------------------------------------

    wxStaticBoxSizer* sbSizer3;
    sbSizer3 = new wxStaticBoxSizer(new wxStaticBox(m_panelRx, wxID_ANY, _("To Speaker/Headphones")), wxHORIZONTAL);

    wxBoxSizer* bSizer81a = new wxBoxSizer(wxVERTICAL);

    m_listCtrlRxOutDevices = new wxListCtrl(m_panelRx, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_HRULES|wxLC_REPORT|wxLC_VRULES);
    bSizer81a->Add(m_listCtrlRxOutDevices, 1, wxALL|wxEXPAND, 1);

    wxBoxSizer* bSizer81;
    bSizer81 = new wxBoxSizer(wxHORIZONTAL);
    m_staticText9 = new wxStaticText(m_panelRx, wxID_ANY, _("Device:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText9->Wrap(-1);
    bSizer81->Add(m_staticText9, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    m_textCtrlRxOut = new wxTextCtrl(m_panelRx, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    bSizer81->Add(m_textCtrlRxOut, 1, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    m_staticText10 = new wxStaticText(m_panelRx, wxID_ANY, _("Sample Rate:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText10->Wrap(-1);
    bSizer81->Add(m_staticText10, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 5);
    m_cbSampleRateRxOut = new wxComboBox(m_panelRx, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(90,-1), 0, NULL, wxCB_DROPDOWN);
    bSizer81->Add(m_cbSampleRateRxOut, 0, wxALIGN_CENTER_VERTICAL|wxALL, 1);

    bSizer81a->Add(bSizer81, 0, wxEXPAND, 5);

    sbSizer3->Add(bSizer81a, 1, wxEXPAND, 2);
    buildTestControls(&m_plotScalarRxOut, &m_btnRxOutTest, m_panelRx, sbSizer3, _("Play 2s"));
 
    gSizer4->Add(sbSizer3, 1, wxEXPAND, 2);
    bSizer20->Add(gSizer4, 1, wxEXPAND, 1);
    m_panelRx->SetSizer(bSizer20);
    m_panelRx->Layout();
    bSizer20->Fit(m_panelRx);
    m_notebook1->AddPage(m_panelRx, _("Receive"), true);

    // Tx Tab -------------------------------------------------------------------------------

    m_panelTx = new wxPanel(m_notebook1, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    wxBoxSizer* bSizer18;
    bSizer18 = new wxBoxSizer(wxVERTICAL);
    wxGridSizer* gSizer2;
    gSizer2 = new wxGridSizer(2, 1, 0, 0);

    // Tx In ----------------------------------------------------------------------------------

    wxStaticBoxSizer* sbSizer22;
    sbSizer22 = new wxStaticBoxSizer(new wxStaticBox(m_panelTx, wxID_ANY, _("From Microphone")), wxHORIZONTAL);

    wxBoxSizer* bSizer83a = new wxBoxSizer(wxVERTICAL);

    m_listCtrlTxInDevices = new wxListCtrl(m_panelTx, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_HRULES|wxLC_REPORT|wxLC_VRULES);
    bSizer83a->Add(m_listCtrlTxInDevices, 1, wxALL|wxEXPAND, 1);
    wxBoxSizer* bSizer83;
    bSizer83 = new wxBoxSizer(wxHORIZONTAL);
    m_staticText12 = new wxStaticText(m_panelTx, wxID_ANY, _("Device:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText12->Wrap(-1);
    bSizer83->Add(m_staticText12, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 5);
    m_textCtrlTxIn = new wxTextCtrl(m_panelTx, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    bSizer83->Add(m_textCtrlTxIn, 1, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    m_staticText11 = new wxStaticText(m_panelTx, wxID_ANY, _("Sample Rate:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText11->Wrap(-1);
    bSizer83->Add(m_staticText11, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 5);
    m_cbSampleRateTxIn = new wxComboBox(m_panelTx, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(90,-1), 0, NULL, wxCB_DROPDOWN);
    bSizer83->Add(m_cbSampleRateTxIn, 0, wxALL, 1);

    bSizer83a->Add(bSizer83, 0, wxEXPAND, 5);

    sbSizer22->Add(bSizer83a, 1, wxEXPAND, 2);
    buildTestControls(&m_plotScalarTxIn, &m_btnTxInTest, m_panelTx, sbSizer22, _("Rec 2s"));

    gSizer2->Add(sbSizer22, 1, wxEXPAND, 5);

    // Tx Out ----------------------------------------------------------------------------------

    wxStaticBoxSizer* sbSizer21;
    sbSizer21 = new wxStaticBoxSizer(new wxStaticBox(m_panelTx, wxID_ANY, _("To Radio")), wxHORIZONTAL);

    wxBoxSizer* bSizer82a = new wxBoxSizer(wxVERTICAL);

    m_listCtrlTxOutDevices = new wxListCtrl(m_panelTx, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_HRULES|wxLC_REPORT|wxLC_VRULES);
    bSizer82a->Add(m_listCtrlTxOutDevices, 1, wxALL|wxEXPAND, 2);
    wxBoxSizer* bSizer82;
    bSizer82 = new wxBoxSizer(wxHORIZONTAL);
    m_staticText81 = new wxStaticText(m_panelTx, wxID_ANY, _("Device:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText81->Wrap(-1);
    bSizer82->Add(m_staticText81, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    m_textCtrlTxOut = new wxTextCtrl(m_panelTx, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    bSizer82->Add(m_textCtrlTxOut, 1, wxALIGN_CENTER_VERTICAL|wxALL, 1);
    m_staticText71 = new wxStaticText(m_panelTx, wxID_ANY, _("Sample Rate:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText71->Wrap(-1);
    bSizer82->Add(m_staticText71, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 5);
    m_cbSampleRateTxOut = new wxComboBox(m_panelTx, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(90,-1), 0, NULL, wxCB_DROPDOWN);
    bSizer82->Add(m_cbSampleRateTxOut, 0, wxALL, 1);

    bSizer82a->Add(bSizer82, 0, wxEXPAND, 5);

    sbSizer21->Add(bSizer82a, 1, wxEXPAND, 2);
    buildTestControls(&m_plotScalarTxOut, &m_btnTxOutTest, m_panelTx, sbSizer21, _("Play 2s"));

    gSizer2->Add(sbSizer21, 1, wxEXPAND, 5);
    bSizer18->Add(gSizer2, 1, wxEXPAND, 1);
    m_panelTx->SetSizer(bSizer18);
    m_panelTx->Layout();
    bSizer18->Fit(m_panelTx);
    m_notebook1->AddPage(m_panelTx, _("Transmit"), false);

    // API Tab -------------------------------------------------------------------

    m_panelAPI = new wxPanel(m_notebook1, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    wxBoxSizer* bSizer12;
    bSizer12 = new wxBoxSizer(wxHORIZONTAL);
    wxGridSizer* gSizer31;
    gSizer31 = new wxGridSizer(2, 1, 0, 0);
    wxStaticBoxSizer* sbSizer1;
    sbSizer1 = new wxStaticBoxSizer(new wxStaticBox(m_panelAPI, wxID_ANY, _("PortAudio")), wxVERTICAL);

    wxGridSizer* gSizer3;
    gSizer3 = new wxGridSizer(4, 2, 0, 0);

    m_staticText7 = new wxStaticText(m_panelAPI, wxID_ANY, _("PortAudio Version String:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText7->Wrap(-1);
    gSizer3->Add(m_staticText7, 1, wxALIGN_RIGHT|wxALL|wxALIGN_CENTER_VERTICAL, 10);
    m_textStringVer = new wxStaticText(m_panelAPI, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    gSizer3->Add(m_textStringVer, 1, wxALIGN_LEFT|wxALL|wxALIGN_CENTER_VERTICAL, 10);

    m_staticText8 = new wxStaticText(m_panelAPI, wxID_ANY, _("PortAudio Int Version:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText8->Wrap(-1);
    gSizer3->Add(m_staticText8, 1, wxALIGN_RIGHT|wxALL|wxALIGN_CENTER_VERTICAL, 10);
    m_textIntVer = new wxStaticText(m_panelAPI, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(45,-1), 0);
    gSizer3->Add(m_textIntVer, 1, wxALIGN_LEFT|wxALL|wxALIGN_CENTER_VERTICAL, 10);

    m_staticText5 = new wxStaticText(m_panelAPI, wxID_ANY, _("Device Count:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText5->Wrap(-1);
    gSizer3->Add(m_staticText5, 1, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 10);
    m_textCDevCount = new wxStaticText(m_panelAPI, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(45,-1), 0);
    gSizer3->Add(m_textCDevCount, 1, wxALIGN_LEFT|wxALL|wxALIGN_CENTER_VERTICAL, 10);

    m_staticText4 = new wxStaticText(m_panelAPI, wxID_ANY, _("API Count:"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText4->Wrap(-1);
    gSizer3->Add(m_staticText4, 1, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxALL, 10);
    m_textAPICount = new wxStaticText(m_panelAPI, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(45,-1), 0);
    m_textAPICount->SetMaxSize(wxSize(45,-1));
    gSizer3->Add(m_textAPICount, 1, wxALIGN_LEFT|wxALL|wxALIGN_CENTER_VERTICAL, 10);

    sbSizer1->Add(gSizer3, 1, wxEXPAND, 2);
    gSizer31->Add(sbSizer1, 1, wxEXPAND, 2);
    wxStaticBoxSizer* sbSizer6;
    sbSizer6 = new wxStaticBoxSizer(new wxStaticBox(m_panelAPI, wxID_ANY, _("Other")), wxVERTICAL);
    gSizer31->Add(sbSizer6, 1, wxEXPAND, 5);
    bSizer12->Add(gSizer31, 1, wxEXPAND, 5);
    m_panelAPI->SetSizer(bSizer12);
    m_panelAPI->Layout();
    bSizer12->Fit(m_panelAPI);
    m_notebook1->AddPage(m_panelAPI, _("API Info"), false);
    bSizer4->Add(m_notebook1, 1, wxEXPAND | wxALL, 0);
    m_panel1->SetSizer(bSizer4);
    m_panel1->Layout();
    bSizer4->Fit(m_panel1);
    mainSizer->Add(m_panel1, 1, wxEXPAND | wxALL, 1);

    wxBoxSizer* bSizer6;
    bSizer6 = new wxBoxSizer(wxHORIZONTAL);
    m_btnRefresh = new wxButton(this, wxID_ANY, _("Refresh"), wxDefaultPosition, wxDefaultSize, 0);
    bSizer6->Add(m_btnRefresh, 0, wxALIGN_CENTER|wxALL, 2);

    m_sdbSizer1 = new wxStdDialogButtonSizer();

    m_sdbSizer1OK = new wxButton(this, wxID_OK);
    m_sdbSizer1->AddButton(m_sdbSizer1OK);

    m_sdbSizer1Cancel = new wxButton(this, wxID_CANCEL);
    m_sdbSizer1->AddButton(m_sdbSizer1Cancel);

    m_sdbSizer1Apply = new wxButton(this, wxID_APPLY);
    m_sdbSizer1->AddButton(m_sdbSizer1Apply);

    m_sdbSizer1->Realize();

    bSizer6->Add(m_sdbSizer1, 1, wxALIGN_CENTER_VERTICAL, 2);
    mainSizer->Add(bSizer6, 0, wxEXPAND, 2);
    this->SetSizer(mainSizer);
    this->Layout();
    this->Centre(wxBOTH);
//    this->Centre(wxBOTH);

    m_notebook1->SetSelection(0);

    showAPIInfo();
    m_RxInDevices.m_listDevices   = m_listCtrlRxInDevices;
    m_RxInDevices.direction       = AUDIO_IN;
    m_RxInDevices.m_textDevice    = m_textCtrlRxIn;
    m_RxInDevices.m_cbSampleRate  = m_cbSampleRateRxIn;

    m_RxOutDevices.m_listDevices  = m_listCtrlRxOutDevices;
    m_RxOutDevices.direction      = AUDIO_OUT;
    m_RxOutDevices.m_textDevice   = m_textCtrlRxOut;
    m_RxOutDevices.m_cbSampleRate = m_cbSampleRateRxOut;

    m_TxInDevices.m_listDevices   = m_listCtrlTxInDevices;
    m_TxInDevices.direction       = AUDIO_IN;
    m_TxInDevices.m_textDevice    = m_textCtrlTxIn;
    m_TxInDevices.m_cbSampleRate  = m_cbSampleRateTxIn;

    m_TxOutDevices.m_listDevices  = m_listCtrlTxOutDevices;
    m_TxOutDevices.direction      = AUDIO_OUT;
    m_TxOutDevices.m_textDevice   = m_textCtrlTxOut;
    m_TxOutDevices.m_cbSampleRate = m_cbSampleRateTxOut;

    populateParams(m_RxInDevices);
    populateParams(m_RxOutDevices);
    populateParams(m_TxInDevices);
    populateParams(m_TxOutDevices);

    m_listCtrlRxInDevices->Connect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( AudioOptsDialog::OnRxInDeviceSelect ), NULL, this );
    m_listCtrlRxOutDevices->Connect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( AudioOptsDialog::OnRxOutDeviceSelect ), NULL, this );
    m_listCtrlTxInDevices->Connect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( AudioOptsDialog::OnTxInDeviceSelect ), NULL, this );
    m_listCtrlTxOutDevices->Connect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( AudioOptsDialog::OnTxOutDeviceSelect ), NULL, this );

    // wire up test buttons
    m_btnRxInTest->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnRxInTest ), NULL, this );
    m_btnRxOutTest->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnRxOutTest ), NULL, this );
    m_btnTxInTest->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnTxInTest ), NULL, this );
    m_btnTxOutTest->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnTxOutTest ), NULL, this );

    m_btnRefresh->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnRefreshClick ), NULL, this );
    m_sdbSizer1Apply->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnApplyAudioParameters ), NULL, this );
    m_sdbSizer1Cancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnCancelAudioParameters ), NULL, this );
    m_sdbSizer1OK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnOkAudioParameters ), NULL, this );
/*
        void OnClose( wxCloseEvent& event ) { event.Skip(); }
        void OnHibernate( wxActivateEvent& event ) { event.Skip(); }
        void OnIconize( wxIconizeEvent& event ) { event.Skip(); }
        void OnInitDialog( wxInitDialogEvent& event ) { event.Skip(); }
*/
//    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(AudioOptsDialog::OnClose));
    this->Connect(wxEVT_HIBERNATE, wxActivateEventHandler(AudioOptsDialog::OnHibernate));
    this->Connect(wxEVT_ICONIZE, wxIconizeEventHandler(AudioOptsDialog::OnIconize));
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(AudioOptsDialog::OnInitDialog));
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// ~AudioOptsDialog()
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
AudioOptsDialog::~AudioOptsDialog()
{
    Pa_Terminate();

    // Disconnect Events
    this->Disconnect(wxEVT_HIBERNATE, wxActivateEventHandler(AudioOptsDialog::OnHibernate));
    this->Disconnect(wxEVT_ICONIZE, wxIconizeEventHandler(AudioOptsDialog::OnIconize));
    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(AudioOptsDialog::OnInitDialog));

    m_listCtrlRxInDevices->Disconnect(wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler(AudioOptsDialog::OnRxInDeviceSelect), NULL, this);
    m_listCtrlRxOutDevices->Disconnect(wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler(AudioOptsDialog::OnRxOutDeviceSelect), NULL, this);
    m_listCtrlTxInDevices->Disconnect(wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler(AudioOptsDialog::OnTxInDeviceSelect), NULL, this);
    m_listCtrlTxOutDevices->Disconnect(wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler(AudioOptsDialog::OnTxOutDeviceSelect), NULL, this);

    m_btnRxInTest->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnRxInTest ), NULL, this );
    m_btnRxOutTest->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnRxOutTest ), NULL, this );
    m_btnTxInTest->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnTxInTest ), NULL, this );
    m_btnTxOutTest->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( AudioOptsDialog::OnTxOutTest ), NULL, this );

    m_btnRefresh->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(AudioOptsDialog::OnRefreshClick), NULL, this);
    m_sdbSizer1Apply->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(AudioOptsDialog::OnApplyAudioParameters), NULL, this);
    m_sdbSizer1Cancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(AudioOptsDialog::OnCancelAudioParameters), NULL, this);
    m_sdbSizer1OK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(AudioOptsDialog::OnOkAudioParameters), NULL, this);

}

//-------------------------------------------------------------------------
// OnInitDialog()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnInitDialog( wxInitDialogEvent& event )
{
    ExchangeData(EXCHANGE_DATA_IN);
}

//-------------------------------------------------------------------------
// OnInitDialog()
//
// helper function to look up name of devNum, and if it exists write
// name to textCtrl.  Used to trap dissapearing devices.
//-------------------------------------------------------------------------
int AudioOptsDialog::setTextCtrlIfDevNumValid(wxTextCtrl *textCtrl, wxListCtrl *listCtrl, int devNum)
{
    int i, aDevNum, found_devNum;

    // ignore last list entry as it is the "none" entry

    found_devNum = 0;
    for(i=0; i<listCtrl->GetItemCount()-1; i++) {
        aDevNum = wxAtoi(listCtrl->GetItemText(i, 1));
        //printf("aDevNum: %d devNum: %d\n", aDevNum, devNum);
        if (aDevNum == devNum) {
            found_devNum = 1;
            textCtrl->SetValue(listCtrl->GetItemText(i, 0) + " (" + wxString::Format(wxT("%i"),devNum) + ")");
            printf("setting focus of %d\n", i);
            listCtrl->SetItemState(i, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
        }
    }

    if (found_devNum) 
        return devNum;
    else {
        textCtrl->SetValue("none");
        return -1;
    }
}

//-------------------------------------------------------------------------
// ExchangeData()
//-------------------------------------------------------------------------
int AudioOptsDialog::ExchangeData(int inout)
{
    if(inout == EXCHANGE_DATA_IN)
    {
        // Map sound card device numbers to tx/rx device numbers depending
        // on number of sound cards in use

        printf("EXCHANGE_DATA_IN:\n");
        printf("  g_nSoundCards: %d\n", g_nSoundCards);
        printf("  g_soundCard1InDeviceNum: %d\n", g_soundCard1InDeviceNum);
        printf("  g_soundCard1OutDeviceNum: %d\n", g_soundCard1OutDeviceNum);
        printf("  g_soundCard1SampleRate: %d\n", g_soundCard1SampleRate);
        printf("  g_soundCard2InDeviceNum: %d\n", g_soundCard2InDeviceNum);
        printf("  g_soundCard2OutDeviceNum: %d\n", g_soundCard2OutDeviceNum);
        printf("  g_soundCard2SampleRate: %d\n", g_soundCard2SampleRate);

        if (g_nSoundCards == 0) {
            m_textCtrlRxIn ->SetValue("none"); rxInAudioDeviceNum  = -1;
            m_textCtrlRxOut->SetValue("none"); rxOutAudioDeviceNum = -1;
            m_textCtrlTxIn ->SetValue("none"); txInAudioDeviceNum  = -1;
            m_textCtrlTxOut->SetValue("none"); txOutAudioDeviceNum = -1;           
        }

        if (g_nSoundCards == 1) {
            rxInAudioDeviceNum  = setTextCtrlIfDevNumValid(m_textCtrlRxIn, 
                                                           m_listCtrlRxInDevices, 
                                                           g_soundCard1InDeviceNum);

            rxOutAudioDeviceNum = setTextCtrlIfDevNumValid(m_textCtrlRxOut, 
                                                           m_listCtrlRxOutDevices, 
                                                           g_soundCard1OutDeviceNum);

            if ((rxInAudioDeviceNum != -1) && (rxInAudioDeviceNum != -1)) {
                m_cbSampleRateRxIn->SetValue(wxString::Format(wxT("%i"),g_soundCard1SampleRate));
                m_cbSampleRateRxOut->SetValue(wxString::Format(wxT("%i"),g_soundCard1SampleRate));
            }

            m_textCtrlTxIn ->SetValue("none"); txInAudioDeviceNum  = -1;
            m_textCtrlTxOut->SetValue("none"); txOutAudioDeviceNum = -1;           
        }

        if (g_nSoundCards == 2) {
 
            rxInAudioDeviceNum  = setTextCtrlIfDevNumValid(m_textCtrlRxIn, 
                                                           m_listCtrlRxInDevices, 
                                                           g_soundCard1InDeviceNum);

            rxOutAudioDeviceNum = setTextCtrlIfDevNumValid(m_textCtrlRxOut, 
                                                           m_listCtrlRxOutDevices, 
                                                           g_soundCard2OutDeviceNum);

            txInAudioDeviceNum  = setTextCtrlIfDevNumValid(m_textCtrlTxIn, 
                                                           m_listCtrlTxInDevices, 
                                                           g_soundCard2InDeviceNum);

            txOutAudioDeviceNum = setTextCtrlIfDevNumValid(m_textCtrlTxOut, 
                                                           m_listCtrlTxOutDevices, 
                                                           g_soundCard1OutDeviceNum);

            if ((rxInAudioDeviceNum != -1) && (txOutAudioDeviceNum != -1)) {
                m_cbSampleRateRxIn->SetValue(wxString::Format(wxT("%i"),g_soundCard1SampleRate));
                m_cbSampleRateTxOut->SetValue(wxString::Format(wxT("%i"),g_soundCard1SampleRate));
            }

            if ((txInAudioDeviceNum != -1) && (rxOutAudioDeviceNum != -1)) {
                m_cbSampleRateTxIn->SetValue(wxString::Format(wxT("%i"),g_soundCard2SampleRate));
                m_cbSampleRateRxOut->SetValue(wxString::Format(wxT("%i"),g_soundCard2SampleRate));
            }
        }
        printf("  rxInAudioDeviceNum: %d\n  rxOutAudioDeviceNum: %d\n  txInAudioDeviceNum: %d\n  txOutAudioDeviceNum: %d\n",
               rxInAudioDeviceNum, rxOutAudioDeviceNum, txInAudioDeviceNum, txOutAudioDeviceNum);
    }

    if(inout == EXCHANGE_DATA_OUT)
    {
        int valid_one_card_config = 0;
        int valid_two_card_config = 0;
        wxString sampleRate1, sampleRate2;

        printf("EXCHANGE_DATA_OUT:\n");
        printf("  rxInAudioDeviceNum: %d\n  rxOutAudioDeviceNum: %d\n  txInAudioDeviceNum: %d\n  txOutAudioDeviceNum: %d\n",
               rxInAudioDeviceNum, rxOutAudioDeviceNum, txInAudioDeviceNum, txOutAudioDeviceNum);

        // ---------------------------------------------------------------
        // check we have a valid 1 or 2 sound card configuration
        // ---------------------------------------------------------------

        // one sound card config, tx device numbers should be set to -1 

        if ((rxInAudioDeviceNum != -1) && (rxOutAudioDeviceNum != -1) &&
            (txInAudioDeviceNum == -1) && (txOutAudioDeviceNum == -1)) {
 
            valid_one_card_config = 1; 

            // in and out sample rate must be the same, as there is one callback
            
            sampleRate1 = m_cbSampleRateRxIn->GetValue();
            if (!sampleRate1.IsSameAs(m_cbSampleRateRxOut->GetValue())) {
                wxMessageBox(wxT("With a single sound card the Sample Rate of "
                                 "From Radio and To Speaker/Headphones must be the same."), wxT(""), wxOK);
                return -1;
            }
        }

        // two card configuration

        if ((rxInAudioDeviceNum != -1) && (rxOutAudioDeviceNum != -1) &&
            (txInAudioDeviceNum != -1) && (txOutAudioDeviceNum != -1)) {

            valid_two_card_config = 1; 

            // Check we haven't doubled up on sound devices

            if (rxInAudioDeviceNum == txInAudioDeviceNum) {
                wxMessageBox(wxT("You must use different devices for From Radio and From Microphone"), wxT(""), wxOK);
                return -1;
            }

            if (rxOutAudioDeviceNum == txOutAudioDeviceNum) {
                wxMessageBox(wxT("You must use different devices for To Radio and To Speaker/Headphones"), wxT(""), wxOK);
                return -1;
            }

            // Check sample rates for callback 1 devices are the same,
            // as input and output are handled synchronously by one
            // portaudio callback
            
            sampleRate1 = m_cbSampleRateRxIn->GetValue();
            if (!sampleRate1.IsSameAs(m_cbSampleRateTxOut->GetValue())) {
                wxMessageBox(wxT("With two sound cards the Sample Rate "
                                 "of From Radio and To Radio must be the same."), wxT(""), wxOK);
                return -1;
            }
 
            // check sample rate for callback 2 devices is the same

            sampleRate2 = m_cbSampleRateTxIn->GetValue();
            if (!sampleRate2.IsSameAs(m_cbSampleRateRxOut->GetValue())) {
                wxMessageBox(wxT("With two sound cards the Sample Rate of "
                                 "From Microphone and To Speaker/Headphones must be the same."), wxT(""), wxOK);
                return -1;
            }
 
        }

        printf("  valid_one_card_config: %d  valid_two_card_config: %d\n", valid_one_card_config, valid_two_card_config);

        if (!valid_one_card_config && !valid_two_card_config) {
            wxMessageBox(wxT("Invalid one or two sound card configuration"), wxT(""), wxOK);
            return -1;
        }

        // ---------------------------------------------------------------
        // Map Rx/TX device numbers to sound card device numbers used
        // in callbacks. Portaudio uses one callback per sound card so
        // we have to be soundcard oriented at run time rather than
        // Tx/Rx oriented as in this dialog.
        // ---------------------------------------------------------------
        g_nSoundCards = 0;
        g_soundCard1InDeviceNum = g_soundCard1OutDeviceNum = g_soundCard2InDeviceNum = g_soundCard2OutDeviceNum = -1;

        if (valid_one_card_config) {

            // Only callback 1 used

            g_nSoundCards = 1;
            g_soundCard1InDeviceNum  = rxInAudioDeviceNum;
            g_soundCard1OutDeviceNum = rxOutAudioDeviceNum;
            g_soundCard1SampleRate = wxAtoi(sampleRate1);
        }

        if (valid_two_card_config) {
            g_nSoundCards = 2;
            g_soundCard1InDeviceNum  = rxInAudioDeviceNum;
            g_soundCard1OutDeviceNum = txOutAudioDeviceNum;
            g_soundCard1SampleRate   = wxAtoi(sampleRate1);
            g_soundCard2InDeviceNum  = txInAudioDeviceNum;
            g_soundCard2OutDeviceNum = rxOutAudioDeviceNum;
            g_soundCard2SampleRate   = wxAtoi(sampleRate2);
        }

        printf("  g_nSoundCards: %d\n", g_nSoundCards);
        printf("  g_soundCard1InDeviceNum: %d\n", g_soundCard1InDeviceNum);
        printf("  g_soundCard1OutDeviceNum: %d\n", g_soundCard1OutDeviceNum);
        printf("  g_soundCard1SampleRate: %d\n", g_soundCard1SampleRate);
        printf("  g_soundCard2InDeviceNum: %d\n", g_soundCard2InDeviceNum);
        printf("  g_soundCard2OutDeviceNum: %d\n", g_soundCard2OutDeviceNum);
        printf("  g_soundCard2SampleRate: %d\n", g_soundCard2SampleRate);

        wxConfigBase *pConfig = wxConfigBase::Get();
        pConfig->Write(wxT("/Audio/soundCard1InDeviceNum"),       g_soundCard1InDeviceNum);
        pConfig->Write(wxT("/Audio/soundCard1OutDeviceNum"),      g_soundCard1OutDeviceNum);
        pConfig->Write(wxT("/Audio/soundCard1SampleRate"),        g_soundCard1SampleRate );

        pConfig->Write(wxT("/Audio/soundCard2InDeviceNum"),       g_soundCard2InDeviceNum);
        pConfig->Write(wxT("/Audio/soundCard2OutDeviceNum"),      g_soundCard2OutDeviceNum);
        pConfig->Write(wxT("/Audio/soundCard2SampleRate"),        g_soundCard2SampleRate );

        pConfig->Flush();
        delete wxConfigBase::Set((wxConfigBase *) NULL);
    }

    return 0;
}

//-------------------------------------------------------------------------
// buildListOfSupportedSampleRates()
//-------------------------------------------------------------------------
int AudioOptsDialog:: buildListOfSupportedSampleRates(wxComboBox *cbSampleRate, int devNum, int in_out)
{
    // every sound device has a different list of supported sample rates, so
    // we work out which ones are supported and populate the list ctrl

    static double standardSampleRates[] =
    {
        8000.0,     9600.0,
        11025.0,    12000.0,
        16000.0,    22050.0,
        24000.0,    32000.0,
        44100.0,    48000.0,
        88200.0,    96000.0,
        192000.0,   -1          // negative terminated  list
    };

    const PaDeviceInfo  *deviceInfo;
    PaStreamParameters   inputParameters, outputParameters;
    PaError              err;
    wxString             str;
    int                  i, numSampleRates;

    deviceInfo = Pa_GetDeviceInfo(devNum);
    if (deviceInfo == NULL) {
        printf("Pa_GetDeviceInfo(%d) failed!\n", devNum);
        cbSampleRate->Clear();
        return 0;
    }

    inputParameters.device = devNum;
    inputParameters.channelCount = deviceInfo->maxInputChannels;
    inputParameters.sampleFormat = paInt16;
    inputParameters.suggestedLatency = 0;
    inputParameters.hostApiSpecificStreamInfo = NULL;
        
    outputParameters.device = devNum;
    outputParameters.channelCount = deviceInfo->maxOutputChannels;
    outputParameters.sampleFormat = paInt16;
    outputParameters.suggestedLatency = 0;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    
    cbSampleRate->Clear();
    //printf("devNum %d supports: ", devNum);
    numSampleRates = 0;
    for(i = 0; standardSampleRates[i] > 0; i++)
    {      
        if (in_out == AUDIO_IN)
            err = Pa_IsFormatSupported(&inputParameters, NULL, standardSampleRates[i]);
        else
            err = Pa_IsFormatSupported(NULL, &outputParameters, standardSampleRates[i]);

        if( err == paFormatIsSupported ) {
            str.Printf("%i", (int)standardSampleRates[i]);
            cbSampleRate->AppendString(str);
            printf("%i ", (int)standardSampleRates[i]);
            numSampleRates++;
        }
    }
    printf("\n");

    return numSampleRates;
}

//-------------------------------------------------------------------------
// showAPIInfo()
//-------------------------------------------------------------------------
void AudioOptsDialog::showAPIInfo()
{
    wxString    strval;
    int         apiVersion;
    int         apiCount        = 0;
    int         numDevices      = 0;

    strval = Pa_GetVersionText();
    m_textStringVer->SetLabel(strval);

    apiVersion = Pa_GetVersion();
    strval.Printf(wxT("%d"), apiVersion);
    m_textIntVer->SetLabel(strval);

    apiCount = Pa_GetHostApiCount();
    strval.Printf(wxT("%d"), apiCount);
    m_textAPICount->SetLabel(strval);

    numDevices = Pa_GetDeviceCount();
    strval.Printf(wxT("%d"), numDevices);
    m_textCDevCount->SetLabel(strval);
}

//-------------------------------------------------------------------------
// populateParams()
//-------------------------------------------------------------------------
void AudioOptsDialog::populateParams(AudioInfoDisplay ai)
{
    const       PaDeviceInfo *deviceInfo = NULL;
    wxListCtrl* ctrl    = ai.m_listDevices;
    int         in_out  = ai.direction;
    long        idx;
    int         numDevices;
    wxListItem  listItem;
    wxString    buf;
    int         devn;
    int         col = 0;

    numDevices = Pa_GetDeviceCount();

    if(ctrl->GetColumnCount() > 0)
    {
        ctrl->ClearAll();
    }

    listItem.SetAlign(wxLIST_FORMAT_LEFT);
    listItem.SetText(wxT("Device"));
    idx = ctrl->InsertColumn(col, listItem);
    ctrl->SetColumnWidth(col++, 300);

    listItem.SetAlign(wxLIST_FORMAT_CENTRE);
    listItem.SetText(wxT("ID"));
    idx = ctrl->InsertColumn(col, listItem);
    ctrl->SetColumnWidth(col++, 45);

    listItem.SetAlign(wxLIST_FORMAT_LEFT);
    listItem.SetText(wxT("API"));
    idx = ctrl->InsertColumn(col, listItem);
    ctrl->SetColumnWidth(col++, 100);

    if(in_out == AUDIO_IN)
    {
        listItem.SetAlign(wxLIST_FORMAT_CENTRE);
        listItem.SetText(wxT("Default Sample Rate"));
        idx = ctrl->InsertColumn(col, listItem);
        ctrl->SetColumnWidth(col++, 160);
    }
    else if(in_out == AUDIO_OUT)
    {
        listItem.SetAlign(wxLIST_FORMAT_CENTRE);
        listItem.SetText(wxT("Default Sample Rate"));
        idx = ctrl->InsertColumn(col, listItem);
        ctrl->SetColumnWidth(col++, 160);
    }

    #ifdef LATENCY
    listItem.SetAlign(wxLIST_FORMAT_CENTRE);
    listItem.SetText(wxT("Min Latency"));
    ctrl->InsertColumn(col, listItem);
    ctrl->SetColumnWidth(col++, 100);

    listItem.SetAlign(wxLIST_FORMAT_CENTRE);
    listItem.SetText(wxT("Max Latency"));
    ctrl->InsertColumn(col, listItem);
    ctrl->SetColumnWidth(col++, 100);
    #endif

    for(devn = 0; devn < numDevices; devn++)
    {
        buf.Printf(wxT(""));
        deviceInfo = Pa_GetDeviceInfo(devn);
        if( ((in_out == AUDIO_IN) && (deviceInfo->maxInputChannels > 0)) ||
            ((in_out == AUDIO_OUT) && (deviceInfo->maxOutputChannels > 0)))
        {
            col = 0;
            buf.Printf(wxT("%s"), deviceInfo->name);
            idx = ctrl->InsertItem(ctrl->GetItemCount(), buf);
            col++;
                
            buf.Printf(wxT("%d"), devn);
            ctrl->SetItem(idx, col++, buf);

            buf.Printf(wxT("%s"), Pa_GetHostApiInfo(deviceInfo->hostApi)->name);
            ctrl->SetItem(idx, col++, buf);

            buf.Printf(wxT("%i"), (int)deviceInfo->defaultSampleRate);
            ctrl->SetItem(idx, col++, buf);

            #ifdef LATENCY
            if (in_out == AUDIO_IN)
                buf.Printf(wxT("%8.4f"), deviceInfo->defaultLowInputLatency);
            else
                buf.Printf(wxT("%8.4f"), deviceInfo->defaultLowOutputLatency);               
            ctrl->SetItem(idx, col++, buf);

            if (in_out == AUDIO_IN)
                buf.Printf(wxT("%8.4f"), deviceInfo->defaultHighInputLatency);
            else
                 buf.Printf(wxT("%8.4f"), deviceInfo->defaultHighOutputLatency);             
            ctrl->SetItem(idx, col++, buf);
            #endif
        }        
    }

    // add "none" option at end

    buf.Printf(wxT("%s"), "none");
    idx = ctrl->InsertItem(ctrl->GetItemCount(), buf);
}

//-------------------------------------------------------------------------
// OnDeviceSelect()
//
// helper function to set up "Device:" and "Sample Rate:" fields when
// we click on a line in the list of devices box
//-------------------------------------------------------------------------
void AudioOptsDialog::OnDeviceSelect(wxComboBox *cbSampleRate, 
                                     wxTextCtrl *textCtrl, 
                                     int        *devNum, 
                                     wxListCtrl *listCtrlDevices, 
                                     int         index,
                                     int         in_out)
{

    wxString devName = listCtrlDevices->GetItemText(index, 0);
     if (devName.IsSameAs("none")) {
        *devNum = -1;
        textCtrl->SetValue("none");
    }
    else {
        *devNum = wxAtoi(listCtrlDevices->GetItemText(index, 1));
        textCtrl->SetValue(devName + " (" + wxString::Format(wxT("%i"),*devNum) + ")");

        int numSampleRates = buildListOfSupportedSampleRates(cbSampleRate, *devNum, in_out);
        if (numSampleRates) {
            wxString defSampleRate = listCtrlDevices->GetItemText(index, 3);        
            cbSampleRate->SetValue(defSampleRate);
        }
        else {
             cbSampleRate->SetValue("None");           
        }
    }
}

//-------------------------------------------------------------------------
// OnRxInDeviceSelect()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnRxInDeviceSelect(wxListEvent& evt)
{
    OnDeviceSelect(m_cbSampleRateRxIn, 
                   m_textCtrlRxIn, 
                   &rxInAudioDeviceNum, 
                   m_listCtrlRxInDevices, 
                   evt.GetIndex(),
                   AUDIO_IN);
}

//-------------------------------------------------------------------------
// OnRxOutDeviceSelect()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnRxOutDeviceSelect(wxListEvent& evt)
{
    OnDeviceSelect(m_cbSampleRateRxOut, 
                   m_textCtrlRxOut, 
                   &rxOutAudioDeviceNum, 
                   m_listCtrlRxOutDevices, 
                   evt.GetIndex(),
                   AUDIO_OUT);
}

//-------------------------------------------------------------------------
// OnTxInDeviceSelect()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnTxInDeviceSelect(wxListEvent& evt)
{
    OnDeviceSelect(m_cbSampleRateTxIn, 
                   m_textCtrlTxIn, 
                   &txInAudioDeviceNum, 
                   m_listCtrlTxInDevices, 
                   evt.GetIndex(),
                   AUDIO_IN);
}

//-------------------------------------------------------------------------
// OnTxOutDeviceSelect()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnTxOutDeviceSelect(wxListEvent& evt)
{
    OnDeviceSelect(m_cbSampleRateTxOut, 
                   m_textCtrlTxOut, 
                   &txOutAudioDeviceNum, 
                   m_listCtrlTxOutDevices, 
                   evt.GetIndex(),
                   AUDIO_OUT);
}

//-------------------------------------------------------------------------
// plotDeviceInputForAFewSecs()
//
// opens a record device and plots the input speech for a few seconds.  This is "modal" using
// synchronous portaudio functions, so the GUI will not respond until after test sample has been
// taken
//-------------------------------------------------------------------------
void AudioOptsDialog::plotDeviceInputForAFewSecs(int devNum, PlotScalar *plotScalar) {
    PaStreamParameters  inputParameters;
    const PaDeviceInfo *deviceInfo = NULL;
    PaStream           *stream = NULL;
    PaError             err;
    short               in48k_stereo_short[2*TEST_BUF_SIZE];
    short               in48k_short[TEST_BUF_SIZE];
    short               in8k_short[TEST_BUF_SIZE];
    int                 numDevices, nBufs, j, src_error,inputChannels, sampleRate, sampleCount;
    SRC_STATE          *src;
    FIFO               *fifo;

    // a basic sanity check
    numDevices = Pa_GetDeviceCount();
    if (devNum >= numDevices)
        return;
    if (devNum < 0)
        return;
    printf("devNum %d\n", devNum);

    fifo = fifo_create((int)(DT*TEST_WAVEFORM_PLOT_FS*2)); assert(fifo != NULL);
    src = src_new(SRC_SINC_FASTEST, 1, &src_error); assert(src != NULL);

    // work out how many input channels this device supports.

    deviceInfo = Pa_GetDeviceInfo(devNum);
    if (deviceInfo == NULL) {
        wxMessageBox(wxT("Couldn't get device info from Port Audio for Sound Card "), wxT("Error"), wxOK);
        return;
    }
    if (deviceInfo->maxInputChannels == 1)
        inputChannels = 1;
    else
        inputChannels = 2;

    // open device

    inputParameters.device = devNum;
    inputParameters.channelCount = inputChannels;
    inputParameters.sampleFormat = paInt16;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultHighInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    sampleRate = wxAtoi(m_cbSampleRateRxIn->GetValue());
    nBufs = TEST_WAVEFORM_PLOT_TIME*sampleRate/TEST_BUF_SIZE;
    printf("inputChannels: %d nBufs %d\n", inputChannels, nBufs);

    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,
              sampleRate,
              TEST_BUF_SIZE,
              paClipOff,    
              NULL,       // no callback, use blocking API
              NULL ); 
    if (err != paNoError) {
        wxMessageBox(wxT("Couldn't initialise sound device."), wxT("Error"), wxOK);       
        return;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        wxMessageBox(wxT("Couldn't start sound device."), wxT("Error"), wxOK);       
        return;
    }

    // Sometimes this buffer doesn't get completely filled.  Unset values show up as
    // junk on the plot.
    memset(in8k_short, 0, TEST_BUF_SIZE * sizeof(short));

    sampleCount = 0;

    while(sampleCount < (TEST_WAVEFORM_PLOT_TIME * TEST_WAVEFORM_PLOT_FS))
    {
        Pa_ReadStream(stream, in48k_stereo_short, TEST_BUF_SIZE);
        if (inputChannels == 2) {
            for(j=0; j<TEST_BUF_SIZE; j++)
                in48k_short[j] = in48k_stereo_short[2*j]; // left channel only
        }
        else {
            for(j=0; j<TEST_BUF_SIZE; j++)
                in48k_short[j] = in48k_stereo_short[j]; 
        }
        int n8k = resample(src, in8k_short, in48k_short, 8000, sampleRate, TEST_BUF_SIZE, TEST_BUF_SIZE);
        resample_for_plot(fifo, in8k_short, n8k, FS);

        short plotSamples[TEST_WAVEFORM_PLOT_BUF];
        if (fifo_read(fifo, plotSamples, TEST_WAVEFORM_PLOT_BUF))
        {
            // come back when the fifo is refilled
            continue;
        }

        plotScalar->add_new_short_samples(0, plotSamples, TEST_WAVEFORM_PLOT_BUF, 32767);
        sampleCount += TEST_WAVEFORM_PLOT_BUF;
        plotScalar->Refresh();
        plotScalar->Update();
    }


    err = Pa_StopStream(stream);
    if (err != paNoError) {
        wxMessageBox(wxT("Couldn't stop sound device."), wxT("Error"), wxOK);       
        return;
    }
    Pa_CloseStream(stream);

    fifo_destroy(fifo);
    src_delete(src);
}

//-------------------------------------------------------------------------
// plotDeviceOutputForAFewSecs()
//
// opens a play device and plays a tone for a few seconds.  This is "modal" using
// synchronous portaudio functions, so the GUI will not respond until after test sample has been
// taken.  Also plots a pretty picture like the record versions
//-------------------------------------------------------------------------
void AudioOptsDialog::plotDeviceOutputForAFewSecs(int devNum, PlotScalar *plotScalar) {
    PaStreamParameters  outputParameters;
    const PaDeviceInfo *deviceInfo = NULL;
    PaStream           *stream = NULL;
    PaError             err;
    short               out48k_stereo_short[2*TEST_BUF_SIZE];
    short               out48k_short[TEST_BUF_SIZE];
    short               out8k_short[TEST_BUF_SIZE];
    int                 numDevices, j, src_error, n, outputChannels, sampleRate, sampleCount;
    SRC_STATE          *src;
    FIFO               *fifo;

    // a basic sanity check
    numDevices = Pa_GetDeviceCount();
    if (devNum >= numDevices)
        return;
    if (devNum < 0)
        return;

    fifo = fifo_create((int)(DT*TEST_WAVEFORM_PLOT_FS*2)); assert(fifo != NULL);
    src = src_new(SRC_SINC_FASTEST, 1, &src_error); assert(src != NULL);

    // work out how many output channels this device supports.

    deviceInfo = Pa_GetDeviceInfo(devNum);
    if (deviceInfo == NULL) {
        wxMessageBox(wxT("Couldn't get device info from Port Audio for Sound Card "), wxT("Error"), wxOK);
        return;
    }
    if (deviceInfo->maxOutputChannels == 1)
        outputChannels = 1;
    else
        outputChannels = 2;

    printf("outputChannels: %d\n", outputChannels);

    outputParameters.device = devNum;
    outputParameters.channelCount = outputChannels;
    outputParameters.sampleFormat = paInt16;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    sampleRate = wxAtoi(m_cbSampleRateRxIn->GetValue());

    err = Pa_OpenStream(
              &stream,
              NULL,
              &outputParameters,
              sampleRate,
              TEST_BUF_SIZE,
              paClipOff,    
              NULL,       // no callback, use blocking API
              NULL ); 
    if (err != paNoError) {
        wxMessageBox(wxT("Couldn't initialise sound device."), wxT("Error"), wxOK);       
        return;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        wxMessageBox(wxT("Couldn't start sound device."), wxT("Error"), wxOK);       
        return;
    }

    // Sometimes this buffer doesn't get completely filled.  Unset values show up as
    // junk on the plot.
    memset(out8k_short, 0, TEST_BUF_SIZE * sizeof(short));

    sampleCount = 0;
    n = 0;

    while(sampleCount < (TEST_WAVEFORM_PLOT_TIME * TEST_WAVEFORM_PLOT_FS)) {
        for(j=0; j<TEST_BUF_SIZE; j++,n++) {
            out48k_short[j] = 2000.0*cos(6.2832*(n++)*400.0/sampleRate);
            if (outputChannels == 2) {
                out48k_stereo_short[2*j] = out48k_short[j];   // left channel
                out48k_stereo_short[2*j+1] = out48k_short[j]; // right channel
            }
            else {
                out48k_stereo_short[j] = out48k_short[j];     // mono
            }
        }
        Pa_WriteStream(stream, out48k_stereo_short, TEST_BUF_SIZE);

        // convert back to 8kHz just for plotting
        int n8k = resample(src, out8k_short, out48k_short, 8000, sampleRate, TEST_BUF_SIZE, TEST_BUF_SIZE);
        resample_for_plot(fifo, out8k_short, n8k, FS);

        // If enough 8 kHz samples are buffered, go ahead and plot, otherwise wait for more
        short plotSamples[TEST_WAVEFORM_PLOT_BUF];
        if (fifo_read(fifo, plotSamples, TEST_WAVEFORM_PLOT_BUF))
        {
            // come back when the fifo is refilled
            continue;
        }

        plotScalar->add_new_short_samples(0, plotSamples, TEST_WAVEFORM_PLOT_BUF, 32767);
        sampleCount += TEST_WAVEFORM_PLOT_BUF;
        plotScalar->Refresh();
        plotScalar->Update();
    }
   
    err = Pa_StopStream(stream);
    if (err != paNoError) {
        wxMessageBox(wxT("Couldn't stop sound device."), wxT("Error"), wxOK);       
        return;
    }
    Pa_CloseStream(stream);

    fifo_destroy(fifo);
    src_delete(src);
}

//-------------------------------------------------------------------------
// OnRxInTest()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnRxInTest(wxCommandEvent& event)
{
    plotDeviceInputForAFewSecs(rxInAudioDeviceNum, m_plotScalarRxIn);
}

//-------------------------------------------------------------------------
// OnRxOutTest()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnRxOutTest(wxCommandEvent& event)
{
    plotDeviceOutputForAFewSecs(rxOutAudioDeviceNum, m_plotScalarRxOut);
}

//-------------------------------------------------------------------------
// OnTxInTest()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnTxInTest(wxCommandEvent& event)
{
    plotDeviceInputForAFewSecs(txInAudioDeviceNum, m_plotScalarTxIn);
}

//-------------------------------------------------------------------------
// OnTxOutTest()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnTxOutTest(wxCommandEvent& event)
{
    plotDeviceOutputForAFewSecs(txOutAudioDeviceNum, m_plotScalarTxOut);
}

//-------------------------------------------------------------------------
// OnRefreshClick()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnRefreshClick(wxCommandEvent& event)
{
    // restart portaudio, to re-sample available devices

    Pa_Terminate();
    Pa_Init();

    m_notebook1->SetSelection(0);
    showAPIInfo();
    populateParams(m_RxInDevices);
    populateParams(m_RxOutDevices);
    populateParams(m_TxInDevices);
    populateParams(m_TxOutDevices);

    // some devices may have dissapeared, so possibily change sound
    // card config

    ExchangeData(EXCHANGE_DATA_IN);
}

//-------------------------------------------------------------------------
// OnApplyAudioParameters()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnApplyAudioParameters(wxCommandEvent& event)
{
    ExchangeData(EXCHANGE_DATA_OUT);
    if(m_isPaInitialized)
    {
        if((pa_err = Pa_Terminate()) == paNoError)
        {
            m_isPaInitialized = false;
        }
        else
        {
            wxMessageBox(wxT("Port Audio failed to Terminate"), wxT("Pa_Terminate"), wxOK);
        }
    }
}

//-------------------------------------------------------------------------
// OnCancelAudioParameters()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnCancelAudioParameters(wxCommandEvent& event)
{
    if(m_isPaInitialized)
    {
        if((pa_err = Pa_Terminate()) == paNoError)
        {
            m_isPaInitialized = false;
        }
        else
        {
            wxMessageBox(wxT("Port Audio failed to Terminate"), wxT("Pa_Terminate"), wxOK);
        }
    }
    EndModal(wxCANCEL);
}

//-------------------------------------------------------------------------
// OnOkAudioParameters()
//-------------------------------------------------------------------------
void AudioOptsDialog::OnOkAudioParameters(wxCommandEvent& event)
{
    int status = ExchangeData(EXCHANGE_DATA_OUT);

    // We only accept OK if config sucessful

    printf("status: %d m_isPaInitialized: %d\n", status, m_isPaInitialized);
    if (status == 0) {
        if(m_isPaInitialized)
        {
            if((pa_err = Pa_Terminate()) == paNoError)
            {
                printf("terminated OK\n");
                m_isPaInitialized = false;
            }
            else
            {
                wxMessageBox(wxT("Port Audio failed to Terminate"), wxT("Pa_Terminate"), wxOK);
            }
        }
        EndModal(wxOK);
    }

}
