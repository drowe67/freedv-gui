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

SNDFILE            *g_sfPlayFileFromRadio;
bool                g_playFileFromRadio;
int                 g_sfFs;
int                 g_sfTxFs;
bool                g_loopPlayFileFromRadio;
int                 g_playFileFromRadioEventId;
float               g_blink;

SNDFILE            *g_sfRecFileFromModulator;
bool                g_recFileFromModulator = false;
int                 g_recFromModulatorSamples;
int                 g_recFileFromModulatorEventId;

extern FreeDVInterface freedvInterface;

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
        m_menuItemPlayFileToMicIn->SetItemLabel(wxString(_("Start Play File - Mic In...")));
        VoiceKeyerProcessEvent(VK_PLAY_FINISHED);
    }
    g_mutexProtectingCallbackData.Unlock();
}

//-------------------------------------------------------------------------
// OnPlayFileToMicIn()
//-------------------------------------------------------------------------
void MainFrame::OnPlayFileToMicIn(wxCommandEvent& event)
{
    wxUnusedVar(event);

    if(g_playFileToMicIn) {
        StopPlayFileToMicIn();
    }
    else
    {
        wxString    soundFile;
        SF_INFO     sfInfo;

        wxFileDialog openFileDialog(
                                    this,
                                    wxT("Play File to Mic In"),
                                    wxGetApp().m_playFileToMicInPath,
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
        wxFileName::SplitPath(soundFile, &wxGetApp().m_playFileToMicInPath, &fileName, &extension);
        //wxLogDebug("m_playFileToMicInPath: %s", wxGetApp().m_playFileToMicInPath);
        sfInfo.format = 0;

        if(!extension.IsEmpty())
        {
            extension.LowerCase();
            if(extension == wxT("raw"))
            {
                sfInfo.format     = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
                sfInfo.channels   = 1;
                sfInfo.samplerate = freedvInterface.getTxSpeechSampleRate();
            }
        }
        
        g_sfPlayFile = NULL;
        SNDFILE* tmpPlayFile = sf_open(soundFile.c_str(), SFM_READ, &sfInfo);
        if(tmpPlayFile == NULL)
        {
            wxString strErr = sf_strerror(NULL);
            wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
            return;
        }

        g_sfTxFs = sfInfo.samplerate;
        g_sfPlayFile = tmpPlayFile;
        wxWindow * const ctrl = openFileDialog.GetExtraControl();

        // Huh?! I just copied wxWidgets-2.9.4/samples/dialogs ....
        g_loopPlayFileToMicIn = static_cast<MyExtraPlayFilePanel*>(ctrl)->getLoopPlayFileToMicIn();

        SetStatusText(wxT("Playing File: ") + fileName + wxT(" to Mic Input") , 0);
        g_playFileToMicIn = true;
        
        m_menuItemPlayFileToMicIn->SetItemLabel(wxString(_("Stop Play File - Mic In...")));
    }
}

void MainFrame::StopPlaybackFileFromRadio()
{
    g_mutexProtectingCallbackData.Lock();
    g_playFileFromRadio = false;
    sf_close(g_sfPlayFileFromRadio);
    g_sfPlayFileFromRadio = nullptr;
    SetStatusText(wxT(""),0);
    SetStatusText(wxT(""),1);
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

    if (g_verbose) fprintf(stderr,"OnPlayFileFromRadio:: %d\n", (int)g_playFileFromRadio);
    if (g_playFileFromRadio)
    {
        if (g_verbose) fprintf(stderr, "OnPlayFileFromRadio:: Stop\n");
        StopPlaybackFileFromRadio();
    }
    else
    {
        wxString    soundFile;
        SF_INFO     sfInfo;

        wxFileDialog openFileDialog(
                                    this,
                                    wxT("Play File - From Radio"),
                                    wxGetApp().m_playFileFromRadioPath,
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
        wxFileName::SplitPath(soundFile, &wxGetApp().m_playFileFromRadioPath, &fileName, &extension);
        //wxLogDebug("m_playFileToFromRadioPath: %s", wxGetApp().m_playFileFromRadioPath);
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

        wxWindow * const ctrl = openFileDialog.GetExtraControl();

        // Huh?! I just copied wxWidgets-2.9.4/samples/dialogs ....
        g_loopPlayFileFromRadio = static_cast<MyExtraPlayFilePanel*>(ctrl)->getLoopPlayFileToMicIn();

        SetStatusText(wxT("Playing into from radio"), 0);
        if(extension == wxT("raw")) {
            wxString stringnumber = wxString::Format(wxT("%d"), (int)sfInfo.samplerate);
            SetStatusText(wxT("raw file assuming Fs=") + stringnumber, 1);
        }
        if (g_verbose) fprintf(stderr, "OnPlayFileFromRadio:: Playing File Fs = %d\n", (int)sfInfo.samplerate);
        m_menuItemPlayFileFromRadio->SetItemLabel(wxString(_("Stop Play File - From Radio...")));
        g_playFileFromRadio = true;
        g_blink = 0.0;
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
    m_secondsToRecord->SetValue(wxString::Format(wxT("%i"), wxGetApp().m_recFileFromRadioSecs));
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
    if (g_recFileFromRadio)
    {
        if (g_verbose) fprintf(stderr, "Stopping Record....\n");
        g_mutexProtectingCallbackData.Lock();
        g_recFileFromRadio = false;
        sf_close(g_sfRecFile);
        g_sfRecFile = nullptr;
        SetStatusText(wxT(""));
        
        m_menuItemRecFileFromRadio->SetItemLabel(wxString(_("Start Record File - From Radio...")));
        
        wxMessageBox(wxT("Recording radio output to file complete")
                     , wxT("Recording radio Output"), wxOK);
        
        g_mutexProtectingCallbackData.Unlock();
        
        m_audioRecord->SetValue(g_recFileFromRadio);
    }
}

//-------------------------------------------------------------------------
// OnRecFileFromRadio()
//-------------------------------------------------------------------------
void MainFrame::OnRecFileFromRadio(wxCommandEvent& event)
{
    wxUnusedVar(event);

    if (g_recFileFromRadio) {
        StopRecFileFromRadio();
    }
    else {

        wxString    soundFile;
        SF_INFO     sfInfo;

        wxFileDialog openFileDialog(
                                    this,
                                    wxT("Record File From Radio"),
                                    wxGetApp().m_recFileFromRadioPath,
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
        wxFileName::SplitPath(soundFile, &wxGetApp().m_recFileFromRadioPath, &fileName, &extension);
        wxLogDebug("m_recFileFromRadioPath: %s", wxGetApp().m_recFileFromRadioPath);
        wxLogDebug("soundFile: %s", soundFile);
        sfInfo.format = 0;

        int sample_rate = wxGetApp().m_soundCard1InSampleRate;

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
        long secs = wxGetApp().m_recFileFromRadioSecs;
        g_recFromRadioSamples = sample_rate*(unsigned int)secs;
#else
        // work out number of samples to record

        wxWindow * const ctrl = openFileDialog.GetExtraControl();
        wxString secsString = static_cast<MyExtraRecFilePanel*>(ctrl)->getSecondsToRecord();
        wxLogDebug("test: %s secsString: %s\n", wxT("testing 123"), secsString);

        long secs;
        if (secsString.ToLong(&secs)) {
            wxGetApp().m_recFileFromRadioSecs = (unsigned int)secs;
            //printf(" secondsToRecord: %d\n",  (unsigned int)secs);
            g_recFromRadioSamples = sample_rate*(unsigned int)secs;
            //printf("g_recFromRadioSamples: %d\n", g_recFromRadioSamples);
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

        SetStatusText(wxT("Recording File: ") + fileName + wxT(" From Radio") , 0);
        m_menuItemRecFileFromRadio->SetItemLabel(wxString(_("Stop Record File - From Radio...")));
        g_recFileFromRadio = true;
    }

    m_audioRecord->SetValue(g_recFileFromRadio);
}

void MainFrame::OnTogBtnRecord( wxCommandEvent& event )
{
    if (g_recFileFromRadio) 
    {
        StopRecFileFromRadio();
    }
    else
    {
        auto currentTime = wxDateTime::Now().Format(_("%Y%m%d-%H%M%S"));
        wxFileName filePath(wxGetApp().m_txtQuickRecordPath, wxString::Format(_("FreeDV_FromRadio_%s.wav"), currentTime));
        wxString    soundFile = filePath.GetFullPath();
        SF_INFO     sfInfo;
    
        sfInfo.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
        sfInfo.channels   = 1;
        sfInfo.samplerate = wxGetApp().m_soundCard1InSampleRate;
    
        g_recFromRadioSamples = UINT32_MAX; // record until stopped
    
        g_sfRecFile = sf_open(soundFile.c_str(), SFM_WRITE, &sfInfo);
        if(g_sfRecFile == NULL)
        {
            wxString strErr = sf_strerror(NULL);
            wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
            return;
        }

        SetStatusText(wxT("Recording File: ") + soundFile + wxT(" From Radio") , 0);
        m_menuItemRecFileFromRadio->SetItemLabel(wxString(_("Stop Record File - From Radio...")));
        g_recFileFromRadio = true;
    }
}

void MainFrame::StopRecFileFromModulator()
{
    // If the event loop takes a while to execute, we may end up being called
    // multiple times. We don't want to repeat the following more than once.
    if (g_recFileFromModulator) {
        g_mutexProtectingCallbackData.Lock();
        g_recFileFromModulator = false;
        g_recFromModulatorSamples = 0;
        sf_close(g_sfRecFileFromModulator);
        g_sfRecFileFromModulator = nullptr;
        SetStatusText(wxT(""));
        m_menuItemRecFileFromModulator->SetItemLabel(wxString(_("Start Record File - From Modulator...")));
        wxMessageBox(wxT("Recording modulator output to file complete")
                     , wxT("Recording Modulation Output"), wxOK);
        g_mutexProtectingCallbackData.Unlock();
    }
}

//-------------------------------------------------------------------------
// OnRecFileFromModulator()
//-------------------------------------------------------------------------
void MainFrame::OnRecFileFromModulator(wxCommandEvent& event)
{
    wxUnusedVar(event);

    if (g_recFileFromModulator) {
        StopRecFileFromModulator();
    }
    else {

        wxString    soundFile;
        SF_INFO     sfInfo;

        if (!freedvInterface.isRunning()) {
            wxMessageBox(wxT("You need to press the Control - Start button before you can configure recording")
                         , wxT("Recording Modulation Output"), wxOK);
            return;
        }

         wxFileDialog openFileDialog(
                                    this,
                                    wxT("Record File From Modulator"),
                                    wxGetApp().m_recFileFromModulatorPath,
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
        wxFileName::SplitPath(soundFile, &wxGetApp().m_recFileFromModulatorPath, &fileName, &extension);
        wxLogDebug("m_recFileFromModulatorPath: %s", wxGetApp().m_recFileFromModulatorPath);
        wxLogDebug("soundFile: %s", soundFile);
        sfInfo.format = 0;

        int sample_rate = wxGetApp().m_soundCard1OutSampleRate;

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

        // Bug: on Win32 I can't read m_recFileFromModemSecs, so have hard coded it
#ifdef __WIN32__
        long secs = wxGetApp().m_recFileFromModulatorSecs;
        g_recFromModulatorSamples = sample_rate * (unsigned int)secs;
#else
        // work out number of samples to record

        wxWindow * const ctrl = openFileDialog.GetExtraControl();
        wxString secsString = static_cast<MyExtraRecFilePanel*>(ctrl)->getSecondsToRecord();
        wxLogDebug("test: %s secsString: %s\n", wxT("testing 123"), secsString);

        long secs;
        if (secsString.ToLong(&secs)) {
            wxGetApp().m_recFileFromModulatorSecs = (unsigned int)secs;
            //printf(" secondsToRecord: %d\n",  (unsigned int)secs);
            g_recFromModulatorSamples = sample_rate*(unsigned int)secs;
            //printf("g_recFromRadioSamples: %d\n", g_recFromRadioSamples);
        }
        else {
            wxMessageBox(wxT("Invalid number of Seconds"), wxT("Record File From Modulator"), wxOK);
            return;
        }
#endif

        g_sfRecFileFromModulator = sf_open(soundFile.c_str(), SFM_WRITE, &sfInfo);
        if(g_sfRecFileFromModulator == NULL)
        {
            wxString strErr = sf_strerror(NULL);
            wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
            return;
        }

        SetStatusText(wxT("Recording File: ") + fileName + wxT(" From Modulator") , 0);
        m_menuItemRecFileFromModulator->SetItemLabel(wxString(_("Stop Record File - From Modulator...")));
        g_recFileFromModulator = true;
    }

}
