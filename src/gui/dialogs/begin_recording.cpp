//==========================================================================
// Name:            begin_recording.cpp
// Purpose:         Dialog for setting up recordings.
// Created:         January 4, 2026
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

#include <wx/wx.h>
#include "begin_recording.h"

BeginRecordingDialog::BeginRecordingDialog(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) 
    : wxDialog(parent, id, title, pos, size, style)
{    
    // XXX - FreeDV only supports English but makes a best effort to at least use regional formatting
    // for e.g. numbers. Thus, we only need to override layout direction.
    SetLayoutDirection(wxLayout_LeftToRight);
       
    // Create top-level of control hierarchy.
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* sectionSizer = new wxBoxSizer(wxVERTICAL);
    
    wxStaticBox* recordingSettingsBox = new wxStaticBox(panel, wxID_ANY, _("Recording Settings"));
    wxStaticBoxSizer* recordingSettingsBoxSizer = new wxStaticBoxSizer(recordingSettingsBox, wxVERTICAL);

    wxFlexGridSizer* gridSizerRecordingSettings = new wxFlexGridSizer(3, 2, 5, 5);
    gridSizerRecordingSettings->AddGrowableCol(1);

    // Recording suffix
    wxStaticText* labelRecordingSuffix = new wxStaticText(recordingSettingsBox, wxID_ANY, wxT("Recording suffix:"), wxDefaultPosition, wxSize(125,-1), 0);
    gridSizerRecordingSettings->Add(labelRecordingSuffix, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 2);

    recordingSuffix_ = new wxTextCtrl(recordingSettingsBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(125, -1), 0);
    gridSizerRecordingSettings->Add(recordingSuffix_, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND, 2);

    wxStaticText* labelRecordingType = new wxStaticText(recordingSettingsBox, wxID_ANY, wxT("Recording type:"), wxDefaultPosition, wxSize(125,-1), 0);
    gridSizerRecordingSettings->Add(labelRecordingType, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 2);

    wxBoxSizer* typeSizer = new wxBoxSizer(wxHORIZONTAL);
    rawRecording_ = new wxRadioButton(recordingSettingsBox, wxID_ANY, _("Off Air"));
    rawRecording_->SetValue(true);
    typeSizer->Add(rawRecording_, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    decodedRecording_ = new wxRadioButton(recordingSettingsBox, wxID_ANY, _("Decoded"));
    typeSizer->Add(decodedRecording_, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    gridSizerRecordingSettings->Add(typeSizer, 0, wxALIGN_CENTER_VERTICAL, 2);

    wxStaticText* labelRecordingFormat = new wxStaticText(recordingSettingsBox, wxID_ANY, wxT("Recording format:"), wxDefaultPosition, wxSize(125,-1), 0);
    gridSizerRecordingSettings->Add(labelRecordingFormat, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT, 2);

    wxBoxSizer* formatSizer = new wxBoxSizer(wxHORIZONTAL);
    formatWav_ = new wxRadioButton(recordingSettingsBox, wxID_ANY, _("WAV"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    formatWav_->SetValue(true);
    formatSizer->Add(formatWav_, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    formatMp3_ = new wxRadioButton(recordingSettingsBox, wxID_ANY, _("MP3"));
    formatSizer->Add(formatMp3_, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
    gridSizerRecordingSettings->Add(formatSizer, 0, wxALIGN_CENTER_VERTICAL, 2);

    recordingSettingsBoxSizer->Add(gridSizerRecordingSettings, 0, wxEXPAND | wxALIGN_LEFT, 2);
    sectionSizer->Add(recordingSettingsBoxSizer, 0, wxALL | wxEXPAND, 2);

    // OK/Cancel buttons
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    m_buttonOK = new wxButton(panel, wxID_ANY, _("Start"));
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
    this->Connect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(BeginRecordingDialog::OnInitDialog));
    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(BeginRecordingDialog::OnClose));
       
    m_buttonOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(BeginRecordingDialog::OnOK), NULL, this);
    m_buttonCancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(BeginRecordingDialog::OnCancel), NULL, this);
}

BeginRecordingDialog::~BeginRecordingDialog()
{
    this->Disconnect(wxEVT_INIT_DIALOG, wxInitDialogEventHandler(BeginRecordingDialog::OnInitDialog));
    this->Disconnect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(BeginRecordingDialog::OnClose));
       
    m_buttonOK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(BeginRecordingDialog::OnOK), NULL, this);
    m_buttonCancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(BeginRecordingDialog::OnCancel), NULL, this);
}

void BeginRecordingDialog::OnInitDialog(wxInitDialogEvent&)
{
    // empty
}

void BeginRecordingDialog::OnCancel(wxCommandEvent&)
{
    this->EndModal(wxCANCEL);
}

void BeginRecordingDialog::OnClose(wxCloseEvent&)
{
    this->EndModal(wxCANCEL);
}

void BeginRecordingDialog::OnOK(wxCommandEvent&)
{
    this->EndModal(wxOK);
}