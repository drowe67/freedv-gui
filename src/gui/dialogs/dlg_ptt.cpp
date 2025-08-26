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
#include <chrono>

#include "rig_control/HamlibRigController.h"
#include "rig_control/SerialPortOutRigController.h"

#if defined(WIN32)
#include "rig_control/omnirig/OmniRigController.h"
#endif // defined(WIN32)

using namespace std::chrono_literals;

extern wxConfigBase *pConfig;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class ComPortsDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
ComPortsDlg::ComPortsDlg(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxDialog(parent, id, title, pos, size, style)
{
    isTesting_ = false;
    
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
    
    if (wxGetApp().customConfigFileName != "")
    {
        SetTitle(wxString::Format("%s (%s)", title, wxGetApp().customConfigFileName));
    }
    
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
    wxGridSizer* gridSizerhl = new wxGridSizer(7, 2, 0, 0);
    staticBoxSizer18->Add(gridSizerhl, 1, wxEXPAND|wxALIGN_LEFT, 5);

    /* Use Hamlib for PTT checkbox. */

    m_ckUseHamlibPTT = new wxCheckBox(hamlibBox, wxID_ANY, _("Enable CAT control via Hamlib"), wxDefaultPosition, wxSize(-1, -1), 0);
    m_ckUseHamlibPTT->SetValue(false);
    gridSizerhl->Add(m_ckUseHamlibPTT, 0, wxALIGN_CENTER_VERTICAL, 0);
    gridSizerhl->Add(new wxStaticText(hamlibBox, -1, wxT("")), 0, wxEXPAND);

    /* Hamlib Rig Type combobox. */

    gridSizerhl->Add(new wxStaticText(hamlibBox, wxID_ANY, _("Rig Model:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 20);
    m_cbRigName = new wxComboBox(hamlibBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(250, -1), 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);

    auto numHamlibDevices = HamlibRigController::GetNumberSupportedRadios();
    for (auto index = 0; index < numHamlibDevices; index++)
    {
        m_cbRigName->Append(HamlibRigController::RigIndexToName(index));
    }
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
    
    /* Hamlib PTT Method combobox. */

    gridSizerhl->Add(new wxStaticText(hamlibBox, wxID_ANY, _("PTT uses:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL |  wxALIGN_RIGHT, 20);
    m_cbPttMethod = new wxComboBox(hamlibBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
    m_cbPttMethod->SetSize(wxSize(140, -1));
    gridSizerhl->Add(m_cbPttMethod, 0, wxALIGN_CENTER_VERTICAL, 0);
    
    /* Hamlib PTT serial port combobox */
    
    gridSizerhl->Add(new wxStaticText(hamlibBox, wxID_ANY, _("PTT Serial Device:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL |  wxALIGN_RIGHT, 20);
    m_cbPttSerialPort = new wxComboBox(hamlibBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
    m_cbPttSerialPort->SetMinSize(wxSize(140, -1));
    gridSizerhl->Add(m_cbPttSerialPort, 0, wxEXPAND, 0);
    
    // Add valid PTT options to combo box.
    m_cbPttMethod->Append(wxT("CAT"));
    m_cbPttMethod->Append(wxT("RTS"));
    m_cbPttMethod->Append(wxT("DTR"));
    m_cbPttMethod->Append(wxT("None"));
    m_cbPttMethod->Append(wxT("CAT via Data port"));
    
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
    
#if defined(WIN32)
    
    //----------------------------------------------------------------------
    // OmniRig for PTT/frequency control
    //----------------------------------------------------------------------

    wxStaticBox* omniRigBox = new wxStaticBox(panel, wxID_ANY, _("OmniRig Settings"));
    wxStaticBoxSizer* omniRigBoxSizer = new wxStaticBoxSizer( omniRigBox, wxHORIZONTAL);

    /* Use OmniRig checkbox. */

    m_ckUseOmniRig = new wxCheckBox(omniRigBox, wxID_ANY, _("Enable CAT control via OmniRig"), wxDefaultPosition, wxSize(-1, -1), 0);
    m_ckUseOmniRig->SetValue(false);
    omniRigBoxSizer->Add(m_ckUseOmniRig, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    /* OmniRig Rig ID combobox. */

    omniRigBoxSizer->Add(new wxStaticText(omniRigBox, wxID_ANY, _("Rig ID:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    m_cbOmniRigRigId = new wxComboBox(omniRigBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(50, -1), 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);

    m_cbOmniRigRigId->Append("1");
    m_cbOmniRigRigId->Append("2");
    omniRigBoxSizer->Add(m_cbOmniRigRigId, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    mainSizer->Add(omniRigBoxSizer, 0, wxEXPAND, 5);
    
#endif // defined(WIN32)
    
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
    
#if defined(WIN32)
    m_ckUseOmniRig->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ComPortsDlg::PTTUseOmniRigClicked), NULL, this);
#endif // defined(WIN32)
    
    m_ckUsePTTInput->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ComPortsDlg::PTTUseSerialInputClicked), NULL, this);
    m_cbRigName->Connect(wxEVT_COMBOBOX, wxCommandEventHandler(ComPortsDlg::HamlibRigNameChanged), NULL, this);
    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnOK), NULL, this);
    m_buttonCancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnCancel), NULL, this);
    m_buttonApply->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnApply), NULL, this);
    m_buttonTest->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnTest), NULL, this);
    
    m_cbSerialPort->Connect(wxEVT_TEXT, wxCommandEventHandler(ComPortsDlg::OnHamlibSerialPortChanged), NULL, this);
    m_cbPttMethod->Connect(wxEVT_TEXT, wxCommandEventHandler(ComPortsDlg::OnHamlibPttMethodChanged), NULL, this);
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
    m_cbSerialPort->Disconnect(wxEVT_TEXT, wxCommandEventHandler(ComPortsDlg::OnHamlibSerialPortChanged), NULL, this);
    m_cbPttMethod->Disconnect(wxEVT_TEXT, wxCommandEventHandler(ComPortsDlg::OnHamlibPttMethodChanged), NULL, this);
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
#if defined(__FreeBSD__)
    if(glob("/dev/tty*", GLOB_MARK, NULL, &gl)==0 ||
#else
    if(glob("/dev/tty.*", GLOB_MARK, NULL, &gl)==0 ||
#endif // defined(__FreeBSD__)
       glob("/dev/cu.*", GLOB_MARK, NULL, &gl)==0) {
        for(unsigned int i=0; i<gl.gl_pathc; i++) {
            if(gl.gl_pathv[i][strlen(gl.gl_pathv[i])-1]=='/')
                continue;

#if defined(__FreeBSD__)                 
            /* Exclude pseudo TTYs */
            if(gl.gl_pathv[i][8] >= 'l' && gl.gl_pathv[i][8] <= 's')
                continue;
            if(gl.gl_pathv[i][8] >= 'L' && gl.gl_pathv[i][8] <= 'S')
                continue;

            /* Exclude virtual TTYs */
            if(gl.gl_pathv[i][8] == 'v')
                continue;
#else
            if (!strcmp("/dev/cu.wlan-debug", gl.gl_pathv[i]))
                continue;
#endif // defined(__FreeBSD__) 

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

    // Support /dev/serial as well
    if (glob("/dev/serial/by-id/*", GLOB_MARK, NULL, &gl) == 0)
    {
        for(unsigned int i=0; i<gl.gl_pathc; i++) 
        {
            wxString path = gl.gl_pathv[i];
            portList.push_back(path);
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
        m_cbPttSerialPort->Append(port);
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
        m_ckLeftChannelVoxTone->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.leftChannelVoxTone);

        /* Hamlib */

        m_ckUseHamlibPTT->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseForPTT);
        m_cbRigName->SetSelection(wxGetApp().m_intHamlibRig);
        resetIcomCIVStatus();
        m_cbSerialPort->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.hamlibSerialPort);
        m_cbPttSerialPort->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.hamlibPttSerialPort);

        if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibSerialRate == 0) {
            m_cbSerialRate->SetSelection(0);
        } else {
            m_cbSerialRate->SetValue(wxString::Format(wxT("%i"), wxGetApp().appConfiguration.rigControlConfiguration.hamlibSerialRate.get()));
        }

        m_tcIcomCIVHex->SetValue(wxString::Format(wxT("%02X"), wxGetApp().appConfiguration.rigControlConfiguration.hamlibIcomCIVAddress.get()));
        
        m_cbPttMethod->SetSelection((int)wxGetApp().appConfiguration.rigControlConfiguration.hamlibPTTType);
        
        /* Serial PTT */

        m_ckUseSerialPTT->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.useSerialPTT);
        str = wxGetApp().appConfiguration.rigControlConfiguration.serialPTTPort;
        m_cbCtlDevicePath->SetValue(str);
        m_rbUseRTS->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.serialPTTUseRTS);
        m_ckRTSPos->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.serialPTTPolarityRTS);
        m_rbUseDTR->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.serialPTTUseDTR);
        m_ckDTRPos->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.serialPTTPolarityDTR);
        
        /* PTT Input */
        m_ckUsePTTInput->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.useSerialPTTInput);
        str = wxGetApp().appConfiguration.rigControlConfiguration.serialPTTInputPort;
        m_cbCtlDevicePathPttIn->SetValue(str);
        m_ckCTSPos->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.serialPTTInputPolarityCTS);

#if defined(WIN32)
        /* OmniRig */
        m_ckUseOmniRig->SetValue(wxGetApp().appConfiguration.rigControlConfiguration.useOmniRig);
        m_cbOmniRigRigId->SetSelection(wxGetApp().appConfiguration.rigControlConfiguration.omniRigRigId);
#endif // defined(WIN32)
        
        updateControlState();
    }

    if (inout == EXCHANGE_DATA_OUT) {
        wxGetApp().appConfiguration.rigControlConfiguration.leftChannelVoxTone = m_ckLeftChannelVoxTone->GetValue();

        /* Hamlib settings. */

        wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseForPTT = m_ckUseHamlibPTT->GetValue();
        wxGetApp().m_intHamlibRig = m_cbRigName->GetSelection();
        
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibRigName = HamlibRigController::RigIndexToName(wxGetApp().m_intHamlibRig);
        
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibSerialPort = m_cbSerialPort->GetValue();
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibPttSerialPort = m_cbPttSerialPort->GetValue();
        
        wxString s = m_tcIcomCIVHex->GetValue();
        long hexAddress = 0;
        m_tcIcomCIVHex->GetValue().ToLong(&hexAddress, 16);
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibIcomCIVAddress = hexAddress;
        
        s = m_cbSerialRate->GetValue();
        if (s == "default") {
            wxGetApp().appConfiguration.rigControlConfiguration.hamlibSerialRate = 0;
        } else {
            long tmp;
            m_cbSerialRate->GetValue().ToLong(&tmp); 
            wxGetApp().appConfiguration.rigControlConfiguration.hamlibSerialRate = tmp;
        }
        log_debug("serial rate: %d", wxGetApp().appConfiguration.rigControlConfiguration.hamlibSerialRate.get());

        wxGetApp().appConfiguration.rigControlConfiguration.hamlibPTTType = m_cbPttMethod->GetSelection();
        
        /* Serial settings */

        wxGetApp().appConfiguration.rigControlConfiguration.useSerialPTT           = m_ckUseSerialPTT->IsChecked();
        wxGetApp().appConfiguration.rigControlConfiguration.serialPTTPort             = m_cbCtlDevicePath->GetValue();
        wxGetApp().appConfiguration.rigControlConfiguration.serialPTTUseRTS                 = m_rbUseRTS->GetValue();
        wxGetApp().appConfiguration.rigControlConfiguration.serialPTTPolarityRTS                 = m_ckRTSPos->IsChecked();
        wxGetApp().appConfiguration.rigControlConfiguration.serialPTTUseDTR                 = m_rbUseDTR->GetValue();
        wxGetApp().appConfiguration.rigControlConfiguration.serialPTTPolarityDTR                 = m_ckDTRPos->IsChecked();

        wxGetApp().appConfiguration.rigControlConfiguration.useSerialPTTInput = m_ckUsePTTInput->IsChecked();
        wxGetApp().appConfiguration.rigControlConfiguration.serialPTTInputPort = m_cbCtlDevicePathPttIn->GetValue();
        wxGetApp().appConfiguration.rigControlConfiguration.serialPTTInputPolarityCTS = m_ckCTSPos->IsChecked();
        
#if defined(WIN32)
        /* OmniRig */
        wxGetApp().appConfiguration.rigControlConfiguration.useOmniRig = m_ckUseOmniRig->GetValue();
        wxGetApp().appConfiguration.rigControlConfiguration.omniRigRigId = m_cbOmniRigRigId->GetCurrentSelection();
#endif // defined(WIN32)
        
        wxGetApp().appConfiguration.save(pConfig);
    }
}

//-------------------------------------------------------------------------
// PTTUseHamLibClicked()
//-------------------------------------------------------------------------
void ComPortsDlg::PTTUseHamLibClicked(wxCommandEvent& event)
{
    m_ckUseSerialPTT->SetValue(false);
    
#if defined(WIN32)
    m_ckUseOmniRig->SetValue(false);
#endif // defined(WIN32)
    
    updateControlState();
}


/* Attempt to toggle PTT for 1 second */

void ComPortsDlg::OnTest(wxCommandEvent& event) {

    std::shared_ptr<std::mutex> mtx = std::make_shared<std::mutex>();
    std::shared_ptr<std::condition_variable> cv = std::make_shared<std::condition_variable>();
    
    /* Tone PTT */

    if (m_ckLeftChannelVoxTone->GetValue()) {
        wxMessageBox("Testing of tone based PTT not supported; try PTT after pressing Start on main window", 
                     wxT("Error"), wxOK | wxICON_ERROR, this);
    }

    isTesting_ = true;
    updateControlState();
    
    /* Hamlib PTT */

    if (m_ckUseHamlibPTT->GetValue()) {

        // set up current hamlib config from GUI

        int rig = m_cbRigName->GetSelection();
        wxString port = m_cbSerialPort->GetValue();
        wxString pttPort = m_cbPttSerialPort->GetValue();
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
        
        auto pttType = (HamlibRigController::PttType)m_cbPttMethod->GetSelection();

        // display serial params

        log_debug("serial rate: %d", serial_rate);

        if (wxGetApp().CanAccessSerialPort((const char*)port.ToUTF8()))
        {
            // try to open rig
            std::shared_ptr<HamlibRigController> hamlib = 
                std::make_shared<HamlibRigController>(
                    rig, (const char*)port.mb_str(wxConvUTF8), serial_rate, hexAddress, pttType,
                    (pttType == HamlibRigController::PTT_VIA_CAT) ? (const char*)port.mb_str(wxConvUTF8) : (const char*)pttPort.mb_str(wxConvUTF8) );

            hamlib->onRigError += [=, this](IRigController*, std::string error) {
                CallAfter([=, this]() {
                    wxMessageBox("Couldn't connect to Radio with Hamlib.  Make sure the Hamlib serial Device, Rate, and Params match your radio", 
                        wxT("Error"), wxOK | wxICON_ERROR, this);

                    cv->notify_one();
                });
            };

            hamlib->onRigConnected += [=, this](IRigController*) {
                hamlib->ptt(true);
            };

            hamlib->onPttChange += [=, this](IRigController*, bool state) {
                if (state)
                {
                    std::this_thread::sleep_for(1s);
                    hamlib->ptt(false);
                }
                else
                {
                    cv->notify_one();
                }
            };

            std::thread hamlibThread([=, this]() {
                hamlib->connect();
            
                std::unique_lock<std::mutex> lk(*mtx);
                cv->wait(lk);
            
                hamlib->disconnect();
                
                CallAfter([this]() {
                    isTesting_ = false;
                    updateControlState();
                });
            });
            hamlibThread.detach();
        }
        else
        {
            isTesting_ = false;
            updateControlState();
        }
    }
    else if (m_ckUseSerialPTT->IsChecked()) 
    {
        /* Serial PTT */       
        wxString ctrlport;
        ctrlport = m_cbCtlDevicePath->GetValue();
        log_debug("opening serial port: ");
        fputs(ctrlport.c_str(), stderr);            // don't escape crazy Microsoft bakslash-ified comm port names
        log_debug("\n");
    
        if (wxGetApp().CanAccessSerialPort((const char*)ctrlport.ToUTF8()))
        {
            std::shared_ptr<SerialPortOutRigController> serialPort = std::make_shared<SerialPortOutRigController>(
                (const char*)ctrlport.c_str(),
                m_rbUseRTS->GetValue(),
                m_ckRTSPos->IsChecked(),
                m_rbUseDTR->GetValue(),
                m_ckDTRPos->IsChecked());

            serialPort->onRigError += [=, this](IRigController*, std::string error)
            {
                CallAfter([=, this]() {
                    wxString errorMessage = "Couldn't open serial port " + ctrlport + ". This is likely due to not having permission to access the chosen port.";
                    wxMessageBox(errorMessage, wxT("Error"), wxOK | wxICON_ERROR, this);
                });

                cv->notify_one();
            };

            serialPort->onRigConnected += [=, this](IRigController*) {
                log_debug("serial port open");
                cv->notify_one();
            };

            std::thread serialPortThread([=, this]() {
                serialPort->connect();
            
                std::unique_lock<std::mutex> lk(*mtx);
                cv->wait(lk);

                if (serialPort->isConnected())
                {
                    // assert PTT port for 1 sec
                    serialPort->ptt(true);
                    wxSleep(1);
                    serialPort->ptt(false);

                    log_debug("closing serial port");
                    serialPort->disconnect();
                    log_debug("serial port closed");
                }
                
                CallAfter([this]() {
                    isTesting_ = false;
                    updateControlState();
                });
            });
            serialPortThread.detach();
        }
        else
        {
            isTesting_ = false;
            updateControlState();
        }
    }
    
#if defined(WIN32)
    else if (m_ckUseOmniRig->IsChecked())
    {
        // try to open rig
        std::shared_ptr<OmniRigController> rig = 
            std::make_shared<OmniRigController>(
                m_cbOmniRigRigId->GetCurrentSelection());

        rig->onRigError += [=, this](IRigController*, std::string error) {
            CallAfter([=, this]() {
                wxMessageBox("Couldn't connect to Radio with OmniRig.  Make sure the rig ID and OmniRig configuration is correct.", 
                    wxT("Error"), wxOK | wxICON_ERROR, this);

                cv->notify_one();
            });
        };

        rig->onRigConnected += [=, this](IRigController*) {
            rig->ptt(true);
        };

        rig->onPttChange += [=, this](IRigController*, bool state) {
            if (state)
            {
                std::this_thread::sleep_for(1s);
                rig->ptt(false);
            }
            else
            {
                cv->notify_one();
            }
        };

        std::thread omniRigThread([=, this]() {
            rig->connect();

            std::unique_lock<std::mutex> lk(*mtx);
            cv->wait(lk);
        
            rig->disconnect();
            
            CallAfter([this]() {
                isTesting_ = false;
                updateControlState();
            });
        });
    }  
#endif // defined(WIN32)
}

#if defined(WIN32)
//-------------------------------------------------------------------------
// PTTUseOmniRigClicked()
//-------------------------------------------------------------------------
void ComPortsDlg::PTTUseOmniRigClicked(wxCommandEvent& event)
{
    m_ckUseHamlibPTT->SetValue(false);
    updateControlState();
}
#endif // defined(WIN32)

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
    m_ckLeftChannelVoxTone->Enable(!isTesting_);
    m_ckUseHamlibPTT->Enable(!isTesting_);
    m_ckUseSerialPTT->Enable(!isTesting_);
    m_ckUsePTTInput->Enable(!isTesting_);
#if defined(WIN32)
    m_ckUseOmniRig->Enable(!isTesting_);
#endif // defined(WIN32)
    
    m_cbRigName->Enable(!isTesting_ && m_ckUseHamlibPTT->GetValue());
    m_cbSerialPort->Enable(!isTesting_ && m_ckUseHamlibPTT->GetValue());
    m_cbSerialRate->Enable(!isTesting_ && m_ckUseHamlibPTT->GetValue());
    m_tcIcomCIVHex->Enable(!isTesting_ && m_ckUseHamlibPTT->GetValue());
    m_cbPttMethod->Enable(!isTesting_ && m_ckUseHamlibPTT->GetValue());
    m_cbPttSerialPort->Enable(!isTesting_ && m_ckUseHamlibPTT->GetValue());

    m_cbCtlDevicePath->Enable(!isTesting_ && m_ckUseSerialPTT->GetValue());
    m_rbUseDTR->Enable(!isTesting_ && m_ckUseSerialPTT->GetValue());
    m_ckRTSPos->Enable(!isTesting_ && m_ckUseSerialPTT->GetValue());
    m_rbUseRTS->Enable(!isTesting_ && m_ckUseSerialPTT->GetValue());
    m_ckDTRPos->Enable(!isTesting_ && m_ckUseSerialPTT->GetValue());
    
    m_cbCtlDevicePathPttIn->Enable(!isTesting_ && m_ckUsePTTInput->GetValue());
    m_ckCTSPos->Enable(!isTesting_ && m_ckUsePTTInput->GetValue());
    
#if defined(WIN32)
    m_cbOmniRigRigId->Enable(!isTesting_ && m_ckUseOmniRig->GetValue());
#endif // defined(WIN32)
    
    m_buttonTest->Enable(!isTesting_ && (m_ckUseHamlibPTT->GetValue() || m_ckUseSerialPTT->GetValue()
#if defined(WIN32)
         || m_ckUseOmniRig->GetValue()
#endif // defined(WIN32)
    ));
    m_buttonOK->Enable(!isTesting_);
    m_buttonCancel->Enable(!isTesting_);
    m_buttonApply->Enable(!isTesting_);
    
    if (m_cbPttMethod->GetValue() == _("CAT") || m_cbPttMethod->GetValue() == _("None"))
    {
        m_cbPttSerialPort->Enable(false);
    }
}

void ComPortsDlg::OnHamlibSerialPortChanged(wxCommandEvent& event)
{
    if (m_cbPttMethod->GetValue() == _("CAT"))
    {
        // Make sure PTT serial port matches CAT port
        m_cbPttSerialPort->SetValue(m_cbSerialPort->GetValue());
    }
}

void ComPortsDlg::OnHamlibPttMethodChanged(wxCommandEvent& event)
{
    updateControlState();
}
