//==========================================================================
// Name:            dlg_easy_setup.h
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

#ifndef __EASY_SETUP_DIALOG__
#define __EASY_SETUP_DIALOG__

#include "main.h"
#include "defines.h"
#include "portaudio.h"
#include "hamlib.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class EasySetupDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class EasySetupDialog : public wxDialog
{
    public:
        EasySetupDialog( wxWindow* parent,
                wxWindowID id = wxID_ANY, const wxString& title = _("Easy Setup"), 
                const wxPoint& pos = wxDefaultPosition, 
                const wxSize& size = wxDefaultSize, 
                long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxTAB_TRAVERSAL );
        ~EasySetupDialog();

        void    ExchangeData(int inout);
        
    protected:

        // Handlers for events.
        void    OnOK(wxCommandEvent& event);
        void    OnCancel(wxCommandEvent& event);
        void    OnApply(wxCommandEvent& event);
        void    OnTest(wxCommandEvent& event);
        void    OnClose(wxCloseEvent& event);
        void    OnInitDialog(wxInitDialogEvent& event);
        void    OnAdvancedSoundSetup(wxCommandEvent& event);
        void    OnAdvancedPTTSetup(wxCommandEvent& event);
        void    HamlibRigNameChanged(wxCommandEvent& event);
        void    PTTUseHamLibClicked(wxCommandEvent& event);
        void    OnPSKReporterChecked(wxCommandEvent& event);

        // Internal section-specific ExchangeData methods.
        void    ExchangeSoundDeviceData(int inout);
        void    ExchangePttDeviceData(int inout);
        void    ExchangeReportingData(int inout);
        
        // Step 1: sound device selection
        wxComboBox* m_radioDevice;
        wxButton* m_advancedSoundSetup;
        wxStaticText* m_analogDevicePlayback;
        wxStaticText* m_analogDeviceRecord;
        
        // Step 2: CAT setup
        wxCheckBox *m_ckUseHamlibPTT;
        wxComboBox *m_cbRigName;
        wxComboBox *m_cbSerialPort;
        wxComboBox *m_cbSerialRate;
        wxStaticText  *m_cbSerialParams;
        wxStaticText *m_stIcomCIVHex;
        wxTextCtrl *m_tcIcomCIVHex;
        wxButton* m_advancedPTTSetup;
        wxButton* m_buttonTest;
        
        // Step 3: PSK Reporter setup
        wxCheckBox    *m_ckbox_psk_enable;
        wxTextCtrl    *m_txt_callsign;
        wxTextCtrl    *m_txt_grid_square;
        
        // Step 4: test/save/cancel setup
        wxButton* m_buttonOK;
        wxButton* m_buttonCancel;
        wxButton* m_buttonApply;

     private:
         struct SoundDeviceData : public wxClientData
         {
             wxString deviceName;
             int deviceIndex;
         };
         
         static int OnPortAudioCallback_(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData);

         void updateAudioDevices_();
         void updateHamlibDevices_();
         void resetIcomCIVStatus();
         
         int analogDevicePlaybackDeviceId_;
         int analogDeviceRecordDeviceId_;
         Hamlib* hamlibTestObject_;
         PaStream* radioOutputStream_;
         int sineWaveSampleNumber_;
};

#endif // __EASY_SETUP_DIALOG__
