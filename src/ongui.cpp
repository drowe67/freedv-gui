/*
  ongui.cpp

  The simpler GUI event handlers.
*/

#include <sstream>
#include <iomanip>
#include <locale>

#include "main.h"
#if !defined(LPCNET_DISABLED)
#include "lpcnet_freedv.h"
#endif // defined(LPCNET_DISABLED)

#include "gui/dialogs/dlg_easy_setup.h"
#include "gui/dialogs/dlg_filter.h"
#include "gui/dialogs/dlg_audiooptions.h"
#include "gui/dialogs/dlg_options.h"
#include "gui/dialogs/dlg_ptt.h"
#include "gui/dialogs/freedv_reporter.h"
#include "gui/dialogs/monitor_volume_adj.h"

#if defined(WIN32)
#include "rig_control/omnirig/OmniRigController.h"
#endif // defined(WIN32)

#include "codec2_fdmdv.h" // for FDMDV_FCENTRE

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
extern paCallBackData* g_rxUserdata;

extern SNDFILE            *g_sfRecFileFromModulator;
extern SNDFILE            *g_sfRecFile;
extern bool g_recFileFromModulator;
extern bool g_recFileFromRadio;

extern SNDFILE            *g_sfRecMicFile;

extern wxMutex g_mutexProtectingCallbackData;

void clickTune(float frequency); // callback to pass new click freq

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
    if (dlg->ShowModal() == wxOK)
    {
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

        // Initialize FreeDV Reporter if required.
        initializeFreeDVReporter_();
        
        // Relayout window so that the changes can take effect.
        m_panel->Layout();
    }
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

    m_reporterDialog->refreshLayout();
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
    bool oldRxOnly = g_nSoundCards <= 1 ? true : false;

    wxUnusedVar(event);
    int rv = 0;
    AudioOptsDialog *dlg = new AudioOptsDialog(NULL);
    rv = dlg->ShowModal();
    if(rv == wxOK)
    {
        dlg->ExchangeData(EXCHANGE_DATA_OUT);

        bool newRxOnly = g_nSoundCards <= 1 ? true : false;
        if (oldRxOnly != newRxOnly &&
            wxGetApp().m_sharedReporterObject->isValidForReporting())
        {
            // Receive Only status has changed, refresh FreeDV Reporter
            initializeFreeDVReporter_();
        }
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
    bool oldFreqAsKHz = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz;

    wxUnusedVar(event);
    if (optionsDlg->ShowModal() == wxOK)
    {
        // Update reporting list.
        updateReportingFreqList_();
    
        // Show/hide frequency box based on reporting status.
        m_freqBox->Show(wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled);

        // Show/hide callsign combo box based on reporting Status
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
        
        // Update voice keyer file if different
        auto newVkFile = wxGetApp().appConfiguration.voiceKeyerWaveFilePath->mb_str();
        if (vkFileName_ != newVkFile)
        {
            // Clear filename to force reselection next time VK is triggered.
            vkFileName_ = "";
            wxGetApp().appConfiguration.voiceKeyerWaveFile = "";
        }
        
        // Adjust frequency labels on main window
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            m_freqBox->SetLabel(_("Report Freq. (kHz)"));
        }
        else
        {
            m_freqBox->SetLabel(_("Report Freq. (MHz)"));
        }

        // If the "Frequency as kHz" option has changed, update the frequencies
        // in the main window's callsign list.
        if (oldFreqAsKHz != wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            for (int index = 0; index < m_lastReportedCallsignListView->GetItemCount(); index++)
            {
                wxString newFreq = "";
                wxString freq = m_lastReportedCallsignListView->GetItemText(index, 1);
                double freqDouble = 0;
                freq.ToDouble(&freqDouble);

                if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
                {
                    freqDouble *= 1000.0;
                    newFreq = wxString::Format("%.01f", freqDouble);
                }
                else
                {
                    freqDouble /= 1000.0;
                    newFreq = wxString::Format("%.04f", freqDouble);
                }

                m_lastReportedCallsignListView->SetItem(index, 1, newFreq);
            }
        }

        // Initialize FreeDV Reporter if required.
        initializeFreeDVReporter_();

        // Refresh distance column label in case setting was changed.
        if (m_reporterDialog != nullptr)
        {
            m_reporterDialog->refreshLayout();
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

    if (dlg->ShowModal() == wxID_OK)
    {
        // Reinitialize FreeDV Reporter again in case we changed PTT method.
        initializeFreeDVReporter_();
    }

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
// OnHelp()
//-------------------------------------------------------------------------
void MainFrame::OnHelp( wxCommandEvent& event )
{
    wxLaunchDefaultBrowser("https://freedv.org/#gethelp");
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
#if !defined(LPCNET_DISABLED)
                wxT("lpcnet git hash: %s\n")
#endif // !defined(LPCNET_DISABLED)
                , FREEDV_VERSION, FREEDV_VERSION, GIT_HASH, freedv_get_hash()
#if !defined(LPCNET_DISABLED)
                , lpcnet_get_hash()
#endif // !defined(LPCNET_DISABLED)
                );

    wxMessageBox(msg, wxT("About"), wxOK | wxICON_INFORMATION, this);
}


// Attempt to talk to rig using Hamlib

bool MainFrame::OpenHamlibRig() {
    if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseForPTT != true)
       return false;
    if (wxGetApp().m_intHamlibRig == 0)
        return false;

    int rig = wxGetApp().m_intHamlibRig;
    wxString port = wxGetApp().appConfiguration.rigControlConfiguration.hamlibSerialPort;
    wxString pttPort = wxGetApp().appConfiguration.rigControlConfiguration.hamlibPttSerialPort;
    auto pttType = (HamlibRigController::PttType)wxGetApp().appConfiguration.rigControlConfiguration.hamlibPTTType.get();
    
    int serial_rate = wxGetApp().appConfiguration.rigControlConfiguration.hamlibSerialRate;
    if (wxGetApp().CanAccessSerialPort((const char*)port.ToUTF8()) && (
        pttType == HamlibRigController::PTT_VIA_CAT || pttType == HamlibRigController::PTT_VIA_NONE || wxGetApp().CanAccessSerialPort((const char*)pttPort.ToUTF8())))
    {
        auto tmp = std::make_shared<HamlibRigController>(
            rig, (const char*)port.mb_str(wxConvUTF8), serial_rate, wxGetApp().appConfiguration.rigControlConfiguration.hamlibIcomCIVAddress, 
            pttType, pttType == HamlibRigController::PTT_VIA_CAT || pttType == HamlibRigController::PTT_VIA_NONE ? (const char*)port.mb_str(wxConvUTF8) : (const char*)pttPort.mb_str(wxConvUTF8),
            wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges);

        // Hamlib also controls PTT.
        firstFreqUpdateOnConnect_ = false;
        wxGetApp().rigFrequencyController = tmp;
        wxGetApp().rigPttController = tmp;
        
        wxGetApp().rigFrequencyController->onRigError += [this](IRigController*, std::string err)
        {
            std::string fullErr = "Couldn't connect to Radio with hamlib: " + err;
            CallAfter([&, fullErr]() {
                wxMessageBox(fullErr, wxT("Error"), wxOK | wxICON_ERROR, this);
            });
        };

        wxGetApp().rigFrequencyController->onRigConnected += [&](IRigController*) {
            if (wxGetApp().rigFrequencyController && 
                wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges &&
                wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0)
            {
                // Suppress the frequency update message that will occur immediately after
                // connect; this will prevent overwriting of whatever's in the text box.
                firstFreqUpdateOnConnect_ = true;

                // Set frequency/mode to the one pre-selected by the user before start.
                wxGetApp().rigFrequencyController->setFrequency(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
                wxGetApp().rigFrequencyController->setMode(getCurrentMode_());
            }
        };

        wxGetApp().rigFrequencyController->onRigDisconnected += [&](IRigController*) {
            CallAfter([&]() {
                m_txtModeStatus->SetLabel(wxT("unk"));
                m_txtModeStatus->Enable(false);
            });
        };

        wxGetApp().rigFrequencyController->onFreqModeChange += [&](IRigFrequencyController*, uint64_t freq, IRigFrequencyController::Mode mode)
        {
            CallAfter([&, mode, freq]() {
                if (firstFreqUpdateOnConnect_)
                {
                    firstFreqUpdateOnConnect_ = false;
                    return;
                }

                // Update string value.
                switch(mode)
                {
                    case IRigFrequencyController::USB:
                    case IRigFrequencyController::DIGU:
                        m_txtModeStatus->SetLabel(wxT("USB"));
                        m_txtModeStatus->Enable(true);
                        break;
                    case IRigFrequencyController::LSB:
                    case IRigFrequencyController::DIGL:
                        m_txtModeStatus->SetLabel(wxT("LSB"));
                        m_txtModeStatus->Enable(true);
                        break;
                    case IRigFrequencyController::FM:
                    case IRigFrequencyController::DIGFM:
                        m_txtModeStatus->SetLabel(wxT("FM"));
                        m_txtModeStatus->Enable(true);
                        break;
                    case IRigFrequencyController::AM:
                        m_txtModeStatus->SetLabel(wxT("AM"));
                        m_txtModeStatus->Enable(true);
                        break;
                    default:
                        m_txtModeStatus->SetLabel(wxT("unk"));
                        m_txtModeStatus->Enable(false);
                        break;
                }

                // Widest 60 meter allocation is 5.250-5.450 MHz per https://en.wikipedia.org/wiki/60-meter_band.
                bool is60MeterBand = freq >= 5250000 && freq <= 5450000;
    
                // Update color based on the mode and current frequency.
                bool isUsbFreq = freq >= 10000000 || is60MeterBand;
                bool isLsbFreq = freq < 10000000 && !is60MeterBand;
    
                bool isMatchingMode = 
                    (isUsbFreq && (mode == IRigFrequencyController::USB || mode == IRigFrequencyController::DIGU)) ||
                    (isLsbFreq && (mode == IRigFrequencyController::LSB || mode == IRigFrequencyController::DIGL));
    
                if (isMatchingMode)
                {
                    m_txtModeStatus->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
                }
                else
                {
                    m_txtModeStatus->SetForegroundColour(wxColor(*wxRED));
                }

                // Update frequency box
                if (!suppressFreqModeUpdates_ && (
                    !wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled ||
                    !wxGetApp().appConfiguration.reportingConfiguration.manualFrequencyReporting))
                {
                    // wxString::Format() doesn't respect locale but C++ iomanip should. Use the latter instead.
                    std::stringstream ss;
                    std::locale loc("");
                    ss.imbue(loc);
                    
                    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
                    {
                        ss << std::fixed << std::setprecision(1) << (freq / 1000.0);
                    }
                    else
                    {
                        ss << std::fixed << std::setprecision(4) << (freq / 1000.0 / 1000.0);
                    }
                    
                    m_cboReportFrequency->SetValue(ss.str());
                }
                m_txtModeStatus->Refresh();
            });
        };

        wxGetApp().rigFrequencyController->connect();
        return true;
    }
    else
    {
        return false;
    }
}

#if defined(WIN32)
// TBD -- a lot of this can be combined with the Hamlib logic above.
void MainFrame::OpenOmniRig() 
{
    auto tmp = std::make_shared<OmniRigController>(
        wxGetApp().appConfiguration.rigControlConfiguration.omniRigRigId,
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges);

    // OmniRig also controls PTT.
    wxGetApp().rigFrequencyController = tmp;
    if (!wxGetApp().rigPttController)
    {
        wxGetApp().rigPttController = tmp;
    }

    wxGetApp().rigFrequencyController->onRigError += [this](IRigController*, std::string err)
    {
        std::string fullErr = "Couldn't connect to Radio with OmniRig: " + err;
        CallAfter([&, fullErr]() {
            wxMessageBox(fullErr, wxT("Error"), wxOK | wxICON_ERROR, this);
        });
    };

    wxGetApp().rigFrequencyController->onRigConnected += [&](IRigController*) {
        if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges)
        {
            wxGetApp().rigFrequencyController->setFrequency(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
            wxGetApp().rigFrequencyController->setMode(getCurrentMode_());
        }
    };

    wxGetApp().rigFrequencyController->onRigDisconnected += [&](IRigController*) {
        CallAfter([&]() {
            m_txtModeStatus->SetLabel(wxT("unk"));
            m_txtModeStatus->Enable(false);
        });
    };

    wxGetApp().rigFrequencyController->onFreqModeChange += [&](IRigFrequencyController*, uint64_t freq, IRigFrequencyController::Mode mode)
    {
        CallAfter([&, mode, freq]() {
            // Update string value.
            switch(mode)
            {
                case IRigFrequencyController::USB:
                case IRigFrequencyController::DIGU:
                    m_txtModeStatus->SetLabel(wxT("USB"));
                    m_txtModeStatus->Enable(true);
                    break;
                case IRigFrequencyController::LSB:
                case IRigFrequencyController::DIGL:
                    m_txtModeStatus->SetLabel(wxT("LSB"));
                    m_txtModeStatus->Enable(true);
                    break;
                case IRigFrequencyController::FM:
                case IRigFrequencyController::DIGFM:
                    m_txtModeStatus->SetLabel(wxT("FM"));
                    m_txtModeStatus->Enable(true);
                    break;
                case IRigFrequencyController::AM:
                    m_txtModeStatus->SetLabel(wxT("AM"));
                    m_txtModeStatus->Enable(true);
                    break;
                default:
                    m_txtModeStatus->SetLabel(wxT("unk"));
                    m_txtModeStatus->Enable(false);
                    break;
            }

            // Widest 60 meter allocation is 5.250-5.450 MHz per https://en.wikipedia.org/wiki/60-meter_band.
            bool is60MeterBand = freq >= 5250000 && freq <= 5450000;

            // Update color based on the mode and current frequency.
            bool isUsbFreq = freq >= 10000000 || is60MeterBand;
            bool isLsbFreq = freq < 10000000 && !is60MeterBand;

            bool isMatchingMode = 
                (isUsbFreq && (mode == IRigFrequencyController::USB || mode == IRigFrequencyController::DIGU)) ||
                (isLsbFreq && (mode == IRigFrequencyController::LSB || mode == IRigFrequencyController::DIGL));

            if (isMatchingMode)
            {
                m_txtModeStatus->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
            }
            else
            {
                m_txtModeStatus->SetForegroundColour(wxColor(*wxRED));
            }

            // Update frequency box
            if (!wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled ||
                !wxGetApp().appConfiguration.reportingConfiguration.manualFrequencyReporting)
            {
                if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
                {
                    m_cboReportFrequency->SetValue(wxString::Format("%.1f", freq / 1000.0));
                }
                else
                {
                    m_cboReportFrequency->SetValue(wxString::Format("%.4f", freq / 1000.0 / 1000.0));
                }
            }
            m_txtModeStatus->Refresh();

            // Suppress updates if the Report Frequency box has focus.
            suppressFreqModeUpdates_ = m_cboReportFrequency->HasFocus();
        });
    };

    // Temporarily suppress frequency updates until we're fully connected.
    suppressFreqModeUpdates_ = true;
    wxGetApp().rigFrequencyController->connect();
}
#endif // defined(WIN32)

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
    auto style = GetWindowStyle();

    // Clear wxSTAY_ON_TOP first if it's already set.
    style &= ~wxSTAY_ON_TOP;
    
    // (Re)set wxSTAY_ON_TOP depending on whether the menu option is checked.
    if (event.IsChecked())
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
    wxMessageDialog messageDialog(
        this, "Would you like to restore configuration to defaults?", wxT("Restore Defaults"),
        wxYES_NO | wxICON_QUESTION | wxCENTRE);

    auto answer = messageDialog.ShowModal();
    if (answer == wxID_YES)
    {
        if(pConfig->DeleteAll())
        {
            wxLogMessage(wxT("Config file/registry key successfully deleted."));
            
            if (wxGetApp().m_sharedReporterObject)
            {
                wxGetApp().m_sharedReporterObject = nullptr;
            }

            if (m_reporterDialog != nullptr)
            {
                m_reporterDialog->setReporter(nullptr);
                m_reporterDialog->Close();
                m_reporterDialog->Destroy();
                m_reporterDialog = nullptr;
            }
            
            // Resets all configuration to defaults.
            loadConfiguration_();
        }
        else
        {
            wxLogError(wxT("Deleting config file/registry key failed."));
        }
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
            bool mainWindowActive = frame->IsActive();
            bool reporterActiveButNotUpdatingTextMessage = 
                frame->m_reporterDialog != nullptr && frame->m_reporterDialog->IsActive() && 
                !frame->m_reporterDialog->isTextMessageFieldInFocus();
            if (frame->m_RxRunning && (mainWindowActive || reporterActiveButNotUpdatingTextMessage) && 
                wxGetApp().appConfiguration.enableSpaceBarForPTT && !frame->isReceiveOnly()) {

                // space bar controls tx/rx if keyer not running
                if (frame->vk_state == VK_IDLE) {
                    if (frame->m_btnTogPTT->GetValue())
                        frame->m_btnTogPTT->SetValue(false);
                    else
                        frame->m_btnTogPTT->SetValue(true);

                    // Update background color of button here because when toggling PTT via keyboard,
                    // the background color for some reason doesn't update inside togglePTT().
                    frame->m_btnTogPTT->SetBackgroundColour(frame->m_btnTogPTT->GetValue() ? *wxRED : wxNullColour);

                    // Actually toggle PTT.
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
    adjustMonitorPttVolMenuItem_->Enable(wxGetApp().appConfiguration.monitorTxAudio);
}

void MainFrame::OnSetMonitorTxAudioVol( wxCommandEvent& event )
{
    auto popup = new MonitorVolumeAdjPopup(this, wxGetApp().appConfiguration.monitorTxAudioVol);
    popup->Popup();
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
    std::chrono::high_resolution_clock highResClock;

    // Change tabbed page in centre panel depending on PTT state

    if (g_tx)
    {
        // Sleep for long enough that we get the remaining [blocksize] ms of audio.
        int msSleep = (1000 * freedvInterface.getTxNumSpeechSamples()) / freedvInterface.getTxSpeechSampleRate();
        if (g_verbose) fprintf(stderr, "Sleeping for %d ms prior to ending TX\n", msSleep);

        auto before = highResClock.now();

        while(true)
        {
            auto diff = highResClock.now() - before;
            if (diff >= std::chrono::milliseconds(msSleep))
            {
                break;
            }

            wxThread::Sleep(1);
            wxGetApp().Yield(true);
        }
        
        // Trigger end of TX processing. This causes us to wait for the remaining samples
        // to flow through the system before toggling PTT.  Note that there is a 1000ms 
        // timeout as backup.
        int sample = g_outfifo1_empty;
        endingTx = true;

        before = highResClock.now();
        while(true)
        {
            auto diff = highResClock.now() - before;
            if (diff >= std::chrono::milliseconds(1000) || (g_outfifo1_empty != sample))
            {
                break;
            }

            wxThread::Sleep(1);

            // Yield() used to avoid lack of UI responsiveness during delay.
            wxGetApp().Yield(true);
        }
        
        // Wait an additional configured timeframe before actually clearing PTT (below)
        if (wxGetApp().appConfiguration.txRxDelayMilliseconds > 0)
        {
            // Delay outbound TX audio if going into TX.
            // Yield() used to avoid lack of UI responsiveness during delay.
            before = highResClock.now();
            while(true)
            {
                auto diff = highResClock.now() - before;
                if (diff >= std::chrono::milliseconds(wxGetApp().appConfiguration.txRxDelayMilliseconds.get()))
                {
                    break;
                }

                wxThread::Sleep(1);
                wxGetApp().Yield(true);
            }
        }
        g_tx = false;
        endingTx = false;

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
        
        // Note: GetPageIndex sometimes returns the incorrect results, so iterating and finding
        // the current page ourselves is a better bet.
        size_t index = 0;
        for (; index < m_auiNbookCtrl->GetPageCount(); index++)
        {
            auto page = m_auiNbookCtrl->GetPage(index);
            if (page == (wxWindow *)m_panelSpeechIn)
            {
                m_auiNbookCtrl->ChangeSelection(index);
                break;
            }
        }

        // disable sync text

        m_textSync->Disable();
        m_textCurrentDecodeMode->Disable();

        // Disable On/Off button.
        m_togBtnOnOff->Enable(false);
    }

    if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseForPTT) {
        if (wxGetApp().rigFrequencyController != nullptr && wxGetApp().rigFrequencyController->isConnected()) {
            // Update mode display on the bottom of the main UI.
            wxGetApp().rigFrequencyController->requestCurrentFrequencyMode();
        }
    }

    auto newTx = m_btnTogPTT->GetValue();
    if (wxGetApp().rigPttController != nullptr && wxGetApp().rigPttController->isConnected()) 
    {
        wxGetApp().rigPttController->ptt(newTx);
    }

    // reset level gauge

    m_maxLevel = 0;
    m_textLevel->SetLabel(wxT(""));
    m_gaugeLevel->SetValue(0);
    
    // Report TX change to registered reporters
    for (auto& obj : wxGetApp().m_reporters)
    {
        obj->transmit(freedvInterface.getCurrentTxModeStr(), newTx);
    }

    // Change button color depending on TX status.
    m_btnTogPTT->SetBackgroundColour(newTx ? *wxRED : wxNullColour);
    
    // If we're recording, switch to/from modulator and radio.
    if (g_sfRecFile != nullptr)
    {
        if (!newTx)
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

    if (newTx)
    {
        endingTx = false;
            
        if (wxGetApp().appConfiguration.txRxDelayMilliseconds > 0)
        {
            // Delay outbound TX audio if going into TX.
            // Note: Yield() used here to avoid lack of UI responsiveness during delay.
            auto before = highResClock.now();
            while(true)
            {
                auto diff = highResClock.now() - before;
                if (diff >= std::chrono::milliseconds(wxGetApp().appConfiguration.txRxDelayMilliseconds.get()))
                {
                    break;
                }

                wxThread::Sleep(1);
                wxGetApp().Yield(true);
            }
        }

        // g_tx governs when audio actually goes out during TX, so don't set to true until
        // after the delay occurs.
        g_tx = true;
    }
}

HamlibRigController::Mode MainFrame::getCurrentMode_()
{
    // Widest 60 meter allocation is 5.250-5.450 MHz per https://en.wikipedia.org/wiki/60-meter_band.
    bool is60MeterBand = 
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency >= 5250000 && 
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency <= 5450000;
    
    bool useAnalog = 
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseAnalogModes || g_analog;
    HamlibRigController::Mode lsbMode = useAnalog ? HamlibRigController::LSB : HamlibRigController::DIGL;
    HamlibRigController::Mode usbMode = useAnalog ? HamlibRigController::USB : HamlibRigController::DIGU;
    
    HamlibRigController::Mode newMode;
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency < 10000000 &&
        !is60MeterBand)
    {
        newMode = lsbMode;
    }
    else
    {
        newMode = usbMode;
    }

    return newMode;
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
    
    if (wxGetApp().rigFrequencyController != nullptr && 
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0 &&
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges)
    {
        // Request mode change on the radio side
        wxGetApp().rigFrequencyController->setMode(getCurrentMode_());
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
    if (wxGetApp().rigFrequencyController != nullptr && suppressFreqModeUpdates_)
    {
        return;
    }
    
    OnChangeReportFrequency(event);
}

void MainFrame::OnChangeReportFrequency( wxCommandEvent& event )
{    
    wxString freqStr = m_cboReportFrequency->GetValue();
    auto oldFreq = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;

    if (freqStr.Length() > 0)
    {
        double tmp = 0;
        std::stringstream ss(std::string(freqStr.ToUTF8()));
        std::locale loc("");
        ss.imbue(loc);
        
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            ss >> std::fixed >> std::setprecision(1) >> tmp;
            wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency = round(tmp * 1000);
        }
        else
        {
            ss >> std::fixed >> std::setprecision(4) >> tmp;
            wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency = round(tmp * 1000 * 1000);
        }

        if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0)
        {
            m_cboReportFrequency->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

            // Round to the nearest 100 Hz.
            uint64_t wholeFreq = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency / 100;
            uint64_t remainder = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency % 100;

            if (remainder >= 50)
            {
                wholeFreq++;
            }

            wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency = wholeFreq * 100;
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

    if (oldFreq != wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency)
    {      
        if (wxGetApp().rigFrequencyController != nullptr && 
            wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0 && 
            wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges)
        {
            // Request frequency/mode change on the radio side
            wxGetApp().rigFrequencyController->setFrequency(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
            wxGetApp().rigFrequencyController->setMode(getCurrentMode_());
        }
    }

    if (m_reporterDialog != nullptr)
    {
        m_reporterDialog->refreshQSYButtonState();
    }
}

void MainFrame::OnReportFrequencySetFocus(wxFocusEvent& event)
{
    suppressFreqModeUpdates_ = true;
    TopFrame::OnReportFrequencySetFocus(event);
}

void MainFrame::OnReportFrequencyKillFocus(wxFocusEvent& event)
{
    suppressFreqModeUpdates_ = false;
    
    // Handle any frequency changes as appropriate.
    wxCommandEvent tmpEvent;
    OnChangeReportFrequency(tmpEvent);

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

void MainFrame::OnCenterRx(wxCommandEvent& event)
{
    clickTune(FDMDV_FCENTRE);
}

void MainFrame::updateReportingFreqList_()
{
    uint64_t prevFreqInt = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;

    double freq =  ((double)prevFreqInt)/1000.0;
    if (!wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
    {
        freq /= 1000.0;
    }

    std::stringstream ss;
    std::locale loc("");
    ss.imbue(loc);
    
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
    {
        ss << std::fixed << std::setprecision(1) << freq;
    }
    else
    {
        ss << std::fixed << std::setprecision(4) << freq;
    }
    std::string prevSelected = ss.str();

    m_cboReportFrequency->Clear();
    
    for (auto& item : wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyList.get())
    {
        m_cboReportFrequency->Append(item);
    }
    
    auto idx = m_cboReportFrequency->FindString(prevSelected);
    if (idx >= 0)
    {
        m_cboReportFrequency->SetSelection(idx);
    }
    m_cboReportFrequency->SetValue(prevSelected);
    
    // Update associated label if the units have changed
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
    {
        m_freqBox->SetLabel(_("Report Freq. (kHz)"));
    }
    else
    {
        m_freqBox->SetLabel(_("Report Freq. (MHz)"));
    }
}
