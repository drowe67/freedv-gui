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

#ifdef __WIN32__
#include <wx/msw/registry.h>
#else
#include <glob.h>
#include <string.h>
#endif

#include "dlg_easy_setup.h"
#include "pa_wrapper.h"

EasySetupDialog::EasySetupDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) 
    : wxDialog(parent, id, title, pos, size, style)
{
    this->SetSizeHints(wxDefaultSize, wxDefaultSize);
    
    // Create top-level of control hierarchy.
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* sectionSizer = new wxBoxSizer(wxVERTICAL);
    
    // Step 1: sound device selection
    // ==============================
    wxStaticBox *selectSoundDeviceBox = new wxStaticBox(panel, wxID_ANY, _("Step 1: Select Sound Device"));
    wxStaticBoxSizer* setupSoundDeviceBoxSizer = new wxStaticBoxSizer( selectSoundDeviceBox, wxVERTICAL);
    
    wxGridSizer* gridSizerSoundDevice = new wxGridSizer(3, 2, 0, 0);
    
    wxStaticText* labelRadioDevice = new wxStaticText(selectSoundDeviceBox, wxID_ANY, wxT("Radio Device: "), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerSoundDevice->Add(labelRadioDevice, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 20);

    m_radioDevice = new wxComboBox(selectSoundDeviceBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
    m_radioDevice->SetMinSize(wxSize(250, -1));
    gridSizerSoundDevice->Add(m_radioDevice, 0, wxEXPAND, 0);
    
    wxStaticText* labelAnalogPlayDevice = new wxStaticText(selectSoundDeviceBox, wxID_ANY, wxT("Decoded audio plays back through: "), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerSoundDevice->Add(labelAnalogPlayDevice, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 20);
    
    m_analogDevicePlayback = new wxStaticText(selectSoundDeviceBox, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerSoundDevice->Add(m_analogDevicePlayback, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND, 0);
    
    wxStaticText* labelAnalogRecordDevice = new wxStaticText(selectSoundDeviceBox, wxID_ANY, wxT("Transmitted audio records through: "), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerSoundDevice->Add(labelAnalogRecordDevice, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 20);
    
    m_analogDeviceRecord = new wxStaticText(selectSoundDeviceBox, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerSoundDevice->Add(m_analogDeviceRecord, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND, 0);
        
    wxBoxSizer* advancedSoundSetupSizer = new wxBoxSizer(wxHORIZONTAL);
    m_advancedSoundSetup = new wxButton(selectSoundDeviceBox, wxID_ANY, wxT("Advanced Sound Settings"),  wxDefaultPosition, wxDefaultSize, 0);
    advancedSoundSetupSizer->Add(m_advancedSoundSetup, 0, 0, 3);
    
    setupSoundDeviceBoxSizer->Add(gridSizerSoundDevice, 1, wxEXPAND | wxALIGN_LEFT, 5);
    setupSoundDeviceBoxSizer->Add(advancedSoundSetupSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 3);
    sectionSizer->Add(setupSoundDeviceBoxSizer, 0, wxALL | wxEXPAND, 3);
    
    // Step 2: setup CAT control
    // =========================
    wxStaticBox *setupCatControlBox = new wxStaticBox(panel, wxID_ANY, _("Step 2: Setup CAT Control"));
    wxStaticBoxSizer* setupCatControlBoxSizer = new wxStaticBoxSizer( setupCatControlBox, wxVERTICAL);
    
    wxGridSizer* gridSizerhl = new wxGridSizer(5, 2, 0, 0);
    setupCatControlBoxSizer->Add(gridSizerhl, 1, wxEXPAND|wxALIGN_LEFT, 5);

    /* Use Hamlib for PTT checkbox. */

    m_ckUseHamlibPTT = new wxCheckBox(setupCatControlBox, wxID_ANY, _("Use Hamlib PTT"), wxDefaultPosition, wxSize(-1, -1), 0);
    m_ckUseHamlibPTT->SetValue(false);
    gridSizerhl->Add(m_ckUseHamlibPTT, 0, wxALIGN_CENTER_VERTICAL, 0);
    gridSizerhl->Add(new wxStaticText(setupCatControlBox, -1, wxT("")), 0, wxEXPAND);

    /* Hamlib Rig Type combobox. */

    gridSizerhl->Add(new wxStaticText(setupCatControlBox, wxID_ANY, _("Rig Model:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 20);
    m_cbRigName = new wxComboBox(setupCatControlBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(250, -1), 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
    gridSizerhl->Add(m_cbRigName, 0, wxEXPAND, 0);

    /* Hamlib Serial Port combobox. */

    gridSizerhl->Add(new wxStaticText(setupCatControlBox, wxID_ANY, _("Serial Device (or hostname:port):"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL |  wxALIGN_RIGHT, 20);
    m_cbSerialPort = new wxComboBox(setupCatControlBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
    m_cbSerialPort->SetMinSize(wxSize(140, -1));
    gridSizerhl->Add(m_cbSerialPort, 0, wxEXPAND, 0);

    /* Hamlib Icom CI-V address text box. */
    m_stIcomCIVHex = new wxStaticText(setupCatControlBox, wxID_ANY, _("Radio Address:"), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerhl->Add(m_stIcomCIVHex, 
                      0, wxALIGN_CENTER_VERTICAL |  wxALIGN_RIGHT, 20);
    m_tcIcomCIVHex = new wxTextCtrl(setupCatControlBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(35, -1), 0, wxNumericPropertyValidator(wxNumericPropertyValidator::Unsigned, 16));
    m_tcIcomCIVHex->SetMaxLength(2);
    gridSizerhl->Add(m_tcIcomCIVHex, 0, wxALIGN_CENTER_VERTICAL, 0);
    
    /* Hamlib Serial Rate combobox. */

    gridSizerhl->Add(new wxStaticText(setupCatControlBox, wxID_ANY, _("Serial Rate:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 20);
    m_cbSerialRate = new wxComboBox(setupCatControlBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(140, -1), 0, NULL, wxCB_DROPDOWN);
    gridSizerhl->Add(m_cbSerialRate, 0, wxALIGN_CENTER_VERTICAL, 0);
    
    wxBoxSizer* pttButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    
    m_advancedPTTSetup = new wxButton(setupCatControlBox, wxID_ANY, wxT("Advanced PTT Settings"),  wxDefaultPosition, wxDefaultSize, 0);
    pttButtonSizer->Add(m_advancedPTTSetup, 0, wxALIGN_CENTER_VERTICAL, 3);

    m_buttonTest = new wxButton(setupCatControlBox, wxID_ANY, wxT("Test"),  wxDefaultPosition, wxDefaultSize, 0);
    pttButtonSizer->Add(m_buttonTest, 0, wxALIGN_CENTER_VERTICAL, 3);
    
    setupCatControlBoxSizer->Add(pttButtonSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 3);
    
    sectionSizer->Add(setupCatControlBoxSizer, 0, wxALL|wxEXPAND, 3);
    
    // Step 3: setup PSK Reporter
    // ==========================
    wxStaticBox *setupPskReporterBox = new wxStaticBox(panel, wxID_ANY, _("Step 3: Setup Reporting"));
    
    wxStaticBoxSizer* sbSizer_psk;
    sbSizer_psk = new wxStaticBoxSizer(setupPskReporterBox, wxHORIZONTAL);
    m_ckbox_psk_enable = new wxCheckBox(setupPskReporterBox, wxID_ANY, _("Enable Reporting"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
    sbSizer_psk->Add(m_ckbox_psk_enable, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelPskCallsign = new wxStaticText(setupPskReporterBox, wxID_ANY, wxT("Callsign: "), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_psk->Add(labelPskCallsign, 0,  wxRIGHT | wxALIGN_CENTER_VERTICAL, 3);
    
    m_txt_callsign = new wxTextCtrl(setupPskReporterBox, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(90,-1), 0, wxTextValidator(wxFILTER_ALPHANUMERIC));
    sbSizer_psk->Add(m_txt_callsign, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
    
    wxStaticText* labelPskGridSquare = new wxStaticText(setupPskReporterBox, wxID_ANY, wxT("Grid Square: "), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer_psk->Add(labelPskGridSquare, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 3);
    
    m_txt_grid_square = new wxTextCtrl(setupPskReporterBox, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxSize(70,-1), 0, wxTextValidator(wxFILTER_ALPHANUMERIC));
    sbSizer_psk->Add(m_txt_grid_square, 0,  wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
    
    sectionSizer->Add(sbSizer_psk, 0, wxALL|wxEXPAND, 3);
    
    // Step 4: save or cancel changes
    // =============================
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    m_buttonOK = new wxButton(panel, wxID_OK);
    buttonSizer->Add(m_buttonOK, 0, wxALL, 2);

    m_buttonCancel = new wxButton(panel, wxID_CANCEL);
    buttonSizer->Add(m_buttonCancel, 0, wxALL, 2);

    m_buttonApply = new wxButton(panel, wxID_APPLY);
    buttonSizer->Add(m_buttonApply, 0, wxALL, 2);

    sectionSizer->Add(buttonSizer, 0, wxALL | wxALIGN_CENTER, 5);
    
    // Trigger auto-layout of window.
    // ==============================
    panel->SetSizer(sectionSizer);
    
    wxBoxSizer* panelSizer = new wxBoxSizer(wxVERTICAL);
    panelSizer->Add(panel, 0, wxEXPAND, 0);
    this->SetSizerAndFit(panelSizer);
    
    this->Layout();
    this->Centre(wxBOTH);
    
    // Hook in events
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(EasySetupDialog::OnInitDialog));
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(EasySetupDialog::OnClose));
    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnOK), NULL, this);
    m_buttonCancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnCancel), NULL, this);
    m_buttonApply->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnApply), NULL, this);
}

EasySetupDialog::~EasySetupDialog()
{
    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(EasySetupDialog::OnInitDialog));
    this->Disconnect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(EasySetupDialog::OnClose));
    m_buttonOK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnOK), NULL, this);
    m_buttonCancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnCancel), NULL, this);
    m_buttonApply->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(EasySetupDialog::OnApply), NULL, this);
}

void EasySetupDialog::ExchangeData(int inout)
{
    if (inout == EXCHANGE_DATA_IN)
    {
        
    }
    else if (inout == EXCHANGE_DATA_OUT)
    {
        
    }
}

void EasySetupDialog::OnInitDialog(wxInitDialogEvent& event)
{
    updateAudioDevices_();
    updateHamlibDevices_();
    ExchangeData(EXCHANGE_DATA_IN);
}

void EasySetupDialog::OnOK(wxCommandEvent& event)
{
    ExchangeData(EXCHANGE_DATA_OUT);
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
    }
}

void EasySetupDialog::updateAudioDevices_()
{
    PaError pa_err;
    if((pa_err = Pa_Initialize()) != paNoError)
    {
        wxMessageBox(wxT("Port Audio failed to initialize"), wxT("Pa_Initialize"), wxOK);
        return;
    }
    
    int numDevices = Pa_GetDeviceCount();
    for (int index = 0; index < numDevices; index++)
    {
        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(index);
        
        // Only devices that have both input and output channels should be considered for radio devices.
        // These are typically ones such as "USB Audio CODEC" as built into SignaLink et al.
        if (deviceInfo->maxInputChannels > 0 && deviceInfo->maxOutputChannels > 0)
        {
            wxString hostApiName(wxString::FromUTF8(Pa_GetHostApiInfo(deviceInfo->hostApi)->name));           

            // Exclude DirectSound devices from the list, as they are duplicates to MME
            // devices and sometimes do not work well for users
            if(hostApiName.Find("DirectSound") != wxNOT_FOUND)
                continue;

            // Exclude "surround" devices as they clutter the dev list and are not used
            wxString devName(wxString::FromUTF8(deviceInfo->name));
            if(devName.Find("surround") != wxNOT_FOUND)
                continue; 
            
            PaStreamParameters   inputParameters, outputParameters;
            
            inputParameters.device = index;
            inputParameters.channelCount = 1;
            inputParameters.sampleFormat = paInt16;
            inputParameters.suggestedLatency = 0;
            inputParameters.hostApiSpecificStreamInfo = NULL;
        
            outputParameters.device = index;
            outputParameters.channelCount = 1;
            outputParameters.sampleFormat = paInt16;
            outputParameters.suggestedLatency = 0;
            outputParameters.hostApiSpecificStreamInfo = NULL;
            
            PaError err = Pa_IsFormatSupported(&inputParameters, &outputParameters, 48000);
            if (err == paFormatIsSupported)
            {
                m_radioDevice->Append(devName);
                m_radioDevice->SetSelection(0);
            }
        }
    }
    
    PaDeviceIndex defaultInIndex = Pa_GetDefaultInputDevice();
    if (defaultInIndex != paNoDevice)
    {
        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(defaultInIndex);
        wxString devName(wxString::FromUTF8(deviceInfo->name));
        m_analogDeviceRecord->SetLabel(devName);
    }
    else
    {
        m_analogDevicePlayback->SetLabel("None (receive only)");
    }
    
    PaDeviceIndex defaultOutIndex = Pa_GetDefaultOutputDevice();
    if (defaultOutIndex != paNoDevice)
    {
        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(defaultOutIndex);
        wxString devName(wxString::FromUTF8(deviceInfo->name));
        m_analogDevicePlayback->SetLabel(devName);
    }
    
    Pa_Terminate();
}