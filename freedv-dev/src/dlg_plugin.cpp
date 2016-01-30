//==========================================================================
// Name:            dlg_plugin.cpp
// Purpose:         Subclasses dialog GUI for PlugIn Config. Creates simple 
//                  wxWidgets dialog GUI to set a few text strings.
// Date:            Jan 2016
// Authors:         David Rowe
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

#include "dlg_plugin.h"
#include "fdmdv2_main.h"

#ifdef __WIN32__
#include <wx/msw/registry.h>
#endif
#if defined(__FreeBSD__) || defined(__WXOSX__)
#include <glob.h>
#include <string.h>
#endif

#include <sstream>

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class PlugInDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
PlugInDlg::PlugInDlg(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxDialog(parent, id, title, pos, size, style)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(mainSizer);

    wxStaticBoxSizer* staticBoxSizer28a = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("Param1")), wxVERTICAL);
    m_txtCtrlPlugIn1 = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300,-1), 0);
    staticBoxSizer28a->Add(m_txtCtrlPlugIn1, 0, 0, 5);
    mainSizer->Add(staticBoxSizer28a, 0, wxEXPAND, 5);

    wxStaticBoxSizer* staticBoxSizer28b = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("Param2")), wxVERTICAL);
    m_txtCtrlPlugIn2 = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300,-1), 0);
    staticBoxSizer28b->Add(m_txtCtrlPlugIn2, 0, 0, 5);
    mainSizer->Add(staticBoxSizer28b, 0, wxEXPAND, 5);

    //----------------------------------------------------------------------
    // OK - Cancel - Apply
    //----------------------------------------------------------------------

    wxBoxSizer* boxSizer12 = new wxBoxSizer(wxHORIZONTAL);

    m_buttonOK = new wxButton(this, wxID_OK, _("OK"), wxDefaultPosition, wxSize(-1,-1), 0);
    m_buttonOK->SetDefault();
    boxSizer12->Add(m_buttonOK, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);

    m_buttonCancel = new wxButton(this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize(-1,-1), 0);
    boxSizer12->Add(m_buttonCancel, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);

    mainSizer->Add(boxSizer12, 0, wxLEFT|wxRIGHT|wxTOP|wxBOTTOM|wxALIGN_CENTER_HORIZONTAL, 5);

    if ( GetSizer() ) 
    {
         GetSizer()->Fit(this);
    }
    Centre(wxBOTH);

    // Connect events
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(PlugInDlg::OnInitDialog), NULL, this);
    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(PlugInDlg::OnOK), NULL, this);
    m_buttonCancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(PlugInDlg::OnCancel), NULL, this);

}

//-------------------------------------------------------------------------
// ~PlugInDlg()
//-------------------------------------------------------------------------
PlugInDlg::~PlugInDlg()
{
    // Disconnect Events
    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(PlugInDlg::OnInitDialog), NULL, this);
    m_buttonOK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(PlugInDlg::OnOK), NULL, this);
    m_buttonCancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(PlugInDlg::OnCancel), NULL, this);
}

//-------------------------------------------------------------------------
// OnInitDialog()
//-------------------------------------------------------------------------
void PlugInDlg::OnInitDialog(wxInitDialogEvent& event)
{
    ExchangeData(EXCHANGE_DATA_IN);
}


//-------------------------------------------------------------------------
// ExchangeData()
//-------------------------------------------------------------------------
void PlugInDlg::ExchangeData(int inout)
{
    wxConfigBase *pConfig = wxConfigBase::Get();
    wxString str;
    
    if(inout == EXCHANGE_DATA_IN)
    {
        m_txtCtrlPlugIn1->SetValue(wxGetApp().m_txtPlugIn1);
        m_txtCtrlPlugIn2->SetValue(wxGetApp().m_txtPlugIn2);
    }
    if(inout == EXCHANGE_DATA_OUT)
    {
        wxGetApp().m_txtPlugIn1 = m_txtCtrlPlugIn1->GetValue();
        pConfig->Write(wxT("/PlugIn/txt1"), wxGetApp().m_txtPlugIn1);
        wxGetApp().m_txtPlugIn2 = m_txtCtrlPlugIn2->GetValue();
        pConfig->Write(wxT("/PlugIn/txt2"), wxGetApp().m_txtPlugIn2);
        pConfig->Flush();
    }
    delete wxConfigBase::Set((wxConfigBase *) NULL);
}


//-------------------------------------------------------------------------
// OnCancel()
//-------------------------------------------------------------------------
void PlugInDlg::OnCancel(wxCommandEvent& event)
{
    this->EndModal(wxID_CANCEL);
}

//-------------------------------------------------------------------------
// OnClose()
//-------------------------------------------------------------------------
void PlugInDlg::OnOK(wxCommandEvent& event)
{
    ExchangeData(EXCHANGE_DATA_OUT);
    this->EndModal(wxID_OK);
}
