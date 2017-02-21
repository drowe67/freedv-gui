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
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(600,630), 
               long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );
        ~OptionsDlg();

        void    ExchangeData(int inout, bool storePersistent);
        void    updateEventLog(wxString event_in, wxString event_out);

        bool    enableEventsChecked() {return m_ckbox_events->GetValue();}

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
        void    OnFreeDV700txClip(wxScrollEvent& event);
        void    OnFreeDV700scatterCombine(wxScrollEvent& event);

        wxTextCtrl   *m_txtCtrlCallSign; // TODO: this should be renamed to tx_txtmsg, and rename all related incl persis strge
        wxCheckBox   *m_ckboxTestFrame;
        wxCheckBox   *m_ckboxChannelNoise;
        wxTextCtrl   *m_txtNoiseSNR;
        wxCheckBox   *m_ckboxFreeDV700txClip;
        wxCheckBox   *m_ckboxFreeDV700scatterCombine;

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

        wxButton*     m_sdbSizer5OK;
        wxButton*     m_sdbSizer5Cancel;
        wxButton*     m_sdbSizer5Apply;

        unsigned int  event_in_serial, event_out_serial;

     private:
};

#endif // __OPTIONS_DIALOG__
