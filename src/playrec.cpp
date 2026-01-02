/*

    playrec.cpp
    
    Playing and recording files.
*/

#include "main.h"

extern wxMutex g_mutexProtectingCallbackData;
std::atomic<SNDFILE*> g_sfPlayFile;
std::atomic<bool>                g_playFileToMicIn;
bool                g_loopPlayFileToMicIn;
int                 g_playFileToMicInEventId;

SNDFILE            *g_sfRecFile;
bool                g_recFileFromRadio;
unsigned int        g_recFromRadioSamples;
int                 g_recFileFromRadioEventId;

SNDFILE            *g_sfRecMicFile;
bool                g_recFileFromMic;

SNDFILE* g_sfRecDecoderFile;
bool g_recFileFromDecoder;
int                 g_recFileFromDecoderEventId;

std::atomic<SNDFILE*> g_sfPlayFileFromRadio;
std::atomic<bool>                g_playFileFromRadio;
int                 g_sfFs;
int                 g_sfTxFs;
bool                g_loopPlayFileFromRadio;
int                 g_playFileFromRadioEventId;

SNDFILE            *g_sfRecFileFromModulator;
bool                g_recFileFromModulator = false;
int                 g_recFromModulatorSamples;
int                 g_recFileFromModulatorEventId;

extern FreeDVInterface freedvInterface;
extern std::atomic<bool> g_tx;

// extra panel added to file open dialog to add loop checkbox
MyExtraPlayFilePanel::MyExtraPlayFilePanel(wxWindow *parent): wxPanel(parent)
{
    m_cb = new wxCheckBox(this, -1, wxT("Loop"));
    m_cb->SetToolTip(_("When checked file will repeat forever"));
    m_cb->SetValue(g_loopPlayFileToMicIn);

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
        g_sfFs = sfInfo.samplerate;
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
        g_loopPlayFileFromRadio = static_cast<MyExtraPlayFilePanel*>(ctrl)->getLoopPlayFileToMicIn();

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

// extra panel added to file save dialog to set number of seconds to record for

MyExtraRecFilePanel::MyExtraRecFilePanel(wxWindow *parent): wxPanel(parent)
{
    wxBoxSizer *sizerTop = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText* label = new wxStaticText(this, wxID_ANY, _("Recording Type:"));
    sizerTop->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    
    rawRecording_ = new wxRadioButton(this, wxID_ANY, _("Raw"));
    decodedRecording_ = new wxRadioButton(this, wxID_ANY, _("Decoded"));
    rawRecording_->SetValue(true);

    sizerTop->Add(rawRecording_, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    sizerTop->Add(decodedRecording_, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    SetSizerAndFit(sizerTop);
}

static wxWindow* createMyExtraRecFilePanel(wxWindow *parent)
{
    return new MyExtraRecFilePanel(parent);
}

void MainFrame::StopRecFileFromRadio()
{
    if (g_sfRecFile != nullptr)
    {
        log_debug("Stopping Record....");
        g_mutexProtectingCallbackData.Lock();
        g_recFileFromRadio = false;
        g_recFileFromModulator = false;
        sf_close(g_sfRecFile);
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
    if (g_sfRecDecoderFile != nullptr)
    {
        log_debug("Stopping Record....");
        g_mutexProtectingCallbackData.Lock();
        g_recFileFromDecoder = false;
        sf_close(g_sfRecDecoderFile);
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

    if (g_sfRecFile != nullptr) {
        StopRecFileFromRadio();
    }
    else if (g_sfRecDecoderFile != nullptr) {
        StopRecFileFromDecoder();
    }
    else {

        wxString    soundFile;
        SF_INFO     sfInfo;

        auto currentTime = wxDateTime::Now().Format(_("%Y%m%d-%H%M%S"));
        wxFileName filePath(wxGetApp().appConfiguration.recFileFromRadioPath, wxString::Format(_("FreeDV_FromRadio_%s.wav"), currentTime));

        wxFileDialog openFileDialog(
                                    this,
                                    wxT("Record File From Radio"),
                                    wxGetApp().appConfiguration.recFileFromRadioPath,
                                    filePath.GetFullName(),
                                    wxT("WAV and RAW files (*.wav;*.raw)|*.wav;*.raw|")
                                    wxT("All files (*.*)|*.*"),
                                    wxFD_SAVE
                                    );

        // add the loop check box
        openFileDialog.SetExtraControlCreator(&createMyExtraRecFilePanel);

        // Default to WAV.
        openFileDialog.SetFilterIndex(0);
        
        if(openFileDialog.ShowModal() == wxID_CANCEL)
        {
            return;     // the user changed their mind...
        }

        wxString fileName, extension;
        soundFile = openFileDialog.GetPath();
        wxString tmpString = wxGetApp().appConfiguration.recFileFromRadioPath;
        wxFileName::SplitPath(soundFile, &tmpString, &fileName, &extension);
        wxLogDebug("m_recFileFromRadioPath: %s", wxGetApp().appConfiguration.recFileFromRadioPath.get());
        wxLogDebug("soundFile: %s", soundFile);
        sfInfo.format = 0;

        int sample_rate = RECORD_FILE_SAMPLE_RATE;

        if(!extension.IsEmpty())
        {
            extension.LowerCase();
            if(extension == wxT("raw"))
            {
                sfInfo.format     = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
                sfInfo.channels   = 1;
                sfInfo.samplerate = sample_rate;
            }
            else if(extension == wxT("wav"))
            {
                sfInfo.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
                sfInfo.channels   = 1;
                sfInfo.samplerate = sample_rate;
            } else {
                wxMessageBox(wxT("Invalid file format"), wxT("Record File From Radio"), wxOK);
                return;
            }
        }
        else {
            wxMessageBox(wxT("Invalid file format"), wxT("Record File From Radio"), wxOK);
            return;
        }

        g_recFromRadioSamples = UINT32_MAX; // record until stopped

        wxWindow * const ctrl = openFileDialog.GetExtraControl();
        bool decodedRecording = static_cast<MyExtraRecFilePanel*>(ctrl)->getDecodedRecording();
        if (decodedRecording)
        {
            g_sfRecDecoderFile = sf_open(soundFile.c_str(), SFM_WRITE, &sfInfo);
            if(g_sfRecDecoderFile == NULL)
            {
                wxString strErr = sf_strerror(NULL);
                wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
                return;
            }

            SetStatusText(wxT("Recording file ") + soundFile + wxT(" from decoder"), 0);
            g_recFileFromDecoder = true;
        }
        else
        {
            g_sfRecFile = sf_open(soundFile.c_str(), SFM_WRITE, &sfInfo);
            if(g_sfRecFile == NULL)
            {
                wxString strErr = sf_strerror(NULL);
                wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
                return;
            }
        
            SetStatusText(wxT("Recording file ") + fileName + wxT(" from radio") , 0);
            g_sfRecFileFromModulator = g_sfRecFile;
        
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

        // Save path for future use
        wxGetApp().appConfiguration.recFileFromRadioPath = tmpString;

        m_audioRecord->SetValue(true);
        m_audioRecord->SetBackgroundColour(*wxRED);
    }
}