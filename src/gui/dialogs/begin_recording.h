//==========================================================================
// Name:            begin_recording.h
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

#ifndef BEGIN_RECORDING_DIALOG_H
#define BEGIN_RECORDING_DIALOG_H

#include "main.h"
#include "defines.h"
#include "../../logging/ILogger.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class BeginRecordingDialog
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class BeginRecordingDialog : public wxDialog
{
    public:
        BeginRecordingDialog( wxWindow* parent, wxString const& defaultRecordingSuffix );
        virtual ~BeginRecordingDialog();

        bool isRawRecording() const { return rawRecording_->GetValue(); }
        wxString getRecordingSuffix() const { return recordingSuffix_->GetValue(); }
        bool isMp3Format() const { return formatMp3_->GetValue(); }
        
    protected:

        // Handlers for events.
        void    OnOK(wxCommandEvent& event);
        void    OnCancel(wxCommandEvent& event);
        void    OnClose(wxCloseEvent& event);
        void    OnInitDialog(wxInitDialogEvent& event);
        
        void OnRecordingTypeChange(wxCommandEvent& event);
       
        wxTextCtrl *recordingSuffix_;
        wxRadioButton *rawRecording_;
        wxRadioButton *decodedRecording_;
        wxRadioButton *formatWav_;
        wxRadioButton *formatMp3_;

        wxButton* m_buttonOK;
        wxButton* m_buttonCancel;
};

#endif // BEGIN_RECORDING_DIALOG_H
