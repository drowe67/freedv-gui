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
#include "audio/IAudioEngine.h"
#include "rig_control/HamlibRigController.h"
#include "rig_control/SerialPortOutRigController.h"

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
        wxComboBox* m_analogDevicePlayback;
        wxComboBox* m_analogDeviceRecord;
        
        // Step 2: CAT setup
        wxRadioButton *m_ckNoPTT;
        wxRadioButton *m_ckUseHamlibPTT;
        wxRadioButton *m_ckUseSerialPTT;

        // Step 2a: Hamlib CAT Control
        wxStaticBox* m_hamlibBox;
        wxComboBox *m_cbRigName;
        wxComboBox *m_cbSerialPort;
        wxComboBox *m_cbSerialRate;
        wxStaticText  *m_cbSerialParams;
        wxStaticText *m_stIcomCIVHex;
        wxTextCtrl *m_tcIcomCIVHex;
        wxComboBox *m_cbPttMethod;

        // Step 2b: Serial PTT
        wxStaticBox* m_serialBox;
        wxComboBox    *m_cbCtlDevicePath;
        wxRadioButton *m_rbUseDTR;
        wxCheckBox    *m_ckRTSPos;
        wxRadioButton *m_rbUseRTS;
        wxCheckBox    *m_ckDTRPos;

        // Step 2c: advanced/test options
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
             SoundDeviceData() 
                 : rxDeviceName("none")
                 , txDeviceName("none")
                 , rxSampleRate(44100)
                 , txSampleRate(44100)
             {
                 // empty
             } 
             
             wxString rxDeviceName;
             wxString txDeviceName;
             int rxSampleRate;
             int txSampleRate;
         };
         
         void updateAudioDevices_();
         void updateHamlibDevices_();
         void resetIcomCIVStatus_();
         bool canTestRadioSettings_();
         bool canSaveSettings_();
         void updateHamlibSerialRates_(int min = 0, int max = 0);
         
         std::shared_ptr<HamlibRigController> hamlibTestObject_;
         std::shared_ptr<SerialPortOutRigController> serialPortTestObject_;
         int sineWaveSampleNumber_;
         bool hasAppliedChanges_;

         std::shared_ptr<IAudioDevice> txTestAudioDevice_;
};

#endif // __EASY_SETUP_DIALOG__
