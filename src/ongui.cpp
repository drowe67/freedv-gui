/*
  ongui.cpp

  The simpler GUI event handlers.
*/

#include "main.h"
#include "lpcnet_freedv.h"

extern int g_mode;

extern int   g_SquelchActive;
extern float g_SquelchLevel;
extern int   g_analog;
extern int   g_tx;
extern int   g_State, g_prev_State;
extern FreeDVInterface freedvInterface;
extern bool g_queueResync;
extern short *g_error_hist, *g_error_histn;
extern int g_resyncs;
extern int g_Nc;
extern int g_txLevel;
extern wxConfigBase *pConfig;
extern bool endingTx;
extern int g_outfifo1_empty;
extern bool g_voice_keyer_tx;

extern SNDFILE            *g_sfRecFileFromModulator;
extern SNDFILE            *g_sfRecFile;
extern bool g_recFileFromModulator;
extern bool g_recFileFromRadio;

extern SNDFILE            *g_sfRecMicFile;
extern bool                g_recFileFromMic;

extern wxMutex g_mutexProtectingCallbackData;

//-------------------------------------------------------------------------
// Forces redraw of main panels on window resize.
//-------------------------------------------------------------------------
void MainFrame::topFrame_OnSize( wxSizeEvent& event )
{
    m_auiNbookCtrl->Refresh();
    TopFrame::topFrame_OnSize(event);
}

//-------------------------------------------------------------------------
// OnExitClick()
//-------------------------------------------------------------------------
void MainFrame::OnExitClick(wxCommandEvent& event)
{
    OnExit(event);
}

//-------------------------------------------------------------------------
// OnToolsEasySetup()
//-------------------------------------------------------------------------
void MainFrame::OnToolsEasySetup(wxCommandEvent& event)
{
    EasySetupDialog* dlg = new EasySetupDialog(this);
    dlg->ShowModal();
}

//-------------------------------------------------------------------------
// OnToolsEasySetupUI()
//-------------------------------------------------------------------------
void MainFrame::OnToolsEasySetupUI(wxUpdateUIEvent& event)
{
    event.Enable(!m_RxRunning);
}

//-------------------------------------------------------------------------
// OnToolsFreeDVReporter()
//-------------------------------------------------------------------------
void MainFrame::OnToolsFreeDVReporter(wxCommandEvent& event)
{
    if (m_reporterDialog == nullptr)
    {
        m_reporterDialog = new FreeDVReporterDialog(this);
    }

    m_reporterDialog->Show();
    m_reporterDialog->Iconize(false); // undo minimize if required
    m_reporterDialog->Raise(); // brings from background to foreground if required
}

//-------------------------------------------------------------------------
// OnToolsFreeDVReporterUI()
//-------------------------------------------------------------------------
void MainFrame::OnToolsFreeDVReporterUI(wxUpdateUIEvent& event)
{
    event.Enable(wxGetApp().appConfiguration.reportingConfiguration.freedvReporterHostname->ToStdString() != "");
}

//-------------------------------------------------------------------------
// OnToolsAudio()
//-------------------------------------------------------------------------
void MainFrame::OnToolsAudio(wxCommandEvent& event)
{
    wxUnusedVar(event);
    int rv = 0;
    AudioOptsDialog *dlg = new AudioOptsDialog(NULL);
    rv = dlg->ShowModal();
    if(rv == wxID_OK)
    {
        dlg->ExchangeData(EXCHANGE_DATA_OUT);
    }
    delete dlg;
}

//-------------------------------------------------------------------------
// OnToolsAudioUI()
//-------------------------------------------------------------------------
void MainFrame::OnToolsAudioUI(wxUpdateUIEvent& event)
{
    event.Enable(!m_RxRunning);
}

//-------------------------------------------------------------------------
// OnToolsFilter()
//-------------------------------------------------------------------------
void MainFrame::OnToolsFilter(wxCommandEvent& event)
{
    wxUnusedVar(event);
    
    if (m_filterDialog == nullptr)
    {
         m_filterDialog = new FilterDlg(NULL, m_RxRunning, &m_newMicInFilter, &m_newSpkOutFilter);
    }
    else
    {
        m_filterDialog->Iconize(false);
        m_filterDialog->SetFocus();
        m_filterDialog->Raise();
    }
    
    m_filterDialog->Show();
}

//-------------------------------------------------------------------------
// OnToolsOptions()
//-------------------------------------------------------------------------
void MainFrame::OnToolsOptions(wxCommandEvent& event)
{
    wxUnusedVar(event);
    if (optionsDlg->ShowModal() == wxOK)
    {
        // Update reporting list.
        updateReportingFreqList_();
    
        // Show/hide frequency box based on PSK Reporter status.
        m_freqBox->Show(wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled);

        // Show/hide callsign combo box based on PSK Reporter Status
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled)
        {
            m_cboLastReportedCallsigns->Show();
            m_txtCtrlCallSign->Hide();
        }
        else
        {
            m_cboLastReportedCallsigns->Hide();
            m_txtCtrlCallSign->Show();
        }

        // Relayout window so that the changes can take effect.
        m_panel->Layout();
    }
}

//-------------------------------------------------------------------------
// OnToolsOptionsUI()
//-------------------------------------------------------------------------
void MainFrame::OnToolsOptionsUI(wxUpdateUIEvent& event)
{
}

//-------------------------------------------------------------------------
// OnToolsComCfg()
//-------------------------------------------------------------------------
void MainFrame::OnToolsComCfg(wxCommandEvent& event)
{
    wxUnusedVar(event);

    ComPortsDlg *dlg = new ComPortsDlg(NULL);

    dlg->ShowModal();

    delete dlg;
}

//-------------------------------------------------------------------------
// OnToolsComCfgUI()
//-------------------------------------------------------------------------
void MainFrame::OnToolsComCfgUI(wxUpdateUIEvent& event)
{
    event.Enable(!m_RxRunning);
}

//-------------------------------------------------------------------------
// OnHelpCheckUpdates()
//-------------------------------------------------------------------------
void MainFrame::OnHelpCheckUpdates(wxCommandEvent& event)
{
    wxLaunchDefaultBrowser("https://github.com/drowe67/freedv-gui/releases");
}

//-------------------------------------------------------------------------
// OnHelpCheckUpdatesUI()
//-------------------------------------------------------------------------
void MainFrame::OnHelpCheckUpdatesUI(wxUpdateUIEvent& event)
{
    event.Enable(!m_RxRunning);
}

//-------------------------------------------------------------------------
// OnHelpManual()
//-------------------------------------------------------------------------
void MainFrame::OnHelpManual( wxCommandEvent& event )
{
    wxLaunchDefaultBrowser("https://github.com/drowe67/freedv-gui/blob/master/USER_MANUAL.pdf");
}

//-------------------------------------------------------------------------
//OnHelpAbout()
//-------------------------------------------------------------------------
void MainFrame::OnHelpAbout(wxCommandEvent& event)
{
    wxUnusedVar(event);
    wxString msg;
    msg.Printf( wxT("FreeDV GUI %s\n\n")
                wxT("For Help and Support visit: http://freedv.org\n\n")

                wxT("GNU Public License V2.1\n\n")
                wxT("Created by David Witten KD0EAG and David Rowe VK5DGR (2012).  ")
                wxT("Currently maintained by Mooneer Salem K6AQ and David Rowe VK5DGR.\n\n")
                wxT("freedv-gui version: %s\n")
                wxT("freedv-gui git hash: %s\n")
                wxT("codec2 git hash: %s\n")
                wxT("lpcnet git hash: %s\n"),
                FREEDV_VERSION, FREEDV_VERSION, GIT_HASH, freedv_get_hash(), lpcnet_get_hash());

    wxMessageBox(msg, wxT("About"), wxOK | wxICON_INFORMATION, this);
}


// Attempt to talk to rig using Hamlib

bool MainFrame::OpenHamlibRig() {
    if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseForPTT != true)
       return false;
    if (wxGetApp().m_intHamlibRig == 0)
        return false;
    if (wxGetApp().m_hamlib == NULL)
        return false;

    int rig = wxGetApp().m_intHamlibRig;
    wxString port = wxGetApp().appConfiguration.rigControlConfiguration.hamlibSerialPort;
    wxString pttPort = wxGetApp().appConfiguration.rigControlConfiguration.hamlibPttSerialPort;
    auto pttType = (Hamlib::PttType)wxGetApp().appConfiguration.rigControlConfiguration.hamlibPTTType.get();
    
    int serial_rate = wxGetApp().appConfiguration.rigControlConfiguration.hamlibSerialRate;
    if (wxGetApp().CanAccessSerialPort((const char*)port.ToUTF8()) && (
        pttType == Hamlib::PTT_VIA_CAT || wxGetApp().CanAccessSerialPort((const char*)pttPort.ToUTF8())))
    {
        bool status = wxGetApp().m_hamlib->connect(
            rig, port.mb_str(wxConvUTF8), serial_rate, wxGetApp().appConfiguration.rigControlConfiguration.hamlibIcomCIVAddress, 
            pttType, pttType == Hamlib::PTT_VIA_CAT ? port.mb_str(wxConvUTF8) : pttPort.mb_str(wxConvUTF8));
        if (status == false)
        {
            wxMessageBox("Couldn't connect to Radio with hamlib", wxT("Error"), wxOK | wxICON_ERROR, this);
        }
        else
        {
            wxGetApp().m_hamlib->readOnly(!wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges);
            if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges)
            {
                wxGetApp().m_hamlib->setFrequencyAndMode(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency, wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseAnalogModes ? true : g_analog);
            }
            wxGetApp().m_hamlib->enable_mode_detection(
                m_txtModeStatus, 
                wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled &&
                    wxGetApp().appConfiguration.reportingConfiguration.manualFrequencyReporting ? nullptr : m_cboReportFrequency, 
                false);
        }
    
        return status;
    }
    else
    {
        return false;
    }
}

//-------------------------------------------------------------------------
// OnCloseFrame()
//-------------------------------------------------------------------------
void MainFrame::OnCloseFrame(wxCloseEvent& event)
{
    //fprintf(stderr, "MainFrame::OnCloseFrame()\n");
    auto engine = AudioEngineFactory::GetAudioEngine();
    engine->stop();
    engine->setOnEngineError(nullptr, nullptr);
    
    Destroy();
}

//-------------------------------------------------------------------------
// OnTop()
//-------------------------------------------------------------------------
void MainFrame::OnTop(wxCommandEvent& event)
{
    int style = GetWindowStyle();

    if (style & wxSTAY_ON_TOP)
    {
        style &= ~wxSTAY_ON_TOP;
    }
    else
    {
        style |= wxSTAY_ON_TOP;
    }
    SetWindowStyle(style);
}

//-------------------------------------------------------------------------
// OnDeleteConfig()
//-------------------------------------------------------------------------
void MainFrame::OnDeleteConfig(wxCommandEvent&)
{
    if(pConfig->DeleteAll())
    {
        wxLogMessage(wxT("Config file/registry key successfully deleted."));
        
        // Resets all configuration to defaults.
        loadConfiguration_();
    }
    else
    {
        wxLogError(wxT("Deleting config file/registry key failed."));
    }
}

//-------------------------------------------------------------------------
// OnDeleteConfigUI()
//-------------------------------------------------------------------------
void MainFrame::OnDeleteConfigUI( wxUpdateUIEvent& event )
{
    event.Enable(!m_RxRunning);
}

//-------------------------------------------------------------------------
// Paint()
//-------------------------------------------------------------------------
void MainFrame::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

    if(GetMenuBar()->IsChecked(ID_PAINT_BG))
    {
        dc.Clear();
    }
    dc.SetUserScale(m_zoom, m_zoom);
}

//-------------------------------------------------------------------------
// OnCmdSliderScroll()
//-------------------------------------------------------------------------
void MainFrame::OnCmdSliderScroll(wxScrollEvent& event)
{
    char sqsnr[15];
    g_SquelchLevel = (float)m_sliderSQ->GetValue()/2.0 - 5.0;
    snprintf(sqsnr, 15, "%4.1f dB", g_SquelchLevel); // 0.5 dB steps
    wxString sqsnr_string(sqsnr);
    m_textSQ->SetLabel(sqsnr_string);

    event.Skip();
}

//-------------------------------------------------------------------------
// OnChangeTxLevel()
//-------------------------------------------------------------------------
void MainFrame::OnChangeTxLevel( wxScrollEvent& event )
{
    char fmt[15];
    g_txLevel = m_sliderTxLevel->GetValue();
    snprintf(fmt, 15, "%0.1f dB", (double)(g_txLevel)/10.0);
    wxString fmtString(fmt);
    m_txtTxLevelNum->SetLabel(fmtString);
    
    wxGetApp().appConfiguration.transmitLevel = g_txLevel;
}

//-------------------------------------------------------------------------
// OnCheckSQClick()
//-------------------------------------------------------------------------
void MainFrame::OnCheckSQClick(wxCommandEvent& event)
{
    if(!g_SquelchActive)
    {
        g_SquelchActive = true;
    }
    else
    {
        g_SquelchActive = false;
    }
}

void MainFrame::setsnrBeta(bool snrSlow)
{
    if(snrSlow)
    {
        m_snrBeta = 0.95; // make this closer to 1.0 to smooth SNR est further
    }
    else
    {
        m_snrBeta = 0.0; // no smoothing of SNR estimate from demodulator
    }
}

//-------------------------------------------------------------------------
// OnCheckSQClick()
//-------------------------------------------------------------------------
void MainFrame::OnCheckSNRClick(wxCommandEvent& event)
{
    wxGetApp().appConfiguration.snrSlow = m_ckboxSNR->GetValue();
    setsnrBeta(wxGetApp().appConfiguration.snrSlow);
    //printf("m_snrSlow: %d\n", (int)wxGetApp().appConfiguration.snrSlow);
}

// check for space bar press (only when running)

int MainApp::FilterEvent(wxEvent& event)
{
    if ((event.GetEventType() == wxEVT_KEY_DOWN) &&
        (((wxKeyEvent&)event).GetKeyCode() == WXK_SPACE))
        {
            // only use space to toggle PTT if we are running and no modal dialogs (like options) up
            if (frame->m_RxRunning && frame->IsActive() && wxGetApp().appConfiguration.enableSpaceBarForPTT) {

                // space bar controls rx/rx if keyer not running
                if (frame->vk_state == VK_IDLE) {
                    if (frame->m_btnTogPTT->GetValue())
                        frame->m_btnTogPTT->SetValue(false);
                    else
                        frame->m_btnTogPTT->SetValue(true);

                    frame->togglePTT();
                }
                else // space bar stops keyer
                    frame->VoiceKeyerProcessEvent(VK_SPACE_BAR);

                return true; // absorb space so we don't toggle control with focus (e.g. Start)

            }
        }

    return -1;
}

void MainFrame::OnSetMonitorTxAudio( wxCommandEvent& event )
{
    wxGetApp().appConfiguration.monitorTxAudio = event.IsChecked();
}

//-------------------------------------------------------------------------
// OnTogBtnPTTRightClick(): show right-click menu for PTT button
//-------------------------------------------------------------------------
void MainFrame::OnTogBtnPTTRightClick( wxContextMenuEvent& event )
{
    auto sz = m_btnTogPTT->GetSize();
    m_btnTogPTT->PopupMenu(pttPopupMenu_, wxPoint(-sz.GetWidth() - 25, 0));
}

//-------------------------------------------------------------------------
// OnTogBtnPTT ()
//-------------------------------------------------------------------------
void MainFrame::OnTogBtnPTT (wxCommandEvent& event)
{
    if (vk_state == VK_TX)
    {
        // Disable TX via VK code to prevent state inconsistencies.
        VoiceKeyerProcessEvent(VK_SPACE_BAR);
    }
    else
    {
        togglePTT();
    }
    event.Skip();
}

void MainFrame::togglePTT(void) {

    // Change tabbed page in centre panel depending on PTT state

    if (g_tx)
    {
        // Sleep for long enough that we get the remaining [blocksize] ms of audio.
        int msSleep = (1000 * freedvInterface.getTxNumSpeechSamples()) / freedvInterface.getTxSpeechSampleRate();
        if (g_verbose) fprintf(stderr, "Sleeping for %d ms prior to ending TX\n", msSleep);
        wxThread::Sleep(msSleep);
        
        // Trigger end of TX processing. This causes us to wait for the remaining samples
        // to flow through the system before toggling PTT.  Note 1000ms timeout as backup
        int sample = g_outfifo1_empty;
        endingTx = true;

        int i = 0;
        while ((i < 20) && (g_outfifo1_empty == sample)) {
            i++;
            wxThread::Sleep(50);
        }
        
        // tx-> rx transition, swap to the page we were on for last rx
        m_auiNbookCtrl->ChangeSelection(wxGetApp().appConfiguration.currentNotebookTab);
        
        // enable sync text

        m_textSync->Enable();
        m_textCurrentDecodeMode->Enable();
        
        // Re-enable buttons.
        m_togBtnOnOff->Enable(true);
        m_togBtnAnalog->Enable(true);
        m_togBtnVoiceKeyer->Enable(true);
    }
    else
    {
        // rx-> tx transition, swap to Mic In page to monitor speech
        wxGetApp().appConfiguration.currentNotebookTab = m_auiNbookCtrl->GetSelection();
        m_auiNbookCtrl->ChangeSelection(m_auiNbookCtrl->GetPageIndex((wxWindow *)m_panelSpeechIn));

        // disable sync text

        m_textSync->Disable();
        m_textCurrentDecodeMode->Disable();

        // Disable On/Off button.
        m_togBtnOnOff->Enable(false);
    }

    g_tx = m_btnTogPTT->GetValue();

    // Hamlib PTT

    if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseForPTT) {
        Hamlib *hamlib = wxGetApp().m_hamlib;
        wxString hamlibError;
        if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseForPTT && hamlib != NULL) {
            // Update mode display on the bottom of the main UI.
            if (hamlib->update_frequency_and_mode() != 0 || hamlib->ptt(g_tx, hamlibError) == false) {
                wxMessageBox(wxString("Hamlib PTT Error: ") + hamlibError, wxT("Error"), wxOK | wxICON_ERROR, this);
            }
        }
    }

    // Serial PTT

    if (wxGetApp().appConfiguration.rigControlConfiguration.useSerialPTT && (wxGetApp().m_serialport->isopen())) {
        wxGetApp().m_serialport->ptt(g_tx);
    }

    // reset level gauge

    m_maxLevel = 0;
    m_textLevel->SetLabel(wxT(""));
    m_gaugeLevel->SetValue(0);
    endingTx = false;
    
    // Report TX change to registered reporters
    for (auto& obj : wxGetApp().m_reporters)
    {
        obj->transmit(freedvInterface.getCurrentTxModeStr(), g_tx);
    }

    // Change button color depending on TX status.
    m_btnTogPTT->SetBackgroundColour(g_tx ? *wxRED : wxNullColour);
    
    // If we're recording, switch to/from modulator and radio.
    if (g_sfRecFile != nullptr)
    {
        if (!g_tx)
        {
            g_recFileFromModulator = false;
            g_recFileFromRadio = true;
        }
        else
        {
            g_recFileFromRadio = false;
            g_recFileFromModulator = true;
        }
    }
}

//-------------------------------------------------------------------------
// OnTogBtnAnalogClick()
//-------------------------------------------------------------------------
void MainFrame::OnTogBtnAnalogClick (wxCommandEvent& event)
{
    if (g_analog == 0) {
        g_analog = 1;
        m_panelSpectrum->setFreqScale(MODEM_STATS_NSPEC*((float)MAX_F_HZ/(FS/2)));
        m_panelWaterfall->setFs(FS);
        
        m_togBtnAnalog->SetLabel(wxT("Di&gital"));
    }
    else {
        g_analog = 0;
        m_panelSpectrum->setFreqScale(MODEM_STATS_NSPEC*((float)MAX_F_HZ/(freedvInterface.getRxModemSampleRate()/2)));
        m_panelWaterfall->setFs(freedvInterface.getRxModemSampleRate());
        
        m_togBtnAnalog->SetLabel(wxT("A&nalog"));
    }

    // Report analog change to registered reporters
    for (auto& obj : wxGetApp().m_reporters)
    {
        obj->inAnalogMode(g_analog);
    }
    
    if (wxGetApp().m_hamlib != nullptr && 
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0 &&
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges)
    {
        // Request mode change on the radio side
        wxGetApp().m_hamlib->setMode(
            wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseAnalogModes ? true : g_analog, 
            wxGetApp().m_hamlib->get_frequency());
    }

    g_State = g_prev_State = 0;
    freedvInterface.getCurrentRxModemStats()->snr_est = 0;

    event.Skip();
}

void MainFrame::OnCallSignReset(wxCommandEvent& event)
{
    m_pcallsign = m_callsign;
    memset(m_callsign, 0, MAX_CALLSIGN);
    wxString s;
    s.Printf("%s", m_callsign);
    m_txtCtrlCallSign->SetValue(s);
    
    m_lastReportedCallsignListView->DeleteAllItems();
    m_cboLastReportedCallsigns->SetText(_(""));
}


// Force manual resync, just in case demod gets stuck on false sync

void MainFrame::OnReSync(wxCommandEvent& event)
{
    if (m_RxRunning)  {
        if (g_verbose) fprintf(stderr,"OnReSync\n");
        
        // Resync must be triggered from the TX/RX thread, so pushing the button queues it until
        // the next execution of the TX/RX loop.
        g_queueResync = true;
    }
}

void MainFrame::resetStats_()
{
    if (m_RxRunning)  {
        freedvInterface.resetBitStats();
        g_resyncs = 0;
        int i;
        for(i=0; i<2*g_Nc; i++) {
            g_error_hist[i] = 0;
            g_error_histn[i] = 0;
        }
        // resets variance stats every time it is called
        freedvInterface.setEq(wxGetApp().appConfiguration.filterConfiguration.enable700CEqualizer);
    }
}

void MainFrame::OnBerReset(wxCommandEvent& event)
{
    resetStats_();
}

void MainFrame::OnChangeReportFrequencyVerify( wxCommandEvent& event )
{
    if (wxGetApp().m_hamlib != nullptr && wxGetApp().m_hamlib->isSuppressFrequencyModeUpdates())
    {
        return;
    }
    
    OnChangeReportFrequency(event);
}

void MainFrame::OnChangeReportFrequency( wxCommandEvent& event )
{    
    wxString freqStr = m_cboReportFrequency->GetValue();
    if (freqStr.Length() > 0)
    {
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency = atof(freqStr.ToUTF8()) * 1000 * 1000;
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0)
        {
            m_cboReportFrequency->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
        }
        else
        {
            m_cboReportFrequency->SetForegroundColour(wxColor(*wxRED));
        }
    }
    else
    {
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency = 0;
        m_cboReportFrequency->SetForegroundColour(wxColor(*wxRED));
    }
    
    // Report current frequency to reporters
    for (auto& ptr : wxGetApp().m_reporters)
    {
        ptr->freqChange(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
    }
    
    if (wxGetApp().m_hamlib != nullptr && 
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0 && 
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency != wxGetApp().m_hamlib->get_frequency() &&
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges)
    {
        // Request frequency/mode change on the radio side
        wxGetApp().m_hamlib->setFrequencyAndMode(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency, wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseAnalogModes ? true : g_analog);
    }
    
    if (m_reporterDialog != nullptr)
    {
        m_reporterDialog->refreshQSYButtonState();
    }
}

void MainFrame::OnReportFrequencySetFocus(wxFocusEvent& event)
{
    if (wxGetApp().m_hamlib != nullptr)
    {
        wxGetApp().m_hamlib->suppressFrequencyModeUpdates(true);
    }
    
    TopFrame::OnReportFrequencySetFocus(event);
}

void MainFrame::OnReportFrequencyKillFocus(wxFocusEvent& event)
{
    // Handle any frequency changes as appropriate.
    wxCommandEvent tmpEvent;
    OnChangeReportFrequency(tmpEvent);
    
    if (wxGetApp().m_hamlib != nullptr)
    {
        wxGetApp().m_hamlib->suppressFrequencyModeUpdates(false);
    }
    
    TopFrame::OnReportFrequencyKillFocus(event);
}

void MainFrame::OnSystemColorChanged(wxSysColourChangedEvent& event)
{
    // Works around issues on wxWidgets with certain controls not changing backgrounds
    // when the user switches between light and dark mode.
    wxColour currentControlBackground = wxTransparentColour;

    m_collpane->SetBackgroundColour(currentControlBackground);
    m_collpane->GetPane()->SetBackgroundColour(currentControlBackground);
    TopFrame::OnSystemColorChanged(event);
}

void MainFrame::updateReportingFreqList_()
{
    uint64_t prevFreqInt = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;
    auto prevSelected = wxString::Format(_("%.04f"), (double)prevFreqInt / (double)1000.0 / (double)1000.0);
    m_cboReportFrequency->Clear();
    
    for (auto& item : wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyList.get())
    {
        m_cboReportFrequency->Append(item);
    }
    
    auto idx = m_cboReportFrequency->FindString(prevSelected);
    if (idx >= 0)
    {
        m_cboReportFrequency->SetSelection(idx);
        m_cboReportFrequency->SetValue(prevSelected);
    }
}
