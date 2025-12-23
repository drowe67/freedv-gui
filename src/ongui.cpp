/*
  ongui.cpp

  The simpler GUI event handlers.
*/

#include <sstream>
#include <iomanip>
#include <locale>

#include "main.h"

#include "git_version.h"
#include "gui/dialogs/dlg_easy_setup.h"
#include "gui/dialogs/dlg_filter.h"
#include "gui/dialogs/dlg_audiooptions.h"
#include "gui/dialogs/dlg_options.h"
#include "gui/dialogs/dlg_ptt.h"
#include "gui/dialogs/freedv_reporter.h"
#include "gui/dialogs/monitor_volume_adj.h"
#include "gui/dialogs/log_entry.h"

#if defined(WIN32)
#include "rig_control/omnirig/OmniRigController.h"
#endif // defined(WIN32)

#include "codec2_fdmdv.h" // for FDMDV_FCENTRE

extern int g_mode;

extern int   g_SquelchActive;
extern float g_SquelchLevel;
extern int   g_analog;
extern std::atomic<int>   g_tx;
extern std::atomic<int>   g_State, g_prev_State;
extern FreeDVInterface freedvInterface;
extern std::atomic<bool> g_queueResync;
extern short *g_error_hist, *g_error_histn;
extern int g_resyncs;
extern int g_Nc;
extern int g_txLevel;
extern std::atomic<float> g_txLevelScale;

extern wxConfigBase *pConfig;
extern std::atomic<bool> endingTx;
extern std::atomic<int> g_outfifo1_empty;
extern std::atomic<bool> g_voice_keyer_tx;
extern paCallBackData* g_rxUserdata;

extern SNDFILE            *g_sfRecFileFromModulator;
extern SNDFILE            *g_sfRecFile;
extern bool g_recFileFromModulator;
extern bool g_recFileFromRadio;

extern SNDFILE            *g_sfRecMicFile;

extern wxMutex g_mutexProtectingCallbackData;

std::atomic<bool> g_eoo_enqueued;

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
void MainFrame::OnToolsEasySetup(wxCommandEvent&)
{
    EasySetupDialog* dlg = new EasySetupDialog(this);
    if (dlg->ShowModal() == wxOK)
    {
        // Show/hide frequency box based on CAT control setup.
        m_freqBox->Show(isFrequencyControlEnabled_());

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
void MainFrame::OnToolsFreeDVReporter(wxCommandEvent&)
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
    
        // Show/hide frequency box based on CAT control configuration.
        m_freqBox->Show(isFrequencyControlEnabled_());
        
        // Show/hide stats box
        statsBox->Show(wxGetApp().appConfiguration.showDecodeStats);
        
        // Show/hide legacy modes
        modeBox->Show(wxGetApp().appConfiguration.enableLegacyModes);
        
        bool isEnabled = wxGetApp().appConfiguration.enableLegacyModes && !m_rbRADE->GetValue();
        squelchBox->Show(wxGetApp().appConfiguration.enableLegacyModes);
        m_sliderSQ->Enable(isEnabled);
        m_ckboxSQ->Enable(isEnabled);
        m_textSQ->Enable(isEnabled);
        m_btnCenterRx->Enable(isEnabled);
        m_btnCenterRx->Show(wxGetApp().appConfiguration.enableLegacyModes);
        m_BtnReSync->Enable(isEnabled);
        m_BtnReSync->Show(wxGetApp().appConfiguration.enableLegacyModes);

        // XXX - with really short windows, wxWidgets sometimes doesn't size
        // the components properly until the user resizes the window (even if only
        // by a pixel or two). As a really hacky workaround, we emulate this behavior
        // when restoring window sizing. These resize events also happen after configuration
        // is restored but I'm not sure this is necessary.
        wxSize size = GetSize();
        auto w = size.GetWidth();
        auto h = size.GetHeight();
        CallAfter([=]()
        {
            SetSize(w, h);
        });
        CallAfter([=]()
        {
            SetSize(w + 1, h + 1);
        });
        CallAfter([=]()
        {
            SetSize(w, h);
        });

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
        wxFileName fullVKPath(wxGetApp().appConfiguration.voiceKeyerWaveFilePath, wxGetApp().appConfiguration.voiceKeyerWaveFile);
        if (wxString::FromUTF8(vkFileName_.c_str()) != fullVKPath.GetFullPath())
        {
            // Clear filename to force reselection next time VK is triggered.
            vkFileName_ = "";
            wxGetApp().appConfiguration.voiceKeyerWaveFile = "";
            setVoiceKeyerButtonLabel_("");
        }
        
        // Adjust frequency labels on main window
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            m_freqBox->SetLabel(_("Radio Freq. (kHz)"));
        }
        else
        {
            m_freqBox->SetLabel(_("Radio Freq. (MHz)"));
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
                    newFreq = wxNumberFormatter::ToString(freqDouble, 1);
                }
                else
                {
                    freqDouble /= 1000.0;
                    newFreq = wxNumberFormatter::ToString(freqDouble, 4);
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
void MainFrame::OnToolsOptionsUI(wxUpdateUIEvent&)
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
        // Show/hide frequency box based on CAT control configuration.
        m_freqBox->Show(isFrequencyControlEnabled_());
        
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
void MainFrame::OnHelpCheckUpdates(wxCommandEvent&)
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
void MainFrame::OnHelpManual( wxCommandEvent& )
{
    wxLaunchDefaultBrowser("https://github.com/drowe67/freedv-gui/blob/master/USER_MANUAL.pdf");
}

//-------------------------------------------------------------------------
// OnHelp()
//-------------------------------------------------------------------------
void MainFrame::OnHelp( wxCommandEvent& )
{
    wxLaunchDefaultBrowser("https://freedv.org/#getting-help");
}

//-------------------------------------------------------------------------
//OnHelpAbout()
//-------------------------------------------------------------------------
void MainFrame::OnHelpAbout(wxCommandEvent& event)
{
    wxUnusedVar(event);
    wxString msg;
    wxString version = wxString::FromUTF8(GetFreeDVVersion().c_str());

    msg.Printf( wxT("FreeDV GUI %s\n\n")
                wxT("For Help and Support visit: http://freedv.org\n\n")

                wxT("GNU Public License V2.1\n\n")
                wxT("Created by David Witten KD0EAG and David Rowe VK5DGR (2012).  ")
                wxT("Currently maintained by Mooneer Salem K6AQ and David Rowe VK5DGR.\n\n")
                wxT("freedv-gui version: %s\n")
                wxT("freedv-gui git hash: %s\n")
                wxT("codec2 git hash: %s\n")
                , version, version, FREEDV_GIT_HASH, freedv_get_hash()
                );

    wxMessageBox(msg, wxT("About"), wxOK | wxICON_INFORMATION, this);
}

void MainFrame::onFrequencyModeChange_(IRigFrequencyController*, uint64_t freq, IRigFrequencyController::Mode mode)
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
                m_txtModeStatus->SetLabel(wxT("USB"));
                m_txtModeStatus->Enable(true);
                break;
            case IRigFrequencyController::DIGU:
                m_txtModeStatus->SetLabel(wxT("USB-D"));
                m_txtModeStatus->Enable(true);
                break;
            case IRigFrequencyController::LSB:
                m_txtModeStatus->SetLabel(wxT("LSB"));
                m_txtModeStatus->Enable(true);
                break;
            case IRigFrequencyController::DIGL:
                m_txtModeStatus->SetLabel(wxT("LSB-D"));
                m_txtModeStatus->Enable(true);
                break;
            case IRigFrequencyController::FM:
                m_txtModeStatus->SetLabel(wxT("FM"));
                m_txtModeStatus->Enable(true);
                break;
            case IRigFrequencyController::DIGFM:
                m_txtModeStatus->SetLabel(wxT("FM-D"));
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
            // wxString::Format() doesn't respect locale but wxNumberFormatter should. Use the latter instead.
            wxString freqString;            
            if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
            {
                freqString = wxNumberFormatter::ToString(freq / 1000.0, 1);
            }
            else
            {
                freqString = wxNumberFormatter::ToString(freq / 1000.0 / 1000.0, 4);
            }
            
            m_cboReportFrequency->SetValue(freqString);
        }
        m_txtModeStatus->Refresh();
    });
}

void MainFrame::onRadioConnected_(IRigController*)
{
    if (wxGetApp().rigFrequencyController && 
        (wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges || wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly) &&
        wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0)
    {
        // Suppress the frequency update message that will occur immediately after
        // connect; this will prevent overwriting of whatever's in the text box.
        firstFreqUpdateOnConnect_ = true;

        // Set frequency/mode to the one pre-selected by the user before start.
        wxGetApp().rigFrequencyController->setFrequency(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
        
        if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges)
        {
            wxGetApp().rigFrequencyController->setMode(getCurrentMode_());
        }
    }
}

void MainFrame::onRadioDisconnected_(IRigController*)
{
    CallAfter([&]() {
        m_txtModeStatus->SetLabel(wxT("unk"));
        m_txtModeStatus->Enable(false);
    });
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
            (wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges || wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly),
            wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly);

        // Hamlib also controls PTT.
        firstFreqUpdateOnConnect_ = false;
        wxGetApp().rigFrequencyController = tmp;
        wxGetApp().rigPttController = tmp;
        
        wxGetApp().rigFrequencyController->onRigError += [this](IRigController*, std::string const& err)
        {
            std::string fullErr = "Couldn't connect to Radio with hamlib: " + err;
            CallAfter([&, fullErr]() {
                wxMessageBox(fullErr, wxT("Error"), wxOK | wxICON_ERROR, this);
            });
        };

        wxGetApp().rigFrequencyController->onRigConnected += [&](IRigController* ptr) {
            onRadioConnected_(ptr);
        };

        wxGetApp().rigFrequencyController->onRigDisconnected += [&](IRigController* ptr) {
            onRadioDisconnected_(ptr);
        };

        wxGetApp().rigFrequencyController->onFreqModeChange += [&](IRigFrequencyController* ptr, uint64_t freq, IRigFrequencyController::Mode mode) {
            onFrequencyModeChange_(ptr, freq, mode);
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
        (wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges || wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly),
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly);

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

    wxGetApp().rigFrequencyController->onRigConnected += [&](IRigController* ptr) {
        onRadioConnected_(ptr);
    };

    wxGetApp().rigFrequencyController->onRigDisconnected += [&](IRigController* ptr) {
        onRadioDisconnected_(ptr);
    };

    wxGetApp().rigFrequencyController->onFreqModeChange += [&](IRigFrequencyController* ptr, uint64_t freq, IRigFrequencyController::Mode mode) {
        onFrequencyModeChange_(ptr, freq, mode);
    };

    // Temporarily suppress frequency updates until we're fully connected.
    suppressFreqModeUpdates_ = true;
    wxGetApp().rigFrequencyController->connect();
}
#endif // defined(WIN32)

//-------------------------------------------------------------------------
// OnCloseFrame()
//-------------------------------------------------------------------------
void MainFrame::OnCloseFrame(wxCloseEvent&)
{
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
                wxGetApp().SafeYield(nullptr, false); // make sure we handle any remaining Reporter messages before dispose
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
    g_SquelchLevel = (float)m_sliderSQ->GetValue()/2.0 - 5.0;
    wxString sqsnr_string = wxNumberFormatter::ToString(g_SquelchLevel, 1) + "dB"; // 0.5 dB steps
    m_textSQ->SetLabel(sqsnr_string);

    event.Skip();
}

//-------------------------------------------------------------------------
// OnChangeTxLevel()
//-------------------------------------------------------------------------
void MainFrame::OnChangeTxLevel( wxScrollEvent& )
{
    g_txLevel = m_sliderTxLevel->GetValue();
    float dbLoss = g_txLevel / 10.0;
    float scaleFactor = exp(dbLoss/20.0 * log(10.0));
    g_txLevelScale.store(scaleFactor, std::memory_order_release);

    wxString fmtString = wxString::Format(MIC_SPKR_LEVEL_FORMAT_STR, wxNumberFormatter::ToString((double)g_txLevel/10.0, 1), DECIBEL_STR);
    m_txtTxLevelNum->SetLabel(fmtString);
    
    wxGetApp().appConfiguration.transmitLevel = g_txLevel;
}

//-------------------------------------------------------------------------
// OnChangeMicSpkrLevel()
//-------------------------------------------------------------------------
void MainFrame::OnChangeMicSpkrLevel( wxScrollEvent& )
{
    auto sliderLevel = (double)m_sliderMicSpkrLevel->GetValue() / 10.0;
    
    if (g_tx.load(std::memory_order_acquire))
    {
        wxGetApp().appConfiguration.filterConfiguration.micInChannel.volInDB = sliderLevel;
        m_newMicInFilter = true;
    }
    else
    {
        wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.volInDB = sliderLevel;
        m_newSpkOutFilter = true;
    }
    
    wxString fmtString = wxString::Format(MIC_SPKR_LEVEL_FORMAT_STR, wxNumberFormatter::ToString((double)sliderLevel, 1), DECIBEL_STR);
    m_txtMicSpkrLevelNum->SetLabel(fmtString);
}

//-------------------------------------------------------------------------
// OnCheckSQClick()
//-------------------------------------------------------------------------
void MainFrame::OnCheckSQClick(wxCommandEvent&)
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
void MainFrame::OnCheckSNRClick(wxCommandEvent&)
{
    wxGetApp().appConfiguration.snrSlow = m_ckboxSNR->GetValue();
    setsnrBeta(wxGetApp().appConfiguration.snrSlow);
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

void MainFrame::OnSetMonitorTxAudioVol( wxCommandEvent& )
{
    auto popup = new MonitorVolumeAdjPopup(this, wxGetApp().appConfiguration.monitorTxAudioVol);
    popup->Popup();
}

//-------------------------------------------------------------------------
// OnTogBtnPTTRightClick(): show right-click menu for PTT button
//-------------------------------------------------------------------------
void MainFrame::OnTogBtnPTTRightClick( wxContextMenuEvent& )
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
        // wxWidgets should already be doing the below logic,
        // but for some reason this intermittently doesn't happen
        // on Windows. Just to be sure, we force the correct state
        // here (similar to what's already done for ending TX while
        // using the voice keyer).
        m_btnTogPTT->SetValue(!g_tx.load(std::memory_order_acquire));
        m_btnTogPTT->SetBackgroundColour(m_btnTogPTT->GetValue() ? *wxRED : wxNullColour);
        
        togglePTT();
    }
    event.Skip();
}

void MainFrame::togglePTT(void) {
    std::chrono::high_resolution_clock highResClock;

    // Change tabbed page in centre panel depending on PTT state

    if (g_tx.load(std::memory_order_acquire))
    {
        // Sleep for long enough that we get the remaining [blocksize] ms of audio.
        int msSleep = (1000 * freedvInterface.getTxNumSpeechSamples()) / freedvInterface.getTxSpeechSampleRate();
        log_debug("Sleeping for %d ms prior to ending TX", msSleep);

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
        if (freedvInterface.getCurrentMode() == FREEDV_MODE_RADE)
        {
            log_info("Waiting for EOO to be queued");
            endingTx.store(true, std::memory_order_release);
            
            auto beginTime = std::chrono::high_resolution_clock::now();
            while(true)
            {
                if (g_eoo_enqueued.load(std::memory_order_acquire))
                {
                    log_info("Detected that EOO has been enqueued");
                    break;
                }
 
                wxThread::Sleep(1);
                wxGetApp().Yield(true);

                auto endTime = std::chrono::high_resolution_clock::now();
                if ((endTime - beginTime) >= std::chrono::seconds(2))
                {
                    log_warn("Timed out waiting for EOO to be enqueued");
                    break;
                }
            }
        }

        int sample = g_outfifo1_empty;
        before = highResClock.now();
        while(true)
        {
            auto diff = highResClock.now() - before;
            auto tmp = g_outfifo1_empty.load(std::memory_order_acquire);
            if (diff >= std::chrono::milliseconds(1000) || (tmp != sample))
            {
                log_info("All TX finished (diff = %d ms, fifo_empty = %d, sample = %d), going out of PTT", (int)std::chrono::duration_cast<std::chrono::milliseconds>(diff).count(), tmp, sample);
                break;
            }

            wxThread::Sleep(1);

            // Yield() used to avoid lack of UI responsiveness during delay.
            wxGetApp().Yield(true);
        }
        
        // Wait for a minimum amount of time before stopping TX to ensure that
        // remaining audio gets piped to the radio from the operating system.
        auto outDevice = txOutSoundDevice;
        auto latency = 0;
        if (outDevice)
        {
            latency = outDevice->getLatencyInMicroseconds();
        }
        auto pttResponseTime = 0;

        // Also take into account any latency between the computer and radio.
        // The only way to do this is by tracking how long it takes to respond
        // to PTT requests (and that's not necessarily great, either). Normally
        // this component should be a small part of the overall latency, but it
        // could be larger when dealing with SDR radios that are on the network.
        //
        // Note: This may not provide accurate results until after going from 
        // TX->RX the first time, but one missed report during a session shouldn't 
        // be a huge deal.
        auto pttController = wxGetApp().rigPttController;
        if (pttController)
        {
            // We only need to worry about the time getting to the radio,
            // not the time to get from the radio to us.
            pttResponseTime = pttController->getRigResponseTimeMicroseconds() / 2;
        }
        
        auto totalPauseTime = latency + pttResponseTime;
        log_info(
            "Pausing for a minimum of %d us (%d us latency + %d us PTT response time) before TX->RX to allow remaining audio to go out", 
            totalPauseTime, latency, pttResponseTime);
        before = highResClock.now();
        while(true)
        {
            auto diff = highResClock.now() - before;
            if (diff >= std::chrono::microseconds(totalPauseTime))
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
        g_tx.store(false, std::memory_order_release);
        endingTx.store(false, std::memory_order_release);

        m_sliderMicSpkrLevel->SetValue(wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.volInDB * 10);
        CallAfter([&]() { m_sliderMicSpkrLevel->Refresh(); }); // Redraw doesn't happen immediately otherwise in some environments
        wxString fmtString = wxString::Format(MIC_SPKR_LEVEL_FORMAT_STR, wxNumberFormatter::ToString((double)wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.volInDB, 1), DECIBEL_STR);
        m_txtMicSpkrLevelNum->SetLabel(fmtString);
        
        // tx-> rx transition, swap to the page we were on for last rx
        m_auiNbookCtrl->ChangeSelection(wxGetApp().appConfiguration.currentNotebookTab);
        for (size_t index = 0; index < m_auiNbookCtrl->GetPageCount(); index++)
        {
            auto page = m_auiNbookCtrl->GetPage(index);
            page->Refresh();
        }

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
                page->Refresh();
                break;
            }
        }

        // disable sync text

        m_textSync->Disable();
        m_textCurrentDecodeMode->Disable();

        // Disable On/Off button.
        m_togBtnOnOff->Enable(false);
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
        endingTx.store(false, std::memory_order_release);
            
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
        g_tx.store(true, std::memory_order_release);
        
        m_sliderMicSpkrLevel->SetValue(wxGetApp().appConfiguration.filterConfiguration.micInChannel.volInDB * 10);
        wxString fmtString = wxString::Format(MIC_SPKR_LEVEL_FORMAT_STR, wxNumberFormatter::ToString((double)wxGetApp().appConfiguration.filterConfiguration.micInChannel.volInDB, 1), DECIBEL_STR);
        m_txtMicSpkrLevelNum->SetLabel(fmtString);
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
        
        m_togBtnAnalog->SetLabel(wxT("Switch to Di&gital"));
    }
    else {
        g_analog = 0;
        m_panelSpectrum->setFreqScale(MODEM_STATS_NSPEC*((float)MAX_F_HZ/(freedvInterface.getRxModemSampleRate()/2)));
        m_panelWaterfall->setFs(freedvInterface.getRxModemSampleRate());
        
        m_togBtnAnalog->SetLabel(wxT("Switch to A&nalog"));
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

    g_State.store(0, std::memory_order_release);
    g_prev_State.store(0, std::memory_order_release);;
    freedvInterface.getCurrentRxModemStats()->snr_est = 0;

    event.Skip();
}

void MainFrame::OnCallSignReset(wxCommandEvent&)
{
    m_pcallsign = m_callsign;
    memset(m_callsign, 0, MAX_CALLSIGN);
    wxString s;
    s.Printf("%s", m_callsign);
    m_txtCtrlCallSign->SetValue(s);
    
    m_lastReportedCallsignListView->DeleteAllItems();
    m_cboLastReportedCallsigns->SetText(_(""));
}

void MainFrame::OnLogQSO(wxCommandEvent&)
{
    if (m_lastReportedCallsignListView->GetItemCount() > 0)
    {
        auto selected = m_lastReportedCallsignListView->GetFirstSelected();
        if (selected == -1)
        {
            // Default to the first/most recent entry if user hasn't explicitly selected
            // a contact.
            selected = 0;
        }
        
        // Get callsign and RX frequency
        auto dxCall = m_lastReportedCallsignListView->GetItemText(selected, 0);
        auto dxFreq = m_lastReportedCallsignListView->GetItemText(selected, 1);
        
        double dxFreqDouble = 0;
        wxNumberFormatter::FromString(dxFreq, &dxFreqDouble);
        
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            dxFreqDouble *= 1000;
        }
        else
        {
            dxFreqDouble *= 1000000;
        }
        
        // If connected to FreeDV Reporter, get DX grid
        wxString dxGrid;
        if (m_reporterDialog != nullptr)
        {
            dxGrid = m_reporterDialog->getGridSquareForCallsign(dxCall);
        }

        // Show log contact dialog 
        auto logDialog = new LogEntryDialog(this);
        logDialog->ShowDialog(dxCall.ToUTF8(), dxGrid.ToUTF8(), (int64_t)dxFreqDouble);
    }
}

// Force manual resync, just in case demod gets stuck on false sync

void MainFrame::OnReSync(wxCommandEvent&)
{
    if (m_RxRunning)  {
        log_debug("OnReSync");
        
        // Resync must be triggered from the TX/RX thread, so pushing the button queues it until
        // the next execution of the TX/RX loop.
        g_queueResync.store(true, std::memory_order_release);
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

void MainFrame::OnBerReset(wxCommandEvent&)
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

void MainFrame::OnChangeReportFrequency( wxCommandEvent& )
{    
    wxString freqStr = m_cboReportFrequency->GetValue();
    auto oldFreq = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;

    if (freqStr.Length() > 0)
    {
        double tmp = 0;
        wxNumberFormatter::FromString(freqStr, &tmp);
        
        if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency = round(tmp * 1000);
        }
        else
        {
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

    if (oldFreq != wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency)
    {      
        // Report current frequency to reporters
        for (auto& ptr : wxGetApp().m_reporters)
        {
            ptr->freqChange(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
        }
        
        if (wxGetApp().rigFrequencyController != nullptr && 
            wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0 && 
            (wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges || wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly))
        {
            // Request frequency/mode change on the radio side
            wxGetApp().rigFrequencyController->setFrequency(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
            if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges)
            {
                wxGetApp().rigFrequencyController->setMode(getCurrentMode_());
            }
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
    TopFrame::OnSystemColorChanged(event);
}

void MainFrame::OnNotebookPageChanging(wxAuiNotebookEvent& event)
{
#if 0
    if (m_rbRADE->GetValue())
    {
        auto newSelection = event.GetSelection();
        auto page = m_auiNbookCtrl->GetPage(newSelection);
        
        // Prevent selection of tabs not yet supported by RADE.
        if (page == m_panelScatter || 
            page == m_panelTimeOffset || 
            page == m_panelFreqOffset || 
            page == m_panelTestFrameErrors ||
            page == m_panelTestFrameErrorsHist)
        {
            log_info("Veto attempt at viewing tab %d not supported by RADE", newSelection);
            event.Veto();
            return;
        }
    }
#endif
    
    TopFrame::OnNotebookPageChanging(event);
}

void MainFrame::OnCenterRx(wxCommandEvent&)
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

    wxString prevSelected;    
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
    {
        prevSelected = wxNumberFormatter::ToString(freq, 1);
    }
    else
    {
        prevSelected = wxNumberFormatter::ToString(freq, 4);
    }
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
        m_freqBox->SetLabel(_("Radio Freq. (kHz)"));
    }
    else
    {
        m_freqBox->SetLabel(_("Radio Freq. (MHz)"));
    }
}

void MainFrame::OnResetMicSpkrLevel(wxMouseEvent&)
{
    auto sliderLevel = 0;
    if (g_tx.load(std::memory_order_acquire))
    {
        wxGetApp().appConfiguration.filterConfiguration.micInChannel.volInDB = sliderLevel;
        m_newMicInFilter = true;
    }
    else
    {
        wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.volInDB = sliderLevel;
        m_newSpkOutFilter = true;
    }
    
    wxString fmtString = wxString::Format(MIC_SPKR_LEVEL_FORMAT_STR, wxNumberFormatter::ToString((double)sliderLevel, 1), DECIBEL_STR);
    m_txtMicSpkrLevelNum->SetLabel(fmtString);
}
