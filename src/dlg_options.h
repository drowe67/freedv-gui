//==========================================================================
// Name:            dlg_options.h
// Purpose:         Dialog for controlling misc FreeDV options
// Created:         Nov 25 2012
// Authors:         David Rowe, David Witten
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

#ifndef __OPTIONS_DIALOG__
#define __OPTIONS_DIALOG__

#include "fdmdv2_main.h"
#include "fdmdv2_defines.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class OptionsDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class OptionsDlg : public wxDialog
{
    public:
    OptionsDlg( wxWindow* parent,
               wxWindowID id = wxID_ANY, const wxString& title = _("Options"), 
                const wxPoint& pos = wxDefaultPosition, 
#ifdef __WXMSW__
                /* we add debug console check box for windows */
                const wxSize& size = wxSize(600,410), 
#else
                const wxSize& size = wxSize(600,380), 
#endif
               long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );
        ~OptionsDlg();

        void    ExchangeData(int inout, bool storePersistent);
        void    updateEventLog(wxString event_in, wxString event_out);

        bool    enableEventsChecked() {return m_ckbox_events->GetValue();}

        void DisplayFifoPACounters();
        
        void SetSpamTimerLight(bool state) {

            // Colours don't work on Windows

            if (state) {
                m_rb_spam_timer->SetForegroundColour( wxColour( 255,0 , 0 ) ); // red
                m_rb_spam_timer->SetValue(true);
            }
            else {
                m_rb_spam_timer->SetForegroundColour( wxColour( 0, 255, 0 ) ); // green
                m_rb_spam_timer->SetValue(false);
            }
        }


    protected:

        // Handlers for events.

        void    OnOK(wxCommandEvent& event);
        void    OnCancel(wxCommandEvent& event);
        void    OnApply(wxCommandEvent& event);
        void    OnClose(wxCloseEvent& event);
        void    OnInitDialog(wxInitDialogEvent& event);
 
        void    OnTestFrame(wxScrollEvent& event);
        void    OnChannelNoise(wxScrollEvent& event);
        void    OnAttnCarrierEn(wxScrollEvent& event);
        void    OnFreeDV700txClip(wxScrollEvent& event);
        void    OnFreeDV700Combine(wxScrollEvent& event);
        void    OnDebugConsole(wxScrollEvent& event);

        void    OnFifoReset(wxCommandEvent& event);
        
        wxTextCtrl   *m_txtCtrlCallSign; // TODO: this should be renamed to tx_txtmsg, and rename all related incl persis strge

        wxCheckBox* m_ckHalfDuplex;

        /* Voice Keyer */

        wxButton     *m_buttonChooseVoiceKeyerWaveFile;
        wxTextCtrl   *m_txtCtrlVoiceKeyerWaveFile;
        wxTextCtrl   *m_txtCtrlVoiceKeyerRxPause;
        wxTextCtrl   *m_txtCtrlVoiceKeyerRepeats;

        /* test frames, other simulated channel impairments */

        wxCheckBox   *m_ckboxTestFrame;
        wxCheckBox   *m_ckboxChannelNoise;
        wxTextCtrl   *m_txtNoiseSNR;
        wxCheckBox   *m_ckboxAttnCarrierEn;
        wxTextCtrl   *m_txtAttnCarrier;

        wxCheckBox   *m_ckboxTone;
        wxTextCtrl   *m_txtToneFreqHz;
        wxTextCtrl   *m_txtToneAmplitude;

        wxCheckBox   *m_ckboxFreeDV700txClip;
        wxCheckBox   *m_ckboxFreeDV700Combine;
        wxTextCtrl   *m_txtInterleave;
        wxCheckBox   *m_ckboxFreeDV700ManualUnSync;
        
        wxRadioButton *m_rb_textEncoding1;
        wxRadioButton *m_rb_textEncoding2;
        wxCheckBox    *m_ckboxEnableChecksum;

        wxCheckBox   *m_ckbox_events;
        wxTextCtrl   *m_txt_events_regexp_match;
        wxTextCtrl   *m_txt_events_regexp_replace;
        wxTextCtrl   *m_txt_events_in;
        wxTextCtrl   *m_txt_events_out;
        wxTextCtrl   *m_txt_spam_timer;
        wxRadioButton *m_rb_spam_timer;

        wxCheckBox   *m_ckbox_udp_enable;
        wxTextCtrl   *m_txt_udp_port;

        wxButton*     m_BtnFifoReset;
        wxStaticText  *m_textFifos;
        wxStaticText  *m_textPA1;
        wxStaticText  *m_textPA2;

        wxButton*     m_sdbSizer5OK;
        wxButton*     m_sdbSizer5Cancel;
        wxButton*     m_sdbSizer5Apply;

        wxCheckBox   *m_ckboxDebugConsole;

        unsigned int  event_in_serial, event_out_serial;

        void OnChooseVoiceKeyerWaveFile(wxCommandEvent& event);

     private:
};

#endif // __OPTIONS_DIALOG__
