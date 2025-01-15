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

#include <wx/clrpicker.h>

#include "main.h"
#include "defines.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class OptionsDlg
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class OptionsDlg : public wxDialog
{
    public:
    OptionsDlg( wxWindow* parent,
                wxWindowID id = wxID_ANY, const wxString& title = _("Options"), 
                const wxPoint& pos = wxDefaultPosition, 
                const wxSize& size = wxDefaultSize, 
                long style = wxDEFAULT_DIALOG_STYLE|wxTAB_TRAVERSAL );
        ~OptionsDlg();

        void    ExchangeData(int inout, bool storePersistent);

        void DisplayFifoPACounters();
        
        void setSessionActive(bool active) 
        {
            sessionActive_ = active; 
        
            updateReportingState();
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
        
        void    OnReportingEnable(wxCommandEvent& event);
        void    OnToneStateEnable(wxCommandEvent& event);
        void    OnMultipleRxEnable(wxCommandEvent& event);
        void    OnFreqModeChangeEnable(wxCommandEvent& event);
        
        wxTextCtrl   *m_txtCtrlCallSign; // TODO: this should be renamed to tx_txtmsg, and rename all related incl persis strge

        wxCheckBox* m_ckHalfDuplex;

        wxNotebook  *m_notebook;
        wxNotebookPage *m_reportingTab; // txt msg/PSK Reporter
        wxNotebookPage *m_rigControlTab; // Rig Control
        wxNotebookPage *m_displayTab; // Waterfall color, other display config
        wxNotebookPage *m_keyerTab; // Voice Keyer
        wxNotebookPage *m_modemTab; // 700/OFDM/duplex
        wxNotebookPage *m_simulationTab; // testing/interference
        wxNotebookPage *m_debugTab; // Debug
        
        /* Hamlib options */
        wxCheckBox    *m_ckboxUseAnalogModes;
        wxRadioButton *m_ckboxEnableFreqModeChanges;
        wxRadioButton *m_ckboxEnableFreqChangesOnly;
        wxRadioButton *m_ckboxNoFreqModeChanges;
        wxCheckBox    *m_ckboxEnableSpacebarForPTT;
        wxTextCtrl    *m_txtTxRxDelayMilliseconds;
        wxCheckBox    *m_ckboxFrequencyEntryAsKHz;
        
        /* Waterfall color */
        wxRadioButton *m_waterfallColorScheme1; // Multicolored
        wxRadioButton *m_waterfallColorScheme2; // Black & white
        wxRadioButton *m_waterfallColorScheme3; // Blue tint?

        /* FreeDV Reporter colors */
        wxColourPickerCtrl* m_freedvReporterTxBackgroundColor;
        wxColourPickerCtrl* m_freedvReporterTxForegroundColor;
        wxColourPickerCtrl* m_freedvReporterRxBackgroundColor;
        wxColourPickerCtrl* m_freedvReporterRxForegroundColor;
        wxColourPickerCtrl* m_freedvReporterMsgBackgroundColor;
        wxColourPickerCtrl* m_freedvReporterMsgForegroundColor;

        /* Voice Keyer */

        wxButton     *m_buttonChooseVoiceKeyerWaveFilePath;
        wxTextCtrl   *m_txtCtrlVoiceKeyerWaveFilePath;
        wxTextCtrl   *m_txtCtrlVoiceKeyerRxPause;
        wxTextCtrl   *m_txtCtrlVoiceKeyerRepeats;

        /* Quick Record */
        wxButton     *m_buttonChooseQuickRecordPath;
        wxTextCtrl   *m_txtCtrlQuickRecordPath;
        
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
        wxCheckBox   *m_ckboxFreeDV700txBPF;
        wxCheckBox   *m_ckboxFreeDV700Combine;

        wxRadioButton *m_rb_textEncoding1;
        wxRadioButton *m_rb_textEncoding2;

        wxCheckBox    *m_ckboxReportingEnable;
        wxTextCtrl    *m_txt_callsign;
        wxTextCtrl    *m_txt_grid_square;
        
        wxCheckBox    *m_ckboxManualFrequencyReporting;
        
        wxCheckBox    *m_ckboxPskReporterEnable;
        
        wxCheckBox    *m_ckboxFreeDVReporterEnable;
        wxTextCtrl    *m_freedvReporterHostname;
        wxCheckBox    *m_useMetricDistances;
        wxCheckBox    *m_useCardinalDirections;
        wxCheckBox    *m_ckboxFreeDVReporterForceReceiveOnly;
        
        wxButton*     m_BtnFifoReset;
        wxStaticText  *m_textFifos;
        wxStaticText  *m_textPA1;
        wxStaticText  *m_textPA2;
        wxTextCtrl    *m_txtCtrlFifoSize;
        wxCheckBox    *m_ckboxTxRxThreadPriority;
        wxCheckBox    *m_ckboxTxRxDumpTiming;
        wxCheckBox    *m_ckboxTxRxDumpFifoState;
        wxCheckBox    *m_ckboxVerbose;
        wxCheckBox    *m_ckboxFreeDVAPIVerbose;
        wxCheckBox    *m_experimentalFeatures;
        
        wxButton*     m_sdbSizer5OK;
        wxButton*     m_sdbSizer5Cancel;
        wxButton*     m_sdbSizer5Apply;

        wxCheckBox   *m_ckboxDebugConsole;

        wxCheckBox*  m_ckboxMultipleRx;
        wxCheckBox*  m_ckboxSingleRxThread;
        wxTextCtrl*  m_statsResetTime;
        
        wxCheckBox*  m_ckbox_use_utc_time;
        
        wxListBox*  m_freqList;
        wxStaticText* m_labelEnterFreq;
        wxTextCtrl* m_txtCtrlNewFrequency;
        wxButton*   m_freqListAdd;
        wxButton*   m_freqListRemove;
        wxButton*   m_freqListMoveUp;
        wxButton*   m_freqListMoveDown;
        
        unsigned int  event_in_serial, event_out_serial;

        void OnChooseVoiceKeyerWaveFilePath(wxCommandEvent& event);
        void OnChooseQuickRecordPath(wxCommandEvent& event);
        
        void OnReportingFreqSelectionChange(wxCommandEvent& event);
        void OnReportingFreqTextChange(wxCommandEvent& event);
        void OnReportingFreqAdd(wxCommandEvent& event);
        void OnReportingFreqRemove(wxCommandEvent& event);
        void OnReportingFreqMoveUp(wxCommandEvent& event);
        void OnReportingFreqMoveDown(wxCommandEvent& event);
        
     private:
         void updateReportingState();
         void updateChannelNoiseState();
         void updateAttnCarrierState();
         void updateToneState();
         void updateMultipleRxState();
         void updateRigControlState();
         
         bool sessionActive_;
};

#endif // __OPTIONS_DIALOG__
