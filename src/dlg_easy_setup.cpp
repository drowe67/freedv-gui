//==========================================================================
// Name:            dlg_easy_setup.cpp
// Purpose:         Dialog for easier initial setup of FreeDV.
// Created:         Nov 13, 2021
// Authors:         Mooneer Salem
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


#include <map>
#include <climits>
#include <cmath>

#include "main.h"
#include "dlg_easy_setup.h"
#include "audio/AudioEngineFactory.h"

#ifdef __WIN32__
#include <wx/msw/registry.h>
#else
#include <glob.h>
#include <string.h>
#endif

#define PI 3.14159

#define RX_ONLY_STRING "None (receive only)"
#define MULTIPLE_DEVICES_STRING "(multiple)"

extern wxConfigBase *pConfig;

EasySetupDialog::EasySetupDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) 
    : wxDialog(parent, id, title, pos, size, style)
{
    hamlibTestObject_ = new Hamlib();
    assert(hamlibTestObject_ != nullptr);

    serialPortTestObject_ = new Serialport();
    assert(serialPortTestObject_ != nullptr);
    
    // Create top-level of control hierarchy.
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* sectionSizer = new wxBoxSizer(wxVERTICAL);
    
    // Step 1: sound device selection
    // ==============================
    wxStaticBox *selectSoundDeviceBox = new wxStaticBox(panel, wxID_ANY, _("Step 1: Select Sound Device"));
    wxStaticBoxSizer* setupSoundDeviceBoxSizer = new wxStaticBoxSizer( selectSoundDeviceBox, wxVERTICAL);
    
    wxFlexGridSizer* gridSizerSoundDevice = new wxFlexGridSizer(3, 2, 5, 0);
    
    wxStaticText* labelRadioDevice = new wxStaticText(selectSoundDeviceBox, wxID_ANY, wxT("Radio Device:"), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerSoundDevice->Add(labelRadioDevice, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 5);

    m_radioDevice = new wxComboBox(selectSoundDeviceBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
    m_radioDevice->SetMinSize(wxSize(250, -1));
    gridSizerSoundDevice->Add(m_radioDevice, 0, wxALL | wxEXPAND, 5);
    
    wxStaticText* labelAnalogPlayDevice = new wxStaticText(selectSoundDeviceBox, wxID_ANY, wxT("Decoded audio plays back through:"), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerSoundDevice->Add(labelAnalogPlayDevice, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 5);
    
    m_analogDevicePlayback = new wxComboBox(selectSoundDeviceBox, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
    gridSizerSoundDevice->Add(m_analogDevicePlayback, 0, wxALL | wxEXPAND, 5);
    
    wxStaticText* labelAnalogRecordDevice = new wxStaticText(selectSoundDeviceBox, wxID_ANY, wxT("Transmitted audio records through:"), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerSoundDevice->Add(labelAnalogRecordDevice, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 5);
    
    m_analogDeviceRecord = new wxComboBox(selectSoundDeviceBox, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
    gridSizerSoundDevice->Add(m_analogDeviceRecord, 0, wxALL | wxEXPAND, 5);
        
    wxBoxSizer* advancedSoundSetupSizer = new wxBoxSizer(wxHORIZONTAL);
    m_advancedSoundSetup = new wxButton(selectSoundDeviceBox, wxID_ANY, wxT("Advanced"),  wxDefaultPosition, wxDefaultSize, 0);
    advancedSoundSetupSizer->Add(m_advancedSoundSetup, 0, wxALL | wxEXPAND, 5);
    
    setupSoundDeviceBoxSizer->Add(gridSizerSoundDevice, 0, wxEXPAND | wxALIGN_LEFT, 5);
    setupSoundDeviceBoxSizer->Add(advancedSoundSetupSizer, 0, wxALL | wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 5);
    sectionSizer->Add(setupSoundDeviceBoxSizer, 0, wxALL | wxEXPAND, 5);
    
    // Step 2: setup CAT control
    // =========================
    wxStaticBox *setupCatControlBox = new wxStaticBox(panel, wxID_ANY, _("Step 2: Setup Radio Control"));
    wxStaticBoxSizer* setupCatControlBoxSizer = new wxStaticBoxSizer( setupCatControlBox, wxVERTICAL);
    
    wxBoxSizer* catTypeSizer = new wxBoxSizer(wxHORIZONTAL);
    setupCatControlBoxSizer->Add(catTypeSizer, 0, wxEXPAND|wxALIGN_LEFT, 5);

    /* No CAT Control radio button */

    m_ckNoPTT = new wxRadioButton(setupCatControlBox, wxID_ANY, _("No PTT/CAT Control"), wxDefaultPosition, wxSize(-1, -1), 0);
    m_ckNoPTT->SetValue(true);
    catTypeSizer->Add(m_ckNoPTT, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        
    /* Use Hamlib for PTT radio button. */

    m_ckUseHamlibPTT = new wxRadioButton(setupCatControlBox, wxID_ANY, _("Hamlib CAT Control"), wxDefaultPosition, wxSize(-1, -1), 0);
    m_ckUseHamlibPTT->SetValue(false);
    catTypeSizer->Add(m_ckUseHamlibPTT, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    /* Use serial port for PTT radio button. */
    m_ckUseSerialPTT = new wxRadioButton(setupCatControlBox, wxID_ANY, _("Serial PTT"), wxDefaultPosition, wxSize(-1, -1), 0);
    m_ckUseSerialPTT->SetValue(false);
    catTypeSizer->Add(m_ckUseSerialPTT, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    /* Hamlib box */
    m_hamlibBox = new wxStaticBox(setupCatControlBox, wxID_ANY, _("Hamlib CAT Control"));
    m_hamlibBox->Hide();
    wxStaticBoxSizer* hamlibBoxSizer = new wxStaticBoxSizer(m_hamlibBox, wxVERTICAL);
    wxGridSizer* gridSizerhl = new wxGridSizer(4, 2, 0, 0);
    hamlibBoxSizer->Add(gridSizerhl);
    setupCatControlBoxSizer->Add(hamlibBoxSizer);

    /* Hamlib Rig Type combobox. */

    gridSizerhl->Add(new wxStaticText(m_hamlibBox, wxID_ANY, _("Rig Model:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 20);
    m_cbRigName = new wxComboBox(m_hamlibBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(250, -1), 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
    gridSizerhl->Add(m_cbRigName, 0, wxALL | wxEXPAND, 5);

    /* Hamlib Serial Port combobox. */

    gridSizerhl->Add(new wxStaticText(m_hamlibBox, wxID_ANY, _("Serial Device (or hostname:port):"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL |  wxALIGN_RIGHT, 20);
    m_cbSerialPort = new wxComboBox(m_hamlibBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
    m_cbSerialPort->SetMinSize(wxSize(140, -1));
    gridSizerhl->Add(m_cbSerialPort, 0, wxALL | wxEXPAND, 5);

    /* Hamlib Icom CI-V address text box. */
    m_stIcomCIVHex = new wxStaticText(m_hamlibBox, wxID_ANY, _("Radio Address:"), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerhl->Add(m_stIcomCIVHex, 
                      0, wxALIGN_CENTER_VERTICAL |  wxALIGN_RIGHT, 5);
    m_tcIcomCIVHex = new wxTextCtrl(m_hamlibBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(35, -1), 0, wxNumericPropertyValidator(wxNumericPropertyValidator::Unsigned, 16));
    m_tcIcomCIVHex->SetMaxLength(2);
    gridSizerhl->Add(m_tcIcomCIVHex, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    /* Hamlib Serial Rate combobox. */

    gridSizerhl->Add(new wxStaticText(m_hamlibBox, wxID_ANY, _("Serial Rate:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 20);
    m_cbSerialRate = new wxComboBox(m_hamlibBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(140, -1), 0, NULL, wxCB_DROPDOWN);
    gridSizerhl->Add(m_cbSerialRate, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    /* Serial port box */
    m_serialBox = new wxStaticBox(setupCatControlBox, wxID_ANY, _("Serial PTT"));
    m_serialBox->Hide();
    wxStaticBoxSizer* serialBoxSizer = new wxStaticBoxSizer(m_serialBox, wxVERTICAL);
    wxGridSizer* gridSizerSerial = new wxGridSizer(4, 2, 0, 0);

    /* Serial device combo box */
    wxStaticText* serialDeviceLabel = new wxStaticText(m_serialBox, wxID_ANY, _("Serial Device:"), wxDefaultPosition, wxDefaultSize, 0);
    serialDeviceLabel->Wrap(-1);
    gridSizerSerial->Add(serialDeviceLabel, 0, wxALL | wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 5);

    m_cbCtlDevicePath = new wxComboBox(m_serialBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
    m_cbCtlDevicePath->SetMinSize(wxSize(140, -1));
    gridSizerSerial->Add(m_cbCtlDevicePath, 0, wxALL | wxEXPAND, 5);

    /* DTR/RTS options */
   
    m_rbUseDTR = new wxRadioButton(m_serialBox, wxID_ANY, _("Use DTR"), wxDefaultPosition, wxSize(-1,-1), wxRB_GROUP);
    m_rbUseDTR->SetToolTip(_("Toggle DTR line for PTT"));
    m_rbUseDTR->SetValue(1);
    gridSizerSerial->Add(m_rbUseDTR, 0, wxALL | wxALIGN_CENTER|wxALIGN_CENTER_VERTICAL, 5);

    m_rbUseRTS = new wxRadioButton(m_serialBox, wxID_ANY, _("Use RTS"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_rbUseRTS->SetToolTip(_("Toggle the RTS pin for PTT"));
    m_rbUseRTS->SetValue(1);
    gridSizerSerial->Add(m_rbUseRTS, 0, wxALL | wxALIGN_CENTER, 5);
    
    m_ckDTRPos = new wxCheckBox(m_serialBox, wxID_ANY, _("DTR = +V"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_ckDTRPos->SetToolTip(_("Set Polarity of the DTR line"));
    m_ckDTRPos->SetValue(false);
    gridSizerSerial->Add(m_ckDTRPos, 0, wxALL | wxALIGN_CENTER, 5);

    m_ckRTSPos = new wxCheckBox(m_serialBox, wxID_ANY, _("RTS = +V"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_ckRTSPos->SetValue(false);
    m_ckRTSPos->SetToolTip(_("Set Polarity of the RTS line"));
    gridSizerSerial->Add(m_ckRTSPos, 0, wxALL | wxALIGN_CENTER, 5);

    // Override tab ordering for Polarity area
    m_rbUseDTR->MoveBeforeInTabOrder(m_rbUseRTS);
    m_rbUseRTS->MoveBeforeInTabOrder(m_ckDTRPos);
    m_ckDTRPos->MoveBeforeInTabOrder(m_ckRTSPos);
    
    serialBoxSizer->Add(gridSizerSerial, 0, wxALL | wxEXPAND, 5);
    setupCatControlBoxSizer->Add(serialBoxSizer, 0, wxALL | wxEXPAND, 5);

    wxBoxSizer* pttButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    
    /* Advanced/test buttons */

    m_advancedPTTSetup = new wxButton(setupCatControlBox, wxID_ANY, wxT("Advanced"),  wxDefaultPosition, wxDefaultSize, 0);
    pttButtonSizer->Add(m_advancedPTTSetup, 0, wxALL | wxEXPAND, 5);

    m_buttonTest = new wxButton(setupCatControlBox, wxID_ANY, wxT("Test"),  wxDefaultPosition, wxDefaultSize, 0);
    pttButtonSizer->Add(m_buttonTest, 0, wxALL | wxEXPAND, 5);
    
    setupCatControlBoxSizer->Add(pttButtonSizer, 0, wxALL | wxALIGN_CENTER, 5);
    
    sectionSizer->Add(setupCatControlBoxSizer, 0, wxALL | wxEXPAND, 5);
    
    // Step 3: setup PSK Reporter
    // ==========================
    wxStaticBox *setupPskReporterBox = new wxStaticBox(panel, wxID_ANY, _("Step 3: Setup Reporting"));
    
    wxStaticBoxSizer* sbSizer_psk;
    sbSizer_psk = new wxStaticBoxSizer(setupPskReporterBox, wxHORIZONTAL);
    m_ckbox_psk_enable = new wxCheckBox(setupPskReporterBox, wxID_ANY, _("Enable Reporting"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_psk->Add(m_ckbox_psk_enable, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelPskCallsign = new wxStaticText(setupPskReporterBox, wxID_ANY, wxT("Callsign: "), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_psk->Add(labelPskCallsign, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_txt_callsign = new wxTextCtrl(setupPskReporterBox, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(90,-1), 0, wxTextValidator(wxFILTER_ALPHANUMERIC));
    sbSizer_psk->Add(m_txt_callsign, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelPskGridSquare = new wxStaticText(setupPskReporterBox, wxID_ANY, wxT("Grid Square: "), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_psk->Add(labelPskGridSquare, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_txt_grid_square = new wxTextCtrl(setupPskReporterBox, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(80,-1), 0, wxTextValidator(wxFILTER_ALPHANUMERIC));
    sbSizer_psk->Add(m_txt_grid_square, 0,  wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    sectionSizer->Add(sbSizer_psk, 0, wxALL | wxEXPAND, 5);
    
    // Step 4: save or cancel changes
    // =============================
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    m_buttonOK = new wxButton(panel, wxID_OK);
    buttonSizer->Add(m_buttonOK, 0, wxALL, 5);

    m_buttonCancel = new wxButton(panel, wxID_CANCEL);
    buttonSizer->Add(m_buttonCancel, 0, wxALL, 5);

    m_buttonApply = new wxButton(panel, wxID_APPLY);
    buttonSizer->Add(m_buttonApply, 0, wxALL, 5);

    sectionSizer->Add(buttonSizer, 0, wxALL | wxALIGN_CENTER, 5);
    
    // Trigger auto-layout of window.
    // ==============================
    panel->SetSizer(sectionSizer);
    
    wxBoxSizer* panelSizer = new wxBoxSizer(wxVERTICAL);
    panelSizer->Add(panel, 0, wxEXPAND, 0);
    this->SetSizerAndFit(panelSizer);
    
    this->Layout();
    this->Centre(wxBOTH);
    this->SetMinSize(GetBestSize());
    
    // Hook in events
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(EasySetupDialog::OnInitDialog));
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(EasySetupDialog::OnClose));
    
    m_ckUseHamlibPTT->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(EasySetupDialog::PTTUseHamLibClicked), NULL, this);
    m_ckNoPTT->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(EasySetupDialog::PTTUseHamLibClicked), NULL, this);
    m_ckUseSerialPTT->Connect(wxEVT_RADIOBUTTON, wxCommandEventHandler(EasySetupDialog::PTTUseHamLibClicked), NULL, this);
    
    m_cbRigName->Connect(wxEVT_COMBOBOX, wxCommandEventHandler(EasySetupDialog::HamlibRigNameChanged), NULL, this);
    
    m_advancedSoundSetup->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnAdvancedSoundSetup), NULL, this);
    m_advancedPTTSetup->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnAdvancedPTTSetup), NULL, this);
    m_ckbox_psk_enable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(EasySetupDialog::OnPSKReporterChecked), NULL, this);
    m_buttonTest->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnTest), NULL, this);
    
    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnOK), NULL, this);
    m_buttonCancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnCancel), NULL, this);
    m_buttonApply->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnApply), NULL, this);
}

EasySetupDialog::~EasySetupDialog()
{
    delete hamlibTestObject_;
    delete serialPortTestObject_;
    
    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(EasySetupDialog::OnInitDialog));
    this->Disconnect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(EasySetupDialog::OnClose));
    
    m_ckUseHamlibPTT->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(EasySetupDialog::PTTUseHamLibClicked), NULL, this);
    m_ckNoPTT->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(EasySetupDialog::PTTUseHamLibClicked), NULL, this);
    m_ckUseSerialPTT->Disconnect(wxEVT_RADIOBUTTON, wxCommandEventHandler(EasySetupDialog::PTTUseHamLibClicked), NULL, this);

    m_cbRigName->Disconnect(wxEVT_COMBOBOX, wxCommandEventHandler(EasySetupDialog::HamlibRigNameChanged), NULL, this);
    
    m_advancedSoundSetup->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnAdvancedSoundSetup), NULL, this);
    m_advancedPTTSetup->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnAdvancedPTTSetup), NULL, this);
    m_ckbox_psk_enable->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(EasySetupDialog::OnPSKReporterChecked), NULL, this);
    m_buttonTest->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnTest), NULL, this);
    
    m_buttonOK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnOK), NULL, this);
    m_buttonCancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnCancel), NULL, this);
    m_buttonApply->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnApply), NULL, this);
}

void EasySetupDialog::ExchangeData(int inout)
{
    ExchangeSoundDeviceData(inout);
    ExchangePttDeviceData(inout);
    ExchangeReportingData(inout);
}

void EasySetupDialog::ExchangeSoundDeviceData(int inout)
{
    if (inout == EXCHANGE_DATA_IN)
    {
        wxString soundCard1InDeviceName = wxGetApp().m_soundCard1InDeviceName;
        int soundCard1InSampleRate = wxGetApp().m_soundCard1InSampleRate;
        wxString soundCard1OutDeviceName = wxGetApp().m_soundCard1OutDeviceName;
        int soundCard1OutSampleRate = wxGetApp().m_soundCard1OutSampleRate;
        wxString soundCard2InDeviceName = wxGetApp().m_soundCard2InDeviceName;
        wxString soundCard2OutDeviceName = wxGetApp().m_soundCard2OutDeviceName;
        wxString radioSoundDevice;
        
        if (soundCard1InDeviceName == "none" && soundCard1OutDeviceName == "none")
        {
            // Set initial selections so we have something valid on exit.
            SoundDeviceData* analogPlaybackDeviceData = 
                (SoundDeviceData*)m_analogDevicePlayback->GetClientObject(0);

            SoundDeviceData* radioDeviceData  = 
                (SoundDeviceData*)m_radioDevice->GetClientObject(0);

            soundCard1InDeviceName = radioDeviceData->rxDeviceName;
            soundCard1OutDeviceName = analogPlaybackDeviceData->rxDeviceName;
        }

        // Previous existing setup, determine what it is
        if (soundCard2InDeviceName == "none" && soundCard2OutDeviceName == "none")
        {
            // RX-only setup
            auto index = m_analogDeviceRecord->FindString(RX_ONLY_STRING);
            assert(index >= 0);
            m_analogDeviceRecord->SetSelection(index);

            index = m_analogDevicePlayback->FindString(soundCard1OutDeviceName);
            if (index >= 0)
            {
                m_analogDevicePlayback->SetSelection(index);
            }

            radioSoundDevice = soundCard1InDeviceName;
        }
        else 
        {
            // RX and TX setup
            auto index = m_analogDeviceRecord->FindString(soundCard2InDeviceName);
            if (index >= 0)
            {
                m_analogDeviceRecord->SetSelection(index);
            }

            index = m_analogDevicePlayback->FindString(soundCard2OutDeviceName);
            if (index >= 0)
            {
                m_analogDevicePlayback->SetSelection(index);
            }                
            
            if (soundCard1OutDeviceName == soundCard1InDeviceName)
            {
                // We're not on a setup with different sound devices on the radio side (e.g. SDRs)
                radioSoundDevice = soundCard1InDeviceName;

                // Remove multiple devices entry if it's in there.
                int index = m_radioDevice->FindString(MULTIPLE_DEVICES_STRING);
                if (index >= 0)
                {
                    m_radioDevice->Delete(index);
                }
            }
            else
            {
                radioSoundDevice = MULTIPLE_DEVICES_STRING;
            }
        }
        
        if (radioSoundDevice == MULTIPLE_DEVICES_STRING)
        {
            for (unsigned int index = 0; index < m_radioDevice->GetCount(); index++)
            {
                SoundDeviceData* data = (SoundDeviceData*)m_radioDevice->GetClientObject(index);
                if (data != nullptr)
                {
                    bool rxDeviceNameMatches = data->rxDeviceName == soundCard1InDeviceName;
                    bool isRxOnly = m_analogDeviceRecord->GetStringSelection() == RX_ONLY_STRING;
                    bool txDeviceNameMatches = data->txDeviceName == soundCard1OutDeviceName;
                    
                    if (rxDeviceNameMatches && (isRxOnly || txDeviceNameMatches))
                    {
                        m_radioDevice->SetSelection(index);
                        return;
                    }
                }
            }
        }

        int index = m_radioDevice->FindString(radioSoundDevice);
        if (index != wxNOT_FOUND)
        {
            m_radioDevice->SetSelection(index);
        }
        else if (radioSoundDevice == MULTIPLE_DEVICES_STRING)
        {
            SoundDeviceData* data = new SoundDeviceData();
            assert(data != nullptr);

            data->rxDeviceName = soundCard1InDeviceName;
            data->rxSampleRate = soundCard1InSampleRate;
            data->txDeviceName = soundCard1OutDeviceName;
            data->txSampleRate = soundCard1OutSampleRate;

            m_radioDevice->Insert(MULTIPLE_DEVICES_STRING, 0, data);
            m_radioDevice->SetSelection(0);
        }
    }
    else if (inout == EXCHANGE_DATA_OUT)
    {
        int index = m_radioDevice->GetSelection();
        wxString selectedString = m_radioDevice->GetString(index);
        
        // DO NOT update the radio devices if the user used advanced settings to set them.
        bool updateRadioDevices = false;
        if (selectedString != MULTIPLE_DEVICES_STRING)
        {
            updateRadioDevices = true;
        }
        
        SoundDeviceData* deviceData = (SoundDeviceData*)m_radioDevice->GetClientObject(index);
            
        SoundDeviceData* analogPlaybackDeviceData = 
            (SoundDeviceData*)m_analogDevicePlayback->GetClientObject(
                m_analogDevicePlayback->GetSelection()
                    );

        SoundDeviceData* analogRecordDeviceData = 
            (SoundDeviceData*)m_analogDeviceRecord->GetClientObject(
                m_analogDeviceRecord->GetSelection()
                    );
            
        if (analogRecordDeviceData->txDeviceName == "none")
        {
            wxGetApp().m_soundCard2InDeviceName = "none";
            wxGetApp().m_soundCard2InSampleRate = -1;
            wxGetApp().m_soundCard2OutDeviceName = "none";
            wxGetApp().m_soundCard2OutSampleRate = -1;
            
            if (updateRadioDevices)
            {
                wxGetApp().m_soundCard1InDeviceName = deviceData->rxDeviceName;
                wxGetApp().m_soundCard1InSampleRate = deviceData->rxSampleRate;
            }
            
            wxGetApp().m_soundCard1OutDeviceName = analogPlaybackDeviceData->rxDeviceName;
            wxGetApp().m_soundCard1OutSampleRate = analogPlaybackDeviceData->rxSampleRate;

            g_nSoundCards = 1;
        }
        else
        {
            wxGetApp().m_soundCard2InDeviceName = analogRecordDeviceData->txDeviceName;
            wxGetApp().m_soundCard2InSampleRate = analogRecordDeviceData->txSampleRate;
            wxGetApp().m_soundCard2OutDeviceName = analogPlaybackDeviceData->rxDeviceName;
            wxGetApp().m_soundCard2OutSampleRate = analogPlaybackDeviceData->rxSampleRate;
            
            if (updateRadioDevices)
            {
                wxGetApp().m_soundCard1InDeviceName = deviceData->rxDeviceName;
                wxGetApp().m_soundCard1InSampleRate = deviceData->rxSampleRate;
                wxGetApp().m_soundCard1OutDeviceName = deviceData->txDeviceName;
                wxGetApp().m_soundCard1OutSampleRate = deviceData->txSampleRate;
            }

            g_nSoundCards = 2;
        }
            
        pConfig->Write(wxT("/Audio/soundCard1InDeviceName"), wxGetApp().m_soundCard1InDeviceName);	
        pConfig->Write(wxT("/Audio/soundCard1OutDeviceName"), wxGetApp().m_soundCard1OutDeviceName);	
        pConfig->Write(wxT("/Audio/soundCard2InDeviceName"), wxGetApp().m_soundCard2InDeviceName);	
        pConfig->Write(wxT("/Audio/soundCard2OutDeviceName"), wxGetApp().m_soundCard2OutDeviceName);
        
        pConfig->Write(wxT("/Audio/soundCard1InSampleRate"), wxGetApp().m_soundCard1InSampleRate);	
        pConfig->Write(wxT("/Audio/soundCard1OutSampleRate"), wxGetApp().m_soundCard1OutSampleRate);	
        pConfig->Write(wxT("/Audio/soundCard2InSampleRate"), wxGetApp().m_soundCard2InSampleRate);	
        pConfig->Write(wxT("/Audio/soundCard2OutSampleRate"), wxGetApp().m_soundCard2OutSampleRate);

        pConfig->Flush();
    }
}

void EasySetupDialog::ExchangePttDeviceData(int inout)
{
    if (inout == EXCHANGE_DATA_IN)
    {
        m_ckUseHamlibPTT->SetValue(wxGetApp().m_boolHamlibUseForPTT);
        m_ckUseSerialPTT->SetValue(wxGetApp().m_boolUseSerialPTT);

        if (wxGetApp().m_boolHamlibUseForPTT)
        {
            m_hamlibBox->Show();
            m_serialBox->Hide();

            m_cbRigName->SetSelection(wxGetApp().m_intHamlibRig);
            resetIcomCIVStatus();
            m_cbSerialPort->SetValue(wxGetApp().m_strHamlibSerialPort);

            if (wxGetApp().m_intHamlibSerialRate == 0) {
                m_cbSerialRate->SetSelection(0);
            } else {
                m_cbSerialRate->SetValue(wxString::Format(wxT("%i"), wxGetApp().m_intHamlibSerialRate));
            }

            m_tcIcomCIVHex->SetValue(wxString::Format(wxT("%02X"), wxGetApp().m_intHamlibIcomCIVHex));
        }
        else if (wxGetApp().m_boolUseSerialPTT)
        {
            m_hamlibBox->Hide();
            m_serialBox->Show();

            auto str = wxGetApp().m_strRigCtrlPort;
            m_cbCtlDevicePath->SetValue(str);
            m_rbUseRTS->SetValue(wxGetApp().m_boolUseRTS);
            m_ckRTSPos->SetValue(wxGetApp().m_boolRTSPos);
            m_rbUseDTR->SetValue(wxGetApp().m_boolUseDTR);
            m_ckDTRPos->SetValue(wxGetApp().m_boolDTRPos);
        }
        else
        {
            m_hamlibBox->Hide();
            m_serialBox->Hide();
        }

        Layout();
        SetSize(GetBestSize());
        SetMinSize(GetBestSize());
    }
    else if (inout == EXCHANGE_DATA_OUT)
    {
        Hamlib *hamlib = wxGetApp().m_hamlib;
        wxGetApp().m_boolHamlibUseForPTT = m_ckUseHamlibPTT->GetValue();
        pConfig->Write(wxT("/Hamlib/UseForPTT"), wxGetApp().m_boolHamlibUseForPTT);

        wxGetApp().m_boolUseSerialPTT = m_ckUseSerialPTT->GetValue();
        pConfig->Write(wxT("/Rig/UseSerialPTT"), wxGetApp().m_boolUseSerialPTT);

        if (m_ckUseHamlibPTT->GetValue())
        {
            wxGetApp().m_intHamlibRig = m_cbRigName->GetSelection();
            wxGetApp().m_strHamlibRigName = hamlib->rigIndexToName(wxGetApp().m_intHamlibRig);
            wxGetApp().m_strHamlibSerialPort = m_cbSerialPort->GetValue();
            
            wxString s = m_tcIcomCIVHex->GetValue();
            long hexAddress = 0;
            m_tcIcomCIVHex->GetValue().ToLong(&hexAddress, 16);
            wxGetApp().m_intHamlibIcomCIVHex = hexAddress;
            
            s = m_cbSerialRate->GetValue();
            if (s == "default") {
                wxGetApp().m_intHamlibSerialRate = 0;
            } else {
                long tmp;
                m_cbSerialRate->GetValue().ToLong(&tmp); 
                wxGetApp().m_intHamlibSerialRate = tmp;
            }
            if (g_verbose) fprintf(stderr, "serial rate: %d\n", wxGetApp().m_intHamlibSerialRate);

            pConfig->Write(wxT("/Hamlib/RigNameStr"), wxGetApp().m_strHamlibRigName);
            pConfig->Write(wxT("/Hamlib/SerialPort"), wxGetApp().m_strHamlibSerialPort);
            pConfig->Write(wxT("/Hamlib/SerialRate"), wxGetApp().m_intHamlibSerialRate);
        }
        else if (m_ckUseSerialPTT->GetValue())
        {
            wxGetApp().m_strRigCtrlPort             = m_cbCtlDevicePath->GetValue();
            wxGetApp().m_boolUseRTS                 = m_rbUseRTS->GetValue();
            wxGetApp().m_boolRTSPos                 = m_ckRTSPos->IsChecked();
            wxGetApp().m_boolUseDTR                 = m_rbUseDTR->GetValue();
            wxGetApp().m_boolDTRPos                 = m_ckDTRPos->IsChecked();
                    
            pConfig->Write(wxT("/Rig/Port"),            wxGetApp().m_strRigCtrlPort); 
            pConfig->Write(wxT("/Rig/UseRTS"),          wxGetApp().m_boolUseRTS);
            pConfig->Write(wxT("/Rig/RTSPolarity"),     wxGetApp().m_boolRTSPos);
            pConfig->Write(wxT("/Rig/UseDTR"),          wxGetApp().m_boolUseDTR);
            pConfig->Write(wxT("/Rig/DTRPolarity"),     wxGetApp().m_boolDTRPos);
        }
    }
}

void EasySetupDialog::ExchangeReportingData(int inout)
{
    if (inout == EXCHANGE_DATA_IN)
    {
        m_ckbox_psk_enable->SetValue(wxGetApp().m_psk_enable);
        m_txt_callsign->SetValue(wxGetApp().m_psk_callsign);
        m_txt_grid_square->SetValue(wxGetApp().m_psk_grid_square);
    }
    else if (inout == EXCHANGE_DATA_OUT)
    {
        wxGetApp().m_psk_enable = m_ckbox_psk_enable->GetValue();
        wxGetApp().m_psk_callsign = m_txt_callsign->GetValue();
        wxGetApp().m_psk_grid_square = m_txt_grid_square->GetValue();
        
        pConfig->Write(wxT("/PSKReporter/Enable"), wxGetApp().m_psk_enable);
        pConfig->Write(wxT("/PSKReporter/Callsign"), wxGetApp().m_psk_callsign);
        pConfig->Write(wxT("/PSKReporter/GridSquare"), wxGetApp().m_psk_grid_square);
        pConfig->Flush();
    }
}

void EasySetupDialog::OnPSKReporterChecked(wxCommandEvent& event)
{
    m_txt_callsign->Enable(m_ckbox_psk_enable->GetValue());
    m_txt_grid_square->Enable(m_ckbox_psk_enable->GetValue());
}

void EasySetupDialog::HamlibRigNameChanged(wxCommandEvent& event)
{
    resetIcomCIVStatus();
}

void EasySetupDialog::resetIcomCIVStatus()
{
    std::string rigName = m_cbRigName->GetString(m_cbRigName->GetCurrentSelection()).ToStdString();
    if (rigName.find("Icom") == 0)
    {
        m_stIcomCIVHex->Show();
        m_tcIcomCIVHex->Show();
    }
    else
    {
        m_stIcomCIVHex->Hide();
        m_tcIcomCIVHex->Hide();
    }
    Layout();
}

void EasySetupDialog::OnInitDialog(wxInitDialogEvent& event)
{
    updateAudioDevices_();
    updateHamlibDevices_();
    ExchangeData(EXCHANGE_DATA_IN);
}

void EasySetupDialog::OnOK(wxCommandEvent& event)
{
    OnApply(event);
    this->EndModal(wxID_OK);
}

void EasySetupDialog::OnCancel(wxCommandEvent& event)
{
    this->EndModal(wxID_CANCEL);
}

void EasySetupDialog::OnClose(wxCloseEvent& event)
{
    this->EndModal(wxID_CANCEL);
}

void EasySetupDialog::OnApply(wxCommandEvent& event)
{
    ExchangeData(EXCHANGE_DATA_OUT);
}

void EasySetupDialog::OnAdvancedSoundSetup(wxCommandEvent& event)
{
    wxUnusedVar(event);
    
    AudioOptsDialog *dlg = new AudioOptsDialog(this);
    int rv = dlg->ShowModal();
    if(rv == wxOK)
    {
        dlg->ExchangeData(EXCHANGE_DATA_OUT);
        ExchangeSoundDeviceData(EXCHANGE_DATA_IN);
    }
    delete dlg;
}

void EasySetupDialog::OnAdvancedPTTSetup(wxCommandEvent& event)
{
    wxUnusedVar(event);
    
    ComPortsDlg *dlg = new ComPortsDlg(this);
    int rv = dlg->ShowModal();
    if(rv == wxID_OK)
    {
        ExchangePttDeviceData(EXCHANGE_DATA_IN);
    }
    delete dlg;
}

void EasySetupDialog::OnTest(wxCommandEvent& event)
{
    if (m_buttonTest->GetLabel() == "Stop Test")
    {
        // Stop the currently running test
        if (txTestAudioDevice_ != nullptr)
        {
            txTestAudioDevice_->stop();
            txTestAudioDevice_ = nullptr;
        }
        
        if (hamlibTestObject_->isActive())
        {
            wxString error;
            hamlibTestObject_->ptt(false, error);
            hamlibTestObject_->close();
        }
        else if (serialPortTestObject_->isopen())
        {
            serialPortTestObject_->ptt(false);
            serialPortTestObject_->closeport();
        }
        
        m_radioDevice->Enable(true);
        m_advancedSoundSetup->Enable(true);
        m_ckUseHamlibPTT->Enable(true);
        m_cbRigName->Enable(true);
        m_cbSerialPort->Enable(true);
        m_cbSerialRate->Enable(true);
        m_tcIcomCIVHex->Enable(true);
        m_advancedPTTSetup->Enable(true);
        m_ckbox_psk_enable->Enable(true);
        m_txt_callsign->Enable(true);
        m_txt_grid_square->Enable(true);
        m_buttonOK->Enable(true);
        m_buttonCancel->Enable(true);
        m_buttonApply->Enable(true);
        
        m_buttonTest->SetLabel("Test");
    }
    else
    {
        int index = m_radioDevice->GetSelection();
        wxString selectedString = m_radioDevice->GetString(index);
        wxString radioOutDeviceName = "none";
        int radioOutSampleRate = -1;
        
        // Use the global settings if we're using multiple sound devices on the radio side.
        if (m_analogDeviceRecord->GetStringSelection() != RX_ONLY_STRING)
        {
            if (selectedString == MULTIPLE_DEVICES_STRING)
            {
                radioOutDeviceName = wxGetApp().m_soundCard1OutDeviceName;
                radioOutSampleRate = wxGetApp().m_soundCard1OutSampleRate;
            }
            else
            {
                SoundDeviceData* deviceData = (SoundDeviceData*)m_radioDevice->GetClientObject(index);
                radioOutDeviceName = deviceData->txDeviceName;
                radioOutSampleRate = deviceData->txSampleRate;
            }
        }
        
        // Turn on transmit if Hamlib is enabled.
        if (m_ckUseHamlibPTT->GetValue())
        {
            int rig = m_cbRigName->GetSelection();
            wxString serialPort = m_cbSerialPort->GetValue();
            
            wxString s = m_tcIcomCIVHex->GetValue();
            long civHexAddress = 0;
            m_tcIcomCIVHex->GetValue().ToLong(&civHexAddress, 16);
            
            long rate = 0;
            s = m_cbSerialRate->GetValue();
            if (s != "default") {
                m_cbSerialRate->GetValue().ToLong(&rate);
            } 
            
            if (!hamlibTestObject_->connect(rig, serialPort, rate, civHexAddress))
            {
                wxMessageBox(
                    "Couldn't connect to Radio with Hamlib.  Make sure the Hamlib serial Device, Rate, and Params match your radio", 
                    wxT("Error"), wxOK | wxICON_ERROR, this);
                return;
            }
            
            wxString error;
            hamlibTestObject_->ptt(true, error);
        }
        else if (m_ckUseSerialPTT->GetValue())
        {
            wxString serialPort = m_cbSerialPort->GetValue();

            bool useRTS = m_rbUseRTS->GetValue();
            bool RTSPos = m_ckRTSPos->IsChecked();
            bool useDTR = m_rbUseDTR->GetValue();
            bool DTRPos = m_ckDTRPos->IsChecked();

            if (!serialPortTestObject_->openport(
                    serialPort,
                    useRTS,
                    RTSPos,
                    useDTR,
                    DTRPos))
            {
                wxMessageBox(
                    "Couldn't connect to Radio.  Make sure the serial Device and Params match your radio", 
                    wxT("Error"), wxOK | wxICON_ERROR, this);
                return;
            }
            
            serialPortTestObject_->ptt(true);
        }
        
        // Start playing a sine wave through the radio's device
        if (radioOutDeviceName != "none")
        {
            auto audioEngine = AudioEngineFactory::GetAudioEngine();
            audioEngine->start();

            txTestAudioDevice_ = audioEngine->getAudioDevice(radioOutDeviceName, IAudioEngine::AUDIO_ENGINE_OUT, radioOutSampleRate, 1);

            if (txTestAudioDevice_ == nullptr)
            {
                wxMessageBox(
                    "Error opening radio sound device. Please double-check configuration and try again.", 
                    wxT("Error"), wxOK | wxICON_ERROR, this);
                    
                wxString error;
                hamlibTestObject_->ptt(false, error);
                hamlibTestObject_->close();
                
                audioEngine->stop();
                return;
            }
            
            sineWaveSampleNumber_ = 0;
            txTestAudioDevice_->setOnAudioData([&](IAudioDevice& dev, void* data, size_t size, void* state) {
                short* audioData = static_cast<short*>(data);
    
                for (unsigned long index = 0; index < size; index++)
                {
                    *audioData++ = (SHRT_MAX) * sin(2 * PI * (1500) * sineWaveSampleNumber_ / 48000);
                    sineWaveSampleNumber_ = (sineWaveSampleNumber_ + 1) % 48000;
                }

            }, this);
            txTestAudioDevice_->start();
        }
        
        // Disable all UI except the Stop button.
        m_radioDevice->Enable(false);
        m_advancedSoundSetup->Enable(false);
        m_ckUseHamlibPTT->Enable(false);
        m_cbRigName->Enable(false);
        m_cbSerialPort->Enable(false);
        m_cbSerialRate->Enable(false);
        m_tcIcomCIVHex->Enable(false);
        m_advancedPTTSetup->Enable(false);
        m_ckbox_psk_enable->Enable(false);
        m_txt_callsign->Enable(false);
        m_txt_grid_square->Enable(false);
        m_buttonOK->Enable(false);
        m_buttonCancel->Enable(false);
        m_buttonApply->Enable(false);
        
        m_buttonTest->SetLabel("Stop Test");
    }
}

void EasySetupDialog::PTTUseHamLibClicked(wxCommandEvent& event)
{
    if (m_ckUseHamlibPTT->GetValue())
    {
        m_hamlibBox->Show();
        resetIcomCIVStatus();
    }
    else
    {
        m_hamlibBox->Hide();
    }

    if (m_ckUseSerialPTT->GetValue())
    {
        m_serialBox->Show();
    }
    else
    {
        m_serialBox->Hide();
    }

    Layout();
    SetSize(GetBestSize());
    SetMinSize(GetBestSize());
}

void EasySetupDialog::updateHamlibDevices_()
{
    if (wxGetApp().m_hamlib)
    {
        wxGetApp().m_hamlib->populateComboBox(m_cbRigName);
        m_cbRigName->SetSelection(wxGetApp().m_intHamlibRig);
    }
    
    /* populate Hamlib serial rate combo box */

    wxString serialRates[] = {"default", "300", "1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"}; 
    unsigned int i;
    for(i=0; i<WXSIZEOF(serialRates); i++) {
        m_cbSerialRate->Append(serialRates[i]);
    }
    m_cbSerialRate->SetSelection(0);
    m_cbSerialPort->Clear();
    
    std::vector<wxString> portList;
    
#ifdef __WXMSW__
    wxRegKey key(wxRegKey::HKLM, _T("HARDWARE\\DEVICEMAP\\SERIALCOMM"));
    if(!key.Exists())
    {
        return;
    }
    else
    {
        // Get the number of subkeys and enumerate them.
        if(!key.Open(wxRegKey::Read))
        {
            return;
        }    
        size_t subkeys;
        size_t values;
        if(!key.GetKeyInfo(&subkeys, NULL, &values, NULL))
        {
            return;
        }
        if(!key.HasValues())
        {
            return;
        }
        wxString key_name;
        long el = 1;
        key.GetFirstValue(key_name, el);
        wxString valType;
        wxString key_data;
        for(unsigned int i = 0; i < values; i++)
        {
            key.QueryValue(key_name, key_data);
            //wxPrintf("Value:  %s Data: %s\n", key_name, key_data);
            portList.push_back(key_data);
            key.GetNextValue(key_name, el);
        }
    }
#endif
#if defined(__WXGTK__) || defined(__WXOSX__)

#if defined(__FreeBSD__) || defined(__WXOSX__)
    glob_t    gl;
#ifdef __FreeBSD__
    if(glob("/dev/tty*", GLOB_MARK, NULL, &gl)==0) {
#else
    if(glob("/dev/tty.*", GLOB_MARK, NULL, &gl)==0) {
#endif
        for(unsigned int i=0; i<gl.gl_pathc; i++) {
            if(gl.gl_pathv[i][strlen(gl.gl_pathv[i])-1]=='/')
                continue;
                
            /* Exclude pseudo TTYs */
            if(gl.gl_pathv[i][8] >= 'l' && gl.gl_pathv[i][8] <= 's')
                continue;
            if(gl.gl_pathv[i][8] >= 'L' && gl.gl_pathv[i][8] <= 'S')
                continue;

            /* Exclude virtual TTYs */
            if(gl.gl_pathv[i][8] == 'v')
                continue;

            /* Exclude initial-state and lock-state devices */
#ifndef __WXOSX__
            if(strchr(gl.gl_pathv[i], '.') != NULL)
                continue;
#endif

            portList.push_back(gl.gl_pathv[i]);
        }
        globfree(&gl);
    }
#else
    glob_t    gl;
    if(glob("/sys/class/tty/*/device/driver", GLOB_MARK, NULL, &gl)==0) 
    {
        wxRegEx pathRegex("/sys/class/tty/([^/]+)");
        for(unsigned int i=0; i<gl.gl_pathc; i++) 
        {
            wxString path = gl.gl_pathv[i];
            if (pathRegex.Matches(path))
            {
                wxString name = "/dev/" + pathRegex.GetMatch(path, 1);
                portList.push_back(name);
            }
        }
        globfree(&gl);
    }
#endif
#endif

    // Sort the list such that the port number is in numeric (not text) order.
    std::sort(portList.begin(), portList.end(), [](const wxString& first, const wxString& second) {
        wxRegEx portRegex("^([^0-9]+)([0-9]+)$");
        wxString firstName = "";
        wxString firstNumber = "";
        wxString secondName = "";
        wxString secondNumber = "";
        int firstNumAsInt = 0;
        int secondNumAsInt = 0;
        
        if (portRegex.Matches(first))
        {
            firstName = portRegex.GetMatch(first, 1);
            firstNumber = portRegex.GetMatch(first, 2);
            firstNumAsInt = atoi(firstNumber.c_str());
        }
        else
        {
            firstName = first;
        }
        
        if (portRegex.Matches(second))
        {
            secondName = portRegex.GetMatch(second, 1);
            secondNumber = portRegex.GetMatch(second, 2);
            secondNumAsInt = atoi(secondNumber.c_str());
        }
        else
        {
            secondName = second;
        }
        
        return 
            (firstName < secondName) || 
            (firstName == secondName && firstNumAsInt < secondNumAsInt);
    });
    
    for (wxString& port : portList)
    {
        m_cbSerialPort->Append(port);
        m_cbCtlDevicePath->Append(port);
    }
}

void EasySetupDialog::updateAudioDevices_()
{
    std::map<wxString, SoundDeviceData*> finalRadioDeviceList;
    std::map<wxString, SoundDeviceData*> finalAnalogRxDeviceList;
    std::map<wxString, SoundDeviceData*> finalAnalogTxDeviceList;

    auto audioEngine = AudioEngineFactory::GetAudioEngine();
    audioEngine->start();

    auto inputDevices = audioEngine->getAudioDeviceList(IAudioEngine::AUDIO_ENGINE_IN);
    auto outputDevices = audioEngine->getAudioDeviceList(IAudioEngine::AUDIO_ENGINE_OUT);
    
    wxRegEx soundDeviceCleanup("^(Microphone|Speakers) \\(");
    wxRegEx rightParenRgx("\\)$");
    
    for (auto& dev : inputDevices)
    {
        wxString cleanedDeviceName = dev.name;
        if (soundDeviceCleanup.Replace(&cleanedDeviceName, "") > 0)
        {
            rightParenRgx.Replace(&cleanedDeviceName, "");
        }
        
        SoundDeviceData* soundData = new SoundDeviceData();
        assert(soundData != nullptr);
        
        soundData->rxDeviceName = dev.name;
        soundData->rxSampleRate = dev.defaultSampleRate;
        
        finalRadioDeviceList[cleanedDeviceName] = soundData;

        if (!cleanedDeviceName.StartsWith("DAX "))
        {
            SoundDeviceData* txSoundData = new SoundDeviceData();
            assert(txSoundData != nullptr);

            txSoundData->txDeviceName = dev.name;
            txSoundData->txSampleRate = dev.defaultSampleRate;

            finalAnalogTxDeviceList[dev.name] = txSoundData;
        }
    }
    
    finalAnalogTxDeviceList[RX_ONLY_STRING] = new SoundDeviceData();
    assert(finalAnalogTxDeviceList[RX_ONLY_STRING] != nullptr);
    finalAnalogTxDeviceList[RX_ONLY_STRING]->txDeviceName = "none";
    finalAnalogTxDeviceList[RX_ONLY_STRING]->txSampleRate = 0;

    for (auto& dev : outputDevices)
    {
        wxString cleanedDeviceName = dev.name;
        if (soundDeviceCleanup.Replace(&cleanedDeviceName, "") > 0)
        {
            rightParenRgx.Replace(&cleanedDeviceName, "");
        }
        
        SoundDeviceData* soundData = finalRadioDeviceList[cleanedDeviceName];
        if (soundData == nullptr)
        {
            soundData = new SoundDeviceData();
            assert(soundData != nullptr);
            
            finalRadioDeviceList[cleanedDeviceName] = soundData;
        }
        
        soundData->txDeviceName = dev.name;
        soundData->txSampleRate = dev.defaultSampleRate;

        if (!cleanedDeviceName.StartsWith("DAX "))
        {
            SoundDeviceData* rxSoundData = new SoundDeviceData();
            assert(rxSoundData != nullptr);

            rxSoundData->rxDeviceName = dev.name;
            rxSoundData->rxSampleRate = dev.defaultSampleRate;
            
            finalAnalogRxDeviceList[dev.name] = rxSoundData;
        }
    }
    
    // FlexRadio shortcut: all devices starting with "DAX Audio RX" should be linked
    // to the "DAX Audio TX" device. There's only one TX device for all digital mode
    // applications intended to be used on a Flex radio.
    wxString fullTxDeviceName;
    int flexTxDeviceSampleRate = -1;
    for (auto& kvp : finalRadioDeviceList)
    {
        if (kvp.first.StartsWith("DAX Audio TX"))
        {
            fullTxDeviceName = kvp.second->txDeviceName;
            flexTxDeviceSampleRate = kvp.second->txSampleRate;
            
            // Suppress the TX device from appearing in the list.
            kvp.second->txDeviceName = "none";
            kvp.second->txSampleRate = -1;
        }
        else if (kvp.first.StartsWith("DAX RESERVED") || kvp.first.StartsWith("DAX IQ") || kvp.first.StartsWith("DAX MIC"))
        {
            // Suppress all reserved and IQ devices from the list.
            kvp.second->txDeviceName = "none";
            kvp.second->txSampleRate = -1;
            kvp.second->rxDeviceName = "none";
            kvp.second->rxSampleRate = -1;
        }
    }
    
    if (fullTxDeviceName != "")
    {
        for (auto& kvp : finalRadioDeviceList)
        {
            if (kvp.first.StartsWith("DAX Audio RX"))
            {
                kvp.second->txDeviceName = fullTxDeviceName;
                kvp.second->txSampleRate = flexTxDeviceSampleRate;
            }
        }
    }
    
    // Add the ones we found to the device list.
    for (auto& kvp : finalRadioDeviceList)
    {
        // Only include devices that we have both RX and TX IDs for.
        if (kvp.second->rxDeviceName == "none" ||
            kvp.second->txDeviceName == "none")
        {
            delete kvp.second;
            continue;
        }
        
        // Note: m_radioDevice owns kvp.second after this call and is responsible
        // for deleting it when the window is closed by the user.
        m_radioDevice->Append(kvp.first, kvp.second);
    }    
    
    if (m_radioDevice->GetCount() > 0)
    {
        // Default to the first found device.
        m_radioDevice->SetSelection(0);
    }
    
    for (auto& kvp : finalAnalogRxDeviceList)
    {
        m_analogDevicePlayback->Append(kvp.first, kvp.second);
    }

    if (m_analogDevicePlayback->GetCount() > 0)
    {
        // Default to the first found device.
        m_analogDevicePlayback->SetSelection(0);
    }

    for (auto& kvp : finalAnalogTxDeviceList)
    {
        m_analogDeviceRecord->Append(kvp.first, kvp.second);
    }
    
    if (m_analogDeviceRecord->GetCount() > 0)
    {
        // Default to the first found device.
        m_analogDeviceRecord->SetSelection(0);
    }

    auto defaultInputDevice = audioEngine->getDefaultAudioDevice(IAudioEngine::AUDIO_ENGINE_IN);
    if (defaultInputDevice.isValid())
    {
        wxString devName(defaultInputDevice.name);

        auto index = m_analogDeviceRecord->FindString(devName);
        if (index > 0)
        {
            m_analogDeviceRecord->SetSelection(index);
        }
    }
    else
    {
        auto index = m_analogDeviceRecord->FindString(RX_ONLY_STRING);
        assert (index > 0);
        m_analogDeviceRecord->SetSelection(index);
    }
    
    auto defaultOutputDevice = audioEngine->getDefaultAudioDevice(IAudioEngine::AUDIO_ENGINE_OUT);
    if (defaultOutputDevice.isValid())
    {
        wxString devName(defaultOutputDevice.name);

        auto index = m_analogDevicePlayback->FindString(devName);
        if (index > 0)
        {
            m_analogDevicePlayback->SetSelection(index);
        }
    }

    audioEngine->stop();
}
