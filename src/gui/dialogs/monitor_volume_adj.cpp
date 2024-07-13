//==========================================================================
// Name:            monitor_volume_adj.cpp
// Purpose:         Popup to allow adjustment of monitor volume
// Created:         Jul 12, 2024
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

#include <wx/sizer.h>
#include "monitor_volume_adj.h"

MontiorVolumeAdjPopup::MontiorVolumeAdjPopup( wxWindow* parent, ConfigurationDataElement<float>& configVal )
    : wxPopupTransientWindow(parent)
    , configVal_(configVal)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* controlLabel = new wxStaticText(this, wxID_ANY, wxT("Monitor volume (dB):"), wxDefaultPosition, wxDefaultSize, 0);
    mainSizer->Add(controlLabel, 0, wxALL | wxEXPAND, 2);
    
    volumeSlider_ = new wxSlider(this, wxID_ANY, configVal, -40, 0, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS | wxSL_LABELS);
    mainSizer->Add(volumeSlider_, 0, wxALL | wxEXPAND, 2);
    
    SetSizerAndFit(mainSizer);
    Layout();
    
    // Link event handlers
    volumeSlider_->Connect(wxEVT_SLIDER, wxCommandEventHandler(MontiorVolumeAdjPopup::OnSliderAdjusted), NULL, this);
    
    // Make popup show up to the left of (and above) mouse cursor position
    wxPoint pt = wxGetMousePosition();
    wxSize sz = GetSize();
    pt -= wxPoint(sz.GetWidth(), sz.GetHeight());
    SetPosition( pt );
}

MontiorVolumeAdjPopup::~MontiorVolumeAdjPopup()
{
    // TBD
}

void MontiorVolumeAdjPopup::OnSliderAdjusted(wxCommandEvent& event)
{
    configVal_ = volumeSlider_->GetValue();
}