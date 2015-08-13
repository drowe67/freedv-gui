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
#include "fdmdv2_main.h"

#ifdef __WIN32__
#include <wx/msw/registry.h>
#endif
#ifdef __FreeBSD__
#include <glob.h>
#include <string.h>
#endif

#include <sstream>

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class ComPortsDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
ComPortsDlg::ComPortsDlg(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxDialog(parent, id, title, pos, size, style)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(mainSizer);
    
    //----------------------------------------------------------------------
    // Half Duplex Flag for VOX PTT
    //----------------------------------------------------------------------

    // DR: should this be on options dialog?

    wxStaticBoxSizer* staticBoxSizer28 = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("VOX PTT Settings")), wxHORIZONTAL);
    m_ckHalfDuplex = new wxCheckBox(this, wxID_ANY, _("Half Duplex"), wxDefaultPosition, wxSize(-1,-1), 0);
    staticBoxSizer28->Add(m_ckHalfDuplex, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
    m_ckLeftChannelVoxTone = new wxCheckBox(this, wxID_ANY, _("Left Channel Vox Tone"), wxDefaultPosition, wxSize(-1,-1), 0);
    staticBoxSizer28->Add(m_ckLeftChannelVoxTone, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);

    mainSizer->Add(staticBoxSizer28, 0, wxEXPAND, 5);

    //----------------------------------------------------------------------
    // Hamlib for CAT PTT
    //----------------------------------------------------------------------

    wxStaticBoxSizer* staticBoxSizer18 = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("Hamlib Settings")), wxVERTICAL);

    wxBoxSizer* gridSizer100 = new wxBoxSizer(wxHORIZONTAL);

    /* Use Hamlib for PTT checkbox. */
    m_ckUseHamlibPTT = new wxCheckBox(this, wxID_ANY, _("Use Hamlib PTT"), wxDefaultPosition, wxSize(-1, -1), 0);
    m_ckUseHamlibPTT->SetValue(false);
    gridSizer100->Add(m_ckUseHamlibPTT, 0, wxALIGN_CENTER_VERTICAL, 0);

    /* Hamlib Rig Type combobox. */
    gridSizer100->Add(new wxStaticText(this, wxID_ANY, _("Rig Model:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL | wxLEFT, 20);
    m_cbRigName = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(250, -1), 0, NULL, wxCB_DROPDOWN);
    /* TODO(Joel): this is a hack. At the least, need to gurantee that m_hamLib
     * exists. */
    wxGetApp().m_hamlib->populateComboBox(m_cbRigName);
    m_cbRigName->SetSelection(wxGetApp().m_intHamlibRig);
    gridSizer100->Add(m_cbRigName, 0, wxALIGN_CENTER_VERTICAL, 0);

    /* Hamlib Serial Port combobox. */
    gridSizer100->Add(new wxStaticText(this, wxID_ANY, _("Serial Device:"), wxDefaultPosition, wxDefaultSize, 0), 
                      0, wxALIGN_CENTER_VERTICAL | wxLEFT, 20);
    m_cbSerialPort = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(140, -1), 0, NULL, wxCB_DROPDOWN);
    gridSizer100->Add(m_cbSerialPort, 0, wxALIGN_CENTER_VERTICAL, 0);

    staticBoxSizer18->Add(gridSizer100, 1);
    mainSizer->Add(staticBoxSizer18, 1);

    //----------------------------------------------------------------------
    // Serial port PTT
    //----------------------------------------------------------------------

    wxStaticBoxSizer* staticBoxSizer17 = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("Serial Port Settings")), wxVERTICAL);
    mainSizer->Add(staticBoxSizer17, 1, wxEXPAND, 5);
    wxStaticBoxSizer* staticBoxSizer31 = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("PTT Port")), wxVERTICAL);
    staticBoxSizer17->Add(staticBoxSizer31, 1, wxEXPAND, 5);

#ifdef __WXMSW__
    m_ckUseSerialPTT = new wxCheckBox(this, wxID_ANY, _("Use Serial Port PTT"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_ckUseSerialPTT->SetValue(false);
    staticBoxSizer31->Add(m_ckUseSerialPTT, 0, wxALIGN_LEFT, 20);

    wxArrayString m_listCtrlPortsArr;
    m_listCtrlPorts = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1,45), m_listCtrlPortsArr, wxLB_SINGLE | wxLB_SORT);
    staticBoxSizer31->Add(m_listCtrlPorts, 1, wxALIGN_CENTER, 0);
#endif

#ifdef __WXGTK__
    wxBoxSizer* bSizer83;
    bSizer83 = new wxBoxSizer(wxHORIZONTAL);

    wxGridSizer* gridSizer200 = new wxGridSizer(1, 3, 0, 0);
    
    m_ckUseSerialPTT = new wxCheckBox(this, wxID_ANY, _("Use Serial Port PTT"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_ckUseSerialPTT->SetValue(false);
    gridSizer200->Add(m_ckUseSerialPTT, 1, wxALIGN_CENTER|wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 2);

    m_staticText12 = new wxStaticText(this, wxID_ANY, _("Serial Device:  "), wxDefaultPosition, wxDefaultSize, 0);
    m_staticText12->Wrap(-1);
    gridSizer200->Add(m_staticText12, 1,wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 2);

    m_cbCtlDevicePath = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(140, -1), 0, NULL, wxCB_DROPDOWN);
    gridSizer200->Add(m_cbCtlDevicePath, 1, wxEXPAND|wxALIGN_CENTER|wxALIGN_RIGHT, 2);
    
    bSizer83->Add(gridSizer200, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxALL, 2);
    staticBoxSizer31->Add(bSizer83, 1, wxALIGN_CENTER_VERTICAL|wxALL, 1);
#endif

    wxBoxSizer* boxSizer19 = new wxBoxSizer(wxVERTICAL);
    staticBoxSizer17->Add(boxSizer19, 1, wxEXPAND, 5);
    wxStaticBoxSizer* staticBoxSizer16 = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("Signal polarity")), wxHORIZONTAL);
    boxSizer19->Add(staticBoxSizer16, 1, wxEXPAND|wxALIGN_CENTER|wxALIGN_RIGHT, 5);

    wxGridSizer* gridSizer17 = new wxGridSizer(2, 2, 0, 0);
    staticBoxSizer16->Add(gridSizer17, 1, wxEXPAND|wxALIGN_RIGHT, 5);

    m_rbUseDTR = new wxRadioButton(this, wxID_ANY, _("Use DTR"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_rbUseDTR->SetToolTip(_("Toggle DTR line for PTT"));
    m_rbUseDTR->SetValue(1);
    gridSizer17->Add(m_rbUseDTR, 0, wxALIGN_CENTER|wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5);

    m_ckDTRPos = new wxCheckBox(this, wxID_ANY, _("DTR = +V"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_ckDTRPos->SetToolTip(_("Set Polarity of the DTR line"));
    m_ckDTRPos->SetValue(false);
    gridSizer17->Add(m_ckDTRPos, 0, wxALIGN_CENTER|wxALIGN_RIGHT, 5);

    m_rbUseRTS = new wxRadioButton(this, wxID_ANY, _("Use RTS"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_rbUseRTS->SetToolTip(_("Toggle the RTS pin for PTT"));
    m_rbUseRTS->SetValue(1);
    gridSizer17->Add(m_rbUseRTS, 0, wxALIGN_CENTER|wxALIGN_RIGHT, 5);

    m_ckRTSPos = new wxCheckBox(this, wxID_ANY, _("RTS = +V"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_ckRTSPos->SetValue(false);
    m_ckRTSPos->SetToolTip(_("Set Polarity of the RTS line"));
    gridSizer17->Add(m_ckRTSPos, 0, wxALIGN_CENTER|wxALIGN_RIGHT, 5);

    //----------------------------------------------------------------------
    // OK - Cancel - Apply
    //----------------------------------------------------------------------

    wxBoxSizer* boxSizer12 = new wxBoxSizer(wxHORIZONTAL);

    m_buttonOK = new wxButton(this, wxID_OK, _("OK"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_buttonOK->SetDefault();
    boxSizer12->Add(m_buttonOK, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);

    m_buttonCancel = new wxButton(this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize(-1,-1), 0);
    boxSizer12->Add(m_buttonCancel, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);

    m_buttonApply = new wxButton(this, wxID_APPLY, _("Apply"), wxDefaultPosition, wxSize(-1,-1), 0);
    boxSizer12->Add(m_buttonApply, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);

    mainSizer->Add(boxSizer12, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM|wxALIGN_CENTER_HORIZONTAL, 5);

    if ( GetSizer() ) 
    {
         GetSizer()->Fit(this);
    }
    Centre(wxBOTH);

    // Connect events
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(ComPortsDlg::OnInitDialog), NULL, this);
    m_ckUseHamlibPTT->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ComPortsDlg::PTTUseHamLibClicked), NULL, this);
    m_ckUseSerialPTT->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ComPortsDlg::PTTUseSerialClicked), NULL, this);
    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnOK), NULL, this);
    m_buttonCancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnCancel), NULL, this);
    m_buttonApply->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnApply), NULL, this);
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
    m_buttonOK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnOK), NULL, this);
    m_buttonCancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnCancel), NULL, this);
    m_buttonApply->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ComPortsDlg::OnApply), NULL, this);
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
#ifdef __WXMSW__
    m_listCtrlPorts->Clear();
    m_cbSerialPort->Clear();
    wxArrayString aStr;
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
            aStr.Add(key_data, 1);
            key.GetNextValue(key_name, el);
        }
    }
    m_listCtrlPorts->Append(aStr);
    m_cbSerialPort->Append(aStr);
#endif
#ifdef __WXGTK__
    m_cbSerialPort->Clear();
    m_cbCtlDevicePath->Clear();
#ifdef __FreeBSD__
	glob_t	gl;
	if(glob("/dev/tty*", GLOB_MARK, NULL, &gl)==0) {
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
			if(strchr(gl.gl_pathv[i], '.') != NULL)
				continue;

			m_cbSerialPort->Append(gl.gl_pathv[i]);
			m_cbCtlDevicePath->Append(gl.gl_pathv[i]);
		}
		globfree(&gl);
	}
#else
    /* TODO(Joel): http://stackoverflow.com/questions/2530096/how-to-find-all-serial-devices-ttys-ttyusb-on-linux-without-opening-them */
    m_cbSerialPort->Append("/dev/ttyUSB0");
    m_cbSerialPort->Append("/dev/ttyUSB1");
    m_cbSerialPort->Append("/dev/ttyS0");
    m_cbSerialPort->Append("/dev/ttyS1");
    m_cbCtlDevicePath->Clear();
    m_cbCtlDevicePath->Append("/dev/ttyUSB0");
    m_cbCtlDevicePath->Append("/dev/ttyUSB1");
    m_cbCtlDevicePath->Append("/dev/ttyS0");
    m_cbCtlDevicePath->Append("/dev/ttyS1");
#endif
#endif
}

//-------------------------------------------------------------------------
// ExchangeData()
//-------------------------------------------------------------------------
void ComPortsDlg::ExchangeData(int inout)
{
    wxConfigBase *pConfig = wxConfigBase::Get();
    wxString str;
    
    if(inout == EXCHANGE_DATA_IN)
    {
        m_ckHalfDuplex->SetValue(wxGetApp().m_boolHalfDuplex);
        m_ckLeftChannelVoxTone->SetValue(wxGetApp().m_leftChannelVoxTone);

        m_ckUseHamlibPTT->SetValue(wxGetApp().m_boolHamlibUseForPTT);
        m_cbRigName->SetSelection(wxGetApp().m_intHamlibRig);
        m_cbSerialPort->SetValue(wxGetApp().m_strHamlibSerialPort);

        m_ckUseSerialPTT->SetValue(wxGetApp().m_boolUseSerialPTT);
        str = wxGetApp().m_strRigCtrlPort;
#ifdef __WXMSW__
        m_listCtrlPorts->SetStringSelection(str);
#endif
#ifdef __WXGTK__
        m_cbCtlDevicePath->SetValue(str);
#endif
        m_rbUseRTS->SetValue(wxGetApp().m_boolUseRTS);
        m_ckRTSPos->SetValue(wxGetApp().m_boolRTSPos);
        m_rbUseDTR->SetValue(wxGetApp().m_boolUseDTR);
        m_ckDTRPos->SetValue(wxGetApp().m_boolDTRPos);
    }
    if(inout == EXCHANGE_DATA_OUT)
    {
        wxGetApp().m_boolHalfDuplex = m_ckHalfDuplex->GetValue();
        pConfig->Write(wxT("/Rig/HalfDuplex"), wxGetApp().m_boolHalfDuplex);
        wxGetApp().m_leftChannelVoxTone = m_ckLeftChannelVoxTone->GetValue();
        pConfig->Write(wxT("/Rig/leftChannelVoxTone"), wxGetApp().m_leftChannelVoxTone);

        /* Hamlib settings. */

        wxGetApp().m_boolHamlibUseForPTT = m_ckUseHamlibPTT->GetValue();
        wxGetApp().m_intHamlibRig = m_cbRigName->GetSelection();
        wxGetApp().m_strHamlibSerialPort = m_cbSerialPort->GetValue();

        pConfig->Write(wxT("/Hamlib/UseForPTT"), wxGetApp().m_boolHamlibUseForPTT);
        pConfig->Write(wxT("/Hamlib/RigName"), wxGetApp().m_intHamlibRig);
        pConfig->Write(wxT("/Hamlib/SerialPort"), wxGetApp().m_strHamlibSerialPort);

        /* Serial settings */

        wxGetApp().m_boolUseSerialPTT           = m_ckUseSerialPTT->IsChecked();
#ifdef __WXMSW__
        wxGetApp().m_strRigCtrlPort             = m_listCtrlPorts->GetStringSelection();
#endif
#ifdef __WXGTK__
        wxGetApp().m_strRigCtrlPort             = m_cbCtlDevicePath->GetValue();
#endif
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

        pConfig->Flush();
    }
    delete wxConfigBase::Set((wxConfigBase *) NULL);
}

//-------------------------------------------------------------------------
// PTTUseHamLibClicked()
//-------------------------------------------------------------------------
void ComPortsDlg::PTTUseHamLibClicked(wxCommandEvent& event)
{
    m_ckUseSerialPTT->SetValue(false);    
}

//-------------------------------------------------------------------------
// PTTUseSerialClicked()
//-------------------------------------------------------------------------
void ComPortsDlg::PTTUseSerialClicked(wxCommandEvent& event)
{
    m_ckUseHamlibPTT->SetValue(false);
}

//-------------------------------------------------------------------------
// OnApply()
//-------------------------------------------------------------------------
void ComPortsDlg::OnApply(wxCommandEvent& event)
{
    ExchangeData(EXCHANGE_DATA_OUT);
}

//-------------------------------------------------------------------------
// OnCancel()
//-------------------------------------------------------------------------
void ComPortsDlg::OnCancel(wxCommandEvent& event)
{
    this->EndModal(wxID_CANCEL);
}

//-------------------------------------------------------------------------
// OnClose()
//-------------------------------------------------------------------------
void ComPortsDlg::OnOK(wxCommandEvent& event)
{
    ExchangeData(EXCHANGE_DATA_OUT);
    this->EndModal(wxID_OK);
}
