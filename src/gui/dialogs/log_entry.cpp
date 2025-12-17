//==========================================================================
// Name:            log_entry.cpp
// Purpose:         Dialog for confirming log entry.
// Created:         Dec 17, 2025
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

#include "../../main.h"
#include <wx/wx.h>
#include <wx/valnum.h>
#include "log_entry.h"

LogEntryDialog::LogEntryDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) 
    : wxDialog(parent, id, title, pos, size, style)
{    
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
       
    // Create top-level of control hierarchy.
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* sectionSizer = new wxBoxSizer(wxVERTICAL);
    
    wxStaticBox* logEntryBox = new wxStaticBox(panel, wxID_ANY, _("Log Entry"));
    wxStaticBoxSizer* logEntryBoxSizer = new wxStaticBoxSizer(logEntryBox, wxVERTICAL);

    wxFlexGridSizer* gridSizerLogEntry = new wxFlexGridSizer(5, 2, 5, 0);

    // Log entry fields
    wxStaticText* labelMyCall = new wxStaticText(logEntryBox, wxID_ANY, wxT("Your Call:"), wxDefaultPosition, wxSize(125,-1), 0);
    gridSizerLogEntry->Add(labelMyCall, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 2);

    wxStaticText* labelMyCallVal = new wxStaticText(logEntryBox, wxID_ANY, wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign, wxDefaultPosition, wxDefaultSize, 0);
    gridSizerLogEntry->Add(labelMyCallVal, 0, wxALIGN_CENTER_VERTICAL, 2);

    wxStaticText* labelMyLocator = new wxStaticText(logEntryBox, wxID_ANY, wxT("Your Locator:"), wxDefaultPosition, wxSize(125,-1), 0);
    gridSizerLogEntry->Add(labelMyLocator, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 2);

    wxStaticText* labelMyLocatorVal = new wxStaticText(logEntryBox, wxID_ANY, wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare, wxDefaultPosition, wxDefaultSize, 0);
    gridSizerLogEntry->Add(labelMyLocatorVal, 0, wxALIGN_CENTER_VERTICAL, 2);

    wxStaticText* labelDxCall = new wxStaticText(logEntryBox, wxID_ANY, wxT("DX Call:"), wxDefaultPosition, wxSize(125,-1), 0);
    gridSizerLogEntry->Add(labelDxCall, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 2);

    dxCall_ = new wxTextCtrl(logEntryBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1), 0);
    gridSizerLogEntry->Add(dxCall_, 0, wxALIGN_CENTER_VERTICAL, 2);

    wxStaticText* labelDxGrid = new wxStaticText(logEntryBox, wxID_ANY, wxT("DX Locator:"), wxDefaultPosition, wxSize(125,-1), 0);
    gridSizerLogEntry->Add(labelDxGrid, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 2);

    dxGrid_ = new wxTextCtrl(logEntryBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1), 0);
    gridSizerLogEntry->Add(dxGrid_, 0, wxALIGN_CENTER_VERTICAL, 2);

    wxStaticText* labelFrequency = new wxStaticText(logEntryBox, wxID_ANY, wxT("Frequency (Hz):"), wxDefaultPosition, wxSize(125,-1), 0);
    gridSizerLogEntry->Add(labelFrequency, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 2);

    frequency_ = new wxTextCtrl(logEntryBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1), 0);
    gridSizerLogEntry->Add(frequency_, 0, wxALIGN_CENTER_VERTICAL, 2);

    logEntryBoxSizer->Add(gridSizerLogEntry, 0, wxEXPAND | wxALIGN_LEFT, 2);
    sectionSizer->Add(logEntryBoxSizer, 0, wxALL | wxEXPAND, 2);
    
    // OK/Cancel buttons
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    m_buttonOK = new wxButton(panel, wxID_OK);
    buttonSizer->Add(m_buttonOK, 0, wxALL, 2);

    m_buttonCancel = new wxButton(panel, wxID_CANCEL);
    buttonSizer->Add(m_buttonCancel, 0, wxALL, 2);

    sectionSizer->Add(buttonSizer, 0, wxALL | wxALIGN_CENTER, 2);
    
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
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(LogEntryDialog::OnInitDialog));
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LogEntryDialog::OnClose));
       
    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LogEntryDialog::OnOK), NULL, this);
    m_buttonCancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LogEntryDialog::OnCancel), NULL, this);
}

LogEntryDialog::~LogEntryDialog()
{
    logger_ = nullptr;

    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(LogEntryDialog::OnInitDialog));
    this->Disconnect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LogEntryDialog::OnClose));
       
    m_buttonOK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LogEntryDialog::OnOK), NULL, this);
    m_buttonCancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(LogEntryDialog::OnCancel), NULL, this);
}

void LogEntryDialog::OnInitDialog(wxInitDialogEvent&)
{
    // empty
}

void LogEntryDialog::ShowDialog(wxString const& dxCall, wxString const& dxGrid, int64_t freqHz)
{
    logger_ = wxGetApp().logger;
    dxCall_->SetValue(dxCall);
    dxGrid_->SetValue(dxGrid);

    wxString freqString = wxNumberFormatter::ToString(freqHz);
    frequency_->SetValue(freqString);

    ShowModal();
}

void LogEntryDialog::OnOK(wxCommandEvent&)
{
    int64_t freqHz = 0;

    if (frequency_->GetValue() == "" ||
        dxCall_->GetValue() == "")
    {
        wxMessageBox( _("Frequency and DX Call must be filled in in order to log the contact."), _("Error"), wxICON_ERROR);
    }
    else
    {
        // Forward to logger
        wxNumberFormatter::FromString(frequency_->GetValue(), &freqHz);

        logger_->logContact(
            (const char*)dxCall_->GetValue().ToUTF8(), 
            (const char*)dxGrid_->GetValue().ToUTF8(),
            (const char*)wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign->ToUTF8(),
            (const char*)wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare->ToUTF8(),
            freqHz);
        this->EndModal(wxOK);
    }
}

void LogEntryDialog::OnCancel(wxCommandEvent&)
{
    this->EndModal(wxCANCEL);
}

void LogEntryDialog::OnClose(wxCloseEvent&)
{
    this->EndModal(wxCANCEL);
}
