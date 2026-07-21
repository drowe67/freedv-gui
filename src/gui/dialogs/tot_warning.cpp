//==========================================================================
// Name:            tot_warning.cpp
// Purpose:         Dialog warning the user of impending Time-Out Timer expiry.
// Created:         June 2026
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
#include "tot_warning.h"

TotWarningDialog::TotWarningDialog(wxWindow* parent, int initialRemainingMs,
                                   std::function<void()> onExtend)
    : wxDialog(parent, wxID_ANY, _("Time-Out Timer Warning"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP)
    , m_onExtend_(std::move(onExtend))
{
    SetLayoutDirection(wxLayout_LeftToRight);

    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* msgText = new wxStaticText(panel, wxID_ANY,
        _("Transmit time-out is imminent!"),
        wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
    topSizer->Add(msgText, 0, static_cast<int>(wxALL) | static_cast<int>(wxALIGN_CENTER_HORIZONTAL), 10);

    m_countdownText_ = new wxStaticText(panel, wxID_ANY, wxT(""),
        wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
    wxFont countdownFont = m_countdownText_->GetFont();
    countdownFont.SetPointSize(countdownFont.GetPointSize() * 2);
    countdownFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_countdownText_->SetFont(countdownFont);
    topSizer->Add(m_countdownText_, 0, static_cast<int>(wxALL) | static_cast<int>(wxALIGN_CENTER_HORIZONTAL), 5);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);

    m_extendBtn_ = new wxButton(panel, wxID_ANY, _("Reset Timer"));
    m_extendBtn_->SetToolTip(_("Resets the Time-Out Timer to extend transmission."));
    btnSizer->Add(m_extendBtn_, 0, static_cast<int>(wxALL), 5);

    topSizer->Add(btnSizer, 0, static_cast<int>(wxALL) | static_cast<int>(wxALIGN_CENTER_HORIZONTAL), 5);

    panel->SetSizer(topSizer);

    wxBoxSizer* panelSizer = new wxBoxSizer(wxVERTICAL);
    panelSizer->Add(panel, 0, wxEXPAND, 0);
    this->SetSizerAndFit(panelSizer);

    this->Layout();
    this->Centre(wxBOTH);
    this->SetMinSize(GetBestSize());

    updateRemainingTime(initialRemainingMs);

    m_extendBtn_->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
        wxCommandEventHandler(TotWarningDialog::OnExtend), nullptr, this);
}

TotWarningDialog::~TotWarningDialog()
{
    m_extendBtn_->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED,
        wxCommandEventHandler(TotWarningDialog::OnExtend), nullptr, this);
}

void TotWarningDialog::updateRemainingTime(int remainingMs)
{
    int sec = (remainingMs + 999) / 1000;
    if (sec < 0) sec = 0;
    m_countdownText_->SetLabel(wxString::Format(_("%d second%s remaining"),
        sec, sec == 1 ? wxT("") : wxT("s")));
    Layout();
}

void TotWarningDialog::OnExtend(wxCommandEvent&)
{
    m_onExtend_();
}
