/*

    playrec.cpp
    
    Playing and recording files.
*/

#include "main.h"

#include "gui/dialogs/begin_recording.h"
#include "gui/dialogs/freedv_reporter.h"

extern wxMutex g_mutexProtectingCallbackData;
std::atomic<SNDFILE*> g_sfPlayFile;
std::atomic<bool>                g_playFileToMicIn;
std::atomic<bool>   g_loopPlayFileToMicIn;
int                 g_playFileToMicInEventId;

std::atomic<SNDFILE*> g_sfRecFile{nullptr};
std::atomic<bool>     g_recFileFromRadio{false};
std::atomic<unsigned int> g_recFromRadioSamples;
int                 g_recFileFromRadioEventId;

std::atomic<SNDFILE*> g_sfRecMicFile;
std::atomic<bool>   g_recFileFromMic;

std::atomic<SNDFILE*> g_sfRecDecoderFile{nullptr};
std::atomic<bool>     g_recFileFromDecoder{false};
int                 g_recFileFromDecoderEventId;

std::atomic<SNDFILE*> g_sfPlayFileFromRadio;
std::atomic<bool>                g_playFileFromRadio;
std::atomic<int>    g_sfFs;
std::atomic<int>    g_sfTxFs;
std::atomic<bool>   g_loopPlayFileFromRadio;
int                 g_playFileFromRadioEventId;

std::atomic<SNDFILE*>            g_sfRecFileFromModulator;
std::atomic<bool>                g_recFileFromModulator;

// Time-Out Timer beep: injected into the speaker output path during the warning window.
std::atomic<bool>     g_totBeepActive(false);

int                 g_recFromModulatorSamples;
int                 g_recFileFromModulatorEventId;

extern FreeDVInterface freedvInterface;
extern std::atomic<bool> g_tx;

// extra panel added to file open dialog to add loop checkbox
MyExtraPlayFilePanel::MyExtraPlayFilePanel(wxWindow *parent): wxPanel(parent)
{
    m_cb = new wxCheckBox(this, -1, wxT("Loop"));
    m_cb->SetToolTip(_("When checked file will repeat forever"));
    m_cb->SetValue(g_loopPlayFileToMicIn.load(std::memory_order_relaxed));

    // bug: I can't this to align right.....
    wxBoxSizer *sizerTop = new wxBoxSizer(wxHORIZONTAL);
    sizerTop->Add(m_cb, 0, 0, 0);
    SetSizerAndFit(sizerTop);
}

static wxWindow* createMyExtraPlayFilePanel(wxWindow *parent)
{
    return new MyExtraPlayFilePanel(parent);
}

void MainFrame::StopPlayFileToMicIn(void)
{
    g_mutexProtectingCallbackData.Lock();
    if (g_playFileToMicIn.load(std::memory_order_acquire))
    {
        g_playFileToMicIn.store(false, std::memory_order_release);
        sf_close(g_sfPlayFile.load(std::memory_order_acquire));
        g_sfPlayFile.store(nullptr, std::memory_order_release);
        SetStatusText(wxT(""));
        VoiceKeyerProcessEvent(VK_PLAY_FINISHED);
    }
    g_mutexProtectingCallbackData.Unlock();
}

void MainFrame::StopPlaybackFileFromRadio()
{
    g_mutexProtectingCallbackData.Lock();
    g_playFileFromRadio.store(false, std::memory_order_release);
    auto tmp = g_sfPlayFileFromRadio.load(std::memory_order_acquire);
    sf_close(tmp);
    g_sfPlayFileFromRadio.store(nullptr, std::memory_order_release);
    SetStatusText(wxT(""));
    m_menuItemPlayFileFromRadio->SetItemLabel(wxString(_("Start Play File - From Radio...")));
    g_mutexProtectingCallbackData.Unlock();
}

//-------------------------------------------------------------------------
// OnPlayFileFromRadio()
// This puppy "plays" a recorded file into the demodulator input, allowing us
// to replay off air signals.
//-------------------------------------------------------------------------
void MainFrame::OnPlayFileFromRadio(wxCommandEvent& event)
{
    wxUnusedVar(event);

    log_debug("OnPlayFileFromRadio:: %d", (int)g_playFileFromRadio.load(std::memory_order_acquire));
    if (g_playFileFromRadio.load(std::memory_order_acquire))
    {
        log_debug("OnPlayFileFromRadio:: Stop");
        StopPlaybackFileFromRadio();
    }
    else
    {
        wxString    soundFile;
        SF_INFO     sfInfo;

        wxFileDialog openFileDialog(
                                    this,
                                    wxT("Play File - From Radio"),
                                    wxGetApp().appConfiguration.playFileFromRadioPath,
                                    wxEmptyString,
                                    wxT("WAV and RAW files (*.wav;*.raw)|*.wav;*.raw|")
                                    wxT("All files (*.*)|*.*"),
                                    wxFD_OPEN | wxFD_FILE_MUST_EXIST
                                    );

        // add the loop check box
        openFileDialog.SetExtraControlCreator(&createMyExtraPlayFilePanel);

        if(openFileDialog.ShowModal() == wxID_CANCEL)
        {
            return;     // the user changed their mind...
        }

        wxString fileName, extension;
        soundFile = openFileDialog.GetPath();
        wxString tmpString = wxGetApp().appConfiguration.playFileFromRadioPath;
        wxFileName::SplitPath(soundFile, &tmpString, &fileName, &extension);
        sfInfo.format = 0;

        if(!extension.IsEmpty())
        {
            extension.LowerCase();
            if(extension == wxT("raw"))
            {
                sfInfo.format     = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
                sfInfo.channels   = 1;
                sfInfo.samplerate = freedvInterface.getRxModemSampleRate();
            }
        }
        g_sfPlayFileFromRadio.store(sf_open(soundFile.c_str(), SFM_READ, &sfInfo), std::memory_order_release);
        g_sfFs.store(sfInfo.samplerate, std::memory_order_release);
        if(g_sfPlayFileFromRadio.load(std::memory_order_acquire) == NULL)
        {
            wxString strErr = sf_strerror(NULL);
            wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
            return;
        }
        
        // Save path for future use
        wxGetApp().appConfiguration.playFileFromRadioPath = tmpString;

        wxWindow * const ctrl = openFileDialog.GetExtraControl();

        // Huh?! I just copied wxWidgets-2.9.4/samples/dialogs ....
        g_loopPlayFileFromRadio.store(static_cast<MyExtraPlayFilePanel*>(ctrl)->getLoopPlayFileToMicIn(), std::memory_order_relaxed);

        wxString statusText = "";
        if(extension == wxT("raw")) {
            statusText = wxString::Format(wxT("Playing raw file %s as radio input (assuming Fs=%d)"), soundFile, (int)sfInfo.samplerate);
        }
        else
        {
            statusText = wxString::Format(wxT("Playing file %s as radio input"), soundFile);
        }
        SetStatusText(statusText, 0);
        log_debug("OnPlayFileFromRadio:: Playing File Fs = %d", (int)sfInfo.samplerate);
        m_menuItemPlayFileFromRadio->SetItemLabel(wxString(_("Stop Play File - From Radio...")));
        g_playFileFromRadio.store(true, std::memory_order_release);
    }
}

void MainFrame::StopRecFileFromRadio()
{
    if (g_sfRecFile.load(std::memory_order_acquire) != nullptr)
    {
        log_debug("Stopping Record....");
        g_mutexProtectingCallbackData.Lock();
        g_recFileFromRadio = false;
        g_recFileFromModulator = false;
        sf_close(g_sfRecFile.load(std::memory_order_acquire));
        g_sfRecFile = nullptr;
        g_sfRecFileFromModulator = nullptr;
        SetStatusText(wxT(""));
        
        g_mutexProtectingCallbackData.Unlock();
        
        m_audioRecord->SetValue(false);
        m_audioRecord->SetBackgroundColour(wxNullColour);
    }
}

void MainFrame::StopRecFileFromDecoder()
{
    if (g_sfRecDecoderFile.load(std::memory_order_acquire) != nullptr)
    {
        log_debug("Stopping Record....");
        g_mutexProtectingCallbackData.Lock();
        g_recFileFromDecoder = false;
        sf_close(g_sfRecDecoderFile.load(std::memory_order_acquire));
        g_sfRecDecoderFile = nullptr;
        SetStatusText(wxT(""));
        
        g_mutexProtectingCallbackData.Unlock();
        
        m_audioRecord->SetValue(false);
        m_audioRecord->SetBackgroundColour(wxNullColour);
    }
}

//-------------------------------------------------------------------------
// OnTogBtnRecord()
//-------------------------------------------------------------------------
void MainFrame::OnTogBtnRecord(wxCommandEvent& event)
{
    wxUnusedVar(event);

    if (g_sfRecFile.load(std::memory_order_acquire) != nullptr || g_sfRecDecoderFile.load(std::memory_order_acquire) != nullptr) 
    {
        if (g_sfRecFile.load(std::memory_order_acquire) != nullptr)
        {
            StopRecFileFromRadio();
        }
        if (g_sfRecDecoderFile.load(std::memory_order_acquire) != nullptr)
        {
            StopRecFileFromDecoder();
        }
    }
    else 
    {
        wxString dxCall;
        auto selected = m_lastReportedCallsignListView->GetFirstSelected();
        if (wxGetApp().lastSelectedLoggingRow == MainApp::MAIN_WINDOW && selected != -1)
        {        
            // Get callsign and RX frequency
            dxCall = m_lastReportedCallsignListView->GetItemText(selected, 0);
            log_info("Using %s from main window drop-down list as default recording suffix", (const char*)dxCall.ToUTF8());
        }
        else if (
            m_reporterDialog != nullptr && 
            wxGetApp().lastSelectedLoggingRow == MainApp::FREEDV_REPORTER && 
            m_reporterDialog->getSelectedCallsignInfo(dxCall))
        {
            log_info("Using %s from FreeDV Reporter as default recording suffix", (const char*)dxCall.ToUTF8());
        }
        
        BeginRecordingDialog recordDialog(this, dxCall);
        if (recordDialog.ShowModal() == wxOK)
        {
            wxString    soundFileRaw;
            wxString    soundFileDecoded;
            SF_INFO     sfInfo;
            auto currentTime = wxDateTime::Now().Format(_("%Y%m%d-%H%M%S"));

            wxString filenameSuffix = currentTime;
            wxString recordingSuffix = recordDialog.getRecordingSuffix();
            if (recordingSuffix != "")
            {
                filenameSuffix += wxString::Format(_("_%s"), recordingSuffix);
            }

            wxString extension;
#if !defined(SNDFILE_NO_MP3_SUPPORT)
            if (recordDialog.isMp3Format())
            {
                extension = _("mp3");
            }
            else
#endif // !defined(SNDFILE_NO_MP3_SUPPORT)
            {
                extension = _("wav");
            }

            if (recordDialog.isRawRecording())
            {
                soundFileRaw = wxFileName(
                    wxGetApp().appConfiguration.quickRecordRawPath,
                    wxString::Format(_("%s_%s.%s"), _("FDV_FromRadio"), filenameSuffix, extension))
                    .GetFullPath();
                log_info("Recording raw to %s", (const char*)soundFileRaw.ToUTF8());
            }
            if (recordDialog.isDecodedRecording())
            {
                soundFileDecoded = wxFileName(
                    wxGetApp().appConfiguration.quickRecordDecodedPath,
                    wxString::Format(_("%s_%s.%s"), _("FDV_FromDecoder"), filenameSuffix, extension))
                    .GetFullPath();
                log_info("Recording decoded to %s", (const char*)soundFileDecoded.ToUTF8());
            }

#if !defined(SNDFILE_NO_MP3_SUPPORT)
            if (recordDialog.isMp3Format())
            {
                sfInfo.format     = SF_FORMAT_MPEG | SF_FORMAT_MPEG_LAYER_III;
            }
            else
#endif // !defined(SNDFILE_NO_MP3_SUPPORT)
            {
                sfInfo.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
            }
            
            sfInfo.channels   = 1;
            sfInfo.samplerate = RECORD_FILE_SAMPLE_RATE;

            g_recFromRadioSamples.store(UINT32_MAX, std::memory_order_release); // record until stopped

            if (recordDialog.isRawRecording())
            {
                g_sfRecFile = sf_open(soundFileRaw.c_str(), SFM_WRITE, &sfInfo);
                if (g_sfRecFile.load(std::memory_order_acquire) == NULL)
                {
                    wxString strErr = sf_strerror(NULL);
                    wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
                    m_audioRecord->SetValue(false);
                    return;
                }
            
                g_sfRecFileFromModulator = g_sfRecFile.load(std::memory_order_acquire);
            
                if (!g_tx.load(std::memory_order_acquire))
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

            if (recordDialog.isDecodedRecording())
            {
                g_sfRecDecoderFile = sf_open(soundFileDecoded.c_str(), SFM_WRITE, &sfInfo);
                if (g_sfRecDecoderFile.load(std::memory_order_acquire) == NULL)
                {
                    wxString strErr = sf_strerror(NULL);
                    wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
                    if (g_sfRecFile.load(std::memory_order_acquire) != nullptr)
                    {
                        // Roll back the raw recording opened above.
                        StopRecFileFromRadio();
                    }
                    m_audioRecord->SetValue(false);
                    return;
                }

                g_recFileFromDecoder = true;
            }

            wxString statusText;
            if (recordDialog.isRawRecording() && recordDialog.isDecodedRecording())
            {
                statusText = wxT("Recording file ") + soundFileRaw + wxT(" from radio and file ") + soundFileDecoded + wxT(" from decoder");
            }
            else if (recordDialog.isRawRecording())
            {
                statusText = wxT("Recording file ") + soundFileRaw + wxT(" from radio");
            }
            else
            {
                statusText = wxT("Recording file ") + soundFileDecoded + wxT(" from decoder");
            }
            SetStatusText(statusText, 0);

            m_audioRecord->SetValue(true);
            m_audioRecord->SetBackgroundColour(*wxRED);
        }
        else
        {
            m_audioRecord->SetValue(false);
        }
    }
}
