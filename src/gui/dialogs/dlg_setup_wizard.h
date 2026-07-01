//==========================================================================
// Name:            dlg_setup_wizard.h
// Purpose:         First-run setup wizard dialog
// Authors:         Mooneer Salem
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.
//
//==========================================================================

#ifndef __DLG_SETUP_WIZARD_H__
#define __DLG_SETUP_WIZARD_H__

#include <wx/wx.h>
#include <wx/simplebook.h>
#include <wx/statline.h>
#include "audio/IAudioEngine.h"
#include "../../main.h"

class SetupWizard : public wxDialog
{
public:
    SetupWizard(wxWindow* parent);

private:
    static constexpr int NUM_PAGES = 4;

    wxSimplebook* m_book;
    int           m_currentPage;

    // Navigation buttons
    wxButton* m_btnPrev;
    wxButton* m_btnNext;
    wxButton* m_btnFinish;

    // Page 0: Receive Audio
    wxComboBox*   m_cbRadioIn;
    wxComboBox*   m_cbSpeakerOut;

    // Page 1: Transmit Audio
    wxCheckBox*   m_ckReceiveOnly;
    wxStaticText* m_stMicIn;
    wxComboBox*   m_cbMicIn;
    wxStaticText* m_stRadioOut;
    wxComboBox*   m_cbRadioOut;

    // Page 2: Radio Control — Hamlib section
    wxCheckBox*    m_ckHamlib;
    wxStaticText*  m_stRigName;
    wxComboBox*    m_cbRigName;
    wxStaticText*  m_stSerialPort;
    wxComboBox*    m_cbSerialPort;
    wxStaticText*  m_stSerialRate;
    wxComboBox*    m_cbSerialRate;
    wxStaticText*  m_stPttMethod;
    wxComboBox*    m_cbPttMethod;
    wxStaticText*  m_stPttSerialPort;
    wxComboBox*    m_cbPttSerialPort;
    // Serial PTT section
    wxCheckBox*    m_ckSerialPTT;
    wxStaticText*  m_stCtlDevice;
    wxComboBox*    m_cbCtlDevicePath;
    wxRadioButton* m_rbUseRTS;
    wxCheckBox*    m_ckRTSPos;
    wxRadioButton* m_rbUseDTR;
    wxCheckBox*    m_ckDTRPos;
#if defined(WIN32)
    // OmniRig section (Windows only)
    wxCheckBox*    m_ckOmniRig;
    wxStaticText*  m_stOmniRigId;
    wxComboBox*    m_cbOmniRigRigId;
#endif

    // Page 3: Reporting
    wxCheckBox*   m_ckReportingEnable;
    wxStaticText* m_stCallsign;
    wxTextCtrl*   m_txtCallsign;
    wxStaticText* m_stGridSquare;
    wxTextCtrl*   m_txtGridSquare;
    wxCheckBox*   m_ckFreeDVReporter;
    wxCheckBox*   m_ckPskReporter;

    // Page factories
    wxPanel* makeReceivePage();
    wxPanel* makeTxPage();
    wxPanel* makeRadioPage();
    wxPanel* makeReportingPage();

    // Helpers
    void populateAudioCombo(wxComboBox* combo, IAudioEngine::AudioDirection dir);
    void populateSerialPorts();
    void populateBaudRates(int rigIndex = -1);
    void loadConfig();
    void saveConfig();
    void updateNavButtons();
    void updateTxState();
    void updateRadioState();
    void updateReportingState();

    // Event handlers
    void OnNext(wxCommandEvent&);
    void OnPrev(wxCommandEvent&);
    void OnFinish(wxCommandEvent&);
    void OnReceiveOnlyChanged(wxCommandEvent&);
    void OnHamlibChanged(wxCommandEvent&);
    void OnSerialPTTChanged(wxCommandEvent&);
    void OnRigNameChanged(wxCommandEvent&);
    void OnReportingEnableChanged(wxCommandEvent&);
};

#endif // __DLG_SETUP_WIZARD_H__
