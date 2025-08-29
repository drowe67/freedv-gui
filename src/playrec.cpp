/*

    playrec.cpp
    
    Playing and recording files.
*/

#include "main.h"

extern wxMutex g_mutexProtectingCallbackData;
SNDFILE            *g_sfPlayFile;
bool                g_playFileToMicIn;
bool                g_loopPlayFileToMicIn;
int                 g_playFileToMicInEventId;

SNDFILE            *g_sfRecFile;
bool                g_recFileFromRadio;
unsigned int        g_recFromRadioSamples;
int                 g_recFileFromRadioEventId;

SNDFILE            *g_sfRecMicFile;
bool                g_recFileFromMic;

SNDFILE            *g_sfPlayFileFromRadio;
bool                g_playFileFromRadio;
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
    if (g_playFileToMicIn)
    {
        g_playFileToMicIn = false;
        sf_close(g_sfPlayFile);
        g_sfPlayFile = nullptr;
        SetStatusText(wxT(""));
        VoiceKeyerProcessEvent(VK_PLAY_FINISHED);
    }
    g_mutexProtectingCallbackData.Unlock();
}

void MainFrame::StopPlaybackFileFromRadio()
{
    g_mutexProtectingCallbackData.Lock();
    g_playFileFromRadio = false;
    sf_close(g_sfPlayFileFromRadio);
    g_sfPlayFileFromRadio = nullptr;
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

    log_debug("OnPlayFileFromRadio:: %d", (int)g_playFileFromRadio);
    if (g_playFileFromRadio)
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
        g_sfPlayFileFromRadio = sf_open(soundFile.c_str(), SFM_READ, &sfInfo);
        g_sfFs = sfInfo.samplerate;
        if(g_sfPlayFileFromRadio == NULL)
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
        g_playFileFromRadio = true;
    }
}

// extra panel added to file save dialog to set number of seconds to record for

MyExtraRecFilePanel::MyExtraRecFilePanel(wxWindow *parent): wxPanel(parent)
{
    wxBoxSizer *sizerTop = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText* staticText = new wxStaticText(this, wxID_ANY, _("Seconds:"), wxDefaultPosition, wxDefaultSize, 0);
    sizerTop->Add(staticText, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    m_secondsToRecord = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
    m_secondsToRecord->SetToolTip(_("Number of seconds to record for"));
    m_secondsToRecord->SetValue(wxString::Format(wxT("%i"), wxGetApp().appConfiguration.recFileFromRadioSecs.get()));
    m_secondsToRecord->SetMinSize(wxSize(50, -1));
    sizerTop->Add(m_secondsToRecord, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
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
        
        m_menuItemRecFileFromRadio->SetItemLabel(wxString(_("Start Record File - From Radio...")));
        g_mutexProtectingCallbackData.Unlock();
        
        m_audioRecord->SetValue(false);
        m_audioRecord->SetBackgroundColour(wxNullColour);
    }
}

//-------------------------------------------------------------------------
// OnRecFileFromRadio()
//-------------------------------------------------------------------------
void MainFrame::OnRecFileFromRadio(wxCommandEvent& event)
{
    wxUnusedVar(event);

    if (g_sfRecFile != nullptr) {
        StopRecFileFromRadio();
    }
    else {

        wxString    soundFile;
        SF_INFO     sfInfo;

        wxFileDialog openFileDialog(
                                    this,
                                    wxT("Record File From Radio"),
                                    wxGetApp().appConfiguration.recFileFromRadioPath,
                                    wxT("Untitled.wav"),
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

        int sample_rate = wxGetApp().appConfiguration.audioConfiguration.soundCard1In.sampleRate;

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

        // Bug: on Win32 I can't read m_recFileFromRadioSecs, so have hard coded it
#ifdef __WIN32__
        long secs = wxGetApp().appConfiguration.recFileFromRadioSecs;
        g_recFromRadioSamples = sample_rate*(unsigned int)secs;
#else
        // work out number of samples to record

        wxWindow * const ctrl = openFileDialog.GetExtraControl();
        wxString secsString = static_cast<MyExtraRecFilePanel*>(ctrl)->getSecondsToRecord();
        wxLogDebug("test: %s secsString: %s\n", wxT("testing 123"), secsString);

        long secs;
        if (secsString.ToLong(&secs)) {
            wxGetApp().appConfiguration.recFileFromRadioSecs = (unsigned int)secs;
            g_recFromRadioSamples = sample_rate*(unsigned int)secs;
        }
        else {
            wxMessageBox(wxT("Invalid number of Seconds"), wxT("Record File From Radio"), wxOK);
            return;
        }
#endif

        g_sfRecFile = sf_open(soundFile.c_str(), SFM_WRITE, &sfInfo);
        if(g_sfRecFile == NULL)
        {
            wxString strErr = sf_strerror(NULL);
            wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
            return;
        }
        
        // Save path for future use
        wxGetApp().appConfiguration.recFileFromRadioPath = tmpString;

        SetStatusText(wxT("Recording file ") + fileName + wxT(" from radio") , 0);
        m_menuItemRecFileFromRadio->SetItemLabel(wxString(_("Stop Record File - From Radio...")));
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
        
        m_audioRecord->SetValue(true);
        m_audioRecord->SetBackgroundColour(*wxRED);
    }
}

void MainFrame::OnTogBtnRecord( wxCommandEvent& event )
{
    if (g_sfRecFile != nullptr) 
    {
        StopRecFileFromRadio();
    }
    else
    {
        auto currentTime = wxDateTime::Now().Format(_("%Y%m%d-%H%M%S"));
        wxFileName filePath(wxGetApp().appConfiguration.quickRecordPath, wxString::Format(_("FreeDV_FromRadio_%s.wav"), currentTime));
        wxString    soundFile = filePath.GetFullPath();
        SF_INFO     sfInfo;
    
        sfInfo.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
        sfInfo.channels   = 1;
        sfInfo.samplerate = wxGetApp().appConfiguration.audioConfiguration.soundCard1In.sampleRate;
    
        g_recFromRadioSamples = UINT32_MAX; // record until stopped
    
        g_sfRecFile = sf_open(soundFile.c_str(), SFM_WRITE, &sfInfo);
        if(g_sfRecFile == NULL)
        {
            wxString strErr = sf_strerror(NULL);
            wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
            return;
        }

        SetStatusText(wxT("Recording file ") + soundFile + wxT(" from radio"), 0);
        m_menuItemRecFileFromRadio->SetItemLabel(wxString(_("Stop Record File - From Radio...")));
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
        
        m_audioRecord->SetBackgroundColour(*wxRED);
    }
}
