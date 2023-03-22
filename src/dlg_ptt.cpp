//==========================================================================
// Name:            dlg_ptt.cpp
// Purpose:         Subclasses dialog GUI for PTT Config. Creates simple 
//                  wxWidgets dialog GUI to select real/virtual Comm ports.
// Date:            May 11 2012
// Authors:         David Rowe, David Witten, Joel Stanley
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
#include "dlg_ptt.h"
#include "main.h"

#ifdef __WIN32__
#include <wx/msw/registry.h>
#else
#include <glob.h>
#include <string.h>
#endif

#include <sstream>
        
extern wxConfigBase *pConfig;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class ComPortsDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
ComPortsDlg::ComPortsDlg(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxDialog(parent, id, title, pos, size, style)
{
    wxPanel* panel = new wxPanel(this);
    
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    //----------------------------------------------------------------------
    // Vox tone option
    //----------------------------------------------------------------------

    wxStaticBox* voxBox = new wxStaticBox(panel, wxID_ANY, _("VOX PTT Settings"));
    wxStaticBoxSizer* staticBoxSizer28 = new wxStaticBoxSizer( voxBox, wxHORIZONTAL);
    m_ckLeftChannelVoxTone = new wxCheckBox(voxBox, wxID_ANY, _("Left Channel Vox Tone"), wxDefaultPosition, wxSize(-1,-1), 0);
    staticBoxSizer28->Add(m_ckLeftChannelVoxTone, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);

    mainSizer->Add(staticBoxSizer28, 0, wxEXPAND, 5);

    //----------------------------------------------------------------------
    // Hamlib for CAT PTT
    //----------------------------------------------------------------------

    wxStaticBox* hamlibBox = new wxStaticBox(panel, wxID_ANY, _("Hamlib Settings"));
    wxStaticBoxSizer* staticBoxSizer18 = new wxStaticBoxSizer( hamlibBox, wxHORIZONTAL);
    wxGridSizer* gridSizerhl = new wxGridSizer(6, 2, 0, 0);
    staticBoxSizer18->Add(gridSizerhl, 1, wxEXPAND|wxALIGN_LEFT, 5);

    /* Use Hamlib for PTT checkbox. */

    m_ckUseHamlibPTT = new wxCheckBox(hamlibBox, wxID_ANY, _("Use Hamlib PTT"), wxDefaultPosition, wxSize(-1, -1), 0);
    m_ckUseHamlibPTT->SetValue(false);
    gridSizerhl->Add(m_ckUseHamlibPTT, 0, wxALIGN_CENTER_VERTICAL, 0);
    gridSizerhl->Add(new wxStaticText(hamlibBox, -1, wxT("")), 0, wxEXPAND);

    /* Hamlib Rig Type combobox. */

    gridSizerhl->Add(new wxStaticText(hamlibBox, wxID_ANY, _("Rig Model:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 20);
    m_cbRigName = new wxComboBox(hamlibBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(250, -1), 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
    wxGetApp().m_hamlib->populateComboBox(m_cbRigName);
    m_cbRigName->SetSelection(wxGetApp().m_intHamlibRig);
    gridSizerhl->Add(m_cbRigName, 0, wxEXPAND, 0);

    /* Hamlib Serial Port combobox. */

    gridSizerhl->Add(new wxStaticText(hamlibBox, wxID_ANY, _("Serial Device (or hostname:port):"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL |  wxALIGN_RIGHT, 20);
    m_cbSerialPort = new wxComboBox(hamlibBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
    m_cbSerialPort->SetMinSize(wxSize(140, -1));
    gridSizerhl->Add(m_cbSerialPort, 0, wxEXPAND, 0);

    /* Hamlib Icom CI-V address text box. */
    m_stIcomCIVHex = new wxStaticText(hamlibBox, wxID_ANY, _("Radio Address:"), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerhl->Add(m_stIcomCIVHex, 
                      0, wxALIGN_CENTER_VERTICAL |  wxALIGN_RIGHT, 20);
    m_tcIcomCIVHex = new wxTextCtrl(hamlibBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(35, -1), 0, wxNumericPropertyValidator(wxNumericPropertyValidator::Unsigned, 16));
    m_tcIcomCIVHex->SetMaxLength(2);
    gridSizerhl->Add(m_tcIcomCIVHex, 0, wxALIGN_CENTER_VERTICAL, 0);
    
    /* Hamlib Serial Rate combobox. */

    gridSizerhl->Add(new wxStaticText(hamlibBox, wxID_ANY, _("Serial Rate:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 20);
    m_cbSerialRate = new wxComboBox(hamlibBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(140, -1), 0, NULL, wxCB_DROPDOWN);
    gridSizerhl->Add(m_cbSerialRate, 0, wxALIGN_CENTER_VERTICAL, 0);

    gridSizerhl->Add(new wxStaticText(hamlibBox, wxID_ANY, _("Serial Params:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 20);
    m_cbSerialParams = new wxStaticText(hamlibBox, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, 0);
    gridSizerhl->Add(m_cbSerialParams, 0, wxALIGN_CENTER_VERTICAL, 0);

    mainSizer->Add(staticBoxSizer18, 0, wxEXPAND, 5);

    //----------------------------------------------------------------------
    // Serial port PTT
    //----------------------------------------------------------------------

    wxStaticBox* serialSettingsBox = new wxStaticBox(panel, wxID_ANY, _("Serial Port Settings"));
    wxStaticBoxSizer* staticBoxSizer17 = new wxStaticBoxSizer( serialSettingsBox, wxVERTICAL);
    mainSizer->Add(staticBoxSizer17, 1, wxEXPAND, 5);
    
    wxStaticBox* pttBox = new wxStaticBox(serialSettingsBox, wxID_ANY, _("PTT Port"));
    wxStaticBoxSizer* staticBoxSizer31 = new wxStaticBoxSizer( pttBox, wxVERTICAL);
    staticBoxSizer17->Add(staticBoxSizer31, 1, wxEXPAND, 5);

    wxGridSizer* gridSizer200 = new wxGridSizer(1, 3, 0, 0);
    
    m_ckUseSerialPTT = new wxCheckBox(pttBox, wxID_ANY, _("Use Serial Port PTT"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_ckUseSerialPTT->SetValue(false);
    gridSizer200->Add(m_ckUseSerialPTT, 1, wxALIGN_CENTER|wxALIGN_CENTER_VERTICAL, 2);

    m_staticText12 = new wxStaticText(pttBox, wxID_ANY, _("Serial Device:  "), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText12->Wrap(-1);
    gridSizer200->Add(m_staticText12, 1,wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 2);

    m_cbCtlDevicePath = new wxComboBox(pttBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
    m_cbCtlDevicePath->SetMinSize(wxSize(140, -1));
    gridSizer200->Add(m_cbCtlDevicePath, 1, wxEXPAND, 2);
    
    staticBoxSizer31->Add(gridSizer200, 1, wxEXPAND, 1);

    wxGridSizer* gridSizer17 = new wxGridSizer(2, 2, 0, 0);
    
    m_rbUseDTR = new wxRadioButton(pttBox, wxID_ANY, _("Use DTR"), wxDefaultPosition, wxSize(-1,-1), wxRB_GROUP);
    m_rbUseDTR->SetToolTip(_("Toggle DTR line for PTT"));
    m_rbUseDTR->SetValue(1);
    gridSizer17->Add(m_rbUseDTR, 0, wxALIGN_CENTER|wxALIGN_CENTER_VERTICAL, 2);

    m_rbUseRTS = new wxRadioButton(pttBox, wxID_ANY, _("Use RTS"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_rbUseRTS->SetToolTip(_("Toggle the RTS pin for PTT"));
    m_rbUseRTS->SetValue(1);
    gridSizer17->Add(m_rbUseRTS, 0, wxALIGN_CENTER, 2);
    
    m_ckDTRPos = new wxCheckBox(pttBox, wxID_ANY, _("DTR = +V"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_ckDTRPos->SetToolTip(_("Set Polarity of the DTR line"));
    m_ckDTRPos->SetValue(false);
    gridSizer17->Add(m_ckDTRPos, 0, wxALIGN_CENTER, 2);

    m_ckRTSPos = new wxCheckBox(pttBox, wxID_ANY, _("RTS = +V"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_ckRTSPos->SetValue(false);
    m_ckRTSPos->SetToolTip(_("Set Polarity of the RTS line"));
    gridSizer17->Add(m_ckRTSPos, 0, wxALIGN_CENTER, 2);

    staticBoxSizer31->Add(gridSizer17, 1, wxEXPAND, 2);

    // Override tab ordering for Polarity area
    m_rbUseDTR->MoveBeforeInTabOrder(m_rbUseRTS);
    m_rbUseRTS->MoveBeforeInTabOrder(m_ckDTRPos);
    m_ckDTRPos->MoveBeforeInTabOrder(m_ckRTSPos);
    
    wxStaticBox* pttInBox = new wxStaticBox(serialSettingsBox, wxID_ANY, _("PTT In"));
    wxStaticBoxSizer* pttInBoxSizer = new wxStaticBoxSizer( pttInBox, wxVERTICAL);
    staticBoxSizer17->Add(pttInBoxSizer, 1, wxEXPAND, 5);
    
    wxGridSizer* gridSizerPttIn = new wxGridSizer(2, 3, 0, 0);
    
    m_ckUsePTTInput = new wxCheckBox(pttInBox, wxID_ANY, _("Enable PTT Input"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_ckUsePTTInput->SetValue(false);
    gridSizerPttIn->Add(m_ckUsePTTInput, 1, wxALIGN_CENTER|wxALIGN_CENTER_VERTICAL, 2);

    m_pttInSerialDeviceLabel = new wxStaticText(pttInBox, wxID_ANY, _("Serial Device:  "), wxDefaultPosition, wxDefaultSize, 0);
    m_pttInSerialDeviceLabel->Wrap(-1);
    gridSizerPttIn->Add(m_pttInSerialDeviceLabel, 1,wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 2);

    m_cbCtlDevicePathPttIn = new wxComboBox(pttInBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
    m_cbCtlDevicePathPttIn->SetMinSize(wxSize(140, -1));
    gridSizerPttIn->Add(m_cbCtlDevicePathPttIn, 1, wxEXPAND, 2);
    
    gridSizerPttIn->AddSpacer(1);
    
    m_ckCTSPos = new wxCheckBox(pttInBox, wxID_ANY, _("CTS = +V"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_ckCTSPos->SetValue(false);
    m_ckCTSPos->SetToolTip(_("Set Polarity of the CTS line"));
    gridSizerPttIn->Add(m_ckCTSPos, 1, wxALIGN_CENTER, 5);
    
    pttInBoxSizer->Add(gridSizerPttIn, 1, wxEXPAND, 5);
    
    //----------------------------------------------------------------------
    // OK - Cancel - Apply
    //----------------------------------------------------------------------

    wxBoxSizer* boxSizer12 = new wxBoxSizer(wxHORIZONTAL);

    m_buttonTest = new wxButton(panel, wxID_APPLY, _("Test PTT"), wxDefaultPosition, wxSize(-1,-1), 0);
    boxSizer12->Add(m_buttonTest, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);

    m_buttonOK = new wxButton(panel, wxID_OK, _("OK"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_buttonOK->SetDefault();
    boxSizer12->Add(m_buttonOK, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);

    m_buttonCancel = new wxButton(panel, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize(-1,-1), 0);
    boxSizer12->Add(m_buttonCancel, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);

    m_buttonApply = new wxButton(panel, wxID_APPLY, _("Apply"), wxDefaultPosition, wxSize(-1,-1), 0);
    boxSizer12->Add(m_buttonApply, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);

    mainSizer->Add(boxSizer12, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM|wxALIGN_CENTER_HORIZONTAL, 5);

    panel->SetSizer(mainSizer);
    
    wxBoxSizer* panelSizer = new wxBoxSizer(wxVERTICAL);
    panelSizer->Add(panel, 0, wxEXPAND, 0);
    this->SetSizerAndFit(panelSizer);
    
    Centre(wxBOTH);

    // Connect events
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(ComPortsDlg::OnInitDialog), NULL, this);
    m_ckUseHamlibPTT->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ComPortsDlg::PTTUseHamLibClicked), NULL, this);
    m_ckUseSerialPTT->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ComPortsDlg::PTTUseSerialClicked), NULL, this);
    m_ckUsePTTInput->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ComPortsDlg::PTTUseSerialInputClicked), NULL, this);
    m_cbRigName->Connect(wxEVT_COMBOBOX, wxCommandEventHandler(ComPortsDlg::HamlibRigNameChanged), NULL, this);
    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnOK), NULL, this);
    m_buttonCancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnCancel), NULL, this);
    m_buttonApply->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnApply), NULL, this);
    m_buttonTest->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnTest), NULL, this);
}

//-------------------------------------------------------------------------
// ~ComPortsDlg()
//-------------------------------------------------------------------------
ComPortsDlg::~ComPortsDlg()
{
    // Disconnect Events
    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(ComPortsDlg::OnInitDialog), NULL, this);
    m_ckUseHamlibPTT->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ComPortsDlg::PTTUseHamLibClicked), NULL, this);
    m_ckUseSerialPTT->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ComPortsDlg::PTTUseSerialClicked), NULL, this);
    m_ckUsePTTInput->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ComPortsDlg::PTTUseSerialInputClicked), NULL, this);
    m_cbRigName->Disconnect(wxEVT_COMBOBOX, wxCommandEventHandler(ComPortsDlg::HamlibRigNameChanged), NULL, this);
    m_buttonOK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnOK), NULL, this);
    m_buttonCancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnCancel), NULL, this);
    m_buttonApply->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnApply), NULL, this);
    m_buttonTest->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnTest), NULL, this);
}

//-------------------------------------------------------------------------
// OnInitDialog()
//-------------------------------------------------------------------------
void ComPortsDlg::OnInitDialog(wxInitDialogEvent& event)
{
    populatePortList();
    ExchangeData(EXCHANGE_DATA_IN);
}

//-------------------------------------------------------------------------
// populatePortList()
//-------------------------------------------------------------------------
void ComPortsDlg::populatePortList()
{

    /* populate Hamlib serial rate combo box */

    wxString serialRates[] = {"default", "300", "1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"}; 
    unsigned int i;
    for(i=0; i<WXSIZEOF(serialRates); i++) {
        m_cbSerialRate->Append(serialRates[i]);
    }

    m_cbSerialPort->Clear();
    m_cbCtlDevicePath->Clear();
    
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
        m_cbCtlDevicePath->Append(port);
        m_cbSerialPort->Append(port);
        m_cbCtlDevicePathPttIn->Append(port);
    }
}

//-------------------------------------------------------------------------
// ExchangeData()
//-------------------------------------------------------------------------
void ComPortsDlg::ExchangeData(int inout)
{
    wxString str;
    
    if(inout == EXCHANGE_DATA_IN) {
        m_ckLeftChannelVoxTone->SetValue(wxGetApp().m_leftChannelVoxTone);

        /* Hamlib */

        m_ckUseHamlibPTT->SetValue(wxGetApp().m_boolHamlibUseForPTT);
        m_cbRigName->SetSelection(wxGetApp().m_intHamlibRig);
        resetIcomCIVStatus();
        m_cbSerialPort->SetValue(wxGetApp().m_strHamlibSerialPort);

        if (wxGetApp().m_intHamlibSerialRate == 0) {
            m_cbSerialRate->SetSelection(0);
        } else {
            m_cbSerialRate->SetValue(wxString::Format(wxT("%i"), wxGetApp().m_intHamlibSerialRate));
        }

        m_tcIcomCIVHex->SetValue(wxString::Format(wxT("%02X"), wxGetApp().m_intHamlibIcomCIVHex));
        
        /* Serial PTT */

        m_ckUseSerialPTT->SetValue(wxGetApp().m_boolUseSerialPTT);
        str = wxGetApp().m_strRigCtrlPort;
        m_cbCtlDevicePath->SetValue(str);
        m_rbUseRTS->SetValue(wxGetApp().m_boolUseRTS);
        m_ckRTSPos->SetValue(wxGetApp().m_boolRTSPos);
        m_rbUseDTR->SetValue(wxGetApp().m_boolUseDTR);
        m_ckDTRPos->SetValue(wxGetApp().m_boolDTRPos);
        
        m_ckUsePTTInput->SetValue(wxGetApp().m_boolUseSerialPTTInput);
        str = wxGetApp().m_strPTTInputPort;
        m_cbCtlDevicePathPttIn->SetValue(str);
        m_ckCTSPos->SetValue(wxGetApp().m_boolCTSPos);

        updateControlState();
    }

    if (inout == EXCHANGE_DATA_OUT) {
        wxGetApp().m_leftChannelVoxTone = m_ckLeftChannelVoxTone->GetValue();
        pConfig->Write(wxT("/Rig/leftChannelVoxTone"), wxGetApp().m_leftChannelVoxTone);

        /* Hamlib settings. */

        wxGetApp().m_boolHamlibUseForPTT = m_ckUseHamlibPTT->GetValue();
        wxGetApp().m_intHamlibRig = m_cbRigName->GetSelection();
        
        Hamlib *hamlib = wxGetApp().m_hamlib;
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

        pConfig->Write(wxT("/Hamlib/UseForPTT"), wxGetApp().m_boolHamlibUseForPTT);
        pConfig->Write(wxT("/Hamlib/RigNameStr"), wxGetApp().m_strHamlibRigName);
        pConfig->Write(wxT("/Hamlib/SerialPort"), wxGetApp().m_strHamlibSerialPort);
        pConfig->Write(wxT("/Hamlib/SerialRate"), wxGetApp().m_intHamlibSerialRate);

        /* Serial settings */

        wxGetApp().m_boolUseSerialPTT           = m_ckUseSerialPTT->IsChecked();
        wxGetApp().m_strRigCtrlPort             = m_cbCtlDevicePath->GetValue();
        wxGetApp().m_boolUseRTS                 = m_rbUseRTS->GetValue();
        wxGetApp().m_boolRTSPos                 = m_ckRTSPos->IsChecked();
        wxGetApp().m_boolUseDTR                 = m_rbUseDTR->GetValue();
        wxGetApp().m_boolDTRPos                 = m_ckDTRPos->IsChecked();
        
        pConfig->Write(wxT("/Rig/UseSerialPTT"),    wxGetApp().m_boolUseSerialPTT);
        pConfig->Write(wxT("/Rig/Port"),            wxGetApp().m_strRigCtrlPort); 
        pConfig->Write(wxT("/Rig/UseRTS"),          wxGetApp().m_boolUseRTS);
        pConfig->Write(wxT("/Rig/RTSPolarity"),     wxGetApp().m_boolRTSPos);
        pConfig->Write(wxT("/Rig/UseDTR"),          wxGetApp().m_boolUseDTR);
        pConfig->Write(wxT("/Rig/DTRPolarity"),     wxGetApp().m_boolDTRPos);

        wxGetApp().m_boolUseSerialPTTInput = m_ckUsePTTInput->IsChecked();
        wxGetApp().m_strPTTInputPort = m_cbCtlDevicePathPttIn->GetValue();
        wxGetApp().m_boolCTSPos = m_ckCTSPos->IsChecked();
        
        pConfig->Write(wxT("/Rig/UseSerialPTTInput"), wxGetApp().m_boolUseSerialPTTInput);
        pConfig->Write(wxT("/Rig/PttInPort"), wxGetApp().m_strPTTInputPort);
        pConfig->Write(wxT("/Rig/CTSPolarity"), wxGetApp().m_boolCTSPos); 

        pConfig->Flush();
    }
}

//-------------------------------------------------------------------------
// PTTUseHamLibClicked()
//-------------------------------------------------------------------------
void ComPortsDlg::PTTUseHamLibClicked(wxCommandEvent& event)
{
    m_ckUseSerialPTT->SetValue(false);
    updateControlState();
}


/* Attempt to toggle PTT for 1 second */

void ComPortsDlg::OnTest(wxCommandEvent& event) {

    /* Tone PTT */

    if (m_ckLeftChannelVoxTone->GetValue()) {
        wxMessageBox("Testing of tone based PTT not supported; try PTT after pressing Start on main window", 
                     wxT("Error"), wxOK | wxICON_ERROR, this);
    }

    /* Hamlib PTT */

    if (m_ckUseHamlibPTT->GetValue()) {

        // set up current hamlib config from GUI

        int rig = m_cbRigName->GetSelection();
        wxString port = m_cbSerialPort->GetValue();
        wxString s = m_cbSerialRate->GetValue();
        int serial_rate;
        if (s == "default") {
            serial_rate = 0;
        } else {
            long tmp;
            m_cbSerialRate->GetValue().ToLong(&tmp); 
            serial_rate = tmp;
        }

        s = m_tcIcomCIVHex->GetValue();
        long hexAddress = 0;
        m_tcIcomCIVHex->GetValue().ToLong(&hexAddress, 16);
        
        // display serial params

        if (g_verbose) fprintf(stderr, "serial rate: %d\n", serial_rate);

        if (wxGetApp().CanAccessSerialPort((const char*)port.ToUTF8()))
        {
            // try to open rig
            Hamlib *hamlib = wxGetApp().m_hamlib; 
            bool status = hamlib->connect(rig, port.mb_str(wxConvUTF8), serial_rate, hexAddress);
            if (status == false) {
                wxMessageBox("Couldn't connect to Radio with Hamlib.  Make sure the Hamlib serial Device, Rate, and Params match your radio", 
                wxT("Error"), wxOK | wxICON_ERROR, this);
                return;
            }
            else {
                wxString hamlib_serial_config;
                hamlib_serial_config.sprintf(" %d, %d, %d", 
                                             hamlib->get_serial_rate(),
                                             hamlib->get_data_bits(),
                                             hamlib->get_stop_bits());
                m_cbSerialParams->SetLabel(hamlib_serial_config);
           }

            // toggle PTT

            wxString hamlibError;
            if (hamlib->ptt(true, hamlibError) == false) {
                wxMessageBox(wxString("Hamlib PTT Error: ") + hamlibError + 
                             wxString(".  Make sure the Hamlib serial Device, Rate, and Params match your radio"), 
                             wxT("Error"), wxOK | wxICON_ERROR, this);
                return;
            }

            wxSleep(1);

            if (hamlib->ptt(false, hamlibError) == false) {
                wxMessageBox(wxString("Hamlib PTT Error: ") + hamlibError +
                             wxString(".  Make sure the Hamlib serial Device, Rate, and Params match your radio"), 
                             wxT("Error"), wxOK | wxICON_ERROR, this);
            }
        }
    }

    /* Serial PTT */

    if (m_ckUseSerialPTT->IsChecked()) {
        Serialport *serialport = wxGetApp().m_serialport; 
        
        wxString ctrlport;
        ctrlport = m_cbCtlDevicePath->GetValue();
        if (g_verbose) fprintf(stderr, "opening serial port: ");
        fputs(ctrlport.c_str(), stderr);            // don't escape crazy Microsoft bakslash-ified comm port names
        if (g_verbose) fprintf(stderr,"\n");
        
        if (wxGetApp().CanAccessSerialPort((const char*)ctrlport.ToUTF8()))
        {
            bool success = serialport->openport(ctrlport.c_str(),
                                                m_rbUseRTS->GetValue(),
                                                m_ckRTSPos->IsChecked(),
                                                m_rbUseDTR->GetValue(),
                                                m_ckDTRPos->IsChecked());

            if (g_verbose) fprintf(stderr, "serial port open\n");

            if (!success) {
                wxString errorMessage = "Couldn't open serial port " + ctrlport + ". This is likely due to not having permission to access the chosen port.";
                wxMessageBox(errorMessage, wxT("Error"), wxOK | wxICON_ERROR, this);
            }

            // assert PTT port for 1 sec

            serialport->ptt(true);
            wxSleep(1);
            serialport->ptt(false);

            if (g_verbose) fprintf(stderr, "closing serial port\n");
            serialport->closeport();
            if (g_verbose) fprintf(stderr, "serial port closed\n");
        }
    }
    
}


//-------------------------------------------------------------------------
// PTTUseSerialClicked()
//-------------------------------------------------------------------------
void ComPortsDlg::PTTUseSerialClicked(wxCommandEvent& event)
{
    m_ckUseHamlibPTT->SetValue(false);
    updateControlState();
}

//-------------------------------------------------------------------------
// PTTUseSerialClicked()
//-------------------------------------------------------------------------
void ComPortsDlg::PTTUseSerialInputClicked(wxCommandEvent& event)
{
    updateControlState();
}

//-------------------------------------------------------------------------
// ComPortsDlg::HamlibRigNameChanged(): hide/show Icom CI-V hex control
// depending on the selected radio.
//-------------------------------------------------------------------------
void ComPortsDlg::HamlibRigNameChanged(wxCommandEvent& event)
{
    resetIcomCIVStatus();
}

void ComPortsDlg::resetIcomCIVStatus()
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

//-------------------------------------------------------------------------
// OnApply()
//-------------------------------------------------------------------------
void ComPortsDlg::OnApply(wxCommandEvent& event)
{
    ComPortsDlg::savePttSettings();
}

//-------------------------------------------------------------------------
// OnCancel()
//-------------------------------------------------------------------------
void ComPortsDlg::OnCancel(wxCommandEvent& event)
{
    this->EndModal(wxID_CANCEL);
}

//-------------------------------------------------------------------------
// savePttSettings()
//-------------------------------------------------------------------------
bool ComPortsDlg::savePttSettings()
{
    ExchangeData(EXCHANGE_DATA_OUT);
    return true;
}

//-------------------------------------------------------------------------
// OnClose()
//-------------------------------------------------------------------------
void ComPortsDlg::OnOK(wxCommandEvent& event)
{
    if (savePttSettings()) this->EndModal(wxID_OK);
}

void ComPortsDlg::updateControlState()
{
    m_cbRigName->Enable(m_ckUseHamlibPTT->GetValue());
    m_cbSerialPort->Enable(m_ckUseHamlibPTT->GetValue());
    m_cbSerialRate->Enable(m_ckUseHamlibPTT->GetValue());
    m_tcIcomCIVHex->Enable(m_ckUseHamlibPTT->GetValue());

    m_cbCtlDevicePath->Enable(m_ckUseSerialPTT->GetValue());
    m_rbUseDTR->Enable(m_ckUseSerialPTT->GetValue());
    m_ckRTSPos->Enable(m_ckUseSerialPTT->GetValue());
    m_rbUseRTS->Enable(m_ckUseSerialPTT->GetValue());
    m_ckDTRPos->Enable(m_ckUseSerialPTT->GetValue());
    
    m_cbCtlDevicePathPttIn->Enable(m_ckUsePTTInput->GetValue());
    m_ckCTSPos->Enable(m_ckUsePTTInput->GetValue());
    
    m_buttonTest->Enable(m_ckUseHamlibPTT->GetValue() || m_ckUseSerialPTT->GetValue());    
}
