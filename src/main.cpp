//==========================================================================
// Name:            main.cpp
//
// Purpose:         FreeDV main()
// Created:         Apr. 9, 2012
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

#include <inttypes.h>
#include <time.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <deque>
#include <random>
#include <chrono>
#include <climits>
#include <wx/cmdline.h>
#include <wx/stdpaths.h>
#include <wx/uiaction.h>

#include "version.h"
#include "main.h"
#include "os/os_interface.h"
#include "freedv_interface.h"
#include "audio/AudioEngineFactory.h"
#include "codec2_fdmdv.h"
#include "pipeline/TxRxThread.h"
#include "reporting/pskreporter.h"
#include "reporting/FreeDVReporter.h"

#include "gui/dialogs/dlg_options.h"
#include "gui/dialogs/dlg_filter.h"
#include "gui/dialogs/dlg_easy_setup.h"
#include "gui/dialogs/freedv_reporter.h"

#include "util/logging/ulog.h"

#include "rade_api.h"

using namespace std::chrono_literals;

#define wxUSE_FILEDLG   1
#define wxUSE_LIBPNG    1
#define wxUSE_LIBJPEG   1
#define wxUSE_GIF       1
#define wxUSE_PCX       1
#define wxUSE_LIBTIFF   1

extern "C" {
    extern void golay23_init(void);
}

#define EXPIRES_AFTER_TIMEFRAME (wxDateSpan(0, 6, 0)) /* 6 months */

//-------------------------------------------------------------------
// Bunch of globals used for communication with sound card call
// back functions
// ------------------------------------------------------------------

// freedv states
int                 g_Nc;
int                 g_mode;

FreeDVInterface     freedvInterface;
float               g_pwr_scale;
int                 g_clip;
int                 g_freedv_verbose;
bool                g_queueResync;

// test Frames
int                 g_testFrames;
int                 g_test_frame_sync_state;
int                 g_test_frame_count;
int                 g_channel_noise;
int                 g_resyncs;
float               g_sig_pwr_av = 0.0;
short              *g_error_hist, *g_error_histn;
float               g_tone_phase;

// time averaged magnitude spectrum used for waterfall and spectrum display
float               g_avmag[MODEM_STATS_NSPEC];

// TX level for attenuation
int g_txLevel = 0;

// GUI controls that affect rx and tx processes
int   g_SquelchActive;
float g_SquelchLevel;
int   g_analog;
int   g_tx;
float g_snr;
bool  g_half_duplex;
bool  g_voice_keyer_tx;
SRC_STATE  *g_spec_src;  // sample rate converter for spectrum

// sending and receiving Call Sign data
struct FIFO         *g_txDataInFifo;
struct FIFO         *g_rxDataOutFifo;

// tx/rx processing states
int                 g_State, g_prev_State;
paCallBackData     *g_rxUserdata;
int                 g_dump_timing;
int                 g_dump_fifo_state;
time_t              g_sync_time;

// FIFOs used for plotting waveforms
struct FIFO        *g_plotDemodInFifo;
struct FIFO        *g_plotSpeechOutFifo;
struct FIFO        *g_plotSpeechInFifo;

// Soundcard config
int                 g_nSoundCards;

// PortAudio over/underflow counters

int                 g_infifo1_full;
int                 g_outfifo1_empty;
int                 g_infifo2_full;
int                 g_outfifo2_empty;
int                 g_AEstatus1[4];
int                 g_AEstatus2[4];

// playing and recording from sound files

extern SNDFILE            *g_sfPlayFile;
extern bool                g_playFileToMicIn;
extern bool                g_loopPlayFileToMicIn;
extern int                 g_playFileToMicInEventId;

extern SNDFILE            *g_sfRecFile;
extern bool                g_recFileFromRadio;
extern unsigned int        g_recFromRadioSamples;
extern int                 g_recFileFromRadioEventId;

extern SNDFILE            *g_sfPlayFileFromRadio;
extern bool                g_playFileFromRadio;
extern int                 g_sfFs;
extern int                 g_sfTxFs;
extern bool                g_loopPlayFileFromRadio;
extern int                 g_playFileFromRadioEventId;

extern SNDFILE            *g_sfRecFileFromModulator;
extern bool                g_recFileFromModulator;
extern int                 g_recFileFromModulatorEventId;

extern SNDFILE            *g_sfRecMicFile;
extern bool                g_recFileFromMic;
extern bool                g_recVoiceKeyerFile;

wxWindow           *g_parent;

// Click to tune rx and tx frequency offset states
float               g_RxFreqOffsetHz;
float               g_TxFreqOffsetHz;

// experimental mutex to make sound card callbacks mutually exclusive
// TODO: review code and see if we need this any more, as fifos should
// now be thread safe

wxMutex g_mutexProtectingCallbackData(wxMUTEX_RECURSIVE);

// TX mode change mutex
wxMutex txModeChangeMutex;

// End of TX state control
bool endingTx;

// Option test file to log samples

FILE *ftest;

// Config file management 
wxConfigBase *pConfig = NULL;

// Unit test management
wxString testName;
wxString utFreeDVMode;
wxString utTxFile;
wxString utRxFile;
wxString utTxFeatureFile;
wxString utRxFeatureFile;
int utTxTimeSeconds;
int utTxAttempts;

// WxWidgets - initialize the application

IMPLEMENT_APP(MainApp);

std::mutex logMutex;
static void LogLockFunction_(bool lock, void *lock_arg)
{
    if (lock)
    {
        logMutex.lock();
    }
    else
    {
        logMutex.unlock();
    }
}

void MainApp::UnitTest_()
{
    // List audio devices
    auto engine = AudioEngineFactory::GetAudioEngine();
    engine->start();
    for (auto& dev : engine->getAudioDeviceList(IAudioEngine::AUDIO_ENGINE_IN))
    {
        log_info("Input audio device: %s (ID %d, sample rate %d, valid channels: %d-%d)", (const char*)dev.name.ToUTF8(), dev.deviceId,  dev.defaultSampleRate, dev.minChannels, dev.maxChannels);
    }
    for (auto& dev : engine->getAudioDeviceList(IAudioEngine::AUDIO_ENGINE_OUT))
    {
        log_info("Output audio device: %s (ID %d, sample rate %d, valid channels: %d-%d)", (const char*)dev.name.ToUTF8(), dev.deviceId,  dev.defaultSampleRate, dev.minChannels, dev.maxChannels);
    }
    engine->stop();

    // Bring window to the front
    CallAfter([&]() {
        frame->Iconize(false);
        frame->SetFocus();
        frame->Raise();
        frame->Show(true);
    });
    
    // Wait 100ms for FreeDV to come to foreground
    std::this_thread::sleep_for(100ms);

    // Select FreeDV mode. Note, 2020 is deprecated so not testable here.
    wxRadioButton* modeBtn = nullptr;
    if (utFreeDVMode == "RADEV1")
    {
        modeBtn = frame->m_rbRADE;
    }
    /*else if (utFreeDVMode == "700C")
    {
        modeBtn = frame->m_rb700c;
    }*/
    else if (utFreeDVMode == "700D")
    {
        modeBtn = frame->m_rb700d;
    }
    else if (utFreeDVMode == "700E")
    {
        modeBtn = frame->m_rb700e;
    }
    /*else if (utFreeDVMode == "800XA")
    {
        modeBtn = frame->m_rb800xa;
    }*/
    else if (utFreeDVMode == "1600")
    {
        modeBtn = frame->m_rb1600;
    }
    
    if (modeBtn != nullptr)
    {
        log_info("Firing mode change");
        /*sim.MouseMove(modeBtn->GetScreenPosition());
        sim.MouseClick();*/
        CallAfter([&]() {
            modeBtn->SetValue(true);
            wxCommandEvent* modeEvent = new wxCommandEvent(wxEVT_RADIOBUTTON, modeBtn->GetId());
            modeEvent->SetEventObject(modeBtn);
            QueueEvent(modeEvent);
        });
    }
    
    // Fire event to start FreeDV
    log_info("Firing start");
    CallAfter([&]() {
        frame->m_togBtnOnOff->SetValue(true);
        wxCommandEvent* onEvent = new wxCommandEvent(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, frame->m_togBtnOnOff->GetId());
        onEvent->SetEventObject(frame->m_togBtnOnOff);
        frame->OnTogBtnOnOff(*onEvent);
        delete onEvent;
        //QueueEvent(onEvent);
    });
    /*sim.MouseMove(frame->m_togBtnOnOff->GetScreenPosition());
    sim.MouseClick();*/
    
    // Wait for FreeDV to start
    std::this_thread::sleep_for(1s);
    while (true)
    {
        bool isRunning = false;
        frame->executeOnUiThreadAndWait_([&]() {
            isRunning = frame->m_togBtnOnOff->IsEnabled();
        });
        if (isRunning) break;
        std::this_thread::sleep_for(20ms);
    }
    
    if (testName == "tx")
    {
        log_info("Transmitting %d times", utTxAttempts);
        for (int numTimes = 0; numTimes < utTxAttempts; numTimes++)
        {
            // Fire event to begin TX
            //sim.MouseMove(frame->m_btnTogPTT->GetScreenPosition());
            //sim.MouseClick();
            log_info("Firing PTT");
            CallAfter([&]() {
                frame->m_btnTogPTT->SetValue(true);
                wxCommandEvent* txEvent = new wxCommandEvent(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, frame->m_btnTogPTT->GetId());
                txEvent->SetEventObject(frame->m_btnTogPTT);
                //QueueEvent(txEvent);
                frame->OnTogBtnPTT(*txEvent);
                delete txEvent;
            });
            
            if (utTxFile != "")
            {
                // Transmit until file has finished playing
                SF_INFO     sfInfo;
                sfInfo.format = 0;
                g_sfPlayFile = sf_open((const char*)utTxFile.ToUTF8(), SFM_READ, &sfInfo);
                g_sfTxFs = sfInfo.samplerate;
                g_loopPlayFileToMicIn = false;
                g_playFileToMicIn = true;

                while (g_playFileToMicIn)
                {
                    std::this_thread::sleep_for(20ms);
                } 
            }
            else
            {
                // Transmit for user given time period (default 60 seconds)
                std::this_thread::sleep_for(std::chrono::seconds(utTxTimeSeconds));
            }
            
            // Stop transmitting
            log_info("Firing PTT");
            std::this_thread::sleep_for(1s);
            CallAfter([&]() {
                frame->m_btnTogPTT->SetValue(false);
                endingTx = true;
                wxCommandEvent* rxEvent = new wxCommandEvent(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, frame->m_btnTogPTT->GetId());
                rxEvent->SetEventObject(frame->m_btnTogPTT);
                frame->OnTogBtnPTT(*rxEvent);
                delete rxEvent;
                //QueueEvent(rxEvent);
            });
            /*sim.MouseMove(frame->m_btnTogPTT->GetScreenPosition());
            sim.MouseClick();*/
            
            // Wait 5 seconds for FreeDV to stop
            std::this_thread::sleep_for(5s);
        }
    }
    else
    {
        if (utRxFile != "")
        {
            // Receive until file has finished playing
            SF_INFO     sfInfo;
            sfInfo.format = 0;
            g_sfPlayFileFromRadio = sf_open((const char*)utRxFile.ToUTF8(), SFM_READ, &sfInfo);
            g_sfFs = sfInfo.samplerate;
            g_loopPlayFileFromRadio = false;
            g_playFileFromRadio = true;

            while (g_playFileFromRadio)
            {
                std::this_thread::sleep_for(20ms);
            }
        }
        else
        {
            // Receive for txtime seconds
            auto sync = 0;
            for (int i = 0; i < utTxTimeSeconds*10; i++)
            {
                std::this_thread::sleep_for(100ms);
                auto newSync = freedvInterface.getSync();
                if (newSync != sync)
                {
                    log_info("Sync changed from %d to %d", sync, newSync);
                    sync = newSync;
                }
            } 
        }
    }
    
    // Wait a second to make sure we're not doing any more processing
    std::this_thread::sleep_for(1000ms);
 
    // Fire event to stop FreeDV
    log_info("Firing stop");
    CallAfter([&]() {
        wxCommandEvent* offEvent = new wxCommandEvent(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, frame->m_togBtnOnOff->GetId());
        offEvent->SetEventObject(frame->m_togBtnOnOff);
        frame->m_togBtnOnOff->SetValue(false);
        //QueueEvent(offEvent);
        frame->OnTogBtnOnOff(*offEvent);
        delete offEvent;
    });
    //sim.MouseMove(frame->m_togBtnOnOff->GetScreenPosition());
    //sim.MouseClick();
    
    // Wait 5 seconds for FreeDV to stop
    std::this_thread::sleep_for(5s);
    
    // Destroy main window to exit application. Must be done in UI thread to avoid problems.
    CallAfter([&]() {
        frame->Destroy();
    });
}

void MainApp::OnInitCmdLine(wxCmdLineParser& parser)
{
    wxApp::OnInitCmdLine(parser);
    parser.AddOption("f", "config", "Use different configuration file instead of the default.");
    parser.AddOption("ut", "unit_test", "Execute FreeDV in unit test mode.");
    parser.AddOption("utmode", wxEmptyString, "Switch FreeDV to the given mode before UT execution.");
    parser.AddOption("rxfile", wxEmptyString, "In UT mode, pipes given WAV file through receive pipeline.");
    parser.AddOption("txfile", wxEmptyString, "In UT mode, pipes given WAV file through transmit pipeline.");
    parser.AddOption("rxfeaturefile", wxEmptyString, "Capture RX features from RADE decoder into the provided file.");
    parser.AddOption("txfeaturefile", wxEmptyString, "Capture TX features from FARGAN encoder into the provided file.");
    parser.AddOption("txtime", "60", "In UT mode, the amount of time to transmit (default 60 seconds)", wxCMD_LINE_VAL_NUMBER);
    parser.AddOption("txattempts", "1", "In UT mode, the number of times to transmit (default 1)", wxCMD_LINE_VAL_NUMBER);
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
    ulog_set_lock(&LogLockFunction_, nullptr);
        
    log_info("FreeDV version %s starting", FREEDV_VERSION);

    if (!wxApp::OnCmdLineParsed(parser))
    {
        return false;
    }
    
    wxString configPath;
    pConfig = wxConfigBase::Get();
    if (parser.Found("f", &configPath))
    {
        log_info("Loading configuration from %s", (const char*)configPath.ToUTF8());
        pConfig = new wxFileConfig(wxT("FreeDV"), wxT("CODEC2-Project"), configPath, configPath, wxCONFIG_USE_LOCAL_FILE);
        wxConfigBase::Set(pConfig);
        
        // On Linux/macOS, this replaces $HOME with "~" to shorten the title a bit.
        wxFileName fn(configPath);        
        customConfigFileName = fn.GetFullName();
    }
    pConfig->SetRecordDefaults();
    
    if (parser.Found("ut", &testName))
    {
        log_info("Executing test %s", (const char*)testName.ToUTF8());
        if (parser.Found("utmode", &utFreeDVMode))
        {
            log_info("Using mode %s for tests", (const char*)utFreeDVMode.ToUTF8());
        }

        if (parser.Found("rxfile", &utRxFile))
        {
            log_info("Piping %s through RX pipeline", (const char*)utRxFile.ToUTF8());
        }
        
        if (parser.Found("txfile", &utTxFile))
        {
            log_info("Piping %s through TX pipeline", (const char*)utTxFile.ToUTF8());
        }

        if (parser.Found("txtime", (long*)&utTxTimeSeconds))
        {
            log_info("Will transmit for %d seconds", utTxTimeSeconds);
        }
	else
	{
            utTxTimeSeconds = 60;
	}

        if (parser.Found("txattempts", (long*)&utTxAttempts))
        {
            log_info("Will transmit %d time(s)", utTxAttempts);
        }
	else
	{
            utTxAttempts = 1;
	}
    }
    
    if (parser.Found("rxfeaturefile", &utRxFeatureFile))
    {
        log_info("Capturing RADE RX features into file %s", (const char*)utRxFeatureFile.ToUTF8());
    }
    
    if (parser.Found("txfeaturefile", &utTxFeatureFile))
    {
        log_info("Capturing RADE TX features into file %s", (const char*)utTxFeatureFile.ToUTF8());
    }
    
    return true;
}

//-------------------------------------------------------------------------
// OnInit()
//-------------------------------------------------------------------------
bool MainApp::OnInit()
{
    // Initialize locale.
    m_locale.Init();

    m_reporters.clear();
    m_reportCounter = 0;
    
    if(!wxApp::OnInit())
    {
        return false;
    }
    SetVendorName(wxT("CODEC2-Project"));
    SetAppName(wxT("FreeDV"));      // not needed, it's the default value
    
    golay23_init();
    
    // Prevent conflicts between numpy/OpenBLAS threading and Python/C++ threading,
    // improving performance.
    wxSetEnv("OMP_NUM_THREADS", "1");
    wxSetEnv("OPENBLAS_NUM_THREADS", "1");
  
#if _WIN32 || __APPLE__
    // Change current folder to the folder containing freedv.exe.
    // This is needed so that Python can find RADE properly. 
    wxFileName f(wxStandardPaths::Get().GetExecutablePath());
    wxString appPath(f.GetPath());
    wxSetWorkingDirectory(appPath);

#if __APPLE__
    // Set PYTHONPATH accordingly. We mainly want to be able to access
    // the model (,pth) as well as the RADE Python code.
    wxFileName path(appPath);
    path.AppendDir("Resources");
    wxSetWorkingDirectory(path.GetPath());
    wxSetEnv("PYTHONPATH", path.GetPath());

    wxString ppath;
    wxGetEnv("PYTHONPATH", &ppath);
    log_info("PYTHONPATH is %s", (const char*)ppath.ToUTF8());
#endif // __APPLE__

#endif // _WIN32 || __APPLE__

#if defined(UNOFFICIAL_RELEASE)
    // Terminate the application if the current date > expiration date
    wxDateTime buildDate;
    wxString::const_iterator iter;
    buildDate.ParseDate(FREEDV_BUILD_DATE, &iter);
    
    auto expireDate = buildDate + EXPIRES_AFTER_TIMEFRAME;
    auto currentDate = wxDateTime::Now();
    
    if (currentDate > expireDate)
    {
        wxMessageBox("This version of FreeDV has expired. Please download a new version from freedv.org.", "Application Expired");
        return false;
    }
#endif // UNOFFICIAL_RELEASE
    
    // Initialize RADE.
    rade_initialize();
 
    m_rTopWindow = wxRect(0, 0, 0, 0);

     // Create the main application window

    frame = new MainFrame(NULL);
    SetTopWindow(frame);

    // Should guarantee that the first plot tab defined is the one
    // displayed. But it doesn't when built from command line.  Why?

    frame->m_auiNbookCtrl->ChangeSelection(0);
    frame->Layout();    
    frame->Show();
    g_parent = frame;

    // Begin test execution
    if (testName != "")
    {
        std::thread utThread(std::bind(&MainApp::UnitTest_, this));
        utThread.detach();
    }
    
    return true;
}

//-------------------------------------------------------------------------
// OnExit()
//-------------------------------------------------------------------------
int MainApp::OnExit()
{
    return 0;
}

#if defined(FREEDV_MODE_2020)
bool MainFrame::test2020HWAllowed_()
{
    bool allowed = true;

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    // AVX checking code on x86 is here due to LPCNet in binary builds being
    // compiled to use it. Running the sanity check below could potentially 
    // cause crashes.
    uint32_t eax, ebx, ecx, edx;
    eax = ebx = ecx = edx = 0;
    __cpuid(1, eax, ebx, ecx, edx);
    
    if (ecx & (1<<27) && ecx & (1<<28)) {
        // CPU supports XSAVE and AVX
        uint32_t xcr0, xcr0_high;
        asm("xgetbv" : "=a" (xcr0), "=d" (xcr0_high) : "c" (0));
        allowed = (xcr0 & 6) == 6;    // AVX state saving enabled?
    } else {
        allowed = false;
    }
#endif // defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)

    return allowed;
}

//-------------------------------------------------------------------------
// test2020Mode_(): Makes sure that 2020 mode will work 
//-------------------------------------------------------------------------
void MainFrame::test2020Mode_()
{    
    log_info("Making sure your machine can handle 2020 mode...");

    bool allowed = false;

#if !defined(LPCNET_DISABLED)
    allowed = test2020HWAllowed_();
    wxGetApp().appConfiguration.freedvAVXSupported = allowed;

    if (!allowed)
    {
        log_warn("Warning: AVX support not found!");
    }
    else
    {
        // Sanity check: encode 1 second of 16 kHz white noise and then try to
        // decode it. If it takes longer than 0.5 seconds, it's unlikely that 
        // 2020/2020B will work properly on this machine.
        log_info("Generating test audio...");
        struct FIFO* inFifo = codec2_fifo_create(24000);
        assert(inFifo != nullptr);
    
        struct freedv* fdv = freedv_open(FREEDV_MODE_2020);
        assert(fdv != nullptr);
    
        int numInSamples = 0;
        int samplesToGenerate = freedv_get_n_speech_samples(fdv);
        int samplesGenerated = freedv_get_n_nom_modem_samples(fdv);
    
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(SHRT_MIN, SHRT_MAX);
    
        while (numInSamples < 16000)
        {
            short inSamples[samplesToGenerate];
            COMP outSamples[samplesGenerated];
            for (int index = 0; index < samplesToGenerate; index++)
            {
                inSamples[index] = distrib(gen);
            }
        
            freedv_comptx(fdv, outSamples, inSamples);
        
            for (int index = 0; index < samplesGenerated; index++)
            {
                short realVal = outSamples[index].real;
                codec2_fifo_write(inFifo, &realVal, 1);
            }
        
            numInSamples += samplesToGenerate;        
        }
    
        log_info("Decoding modulated audio...");
    
        std::chrono::high_resolution_clock systemClock;
        auto startTime = systemClock.now();
    
        int nin = freedv_nin(fdv);
        short inputBuf[freedv_get_n_max_modem_samples(fdv)];
        short outputBuf[freedv_get_n_speech_samples(fdv)];
        COMP  rx_fdm[freedv_get_n_max_modem_samples(fdv)];
        while(codec2_fifo_read(inFifo, inputBuf, nin) == 0)
        {
            for(int i=0; i<nin; i++) 
            {
                rx_fdm[i].real = (float)inputBuf[i];
                rx_fdm[i].imag = 0.0;
            }
        
            freedv_comprx(fdv, outputBuf, rx_fdm);
        }
        auto endTime = systemClock.now();
        auto timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        if (timeTaken > std::chrono::milliseconds(600))
        {
            allowed = false;
        }
    
        log_info("One second of 2020 decoded in %d ms", (int)timeTaken.count());
    }
#endif // !defined(LPCNET_DISABLED)
    
    log_info("2020 allowed: %d", (int)allowed);
    
    // Save results to configuration.
    wxGetApp().appConfiguration.freedv2020Allowed = allowed;
}
#endif // defined(FREEDV_MODE_2020)

//-------------------------------------------------------------------------
// loadConfiguration_(): Loads or sets default configuration options.
//-------------------------------------------------------------------------
void MainFrame::loadConfiguration_()
{
    wxGetApp().appConfiguration.load(pConfig);
    
    // restore frame position and size
    int x = wxGetApp().appConfiguration.mainWindowLeft;
    int y = wxGetApp().appConfiguration.mainWindowTop;
    int w = wxGetApp().appConfiguration.mainWindowWidth;
    int h = wxGetApp().appConfiguration.mainWindowHeight;

    // sanitise frame position as a first pass at Win32 registry bug

    if (x < 0 || x > 2048) x = 20;
    if (y < 0 || y > 2048) y = 20;
    if (w < 0 || w > 2048) w = 800;
    if (h < 0 || h > 2048) h = 780;

    g_SquelchActive = wxGetApp().appConfiguration.squelchActive;
    g_SquelchLevel = wxGetApp().appConfiguration.squelchLevel;
    g_SquelchLevel /= 2.0;
    
    Move(x, y);
    wxSize size = GetMinSize();

    if (w < size.GetWidth()) w = size.GetWidth();
    if (h < size.GetHeight()) h = size.GetHeight();
    
    // XXX - with really short windows, wxWidgets sometimes doesn't size
    // the components properly until the user resizes the window (even if only
    // by a pixel or two). As a really hacky workaround, we emulate this behavior
    // when restoring window sizing. These resize events also happen after configuration
    // is restored but I'm not sure this is necessary.
    CallAfter([=]()
    {
        SetSize(w, h);
    });
    CallAfter([=]()
    {
        SetSize(w, h - 1);
    });
    CallAfter([=]()
    {
        SetSize(w, h);
    });
    
    g_txLevel = wxGetApp().appConfiguration.transmitLevel;
    char fmt[15];
    m_sliderTxLevel->SetValue(g_txLevel);
    snprintf(fmt, 15, "%0.1f dB", (double)g_txLevel / 10.0);
    wxString fmtString(fmt);
    m_txtTxLevelNum->SetLabel(fmtString);

    // Adjust frequency entry labels
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
    {
        m_freqBox->SetLabel(_("Report Freq. (kHz)"));
    }
    else
    {
        m_freqBox->SetLabel(_("Report Freq. (MHz)"));
    }
    
    // PTT -------------------------------------------------------------------
    
    // Note: we're no longer using RigName but we need to bring over the old data
    // for backwards compatibility.    
    if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibRigName == wxT(""))
    {
        wxGetApp().m_intHamlibRig = pConfig->ReadLong("/Hamlib/RigName", 0);
        wxGetApp().appConfiguration.rigControlConfiguration.hamlibRigName = HamlibRigController::RigIndexToName(wxGetApp().m_intHamlibRig);
    }
    else
    {
        wxGetApp().m_intHamlibRig = HamlibRigController::RigNameToIndex(std::string(wxGetApp().appConfiguration.rigControlConfiguration.hamlibRigName->ToUTF8()));
    }
    
    // -----------------------------------------------------------------------

    wxGetApp().m_FreeDV700Combine = 1;

    if (wxGetApp().appConfiguration.debugVerbose)
    {
        ulog_set_level(LOG_TRACE);
    }
    else
    {
        ulog_set_level(LOG_INFO);
    }
    g_freedv_verbose = wxGetApp().appConfiguration.apiVerbose;

    wxGetApp().m_attn_carrier_en = 0;
    wxGetApp().m_attn_carrier    = 0;

    wxGetApp().m_tone = 0;
    wxGetApp().m_tone_freq_hz = 1000;
    wxGetApp().m_tone_amplitude = 500;
    
    // General reporting parameters

    // wxString::Format() doesn't respect locale but C++ iomanip should. Use the latter instead.
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency > 0)
    {
        double freqFactor = 1000.0;
        
        if (!wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            freqFactor *= 1000.0;
        }
        
        double freq =  ((double)wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency) / freqFactor;

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
        
        std::string sVal = ss.str();
        m_cboReportFrequency->SetValue(sVal);
    }

    int defaultMode = wxGetApp().appConfiguration.currentFreeDVMode.getDefaultVal();
    int mode = wxGetApp().appConfiguration.currentFreeDVMode;
setDefaultMode:
    if (mode == 0)
    {
        m_rb1600->SetValue(1);
    }
    else if (mode == 3)
    {
        m_rb700c->SetValue(1);
    }
    else if (mode == 4)
    {
        m_rb700d->SetValue(1);
    }
    else if (mode == 5)
    {
        m_rb700e->SetValue(1);
    }
    else if (mode == 6)
    {
        m_rb800xa->SetValue(1);
    }
    // mode 7 was the former 2400B mode, now removed.
    else if ((mode == 9) && wxGetApp().appConfiguration.freedv2020Allowed && wxGetApp().appConfiguration.freedvAVXSupported)
    {
        m_rb2020->SetValue(1);
    }
    else if (mode == FREEDV_MODE_RADE)
    {
        m_rbRADE->SetValue(1);
    }
#if defined(FREEDV_MODE_2020B)
    else if ((mode == 10) && wxGetApp().appConfiguration.freedv2020Allowed && wxGetApp().appConfiguration.freedvAVXSupported)
    {
        m_rb2020b->SetValue(1);
    }
#endif // defined(FREEDV_MODE_2020B)
    else
    {
        // Default to RADE otherwise
        mode = defaultMode;
        goto setDefaultMode;
    }
    pConfig->SetPath(wxT("/"));
    
    // Set initial state of additional modes.
    switch(mode)
    {
        case 0:
        case 4:
        case 5:
            // 700D/E and 1600; don't expand additional modes
            break;
        default:
            m_collpane->Collapse(false);
            wxCollapsiblePaneEvent evt;
            OnChangeCollapseState(evt);
            break;
    }
    
    m_togBtnAnalog->Disable();
    m_btnTogPTT->Disable();
    m_togBtnVoiceKeyer->Disable();

    // squelch settings
    char sqsnr[15];
    m_sliderSQ->SetValue((int)((g_SquelchLevel+5.0)*2.0));
    snprintf(sqsnr, 15, "%4.1f dB", g_SquelchLevel);
    wxString sqsnr_string(sqsnr);
    m_textSQ->SetLabel(sqsnr_string);
    m_ckboxSQ->SetValue(g_SquelchActive);

    // SNR settings

    m_ckboxSNR->SetValue(wxGetApp().appConfiguration.snrSlow);
    setsnrBeta(wxGetApp().appConfiguration.snrSlow);
    
    // Show/hide frequency box based on reporting enablement
    m_freqBox->Show(wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled);

    // Show/hide callsign combo box based on reporting enablement
    if (wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled)
    {
        m_cboLastReportedCallsigns->Show();
        m_txtCtrlCallSign->Hide();
        m_cboLastReportedCallsigns->Enable(m_lastReportedCallsignListView->GetItemCount() > 0);
    }
    else
    {
        m_cboLastReportedCallsigns->Hide();
        m_txtCtrlCallSign->Show();
    }
    
    // Ensure that sound card count is correct. Otherwise the Audio Options won't show
    // the correct devices prior to start.
    bool hasSoundCard1InDevice = wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName != "none";
    bool hasSoundCard1OutDevice = wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName != "none";
    bool hasSoundCard2InDevice = wxGetApp().appConfiguration.audioConfiguration.soundCard2In.deviceName != "none";
    bool hasSoundCard2OutDevice = wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.deviceName != "none";
    
    g_nSoundCards = 0;
    if (hasSoundCard1InDevice && hasSoundCard1OutDevice) {
        g_nSoundCards = 1;
        if (hasSoundCard2InDevice && hasSoundCard2OutDevice)
            g_nSoundCards = 2;
    }
    
    // Update the reporting list as needed.
    updateReportingFreqList_();
    
    // Relayout window so that the changes can take effect.
    auto currentSizer = m_panel->GetSizer();
    m_panel->SetSizerAndFit(currentSizer, false);
    m_panel->Layout();
    
    // Load default voice keyer file as current.
    if (wxGetApp().appConfiguration.voiceKeyerWaveFile != "")
    {
        wxFileName fullVKPath(wxGetApp().appConfiguration.voiceKeyerWaveFilePath, wxGetApp().appConfiguration.voiceKeyerWaveFile);
        vkFileName_ = fullVKPath.GetFullPath().mb_str();
        
        m_togBtnVoiceKeyer->SetToolTip(_("Toggle Voice Keyer using file ") + wxGetApp().appConfiguration.voiceKeyerWaveFile + _(". Right-click for additional options."));
        
        wxString fileNameWithoutExt;
        wxFileName::SplitPath(wxGetApp().appConfiguration.voiceKeyerWaveFile, nullptr, &fileNameWithoutExt, nullptr);
        setVoiceKeyerButtonLabel_(fileNameWithoutExt);
    }
    else
    {
        vkFileName_ = "";
    }
    
    if (wxGetApp().appConfiguration.experimentalFeatures && wxGetApp().appConfiguration.tabLayout != "")
    {
        ((TabFreeAuiNotebook*)m_auiNbookCtrl)->LoadPerspective(wxGetApp().appConfiguration.tabLayout);
        const_cast<wxAuiManager&>(m_auiNbookCtrl->GetAuiManager()).Update();
    }

    // Initialize FreeDV Reporter as required
    initializeFreeDVReporter_();
    
    // If the FreeDV Reporter window was open on last execution, reopen it now.
    CallAfter([&]() {
        if (wxGetApp().appConfiguration.reporterWindowVisible)
        {
            wxCommandEvent event;
            OnToolsFreeDVReporter(event);
        }
    });
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class MainFrame(wxFrame* pa->ent) : TopFrame(parent)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
MainFrame::MainFrame(wxWindow *parent) : TopFrame(parent, wxID_ANY, _("FreeDV ") + _(FREEDV_VERSION))
{
#if defined(__linux__)
    pthread_setname_np(pthread_self(), "FreeDV GUI");
#endif // defined(__linux__)

    terminating_ = false;

    // Add config file name to title bar if provided at the command line.
    if (wxGetApp().customConfigFileName != "")
    {
        SetTitle(wxString::Format("%s (%s)", _("FreeDV ") + _(FREEDV_VERSION), wxGetApp().customConfigFileName));
    }
    
#if defined(UNOFFICIAL_RELEASE)
    wxDateTime buildDate;
    wxString::const_iterator iter;
    buildDate.ParseDate(FREEDV_BUILD_DATE, &iter);
    
    auto expireDate = buildDate + EXPIRES_AFTER_TIMEFRAME;
    auto currentTitle = GetTitle();
    
    currentTitle += wxString::Format(" [Expires %s]", expireDate.FormatDate());
    SetTitle(currentTitle);
#endif // defined(UNOFFICIAL_RELEASE)
    
    m_reporterDialog = nullptr;
    m_filterDialog = nullptr;

    m_zoom              = 1.;
    suppressFreqModeUpdates_ = false;
    
    tools->AppendSeparator();
    wxMenuItem* m_menuItemToolsConfigDelete;
    m_menuItemToolsConfigDelete = new wxMenuItem(tools, wxID_ANY, wxString(_("&Restore defaults")) , wxT("Delete config file/keys and restore defaults"), wxITEM_NORMAL);
    this->Connect(m_menuItemToolsConfigDelete->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainFrame::OnDeleteConfig));
    this->Connect(m_menuItemToolsConfigDelete->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnDeleteConfigUI));

    tools->Append(m_menuItemToolsConfigDelete);
    
    // Add Waterfall Plot window
    m_panelWaterfall = new PlotWaterfall((wxFrame*) m_auiNbookCtrl, false, 0);
    m_panelWaterfall->SetToolTip(_("Double click to tune, middle click to re-center"));
    m_auiNbookCtrl->AddPage(m_panelWaterfall, _("Waterfall"), true, wxNullBitmap);

    // Add Spectrum Plot window
    wxPanel* spectrumPanel = new wxPanel(m_auiNbookCtrl);
    
    wxFlexGridSizer* spectrumPanelSizer = new wxFlexGridSizer(2, 1, 5, 5);
    wxBoxSizer* spectrumPanelControlSizer = new wxBoxSizer(wxHORIZONTAL);
    spectrumPanelSizer->AddGrowableRow(0);
    spectrumPanelSizer->AddGrowableCol(0);
    
    // Actual Spectrum plot
    m_panelSpectrum = new PlotSpectrum(spectrumPanel, g_avmag,
                                       MODEM_STATS_NSPEC*((float)MAX_F_HZ/MODEM_STATS_MAX_F_HZ));
    m_panelSpectrum->SetToolTip(_("Double click to tune, middle click to re-center"));
    spectrumPanelSizer->Add(m_panelSpectrum, 0, wxALL | wxEXPAND, 5);
    
    // Spectrum plot control interface
    wxStaticText* labelAveraging = new wxStaticText(spectrumPanel, wxID_ANY, wxT("Average across"), wxDefaultPosition, wxDefaultSize, 0);
    spectrumPanelControlSizer->Add(labelAveraging, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    wxString samplingChoices[] = {
        "1",
        "2",
        "3"
    };
    m_cbxNumSpectrumAveraging = new wxComboBox(spectrumPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 3, samplingChoices, wxCB_DROPDOWN | wxCB_READONLY);
    m_cbxNumSpectrumAveraging->SetSelection(wxGetApp().appConfiguration.currentSpectrumAveraging);
    spectrumPanelControlSizer->Add(m_cbxNumSpectrumAveraging, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_cbxNumSpectrumAveraging->Connect(wxEVT_TEXT, wxCommandEventHandler(MainFrame::OnAveragingChange), NULL, this);
    
    wxStaticText* labelSamples = new wxStaticText(spectrumPanel, wxID_ANY, wxT("sample(s)"), wxDefaultPosition, wxDefaultSize, 0);
    spectrumPanelControlSizer->Add(labelSamples, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    spectrumPanelSizer->Add(spectrumPanelControlSizer, 0, wxALL | wxEXPAND, 5);
    spectrumPanel->SetSizerAndFit(spectrumPanelSizer);
    
    m_auiNbookCtrl->AddPage(spectrumPanel, _("Spectrum"), true, wxNullBitmap);

    // Add Scatter Plot window
    m_panelScatter = new PlotScatter((wxFrame*) m_auiNbookCtrl);
    m_auiNbookCtrl->AddPage(m_panelScatter, _("Scatter"), true, wxNullBitmap);

    // Add Demod Input window
    m_panelDemodIn = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, WAVEFORM_PLOT_TIME, 1.0/WAVEFORM_PLOT_FS, -1, 1, 1, 0.2, "%2.1f", 0);
    m_auiNbookCtrl->AddPage(m_panelDemodIn, _("Frm Radio"), true, wxNullBitmap);
    g_plotDemodInFifo = codec2_fifo_create(4*WAVEFORM_PLOT_BUF);

    // Add Speech Input window
    m_panelSpeechIn = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, WAVEFORM_PLOT_TIME, 1.0/WAVEFORM_PLOT_FS, -1, 1, 1, 0.2, "%2.1f", 0);
    m_auiNbookCtrl->AddPage(m_panelSpeechIn, _("Frm Mic"), true, wxNullBitmap);
    g_plotSpeechInFifo = codec2_fifo_create(4*WAVEFORM_PLOT_BUF);

    // Add Speech Output window
    m_panelSpeechOut = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, WAVEFORM_PLOT_TIME, 1.0/WAVEFORM_PLOT_FS, -1, 1, 1, 0.2, "%2.1f", 0);
    m_auiNbookCtrl->AddPage(m_panelSpeechOut, _("To Spkr/Hdphns"), true, wxNullBitmap);
    g_plotSpeechOutFifo = codec2_fifo_create(4*WAVEFORM_PLOT_BUF);

    // Add Timing Offset window
    m_panelTimeOffset = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, 5.0, DT, -0.5, 0.5, 1, 0.1, "%2.1f", 0);
    m_auiNbookCtrl->AddPage(m_panelTimeOffset, L"Timing \u0394", true, wxNullBitmap);

    // Add Frequency Offset window
    m_panelFreqOffset = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, 5.0, DT, -200, 200, 1, 50, "%3.0fHz", 0);
    m_auiNbookCtrl->AddPage(m_panelFreqOffset, L"Frequency \u0394", true, wxNullBitmap);

    // Add Test Frame Errors window
    m_panelTestFrameErrors = new PlotScalar((wxFrame*) m_auiNbookCtrl, 2*MODEM_STATS_NC_MAX, 30.0, DT, 0, 2*MODEM_STATS_NC_MAX+2, 1, 1, "", 1);
    m_auiNbookCtrl->AddPage(m_panelTestFrameErrors, L"Test Frame Errors", true, wxNullBitmap);

    // Add Test Frame Histogram window.  1 column for every bit, 2 bits per carrier
    m_panelTestFrameErrorsHist = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, 1.0, 1.0/(2*MODEM_STATS_NC_MAX), 0.001, 0.1, 1.0/MODEM_STATS_NC_MAX, 0.1, "%0.0E", 0);
    m_auiNbookCtrl->AddPage(m_panelTestFrameErrorsHist, L"Test Frame Histogram", true, wxNullBitmap);
    m_panelTestFrameErrorsHist->setBarGraph(1);
    m_panelTestFrameErrorsHist->setLogY(1);

//    this->Connect(m_menuItemHelpUpdates->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnHelpCheckUpdatesUI));
     m_togBtnOnOff->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnOnOffUI), NULL, this);
    m_togBtnAnalog->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnAnalogClickUI), NULL, this);
   // m_btnTogPTT->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnPTT_UI), NULL, this);

    loadConfiguration_();
    
#ifdef _USE_TIMER
    Bind(wxEVT_TIMER, &MainFrame::OnTimer, this);       // ID_MY_WINDOW);
    m_plotTimer.SetOwner(this, ID_TIMER_WATERFALL);
    m_pskReporterTimer.SetOwner(this, ID_TIMER_PSKREPORTER);
    m_updFreqStatusTimer.SetOwner(this,ID_TIMER_UPD_FREQ);  //[UP]
    //m_panelWaterfall->Refresh();
#endif
    
    // Create voice keyer popup menu.
    voiceKeyerPopupMenu_ = new wxMenu();
    assert(voiceKeyerPopupMenu_ != nullptr);

    chooseVKFileMenuItem_ = voiceKeyerPopupMenu_->Append(wxID_ANY, _("&Use another voice keyer file..."));
    voiceKeyerPopupMenu_->Connect(
        chooseVKFileMenuItem_->GetId(), wxEVT_COMMAND_MENU_SELECTED, 
        wxCommandEventHandler(MainFrame::OnChooseAlternateVoiceKeyerFile),
        NULL,
        this);
        
    recordNewVoiceKeyerFileMenuItem_ = voiceKeyerPopupMenu_->Append(wxID_ANY, _("&Record new voice keyer file..."));
    voiceKeyerPopupMenu_->Connect(
        recordNewVoiceKeyerFileMenuItem_->GetId(), wxEVT_COMMAND_MENU_SELECTED, 
        wxCommandEventHandler(MainFrame::OnRecordNewVoiceKeyerFile),
        NULL,
        this);
    
    voiceKeyerPopupMenu_->AppendSeparator();
    
    auto monitorVKMenuItem = voiceKeyerPopupMenu_->AppendCheckItem(wxID_ANY, _("Monitor transmitted audio"));
    voiceKeyerPopupMenu_->Check(monitorVKMenuItem->GetId(), wxGetApp().appConfiguration.monitorVoiceKeyerAudio);
    voiceKeyerPopupMenu_->Connect(
        monitorVKMenuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, 
        wxCommandEventHandler(MainFrame::OnSetMonitorVKAudio),
        NULL,
        this);
        
    adjustMonitorVKVolMenuItem_ = voiceKeyerPopupMenu_->Append(wxID_ANY, _("Adjust Monitor Volume..."));
    adjustMonitorVKVolMenuItem_->Enable(wxGetApp().appConfiguration.monitorVoiceKeyerAudio);
    voiceKeyerPopupMenu_->Connect(
        adjustMonitorVKVolMenuItem_->GetId(), wxEVT_COMMAND_MENU_SELECTED,
        wxCommandEventHandler(MainFrame::OnSetMonitorVKAudioVol),
        NULL,
        this
        );
        
    // Create PTT popup menu
    pttPopupMenu_ = new wxMenu();
    assert(pttPopupMenu_ != nullptr);
    
    auto monitorMenuItem = pttPopupMenu_->AppendCheckItem(wxID_ANY, _("Monitor transmitted audio"));
    pttPopupMenu_->Check(monitorMenuItem->GetId(), wxGetApp().appConfiguration.monitorTxAudio);
    pttPopupMenu_->Connect(
        monitorMenuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, 
        wxCommandEventHandler(MainFrame::OnSetMonitorTxAudio),
        NULL,
        this);
        
    adjustMonitorPttVolMenuItem_ = pttPopupMenu_->Append(wxID_ANY, _("Adjust Monitor Volume..."));
    adjustMonitorPttVolMenuItem_->Enable(wxGetApp().appConfiguration.monitorTxAudio);
    pttPopupMenu_->Connect(
        adjustMonitorPttVolMenuItem_->GetId(), wxEVT_COMMAND_MENU_SELECTED,
        wxCommandEventHandler(MainFrame::OnSetMonitorTxAudioVol),
        NULL,
        this
        );

    m_RxRunning = false;

    m_txThread = nullptr;
    m_rxThread = nullptr;
    wxGetApp().linkStep = nullptr;
    
#ifdef _USE_ONIDLE
    Connect(wxEVT_IDLE, wxIdleEventHandler(MainFrame::OnIdle), NULL, this);
#endif //_USE_ONIDLE

    g_sfPlayFile = NULL;
    g_playFileToMicIn = false;
    g_loopPlayFileToMicIn = false;

    g_sfRecFile = NULL;
    g_recFileFromRadio = false;

    g_sfPlayFileFromRadio = NULL;
    g_playFileFromRadio = false;
    g_loopPlayFileFromRadio = false;

    g_sfRecFileFromModulator = NULL;
    g_recFileFromModulator = false;
    
    g_sfRecMicFile = nullptr;
    g_recFileFromMic = false;
    g_recVoiceKeyerFile = false;

    // init click-tune states

    g_RxFreqOffsetHz = 0.0;
    m_panelWaterfall->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);
    m_panelSpectrum->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);

    g_TxFreqOffsetHz = 0.0;

    g_tx = 0;

    // data states
    g_txDataInFifo = codec2_fifo_create(MAX_CALLSIGN*FREEDV_VARICODE_MAX_BITS);
    g_rxDataOutFifo = codec2_fifo_create(MAX_CALLSIGN*FREEDV_VARICODE_MAX_BITS);

    sox_biquad_start();

    g_testFrames = 0;
    g_test_frame_sync_state = 0;
    g_resyncs = 0;
    wxGetApp().m_testFrames = false;
    wxGetApp().m_channel_noise = false;
    g_tone_phase = 0.0;

    optionsDlg = new OptionsDlg(NULL);
    m_schedule_restore = false;

    vk_state = VK_IDLE;

    m_timeSinceSyncLoss = 0;

    // Init optional Windows debug console so we can see all those printfs

#ifdef __WXMSW__
    if (wxGetApp().appConfiguration.debugConsoleEnabled || wxGetApp().appConfiguration.firstTimeUse) {
        // somewhere to send printfs while developing
        int ret = AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        log_info("AllocConsole: %d m_debug_console: %d", ret, wxGetApp().appConfiguration.debugConsoleEnabled.get());
    }
#endif
    
    // Print RADE API version. This also forces the RADE library to be linked.
    log_info("Using RADE API version %d", rade_version());

#if defined(FREEDV_MODE_2020) && !defined(LPCNET_DISABLED)
    // First time use: make sure 2020 mode will actually work on this machine.
    if (wxGetApp().appConfiguration.firstTimeUse)
    {
        test2020Mode_();
    }
    else
    {
        wxGetApp().appConfiguration.freedvAVXSupported = test2020HWAllowed_();
    }
#else
    // Disable LPCNet if not compiled in.
    wxGetApp().appConfiguration.freedv2020Allowed = false;
#endif // defined(FREEDV_MODE_2020) && !defined(LPCNET_DISABLED)
    
    if(!wxGetApp().appConfiguration.freedv2020Allowed || !wxGetApp().appConfiguration.freedvAVXSupported)
    {
        m_rb2020->Disable();
#if defined(FREEDV_MODE_2020B)
        m_rb2020b->Disable();
#endif // FREEDV_MODE_2020B
    }

    if (wxGetApp().appConfiguration.firstTimeUse)
    {
        // Initial setup. Display Easy Setup dialog.
        CallAfter([&]() {
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

                // Relayout window so that the changes can take effect.
                m_panel->Layout();
            }
        });
    }
    
    wxGetApp().appConfiguration.firstTimeUse = false;
    
    //#define FTEST
    #ifdef FTEST
    ftest = fopen("ftest.raw", "wb");
    assert(ftest != NULL);
    #endif

    /* experimental checkbox control of thread priority, used
       to helpo debug 700D windows sound break up */

    wxGetApp().m_txRxThreadHighPriority = true;
    g_dump_timing = g_dump_fifo_state = 0;
}

//-------------------------------------------------------------------------
// ~MainFrame()
//-------------------------------------------------------------------------
MainFrame::~MainFrame()
{
    delete voiceKeyerPopupMenu_;
    
    int x;
    int y;
    int w;
    int h;

    if (m_filterDialog != nullptr)
    {
        m_filterDialog->Close();
    }
    
    if (m_reporterDialog != nullptr)
    {
        // wxWidgets doesn't fire wxEVT_MOVE events on Linux for some
        // reason, so we need to grab and save the current position again.
        auto pos = m_reporterDialog->GetPosition();
        wxGetApp().appConfiguration.reporterWindowLeft = pos.x;
        wxGetApp().appConfiguration.reporterWindowTop = pos.y;

        m_reporterDialog->setReporter(nullptr);
        m_reporterDialog->Close();
        m_reporterDialog->Destroy();
        m_reporterDialog = nullptr;
    }
    
#ifdef FTEST
    fclose(ftest);
    #endif

    wxGetApp().rigPttController = nullptr;
    wxGetApp().rigFrequencyController = nullptr;
    wxGetApp().m_pttInSerialPort = nullptr;
    
    if (!IsIconized()) {
        GetSize(&w, &h);
        GetPosition(&x, &y);
        
        wxGetApp().appConfiguration.mainWindowLeft = x;
        wxGetApp().appConfiguration.mainWindowTop = y;
        wxGetApp().appConfiguration.mainWindowWidth = w;
        wxGetApp().appConfiguration.mainWindowHeight = h;
    }

    if (wxGetApp().appConfiguration.experimentalFeatures)
    {
        wxGetApp().appConfiguration.tabLayout = ((TabFreeAuiNotebook*)m_auiNbookCtrl)->SavePerspective();
    }
    
    wxGetApp().appConfiguration.squelchActive = g_SquelchActive;
    wxGetApp().appConfiguration.squelchLevel = (int)(g_SquelchLevel*2.0);

    wxGetApp().appConfiguration.transmitLevel = g_txLevel;
    
    int mode;
    if (m_rb1600->GetValue())
        mode = 0;
    if (m_rb700c->GetValue())
        mode = 3;
    if (m_rb700d->GetValue())
        mode = 4;
    if (m_rb700e->GetValue())
        mode = 5;
    if (m_rb800xa->GetValue())
        mode = 6;
    if (m_rb2020->GetValue())
        mode = 9;
    if (m_rbRADE->GetValue())
        mode = FREEDV_MODE_RADE;
#if defined(FREEDV_MODE_2020B)
    if (m_rb2020b->GetValue())
        mode = 10;
#endif // defined(FREEDV_MODE_2020B)
    
    wxGetApp().appConfiguration.currentFreeDVMode = mode;
    wxGetApp().appConfiguration.save(pConfig);

    m_cbxNumSpectrumAveraging->Disconnect(wxEVT_TEXT, wxCommandEventHandler(MainFrame::OnAveragingChange), NULL, this);
    m_togBtnOnOff->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnOnOffUI), NULL, this);
    m_togBtnAnalog->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnAnalogClickUI), NULL, this);

    if (m_RxRunning)
    {
        stopRxStream();
        freedvInterface.stop();
    } 
    sox_biquad_finish();

    if (g_sfPlayFile != NULL)
    {
        sf_close(g_sfPlayFile);
        g_sfPlayFile = NULL;
    }
    if (g_sfRecFile != NULL)
    {
        sf_close(g_sfRecFile);
        g_sfRecFile = NULL;
    }
    if (g_sfRecFileFromModulator != NULL)
    {
        sf_close(g_sfRecFileFromModulator);
        g_sfRecFileFromModulator = NULL;
    }
#ifdef _USE_TIMER
    if(m_pskReporterTimer.IsRunning())
    {
        m_pskReporterTimer.Stop();
    }
    if(m_plotTimer.IsRunning())
    {
        m_plotTimer.Stop();
        Unbind(wxEVT_TIMER, &MainFrame::OnTimer, this);
    }
#endif //_USE_TIMER

#ifdef _USE_ONIDLE
    Disconnect(wxEVT_IDLE, wxIdleEventHandler(MainFrame::OnIdle), NULL, this);
#endif // _USE_ONIDLE

    if (optionsDlg != NULL) {
        delete optionsDlg;
        optionsDlg = NULL;
    }

    wxGetApp().rigFrequencyController = nullptr;
    wxGetApp().rigPttController = nullptr;
    wxGetApp().m_reporters.clear();

    // Clean up RADE.
    rade_finalize();
    
    auto engine = AudioEngineFactory::GetAudioEngine();
    engine->stop();
    engine->setOnEngineError(nullptr, nullptr);
}


#ifdef _USE_ONIDLE
void MainFrame::OnIdle(wxIdleEvent &evt) {
}
#endif

void MainFrame::OnAveragingChange(wxCommandEvent& event)
{
    wxGetApp().appConfiguration.currentSpectrumAveraging = m_cbxNumSpectrumAveraging->GetSelection();
}

#ifdef _USE_TIMER
//----------------------------------------------------------------
// OnTimer()
//
// when the timer fires every DT seconds we update the GUI displays.
// the tabs only the plot that is visible actually gets updated, this
// keeps CPU load reasonable
//----------------------------------------------------------------
void MainFrame::OnTimer(wxTimerEvent &evt)
{
    if (!m_RxRunning)
    {
        return;
    }

    if (evt.GetTimer().GetId() == ID_TIMER_PSKREPORTER)
    {
        // Reporter timer fired; send in-progress packet.
        for (auto& obj : wxGetApp().m_reporters)
        {
            obj->send();
        }
    }
    else if (evt.GetTimer().GetId() == ID_TIMER_UPD_FREQ)
    {
        // show freq. and mode [UP]
        if (wxGetApp().rigFrequencyController && wxGetApp().rigFrequencyController->isConnected()) 
        {
            log_debug("update freq and mode ...."); 
            wxGetApp().rigFrequencyController->requestCurrentFrequencyMode();
        }
     }
     else
     {         
        int r,c;

        if (m_panelWaterfall->checkDT()) {
            m_panelWaterfall->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);
            m_panelWaterfall->m_newdata = true;
            m_panelWaterfall->setColor(wxGetApp().appConfiguration.waterfallColor);
            m_panelWaterfall->addOffset(freedvInterface.getCurrentRxModemStats()->foff);
            m_panelWaterfall->setSync(freedvInterface.getSync() ? true : false);
            m_panelWaterfall->Refresh();
        }

        m_panelSpectrum->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);
        
        // Note: each element in this combo box is a numeric value starting from 1,
        // so just incrementing the selected index should get us the correct results.
        m_panelSpectrum->setNumAveraging(m_cbxNumSpectrumAveraging->GetSelection() + 1);
        m_panelSpectrum->addOffset(freedvInterface.getCurrentRxModemStats()->foff);
        m_panelSpectrum->setSync(freedvInterface.getSync() ? true : false);
        m_panelSpectrum->m_newdata = true;
        m_panelSpectrum->Refresh();

        /* update scatter/eye plot ------------------------------------------------------------*/

        if (freedvInterface.isRunning()) {
            int currentMode = freedvInterface.getCurrentMode();
            if (currentMode != wxGetApp().m_prevMode)
            {
                // Force recreation of EQ filters.
                m_newMicInFilter = true;
                m_newSpkOutFilter = true;

                // The receive mode changed, so the previous samples are no longer valid.
                m_panelScatter->clearCurrentSamples();
            }
            wxGetApp().m_prevMode = currentMode;
        
            if (currentMode == FREEDV_MODE_800XA) {

                /* FSK Mode - eye diagram ---------------------------------------------------------*/

                /* add samples row by row */

                int i;
                for (i=0; i<freedvInterface.getCurrentRxModemStats()->neyetr; i++) {
                    m_panelScatter->add_new_samples_eye(&freedvInterface.getCurrentRxModemStats()->rx_eye[i][0], freedvInterface.getCurrentRxModemStats()->neyesamp);
                }
            }
            else {
                // Reset g_Nc accordingly.
                switch(currentMode)
                {
                    case FREEDV_MODE_1600:
                        g_Nc = 16;
                        m_panelScatter->setNc(g_Nc+1);  /* +1 for BPSK pilot */
                        break;
                    case FREEDV_MODE_700C:
                        /* m_FreeDV700Combine may have changed at run time */
                        g_Nc = 14;
                        if (wxGetApp().m_FreeDV700Combine) {
                            m_panelScatter->setNc(g_Nc/2);  /* diversity combnation */
                        }
                        else {
                            m_panelScatter->setNc(g_Nc);
                        }
                        break;
                    case FREEDV_MODE_700D:
                    case FREEDV_MODE_700E:
                        g_Nc = 17; 
                        m_panelScatter->setNc(g_Nc);
                        break;
                    case FREEDV_MODE_2020:
    #if defined(FREEDV_MODE_2020B)
                    case FREEDV_MODE_2020B:
    #endif // FREEDV_MODE_2020B
                        g_Nc = 31;
                        m_panelScatter->setNc(g_Nc);
                        break;
                }
            
                /* PSK Modes - scatter plot -------------------------------------------------------*/
                for (r=0; r<freedvInterface.getCurrentRxModemStats()->nr; r++) {

                    if ((currentMode == FREEDV_MODE_1600) ||
                        (currentMode == FREEDV_MODE_700D) ||
                        (currentMode == FREEDV_MODE_700E) ||
                        (currentMode == FREEDV_MODE_2020) 
    #if defined(FREEDV_MODE_2020B)
                    ||  (currentMode == FREEDV_MODE_2020B)
    #endif // FREEDV_MODE_2020B
                    ) {
                        m_panelScatter->add_new_samples_scatter(&freedvInterface.getCurrentRxModemStats()->rx_symbols[r][0]);
                    }
                    else if (currentMode == FREEDV_MODE_700C) {

                        if (wxGetApp().m_FreeDV700Combine) {
                            /*
                               FreeDV 700C uses diversity, so optionally combine
                               symbols for scatter plot, as combined symbols are
                               used for demodulation.  Note we need to use a copy
                               of the symbols, as we are not sure when the stats
                               will be updated.
                            */

                            COMP rx_symbols_copy[g_Nc/2];

                            for(c=0; c<g_Nc/2; c++)
                                rx_symbols_copy[c] = fcmult(0.5, cadd(freedvInterface.getCurrentRxModemStats()->rx_symbols[r][c], freedvInterface.getCurrentRxModemStats()->rx_symbols[r][c+g_Nc/2]));
                            m_panelScatter->add_new_samples_scatter(rx_symbols_copy);
                        }
                        else {
                            /*
                              Sometimes useful to plot carriers separately, e.g. to determine if tx carrier power is constant
                              across carriers.
                            */
                            m_panelScatter->add_new_samples_scatter(&freedvInterface.getCurrentRxModemStats()->rx_symbols[r][0]);
                        }
                    }

                }
            }
        }

        m_panelScatter->Refresh();

        // Oscilloscope type speech plots -------------------------------------------------------

        short speechInPlotSamples[WAVEFORM_PLOT_BUF];
        if (codec2_fifo_read(g_plotSpeechInFifo, speechInPlotSamples, WAVEFORM_PLOT_BUF)) {
            memset(speechInPlotSamples, 0, WAVEFORM_PLOT_BUF*sizeof(short));
        }
        m_panelSpeechIn->add_new_short_samples(0, speechInPlotSamples, WAVEFORM_PLOT_BUF, 32767);
        m_panelSpeechIn->Refresh();

        short speechOutPlotSamples[WAVEFORM_PLOT_BUF];
        if (codec2_fifo_read(g_plotSpeechOutFifo, speechOutPlotSamples, WAVEFORM_PLOT_BUF))
            memset(speechOutPlotSamples, 0, WAVEFORM_PLOT_BUF*sizeof(short));
        m_panelSpeechOut->add_new_short_samples(0, speechOutPlotSamples, WAVEFORM_PLOT_BUF, 32767);
        m_panelSpeechOut->Refresh();

        short demodInPlotSamples[WAVEFORM_PLOT_BUF];
        if (codec2_fifo_read(g_plotDemodInFifo, demodInPlotSamples, WAVEFORM_PLOT_BUF)) {
            memset(demodInPlotSamples, 0, WAVEFORM_PLOT_BUF*sizeof(short));
        }
        m_panelDemodIn->add_new_short_samples(0,demodInPlotSamples, WAVEFORM_PLOT_BUF, 32767);
        m_panelDemodIn->Refresh();

        // Demod states -----------------------------------------------------------------------

        m_panelTimeOffset->add_new_sample(0, (float)freedvInterface.getCurrentRxModemStats()->rx_timing/FDMDV_NOM_SAMPLES_PER_FRAME);
        m_panelTimeOffset->Refresh();

        m_panelFreqOffset->add_new_sample(0, freedvInterface.getCurrentRxModemStats()->foff);
        m_panelFreqOffset->Refresh();

        // SNR text box and gauge ------------------------------------------------------------

        // LP filter freedvInterface.getCurrentRxModemStats()->snr_est some more to stabilise the
        // display. freedvInterface.getCurrentRxModemStats()->snr_est already has some low pass filtering
        // but we need it fairly fast to activate squelch.  So we
        // optionally perform some further filtering for the display
        // version of SNR.  The "Slow" checkbox controls the amount of
        // filtering.  The filtered snr also controls the squelch

        float snr_limited;
        // some APIs pass us invalid values, so lets trap it rather than bombing
        float snrEstimate = freedvInterface.getSNREstimate();
        if (!(isnan(snrEstimate) || isinf(snrEstimate))) {
            g_snr = m_snrBeta*g_snr + (1.0 - m_snrBeta)*snrEstimate;
        }
        snr_limited = g_snr;
        if (snr_limited < -5.0) snr_limited = -5.0;
        if (snr_limited > 40.0) snr_limited = 40.0;
        char snr[15];
        snprintf(snr, 15, "%d dB", (int)(g_snr + 0.5));

        if (freedvInterface.getSync())
        {
            wxString snr_string(snr);
            m_textSNR->SetLabel(snr_string);
            m_gaugeSNR->SetValue((int)(snr_limited+5));
        }
        else
        {
            m_textSNR->SetLabel("--");
            m_gaugeSNR->SetValue(0);
        }

        // Level Gauge -----------------------------------------------------------------------

        float tooHighThresh;
        if (!g_tx && m_RxRunning)
        {
            // receive mode - display From Radio peaks
            // peak from this DT sampling period
            int maxDemodIn = 0;
            for(int i=0; i<WAVEFORM_PLOT_BUF; i++)
                if (maxDemodIn < abs(demodInPlotSamples[i]))
                    maxDemodIn = abs(demodInPlotSamples[i]);

            // peak from last second
            if (maxDemodIn > m_maxLevel)
                m_maxLevel = maxDemodIn;

            tooHighThresh = FROM_RADIO_MAX;
        }
        else
        {
            // transmit mode - display From Mic peaks

            // peak from this DT sampling period
            int maxSpeechIn = 0;
            for(int i=0; i<WAVEFORM_PLOT_BUF; i++)
                if (maxSpeechIn < abs(speechInPlotSamples[i]))
                    maxSpeechIn = abs(speechInPlotSamples[i]);

            // peak from last second
            if (maxSpeechIn > m_maxLevel)
                m_maxLevel = maxSpeechIn;

           tooHighThresh = FROM_MIC_MAX;
        }

        // Peak Reading meter: updates peaks immediately, then slowly decays
        int maxScaled = (int)(100.0 * ((float)m_maxLevel/32767.0));
        m_gaugeLevel->SetValue(maxScaled);
        if (((float)maxScaled/100) > tooHighThresh)
            m_textLevel->SetLabel("Too High");
        else
            m_textLevel->SetLabel("");

        m_maxLevel *= LEVEL_BETA;

        // sync LED (Colours don't work on Windows) ------------------------

        if (m_textSync->IsEnabled())
        {
            auto oldColor = m_textSync->GetForegroundColour();
            wxColour newColor = g_State ? wxColour( 0, 255, 0 ) : wxColour( 255, 0, 0 ); // green if sync, red otherwise
        
            if (g_State) 
            {
                if (g_prev_State == 0) 
                {
                    g_resyncs++;
                
                    // Auto-reset stats if we've gone long enough since losing sync.
                    // NOTE: m_timeSinceSyncLoss is in milliseconds.
                    if (m_timeSinceSyncLoss >= wxGetApp().appConfiguration.statsResetTimeSecs * 1000)
                    {
                        resetStats_();
                        
                        // Clear RX text to reduce the incidence of incorrect callsigns extracted with
                        // the PSK Reporter callsign extraction logic.
                        m_txtCtrlCallSign->SetValue(wxT(""));
                        m_cboLastReportedCallsigns->SetValue(wxT(""));
                        m_cboLastReportedCallsigns->Enable(m_lastReportedCallsignListView->GetItemCount() > 0);
                        memset(m_callsign, 0, MAX_CALLSIGN);
                        m_pcallsign = m_callsign;
            
                        // Get current time to enforce minimum sync time requirement for PSK Reporter.
                        g_sync_time = time(0);
            
                        freedvInterface.resetReliableText();
                    }
                }
                m_timeSinceSyncLoss = 0;
            }
            else
            {
                // Counts the amount of time since losing sync. Once we exceed
                // wxGetApp().appConfiguration.statsResetTimeSecs, we will reset the stats. 
                m_timeSinceSyncLoss += _REFRESH_TIMER_PERIOD;
            }
        
            if (oldColor != newColor)
            {
                m_textSync->SetForegroundColour(newColor);
        	    m_textSync->SetLabel("Modem");
                m_textSync->Refresh();
            }
        }
        g_prev_State = g_State;

        // send Callsign ----------------------------------------------------

        char callsign[MAX_CALLSIGN];
        memset(callsign, 0, MAX_CALLSIGN);
    
        if (!wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled)
        {
            strncpy(callsign, (const char*) wxGetApp().appConfiguration.reportingConfiguration.reportingFreeTextString->mb_str(wxConvUTF8), MAX_CALLSIGN - 2);
            if (strlen(callsign) < MAX_CALLSIGN - 1)
            {
                strncat(callsign, "\r", 2);
            }     
     
            // buffer 1 txt message to ensure tx data fifo doesn't "run dry"
            char* sendBuffer = &callsign[0];
            if ((unsigned)codec2_fifo_used(g_txDataInFifo) < strlen(sendBuffer)) {
                unsigned int  i;

                // write chars to tx data fifo
                for(i = 0; i < strlen(sendBuffer); i++) {
                    short ashort = (unsigned char)sendBuffer[i];
                    codec2_fifo_write(g_txDataInFifo, &ashort, 1);
                }
            }

            // See if any Callsign info received --------------------------------

            short ashort;
            while (codec2_fifo_read(g_rxDataOutFifo, &ashort, 1) == 0) {
                unsigned char incomingChar = (unsigned char)ashort;
        
                // Pre-1.5.1 behavior, where text is handled as-is.
                if (incomingChar == '\r' || incomingChar == '\n' || incomingChar == 0 || ((m_pcallsign - m_callsign) > MAX_CALLSIGN-1))
                {                        
                    // CR completes line. Fill in remaining positions with zeroes.
                    if ((m_pcallsign - m_callsign) <= MAX_CALLSIGN-1)
                    {
                        memset(m_pcallsign, 0, MAX_CALLSIGN - (m_pcallsign - m_callsign));
                    }
            
                    // Reset to the beginning.
                    m_pcallsign = m_callsign;
                }
                else
                {
                    *m_pcallsign++ = incomingChar;
                }
                m_txtCtrlCallSign->SetValue(m_callsign);
            }
        }

        // We should only report to reporters when all of the following are true:
        // a) The callsign encoder indicates a valid callsign has been received.
        // b) We detect a valid format callsign in the text (see https://en.wikipedia.org/wiki/Amateur_radio_call_signs).
        // c) We don't currently have a pending report to add to the outbound list for the active callsign.
        // When the above is true, capture the callsign and current SNR and add to the PSK Reporter object's outbound list.
        if (wxGetApp().m_reporters.size() > 0 && wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled)
        {
            const char* text = freedvInterface.getReliableText();
            assert(text != nullptr);
            wxString wxCallsign = text;
            delete[] text;
        
            auto pendingSnr = (int)(g_snr + 0.5);
            if (wxCallsign.Length() > 0)
            {
                freedvInterface.resetReliableText();
                
                wxRegEx callsignFormat("(([A-Za-z0-9]+/)?[A-Za-z0-9]{1,3}[0-9][A-Za-z0-9]*[A-Za-z](/[A-Za-z0-9]+)?)");
                if (callsignFormat.Matches(wxCallsign))
                {
                    wxString rxCallsign = callsignFormat.GetMatch(wxCallsign, 1);
                    std::string pendingCallsign = rxCallsign.ToStdString();

                    wxString freqString;
                    if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
                    {
                        double freq = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency.get() / 1000.0;
                        freqString = wxString::Format("%.01f", freq);
                    }
                    else
                    {
                        double freq = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency.get() / 1000000.0;
                        freqString = wxString::Format("%.04f", freq);
                    }

                    if (m_lastReportedCallsignListView->GetItemCount() == 0 || 
                        m_lastReportedCallsignListView->GetItemText(0, 0) != rxCallsign ||
                        m_lastReportedCallsignListView->GetItemText(0, 1) != freqString)
                    {
                        auto currentTime = wxDateTime::Now();
                        wxString currentTimeAsString = "";
                        
                        if (wxGetApp().appConfiguration.reportingConfiguration.useUTCForReporting)
                        {
                            currentTime = currentTime.ToUTC();
                        }
                        currentTimeAsString.Printf(wxT("%s %s"), currentTime.FormatISODate(), currentTime.FormatISOTime());
                        
                        auto index = m_lastReportedCallsignListView->InsertItem(0, rxCallsign, 0);
                        m_lastReportedCallsignListView->SetItem(index, 1, freqString);
                        m_lastReportedCallsignListView->SetItem(index, 2, currentTimeAsString);
                    }
                    
                    wxString snrAsString;
                    snrAsString.Printf(wxT("%0.1f"), g_snr);
                    auto index = m_lastReportedCallsignListView->GetTopItem();
                    m_lastReportedCallsignListView->SetItem(index, 3, snrAsString);
                    
                    m_cboLastReportedCallsigns->SetText(rxCallsign);
                    m_cboLastReportedCallsigns->Enable(m_lastReportedCallsignListView->GetItemCount() > 0);
           
                    if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseForPTT)
                    {
                        wxGetApp().rigFrequencyController->requestCurrentFrequencyMode();
                    }
            
                    int64_t freq = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;

                    // Only report if there's a valid reporting frequency and if we're not playing 
                    // a recording through ourselves (to avoid false reports).
                    if (freq > 0)
                    {
                        long long freqLongLong = freq;
                        log_info(
                            "Reporting callsign %s @ SNR %d, freq %lld to reporting services.\n", 
                            pendingCallsign.c_str(), 
                            pendingSnr,
                            freqLongLong);
        
                        if (!g_playFileFromRadio)
                        {
                            for (auto& obj : wxGetApp().m_reporters)
                            {
                                obj->addReceiveRecord(
                                    pendingCallsign,
                                    freedvInterface.getCurrentModeStr(),
                                    freq,
                                    pendingSnr);
                            }
                        }
                    }
                }
            }
            else if (
                wxGetApp().m_sharedReporterObject && 
                freedvInterface.getCurrentMode() == FREEDV_MODE_RADE && 
                freedvInterface.getSync())
            {               
                // Special case for RADE--report '--' for callsign so we can
                // at least report that we're receiving *something*.
                int64_t freq = wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency;

                // Only report if there's a valid reporting frequency and if we're not playing 
                // a recording through ourselves (to avoid false reports).
                wxGetApp().m_reportCounter = (wxGetApp().m_reportCounter + 1) % 10;
                if (freq > 0 && wxGetApp().m_reportCounter == 0)
                {
                    wxGetApp().m_reportCounter = 0;
                    if (!g_playFileFromRadio)
                    {                
                        wxGetApp().m_sharedReporterObject->addReceiveRecord(
                            "",
                            freedvInterface.getCurrentModeStr(),
                            freq,
                            pendingSnr
                        );
                    }
                }
            }
        }
    
        // Run time update of EQ filters -----------------------------------

        g_mutexProtectingCallbackData.Lock();

        bool micEqEnableOld = g_rxUserdata->micInEQEnable;
        bool spkrEqEnableOld = g_rxUserdata->spkOutEQEnable;

        if (m_newMicInFilter || m_newSpkOutFilter ||
            micEqEnableOld != wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable ||
            spkrEqEnableOld != wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable) {
            
            deleteEQFilters(g_rxUserdata);
        
            g_rxUserdata->micInEQEnable = wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable;
            g_rxUserdata->spkOutEQEnable = wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable;

            if (m_newMicInFilter || m_newSpkOutFilter)
            {
                if (g_nSoundCards == 1)
                {
                    designEQFilters(g_rxUserdata, wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.sampleRate, 0);
                }
                else
                {   
                    designEQFilters(g_rxUserdata, wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.sampleRate, wxGetApp().appConfiguration.audioConfiguration.soundCard2In.sampleRate);
                }
            }

            m_newMicInFilter = m_newSpkOutFilter = false;
        }
        g_mutexProtectingCallbackData.Unlock();
    
        // set some run time options (if applicable)
        freedvInterface.setRunTimeOptions(
            (int)wxGetApp().appConfiguration.freedv700Clip,
            (int)wxGetApp().appConfiguration.freedv700TxBPF);

        // Test Frame Bit Error Updates ------------------------------------

        // Toggle test frame mode at run time

        if (!freedvInterface.usingTestFrames() && wxGetApp().m_testFrames) {
            // reset stats on check box off to on transition
            freedvInterface.resetTestFrameStats();
        }
        freedvInterface.setTestFrames(wxGetApp().m_testFrames, wxGetApp().m_FreeDV700Combine);
        g_channel_noise = wxGetApp().m_channel_noise;

        // update stats on main page

        const int STR_LENGTH = 80;
        char 
            mode[STR_LENGTH], bits[STR_LENGTH], errors[STR_LENGTH], ber[STR_LENGTH], 
            resyncs[STR_LENGTH], clockoffset[STR_LENGTH], freqoffset[STR_LENGTH], syncmetric[STR_LENGTH];
        snprintf(mode, STR_LENGTH, "Mode: %s", freedvInterface.getCurrentModeStr()); wxString modeString(mode); m_textCurrentDecodeMode->SetLabel(modeString);
        snprintf(bits, STR_LENGTH, "Bits: %d", freedvInterface.getTotalBits()); wxString bits_string(bits); m_textBits->SetLabel(bits_string);
        snprintf(errors, STR_LENGTH, "Errs: %d", freedvInterface.getTotalBitErrors()); wxString errors_string(errors); m_textErrors->SetLabel(errors_string);
        float b = (float)freedvInterface.getTotalBitErrors()/(1E-6+freedvInterface.getTotalBits());
        snprintf(ber, STR_LENGTH, "BER: %4.3f", b); wxString ber_string(ber); m_textBER->SetLabel(ber_string);
        snprintf(resyncs, STR_LENGTH, "Resyncs: %d", g_resyncs); wxString resyncs_string(resyncs); m_textResyncs->SetLabel(resyncs_string);

        snprintf(freqoffset, STR_LENGTH, "FrqOff: %3.1f", freedvInterface.getCurrentRxModemStats()->foff);
        wxString freqoffset_string(freqoffset); m_textFreqOffset->SetLabel(freqoffset_string);
        snprintf(syncmetric, STR_LENGTH, "Sync: %3.2f", freedvInterface.getCurrentRxModemStats()->sync_metric);
        wxString syncmetric_string(syncmetric); m_textSyncMetric->SetLabel(syncmetric_string);

        // Codec 2 700C/D/E & 800XA VQ "auto EQ" equaliser variance
        auto var = freedvInterface.getVariance();
        char var_str[STR_LENGTH]; snprintf(var_str, STR_LENGTH, "Var: %4.1f", var);
        wxString var_string(var_str); m_textCodec2Var->SetLabel(var_string);

        if (g_State) {

            snprintf(clockoffset, STR_LENGTH, "ClkOff: %+-d", (int)round(freedvInterface.getCurrentRxModemStats()->clock_offset*1E6) % 10000);
            wxString clockoffset_string(clockoffset); m_textClockOffset->SetLabel(clockoffset_string);

            // update error pattern plots if supported
            short* error_pattern = nullptr;
            int sz_error_pattern = freedvInterface.getErrorPattern(&error_pattern);
            if (sz_error_pattern) {
                int i,b;

                /* both modes map IQ to alternate bits, but on same carrier */

                if (freedvInterface.getCurrentMode() == FREEDV_MODE_1600) {
                    /* FreeDV 1600 mapping from error pattern to two bits on each carrier */

                    for(b=0; b<g_Nc*2; b++) {
                        for(i=b; i<sz_error_pattern; i+= 2*g_Nc) {
                            m_panelTestFrameErrors->add_new_sample(b, b + 0.8*error_pattern[i]);
                            g_error_hist[b] += error_pattern[i];
                            g_error_histn[b]++;
                        }
                    }

                     /* calculate BERs and send to plot */

                    float ber[2*MODEM_STATS_NC_MAX];
                    for(b=0; b<2*MODEM_STATS_NC_MAX; b++) {
                        ber[b] = 0.0;
                    }
                    for(b=0; b<g_Nc*2; b++) {
                        ber[b+1] = (float)g_error_hist[b]/g_error_histn[b];
                    }
                    assert(g_Nc*2 <= 2*MODEM_STATS_NC_MAX);
                    m_panelTestFrameErrorsHist->add_new_samples(0, ber, 2*MODEM_STATS_NC_MAX);
                }

                if ((freedvInterface.getCurrentMode() == FREEDV_MODE_700C)) {
                    int c;

                    /*
                       FreeDV 700 mapping from error pattern to bit on each carrier, see
                       data bit to carrier mapping in:

                          codec2-dev/octave/cohpsk_frame_design.ods

                       We can plot a histogram of the errors/carrier before or after diversity
                       recombination.  Actually one bar for each IQ bit in carrier order.
                    */

                    int hist_Nc = sz_error_pattern/4;

                    for(i=0; i<sz_error_pattern; i++) {
                        /* maps to IQ bits from each symbol to a "carrier" (actually one line for each IQ bit in carrier order) */
                        c = floor(i/4);
                        /* this will clock in 4 bits/carrier to plot */
                        m_panelTestFrameErrors->add_new_sample(c, c + 0.8*error_pattern[i]);
                        g_error_hist[c] += error_pattern[i];
                        g_error_histn[c]++;
                    }
                    for(; i<2*MODEM_STATS_NC_MAX*4; i++) {
                        c = floor(i/4);
                        m_panelTestFrameErrors->add_new_sample(c, c);
                    }

                    /* calculate BERs and send to plot */

                    float ber[2*MODEM_STATS_NC_MAX];
                    for(b=0; b<2*MODEM_STATS_NC_MAX; b++) {
                        ber[b] = 0.0;
                    }
                    for(b=0; b<hist_Nc; b++) {
                        ber[b+1] = (float)g_error_hist[b]/g_error_histn[b];
                    }
                    assert(hist_Nc <= 2*MODEM_STATS_NC_MAX);
                    m_panelTestFrameErrorsHist->add_new_samples(0, ber, 2*MODEM_STATS_NC_MAX);
                }

                m_panelTestFrameErrors->Refresh();
                m_panelTestFrameErrorsHist->Refresh();
            
                delete[] error_pattern;
            }
        }

        /* FIFO and PortAudio under/overflow debug counters */
        optionsDlg->DisplayFifoPACounters();

        // command from UDP thread that is best processed in main thread to avoid seg faults

        if (m_schedule_restore) {
            if (IsIconized())
                Restore();
            m_schedule_restore = false;
        }
    
        // Voice Keyer state machine
        VoiceKeyerProcessEvent(VK_DT);

        // Detect Sync state machine
        DetectSyncProcessEvent();
    }
}
#endif

void MainFrame::topFrame_OnClose( wxCloseEvent& event )
{
    if (m_RxRunning)
    {
        if (m_btnTogPTT->GetValue())
        {
            // Stop PTT first
            togglePTT();
        }
        
        // Stop execution.
        terminating_ = true;
        wxCommandEvent* offEvent = new wxCommandEvent(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, m_togBtnOnOff->GetId());
        offEvent->SetEventObject(m_togBtnOnOff);
        m_togBtnOnOff->SetValue(false);
        OnTogBtnOnOff(*offEvent);
        delete offEvent;
    } 
    else
    {
        TopFrame::topFrame_OnClose(event);
    }
}

//-------------------------------------------------------------------------
// OnExit()
//-------------------------------------------------------------------------
void MainFrame::OnExit(wxCommandEvent& event)
{
    if (m_RxRunning)
    {
        if (m_btnTogPTT->GetValue())
        {
            // Stop PTT first
            togglePTT();
        }
        
        // Stop execution.
        terminating_ = true;
        wxCommandEvent* offEvent = new wxCommandEvent(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, m_togBtnOnOff->GetId());
        offEvent->SetEventObject(m_togBtnOnOff);
        m_togBtnOnOff->SetValue(false);
        OnTogBtnOnOff(*offEvent);
        delete offEvent;
    } 
    else
    {
        Destroy();
    }
}

void MainFrame::OnChangeTxMode( wxCommandEvent& event )
{
    wxRadioButton* hiddenModeToSet = nullptr;
    std::vector<wxRadioButton*> buttonsToClear 
    {
        m_hiddenMode1,
        m_hiddenMode2,

        m_rbRADE,
        m_rb700c,
        m_rb700d,
        m_rb700e,
        m_rb800xa,
        m_rb1600,
        m_rb2020,
#if defined(FREEDV_MODE_2020B)
        m_rb2020b,
#endif // FREEDV_MODE_2020B
    };

    auto eventObject = (wxRadioButton*)event.GetEventObject();
    if (eventObject != nullptr)
    {
        std::string label = (const char*)eventObject->GetLabel().ToUTF8();
        if (label == "700D" || label == "700E" || label == "1600" || label == "RADEV1")
        {
            hiddenModeToSet = m_hiddenMode2;
        } 
        else
        {
            hiddenModeToSet = m_hiddenMode1;
        } 
 
        buttonsToClear.erase(std::find(buttonsToClear.begin(), buttonsToClear.end(), (wxRadioButton*)eventObject));
    }

    txModeChangeMutex.Lock();
    
    if (eventObject == m_rb1600 || (eventObject == nullptr && m_rb1600->GetValue())) 
    {
        g_mode = FREEDV_MODE_1600;
    }
    else if (eventObject == m_rb700c || (eventObject == nullptr && m_rb700c->GetValue())) 
    {
        g_mode = FREEDV_MODE_700C;
    }
    else if (eventObject == m_rbRADE || (eventObject == nullptr && m_rbRADE->GetValue())) 
    {
        g_mode = FREEDV_MODE_RADE;
    }
    else if (eventObject == m_rb700d || (eventObject == nullptr && m_rb700d->GetValue())) 
    {
        g_mode = FREEDV_MODE_700D;
    }
    else if (eventObject == m_rb700e || (eventObject == nullptr && m_rb700e->GetValue())) 
    {
        g_mode = FREEDV_MODE_700E;
    }
    else if (eventObject == m_rb800xa || (eventObject == nullptr && m_rb800xa->GetValue())) 
    {
        g_mode = FREEDV_MODE_800XA;
    }
    else if (eventObject == m_rb2020 || (eventObject == nullptr && m_rb2020->GetValue())) 
    {
        assert(wxGetApp().appConfiguration.freedv2020Allowed && wxGetApp().appConfiguration.freedvAVXSupported);
        
        g_mode = FREEDV_MODE_2020;
    }
#if defined(FREEDV_MODE_2020B)
    else if (eventObject == m_rb2020b || (eventObject == nullptr && m_rb2020b->GetValue())) 
    {
        assert(wxGetApp().appConfiguration.freedv2020Allowed && wxGetApp().appConfiguration.freedvAVXSupported);
        
        g_mode = FREEDV_MODE_2020B;
    }
#endif // FREEDV_MODE_2020B
    
    if (freedvInterface.isRunning())
    {
        // Need to change the TX interface live.
        freedvInterface.changeTxMode(g_mode);
    }
    
    // Force recreation of EQ filters.
    m_newMicInFilter = true;
    m_newSpkOutFilter = true;
    
    txModeChangeMutex.Unlock();
    
    // Manually implement mutually exclusive behavior as
    // we can't rely on wxWidgets doing it on account of
    // how we're splitting the modes.
    if (eventObject != nullptr)
    {
        buttonsToClear.erase(std::find(buttonsToClear.begin(), buttonsToClear.end(), hiddenModeToSet));

        for (auto& var : buttonsToClear)
        {
            var->SetValue(false);
        }

        hiddenModeToSet->SetValue(true);
    }
    
    // Report TX change to registered reporters
    for (auto& obj : wxGetApp().m_reporters)
    {
        obj->transmit(freedvInterface.getCurrentTxModeStr(), g_tx);
    }
}

void MainFrame::performFreeDVOn_()
{
    log_debug("Start .....");
    g_queueResync = false;
    endingTx = false;
    
    m_timeSinceSyncLoss = 0;
    
    executeOnUiThreadAndWait_([&]() 
    {        
        m_txtCtrlCallSign->SetValue(wxT(""));
        m_lastReportedCallsignListView->DeleteAllItems();
        m_cboLastReportedCallsigns->Enable(false);
            
        m_cboLastReportedCallsigns->SetText(wxT(""));
    });
    
    memset(m_callsign, 0, MAX_CALLSIGN);
    m_pcallsign = m_callsign;

    freedvInterface.resetReliableText();
    
    //
    // Start Running -------------------------------------------------
    //

    vk_state = VK_IDLE;

    // modify some button states when running
    executeOnUiThreadAndWait_([&]() 
    {
        m_textSync->Enable();
        m_textCurrentDecodeMode->Enable();
        
        // determine what mode we are using
        wxCommandEvent tmpEvent;
        OnChangeTxMode(tmpEvent);

        if (!wxGetApp().appConfiguration.multipleReceiveEnabled || m_rbRADE->GetValue())
        {
            m_rb1600->Disable();
            m_rbRADE->Disable();
            m_rb700c->Disable();
            m_rb700d->Disable();
            m_rb700e->Disable();
            m_rb800xa->Disable();
            m_rb2020->Disable();
    #if defined(FREEDV_MODE_2020B)
            m_rb2020b->Disable();
    #endif // FREEDV_MODE_2020B
            freedvInterface.addRxMode(g_mode);
        }
        else
        {
            m_rbRADE->Disable();
            
            if(wxGetApp().appConfiguration.freedv2020Allowed && wxGetApp().appConfiguration.freedvAVXSupported)
            {
                freedvInterface.addRxMode(FREEDV_MODE_2020);
    #if defined(FREEDV_MODE_2020B)
                freedvInterface.addRxMode(FREEDV_MODE_2020B);
    #endif // FREEDV_MODE_2020B
            }
        
            int rxModes[] = {
                FREEDV_MODE_1600,
                FREEDV_MODE_700E,
                FREEDV_MODE_700C,
                FREEDV_MODE_700D,
                FREEDV_MODE_800XA
            };

            for (auto& mode : rxModes)
            {
                freedvInterface.addRxMode(mode);
            }
        
            // If we're receive-only, it doesn't make sense to be able to change TX mode.
            if (g_nSoundCards <= 1)
            {
                m_rb1600->Disable();
                m_rbRADE->Disable();
                m_rb700c->Disable();
                m_rb700d->Disable();
                m_rb700e->Disable();
                m_rb800xa->Disable();
                m_rb2020->Disable();
        #if defined(FREEDV_MODE_2020B)
                m_rb2020b->Disable();
        #endif // FREEDV_MODE_2020B
            }
        }
        
        // Default voice keyer sample rate to 8K. The exact voice keyer
        // sample rate will be determined when the .wav file is loaded.
        g_sfTxFs = FS;
    
        wxGetApp().m_prevMode = g_mode;
        freedvInterface.start(g_mode, wxGetApp().appConfiguration.fifoSizeMs, !wxGetApp().appConfiguration.multipleReceiveEnabled || wxGetApp().appConfiguration.multipleReceiveOnSingleThread, wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled);

        // Codec 2 VQ Equaliser
        freedvInterface.setEq(wxGetApp().appConfiguration.filterConfiguration.enable700CEqualizer);

        // Codec2 verbosity setting
        freedvInterface.setVerbose(g_freedv_verbose);

        // Text field/callsign callbacks.
        if (!wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled)
        {
            freedvInterface.setTextCallbackFn(&my_put_next_rx_char, &my_get_next_tx_char);
        }
        else
        {
            char temp[9];
            memset(temp, 0, 9);
            strncpy(temp, wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign->ToUTF8(), 8); // One less than the size of temp to ensure we don't overwrite the null.
            log_info("Setting callsign to %s", temp);
            freedvInterface.setReliableText(temp);
        }
    
        g_error_hist = new short[MODEM_STATS_NC_MAX*2];
        g_error_histn = new short[MODEM_STATS_NC_MAX*2];
        int i;
        for(i=0; i<2*MODEM_STATS_NC_MAX; i++) {
            g_error_hist[i] = 0;
            g_error_histn[i] = 0;
        }

        // init Codec 2 LPC Post Filter (FreeDV 1600)
        freedvInterface.setLpcPostFilter(
                                       wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterEnable,
                                       wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterBassBoost,
                                       wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterBeta,
                                       wxGetApp().appConfiguration.filterConfiguration.codec2LPCPostFilterGamma);

        // Init Speex pre-processor states
        // by inspecting Speex source it seems that only denoiser is on by default

        log_debug("freedv_get_n_speech_samples(tx): %d", freedvInterface.getTxNumSpeechSamples());
        log_debug("freedv_get_speech_sample_rate(tx): %d", freedvInterface.getTxSpeechSampleRate());
    
        // adjust spectrum and waterfall freq scaling base on mode
        m_panelSpectrum->setFreqScale(MODEM_STATS_NSPEC*((float)MAX_F_HZ/(freedvInterface.getTxModemSampleRate()/2)));
        m_panelWaterfall->setFs(freedvInterface.getTxModemSampleRate());
    
        // Init text msg decoding
        if (!wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled)
            freedvInterface.setTextVaricodeNum(1);

        // scatter plot (PSK) or Eye (FSK) mode
        if (g_mode == FREEDV_MODE_800XA) {
            m_panelScatter->setEyeScatter(PLOT_SCATTER_MODE_EYE);
        }
        else {
            m_panelScatter->setEyeScatter(PLOT_SCATTER_MODE_SCATTER);
        }
    });

    g_State = g_prev_State = 0;
    g_snr = 0.0;
    g_half_duplex = wxGetApp().appConfiguration.halfDuplexMode;

    m_pcallsign = m_callsign;
    memset(m_callsign, 0, sizeof(m_callsign));

    m_maxLevel = 0;
    executeOnUiThreadAndWait_([&]() 
    {
        m_textLevel->SetLabel(wxT(""));
        m_gaugeLevel->SetValue(0);
    });
    
    // attempt to start sound cards and tx/rx processing
    std::promise<bool> tmpPromise;
    std::future<bool> tmpFuture = tmpPromise.get_future();

    // Note: this executes on the UI thread as macOS may need to display popups
    // to process this request.
    CallAfter([&]() {
        VerifyMicrophonePermissions(tmpPromise);
    });

    tmpFuture.wait();
    
    if (tmpFuture.get())
    {
        bool soundCardSetupValid = false;
        executeOnUiThreadAndWait_([&]() {
            soundCardSetupValid = validateSoundCardSetup();
        });
        
        if (soundCardSetupValid)
        {
            wxGetApp().m_reporters.clear();
            
            startRxStream();

            if (m_RxRunning)
            {
                // attempt to start PTT ......            
                if (wxGetApp().appConfiguration.rigControlConfiguration.hamlibUseForPTT)
                {
                    OpenHamlibRig();
                }
                else if (wxGetApp().appConfiguration.rigControlConfiguration.useSerialPTT) 
                {
                    OpenSerialPort();
                }
                else
                {
                    wxGetApp().rigPttController = nullptr;
                }
                
    #if defined(WIN32)
                if (wxGetApp().appConfiguration.rigControlConfiguration.useOmniRig)
                {
                    // OmniRig can be anbled along with serial port PTT.
                    // The logic below will ensure we don't overwrite the serial PTT
                    // handler.
                    OpenOmniRig();
                }
    #endif // defined(WIN32)
                    
                // Initialize PSK Reporter reporting.
                if (wxGetApp().appConfiguration.reportingConfiguration.reportingEnabled)
                {        
                    if (wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign->ToStdString() == "" || wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare->ToStdString() == "")
                    {
                        executeOnUiThreadAndWait_([&]() 
                        {
                            wxMessageBox("Reporting requires a valid callsign and grid square in Tools->Options. Reporting will be disabled.", wxT("Error"), wxOK | wxICON_ERROR, this);
                        });
                    }
                    else
                    {
                        if (wxGetApp().appConfiguration.reportingConfiguration.pskReporterEnabled)
                        {
                            auto pskReporter = 
                                std::make_shared<PskReporter>(
                                    wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign->ToStdString(), 
                                    wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare->ToStdString(),
                                    std::string("FreeDV ") + FREEDV_VERSION);
                            assert(pskReporter != nullptr);
                            wxGetApp().m_reporters.push_back(pskReporter);
                        }
                        
                        if (wxGetApp().appConfiguration.reportingConfiguration.freedvReporterEnabled)
                        {
                            wxGetApp().m_reporters.push_back(wxGetApp().m_sharedReporterObject);
                            wxGetApp().m_sharedReporterObject->showOurselves();
                        }

                        // Enable FreeDV Reporter timer (every 5 minutes).
                        executeOnUiThreadAndWait_([&]() 
                        {
                            m_pskReporterTimer.Start(5 * 60 * 1000);
                        });

                        // Make sure QSY button becomes enabled after start.
                        executeOnUiThreadAndWait_([&]() 
                        {
                            if (m_reporterDialog != nullptr)
                            {
                                m_reporterDialog->refreshQSYButtonState();
                            }
                        });
                        
                        // Immediately transmit selected TX mode and frequency to avoid UI glitches.
                        for (auto& obj : wxGetApp().m_reporters)
                        {
                            obj->transmit(freedvInterface.getCurrentTxModeStr(), g_tx);
                            obj->freqChange(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
                        }
                    }
                }
                else
                {
                    wxGetApp().m_reporters.clear();
                }

                if (wxGetApp().appConfiguration.rigControlConfiguration.useSerialPTTInput)
                {
                    OpenPTTInPort();
                }

                executeOnUiThreadAndWait_([&]() 
                {
        #ifdef _USE_TIMER
                    m_plotTimer.Start(_REFRESH_TIMER_PERIOD, wxTIMER_CONTINUOUS);
                    m_updFreqStatusTimer.Start(5*1000); // every 5 seconds[UP]
        #endif // _USE_TIMER
                });
            }
        }
    }
    else
    {
        executeOnUiThreadAndWait_([&]() 
        {
            wxMessageBox(wxString("Microphone permissions must be granted to FreeDV for it to function properly."), wxT("Error"), wxOK | wxICON_ERROR, this);
        });
    }

    // Clear existing TX text, if any.
    codec2_fifo_destroy(g_txDataInFifo);
    g_txDataInFifo = codec2_fifo_create(MAX_CALLSIGN*FREEDV_VARICODE_MAX_BITS);
}

void MainFrame::performFreeDVOff_()
{
    log_debug("Stop .....");
    
    //
    // Stop Running -------------------------------------------------
    //

#ifdef _USE_TIMER
    executeOnUiThreadAndWait_([&]() 
    {
        m_plotTimer.Stop();
        m_pskReporterTimer.Stop();
        m_updFreqStatusTimer.Stop(); // [UP]
    });
#endif // _USE_TIMER
    
    // always end with PTT in rx state
    if (wxGetApp().rigPttController != nullptr && wxGetApp().rigPttController->isConnected())
    {
        wxGetApp().rigPttController->ptt(false);
        wxGetApp().rigPttController->disconnect();
    }
    wxGetApp().rigPttController = nullptr;

    if (wxGetApp().rigFrequencyController != nullptr && wxGetApp().rigFrequencyController->isConnected())
    {
        wxGetApp().rigFrequencyController->disconnect();
    }
    wxGetApp().rigFrequencyController = nullptr;

    if (wxGetApp().appConfiguration.rigControlConfiguration.useSerialPTTInput)
    {
        ClosePTTInPort();
    }
    
    executeOnUiThreadAndWait_([&]() 
    {
        m_btnTogPTT->SetValue(false);
        VoiceKeyerProcessEvent(VK_SPACE_BAR);
    });
    
    stopRxStream();
         
    wxGetApp().m_reporters.clear();
    if (wxGetApp().m_sharedReporterObject)
    {
        wxGetApp().m_sharedReporterObject->hideFromView();
    }
    
    // FreeDV clean up
    delete[] g_error_hist;
    delete[] g_error_histn;
    freedvInterface.stop();
    
    m_newMicInFilter = m_newSpkOutFilter = true;

    executeOnUiThreadAndWait_([&]() 
    {
        m_textSync->Disable();
        m_textCurrentDecodeMode->Disable();

        m_togBtnAnalog->Disable();
        m_btnTogPTT->Disable();
        m_togBtnVoiceKeyer->Disable();
    
        m_rbRADE->Enable();
        m_rb1600->Enable();
        m_rb700c->Enable();
        m_rb700d->Enable();
        m_rb700e->Enable();
        m_rb800xa->Enable();
        if(wxGetApp().appConfiguration.freedv2020Allowed && wxGetApp().appConfiguration.freedvAVXSupported)
        {
            m_rb2020->Enable();
    #if defined(FREEDV_MODE_2020B)
            m_rb2020b->Enable();
    #endif // FREEDV_MODE_2020B
        }
        
        // Make sure QSY button becomes disabled after stop.
        if (m_reporterDialog != nullptr)
        {
            m_reporterDialog->refreshQSYButtonState();
        }
    });
}

//-------------------------------------------------------------------------
// OnTogBtnOnOff()
//-------------------------------------------------------------------------
void MainFrame::OnTogBtnOnOff(wxCommandEvent& event)
{
    if (!m_togBtnOnOff->IsEnabled()) return;

    m_togBtnOnOff->SetFocus();
    
    // Disable buttons while on/off is occurring
    m_togBtnOnOff->Enable(false);
    m_togBtnAnalog->Enable(false);
    m_togBtnVoiceKeyer->Enable(false);
    m_btnTogPTT->Enable(false);
        
    // we are attempting to start

    if (m_togBtnOnOff->GetValue())
    {
        std::thread onOffExec([this]() 
        {
#if defined(__linux__)
    pthread_setname_np(pthread_self(), "FreeDV TurningOn");
#endif // defined(__linux__)

            performFreeDVOn_();
            
            if (!m_RxRunning)
            {
                // Startup failed.
                performFreeDVOff_();
            }

            // On/Off actions complete, re-enable button.
            executeOnUiThreadAndWait_([&]() {
                bool txEnabled = 
                    m_RxRunning && 
                    !wxGetApp().appConfiguration.reportingConfiguration.freedvReporterForceReceiveOnly &&
                    (g_nSoundCards == 2);
                
                m_togBtnAnalog->Enable(m_RxRunning);
                m_togBtnVoiceKeyer->Enable(txEnabled);
                m_btnTogPTT->Enable(txEnabled);
                optionsDlg->setSessionActive(m_RxRunning);

                if (m_RxRunning)
                {
                    m_togBtnOnOff->SetLabel(wxT("&Stop"));
                }
                m_togBtnOnOff->SetValue(m_RxRunning);
                m_togBtnOnOff->Enable(true);

                // On some systems the Report Frequency box ends up getting
                // focus after clicking on Start. This causes the frequency
                // to never update. To avoid this, we force focus to be elsewhere
                // in the window.
                m_auiNbookCtrl->SetFocus();
            });
        });
        onOffExec.detach();
    }
    else
    {
        std::thread onOffExec([this]() 
        {
#if defined(__linux__)
    pthread_setname_np(pthread_self(), "FreeDV TurningOff");
#endif // defined(__linux__)

            performFreeDVOff_();
            
            // On/Off actions complete, re-enable button.
            executeOnUiThreadAndWait_([&]() {
                m_togBtnAnalog->Enable(m_RxRunning);
                m_togBtnVoiceKeyer->Enable(m_RxRunning);
                m_btnTogPTT->Enable(m_RxRunning);
                optionsDlg->setSessionActive(m_RxRunning);
                m_togBtnOnOff->SetValue(m_RxRunning);
                m_togBtnOnOff->SetLabel(wxT("&Start"));
                m_togBtnOnOff->Enable(true);

                if (terminating_)
                {
                    CallAfter([&]() { Destroy(); });
                }
            });
        });
        onOffExec.detach();
    }
}

static std::mutex stoppingMutex;

//-------------------------------------------------------------------------
// stopRxStream()
//-------------------------------------------------------------------------
void MainFrame::stopRxStream()
{
    std::unique_lock<std::mutex> lk(stoppingMutex);

    if(m_RxRunning)
    {
        m_RxRunning = false;

        if (m_txThread)
        {
            if (txInSoundDevice)
            {
                txInSoundDevice->stop();
                txInSoundDevice.reset();
            }
            
            if (txOutSoundDevice)
            {
                txOutSoundDevice->stop();
                txOutSoundDevice.reset();
            }
            
            m_txThread->terminateThread();
            m_txThread->Wait();
            
            delete m_txThread;
            m_txThread = nullptr;
        }

        if (m_rxThread)
        {
            if (rxInSoundDevice)
            {
                rxInSoundDevice->stop();
                rxInSoundDevice.reset();
            }
        
            if (rxOutSoundDevice)
            {
                rxOutSoundDevice->stop();
                rxOutSoundDevice.reset();
            }
            
            m_rxThread->terminateThread();
            m_rxThread->Wait();
            
            delete m_txThread;
            m_rxThread = nullptr;
        }

        wxGetApp().linkStep = nullptr;
        destroy_fifos();
        
        // Free memory allocated for filters.
        m_newMicInFilter = true;
        m_newSpkOutFilter = true;
        deleteEQFilters(g_rxUserdata);
        delete g_rxUserdata;
        
        auto engine = AudioEngineFactory::GetAudioEngine();
        engine->stop();
        engine->setOnEngineError(nullptr, nullptr);
    }
}

void MainFrame::destroy_fifos(void)
{
    if (g_rxUserdata->infifo1) codec2_fifo_destroy(g_rxUserdata->infifo1);
    if (g_rxUserdata->outfifo1) codec2_fifo_destroy(g_rxUserdata->outfifo1);
    if (g_rxUserdata->infifo2) codec2_fifo_destroy(g_rxUserdata->infifo2);
    if (g_rxUserdata->outfifo2) codec2_fifo_destroy(g_rxUserdata->outfifo2);
    codec2_fifo_destroy(g_rxUserdata->rxinfifo);
    codec2_fifo_destroy(g_rxUserdata->rxoutfifo);
    
    g_rxUserdata->infifo1 = nullptr;
    g_rxUserdata->infifo2 = nullptr;
    g_rxUserdata->outfifo1 = nullptr;
    g_rxUserdata->outfifo2 = nullptr;
    g_rxUserdata->rxinfifo = nullptr;
    g_rxUserdata->rxoutfifo = nullptr;
}

//-------------------------------------------------------------------------
// startRxStream()
//-------------------------------------------------------------------------
void MainFrame::startRxStream()
{
    log_debug("startRxStream .....");
    if(!m_RxRunning) {
        m_RxRunning = true;
        
        auto engine = AudioEngineFactory::GetAudioEngine();
        engine->setOnEngineError([&](IAudioEngine&, std::string error, void* state) {
            executeOnUiThreadAndWait_([&, error]() {
                wxMessageBox(wxString::Format(
                             "Error encountered while initializing the audio engine: %s.", 
                             error), wxT("Error"), wxOK, this); 
            });
        }, nullptr);
        engine->start();

        if (g_nSoundCards == 0) 
        {
            executeOnUiThreadAndWait_([&]() {
                wxMessageBox(wxT("No Sound Cards configured, use Tools - Audio Config to configure"), wxT("Error"), wxOK);
            });
            
            m_RxRunning = false;
            
            engine->stop();
            engine->setOnEngineError(nullptr, nullptr);
            
            return;
        }
        else if (g_nSoundCards == 1)
        {
            // RX-only setup.
            // Note: we assume 2 channels, but IAudioEngine will automatically downgrade to 1 channel if needed.
            rxInSoundDevice = engine->getAudioDevice(wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName, IAudioEngine::AUDIO_ENGINE_IN, wxGetApp().appConfiguration.audioConfiguration.soundCard1In.sampleRate, 2);
            rxInSoundDevice->setDescription("Radio to FreeDV");
            rxInSoundDevice->setOnAudioDeviceChanged([&](IAudioDevice&, std::string newDeviceName, void*) {
                CallAfter([&, newDeviceName]() {
                    wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName = wxString::FromUTF8(newDeviceName.c_str());
                    wxGetApp().appConfiguration.save(pConfig);
                });
            }, nullptr);
            
            rxOutSoundDevice = engine->getAudioDevice(wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName, IAudioEngine::AUDIO_ENGINE_OUT, wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.sampleRate, 2);
            rxOutSoundDevice->setDescription("FreeDV to Speaker");
            rxOutSoundDevice->setOnAudioDeviceChanged([&](IAudioDevice&, std::string newDeviceName, void*) {
                CallAfter([&, newDeviceName]() {
                    wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName = wxString::FromUTF8(newDeviceName.c_str());
                    wxGetApp().appConfiguration.save(pConfig);
                });
            }, nullptr);
            
            bool failed = false;
            if (!rxInSoundDevice)
            {
                executeOnUiThreadAndWait_([&]() {
                    wxMessageBox(wxString::Format("Could not find RX input sound device '%s'. Please check settings and try again.", wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName.get()), wxT("Error"), wxOK);
                });
                failed = true;
            }
            
            if (!rxOutSoundDevice)
            {
                executeOnUiThreadAndWait_([&]() {
                    wxMessageBox(wxString::Format("Could not find RX output sound device '%s'. Please check settings and try again.", wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName.get()), wxT("Error"), wxOK);
                });
                failed = true;
            }
            
            if (failed)
            {
                if (rxInSoundDevice)
                {
                    rxInSoundDevice.reset();
                }
                
                if (rxOutSoundDevice)
                {
                    rxOutSoundDevice.reset();
                }
                
                m_RxRunning = false;
            
                engine->stop();
                engine->setOnEngineError(nullptr, nullptr);
            
                return;
            }
            else
            {
                // Re-save sample rates in case they were somehow invalid before
                // device creation.
                wxGetApp().appConfiguration.audioConfiguration.soundCard1In.sampleRate = rxInSoundDevice->getSampleRate();
                wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.sampleRate = rxOutSoundDevice->getSampleRate();
            }
        }
        else
        {
            // RX + TX setup
            // Same note as above re: number of channels.
            rxInSoundDevice = engine->getAudioDevice(wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName, IAudioEngine::AUDIO_ENGINE_IN, wxGetApp().appConfiguration.audioConfiguration.soundCard1In.sampleRate, 2);
            rxInSoundDevice->setDescription("Radio to FreeDV");
            rxInSoundDevice->setOnAudioDeviceChanged([&](IAudioDevice&, std::string newDeviceName, void*) {
                CallAfter([&, newDeviceName]() {
                    wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName = wxString::FromUTF8(newDeviceName.c_str());
                    wxGetApp().appConfiguration.save(pConfig);
                });
            }, nullptr);

            rxOutSoundDevice = engine->getAudioDevice(wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.deviceName, IAudioEngine::AUDIO_ENGINE_OUT, wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.sampleRate, 2);
            rxOutSoundDevice->setDescription("FreeDV to Speaker");
            rxOutSoundDevice->setOnAudioDeviceChanged([&](IAudioDevice&, std::string newDeviceName, void*) {
                CallAfter([&, newDeviceName]() {
                    wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.deviceName = wxString::FromUTF8(newDeviceName.c_str());
                    wxGetApp().appConfiguration.save(pConfig);
                });
            }, nullptr);

            txInSoundDevice = engine->getAudioDevice(wxGetApp().appConfiguration.audioConfiguration.soundCard2In.deviceName, IAudioEngine::AUDIO_ENGINE_IN, wxGetApp().appConfiguration.audioConfiguration.soundCard2In.sampleRate, 2);
            txInSoundDevice->setDescription("Mic to FreeDV");
            txInSoundDevice->setOnAudioDeviceChanged([&](IAudioDevice&, std::string newDeviceName, void*) {
                CallAfter([&, newDeviceName]() {
                    wxGetApp().appConfiguration.audioConfiguration.soundCard2In.deviceName = wxString::FromUTF8(newDeviceName.c_str());
                    wxGetApp().appConfiguration.save(pConfig);
                });
            }, nullptr);

            txOutSoundDevice = engine->getAudioDevice(wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName, IAudioEngine::AUDIO_ENGINE_OUT, wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.sampleRate, 2);
            txOutSoundDevice->setDescription("FreeDV to Radio");
            txOutSoundDevice->setOnAudioDeviceChanged([&](IAudioDevice&, std::string newDeviceName, void*) {
                CallAfter([&, newDeviceName]() {
                    wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName = wxString::FromUTF8(newDeviceName.c_str());
                    wxGetApp().appConfiguration.save(pConfig);
                });
            }, nullptr);
            
            bool failed = false;
            if (!rxInSoundDevice)
            {
                executeOnUiThreadAndWait_([&]() {
                    wxMessageBox(wxString::Format("Could not find RX input sound device '%s'. Please check settings and try again.", wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName.get()), wxT("Error"), wxOK);
                });
                failed = true;
            }
            
            if (!rxOutSoundDevice)
            {
                executeOnUiThreadAndWait_([&]() {
                    wxMessageBox(wxString::Format("Could not find RX output sound device '%s'. Please check settings and try again.", wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.deviceName.get()), wxT("Error"), wxOK);
                });
                failed = true;
            }
            
            if (!txInSoundDevice)
            {
                executeOnUiThreadAndWait_([&]() {
                    wxMessageBox(wxString::Format("Could not find TX input sound device '%s'. Please check settings and try again.", wxGetApp().appConfiguration.audioConfiguration.soundCard2In.deviceName.get()), wxT("Error"), wxOK);
                });
                failed = true;
            }
            
            if (!txOutSoundDevice)
            {
                executeOnUiThreadAndWait_([&]() {
                    wxMessageBox(wxString::Format("Could not find TX output sound device '%s'. Please check settings and try again.", wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName.get()), wxT("Error"), wxOK);
                });
                failed = true;
            }
            
            if (failed)
            {
                if (rxInSoundDevice)
                {
                    rxInSoundDevice.reset();
                }
                
                if (rxOutSoundDevice)
                {
                    rxOutSoundDevice.reset();
                }
                
                if (txInSoundDevice)
                {
                    txInSoundDevice.reset();
                }
                
                if (txOutSoundDevice)
                {
                    txOutSoundDevice.reset();
                }
                
                m_RxRunning = false;
            
                engine->stop();
                engine->setOnEngineError(nullptr, nullptr);
            
                return;
            }
            else
            {
                // Re-save sample rates in case they were somehow invalid before
                // device creation.
                wxGetApp().appConfiguration.audioConfiguration.soundCard1In.sampleRate = rxInSoundDevice->getSampleRate();
                wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.sampleRate = rxOutSoundDevice->getSampleRate();

                wxGetApp().appConfiguration.audioConfiguration.soundCard2In.sampleRate = txInSoundDevice->getSampleRate();
                wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.sampleRate = txOutSoundDevice->getSampleRate();
            }
        }

        // Init call back data structure ----------------------------------------------

        g_rxUserdata = new paCallBackData;
                
        // create FIFOs used to interface between IAudioEngine and txRx
        // processing loop, which iterates about once every 20ms.
        // Sample rate conversion, stats for spectral plots, and
        // transmit processng are all performed in the tx/rxProcessing
        // loop.

        int m_fifoSize_ms = wxGetApp().appConfiguration.fifoSizeMs;
        int soundCard1InFifoSizeSamples = m_fifoSize_ms*wxGetApp().appConfiguration.audioConfiguration.soundCard1In.sampleRate / 1000;
        int soundCard1OutFifoSizeSamples = m_fifoSize_ms*wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.sampleRate / 1000;

        if (txInSoundDevice && txOutSoundDevice)
        {
            int soundCard2InFifoSizeSamples = m_fifoSize_ms*wxGetApp().appConfiguration.audioConfiguration.soundCard2In.sampleRate / 1000;
            int soundCard2OutFifoSizeSamples = m_fifoSize_ms*wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.sampleRate / 1000;
            g_rxUserdata->outfifo1 = codec2_fifo_create(soundCard1OutFifoSizeSamples);
            g_rxUserdata->infifo2 = codec2_fifo_create(soundCard2InFifoSizeSamples);
            g_rxUserdata->infifo1 = codec2_fifo_create(soundCard1InFifoSizeSamples);
            g_rxUserdata->outfifo2 = codec2_fifo_create(soundCard2OutFifoSizeSamples);
        
            log_debug("fifoSize_ms:  %d infifo2: %d/outfilo2: %d",
                wxGetApp().appConfiguration.fifoSizeMs.get(), soundCard2InFifoSizeSamples, soundCard2OutFifoSizeSamples);
        }
        else
        {
            g_rxUserdata->infifo1 = codec2_fifo_create(soundCard1InFifoSizeSamples);
            g_rxUserdata->outfifo1 = codec2_fifo_create(soundCard1OutFifoSizeSamples);
            g_rxUserdata->infifo2 = nullptr;
            g_rxUserdata->outfifo2 = nullptr;
        }

        log_debug("fifoSize_ms: %d infifo1: %d/outfilo1 %d",
                wxGetApp().appConfiguration.fifoSizeMs.get(), soundCard1InFifoSizeSamples, soundCard1OutFifoSizeSamples);

        // reset debug stats for FIFOs

        g_infifo1_full = g_outfifo1_empty = g_infifo2_full = g_outfifo2_empty = 0;
        g_infifo1_full = g_outfifo1_empty = g_infifo2_full = g_outfifo2_empty = 0;
        for (int i=0; i<4; i++) {
            g_AEstatus1[i] = g_AEstatus2[i] = 0;
        }

        // These FIFOs interface between the 20ms tx/rxProcessing()
        // loop and the demodulator, which requires a variable number
        // of input samples to adjust for timing clock differences
        // between remote tx and rx.  These FIFOs also help with the
        // different processing block size of different FreeDV modes.

        // TODO: might be able to tune these on a per waveform basis, or refactor
        // to a neater design with less layers of FIFOs

        int modem_samplerate, rxInFifoSizeSamples, rxOutFifoSizeSamples;
        modem_samplerate = freedvInterface.getRxModemSampleRate();
        rxInFifoSizeSamples = freedvInterface.getRxNumModemSamples();
        rxOutFifoSizeSamples = freedvInterface.getRxNumSpeechSamples();

        // add an extra 40ms to give a bit of headroom for processing loop adding samples
        // which operates on 20ms buffers

        rxInFifoSizeSamples += 0.04*modem_samplerate;
        rxOutFifoSizeSamples += 0.04*modem_samplerate;

        g_rxUserdata->rxinfifo = codec2_fifo_create(rxInFifoSizeSamples);
        g_rxUserdata->rxoutfifo = codec2_fifo_create(rxOutFifoSizeSamples);

        log_debug("rxInFifoSizeSamples: %d rxOutFifoSizeSamples: %d", rxInFifoSizeSamples, rxOutFifoSizeSamples);

        // Init Equaliser Filters ------------------------------------------------------

        m_newMicInFilter = m_newSpkOutFilter = true;
        g_mutexProtectingCallbackData.Lock();

        g_rxUserdata->micInEQEnable = wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable;
        g_rxUserdata->spkOutEQEnable = wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable;

        if (
            wxGetApp().appConfiguration.filterConfiguration.micInChannel.eqEnable ||
            wxGetApp().appConfiguration.filterConfiguration.spkOutChannel.eqEnable)
        {
            if (g_nSoundCards == 1)
            {
                designEQFilters(
                    g_rxUserdata, 
                    wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.sampleRate, 
                    0);
            }
            else
            {
                designEQFilters(
                    g_rxUserdata, 
                    wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.sampleRate, 
                    wxGetApp().appConfiguration.audioConfiguration.soundCard2In.sampleRate);
            }
        }

        m_newMicInFilter = m_newSpkOutFilter = false;
        g_mutexProtectingCallbackData.Unlock();

        // optional tone in left channel to reliably trigger vox

        g_rxUserdata->leftChannelVoxTone = wxGetApp().appConfiguration.rigControlConfiguration.leftChannelVoxTone;
        g_rxUserdata->voxTonePhase = 0;

        // Set sound card callbacks
        auto errorCallback = [&](IAudioDevice&, std::string error, void*)
        {
            log_error("%s", error.c_str());
            CallAfter([&, error]() {
                wxMessageBox(wxString::Format("Error encountered while processing audio: %s", error), wxT("Error"), wxOK);
            });
        };

        rxInSoundDevice->setOnAudioData([&](IAudioDevice& dev, void* data, size_t size, void* state) {
            paCallBackData* cbData = static_cast<paCallBackData*>(state);
            short* audioData = static_cast<short*>(data);
            short  indata[size];

            for (size_t i = 0; i < size; i++, audioData += dev.getNumChannels())
            {
                indata[i] = audioData[0];
            }
            
            if (codec2_fifo_write(cbData->infifo1, indata, size)) 
            {
                log_warn("RX FIFO full");
                g_infifo1_full++;
            }

            m_rxThread->notify();
        }, g_rxUserdata);
        
        rxInSoundDevice->setOnAudioOverflow([](IAudioDevice& dev, void* state)
        {
            g_AEstatus1[1]++;
        }, nullptr);
        
        rxInSoundDevice->setOnAudioUnderflow([](IAudioDevice& dev, void* state)
        {
            g_AEstatus1[0]++;
        }, nullptr);
        
        rxInSoundDevice->setOnAudioError(errorCallback, nullptr);
        rxOutSoundDevice->setOnAudioError(errorCallback, nullptr);
        
        if (txInSoundDevice && txOutSoundDevice)
        {
            rxOutSoundDevice->setOnAudioData([](IAudioDevice& dev, void* data, size_t size, void* state) {
                paCallBackData* cbData = static_cast<paCallBackData*>(state);
                short* audioData = static_cast<short*>(data);
                short  outdata[size];
 
                int result = codec2_fifo_read(cbData->outfifo2, outdata, size);
                if (result == 0) 
                {
                    for (size_t i = 0; i < size; i++)
                    {
                        for (int j = 0; j < dev.getNumChannels(); j++)
                        {
                            *audioData++ = outdata[i];
                        }
                    }
                }
                else 
                {
                    g_outfifo2_empty++;
                }
            }, g_rxUserdata);
            
            rxOutSoundDevice->setOnAudioOverflow([](IAudioDevice& dev, void* state)
            {
                g_AEstatus2[3]++;
            }, nullptr);
        
            rxOutSoundDevice->setOnAudioUnderflow([](IAudioDevice& dev, void* state)
            {
                g_AEstatus2[2]++;
            }, nullptr);
            
            txInSoundDevice->setOnAudioData([&](IAudioDevice& dev, void* data, size_t size, void* state) {
                paCallBackData* cbData = static_cast<paCallBackData*>(state);
                short* audioData = static_cast<short*>(data);
                short  indata[size];
                
                if (!endingTx) 
                {
                    for(size_t i = 0; i < size; i++, audioData += dev.getNumChannels())
                    {
                        indata[i] = audioData[0];
                    }
                    
                    if (codec2_fifo_write(cbData->infifo2, indata, size)) 
                    {
                        g_infifo2_full++;
                    }
                }

                m_txThread->notify();
            }, g_rxUserdata);
        
            txInSoundDevice->setOnAudioOverflow([](IAudioDevice& dev, void* state)
            {
                g_AEstatus2[1]++;
            }, nullptr);
        
            txInSoundDevice->setOnAudioUnderflow([](IAudioDevice& dev, void* state)
            {
                g_AEstatus2[0]++;
            }, nullptr);
            
            txOutSoundDevice->setOnAudioData([](IAudioDevice& dev, void* data, size_t size, void* state) {
                paCallBackData* cbData = static_cast<paCallBackData*>(state);
                short* audioData = static_cast<short*>(data);
                short  outdata[size];
                
                unsigned int available = std::min(codec2_fifo_used(cbData->outfifo1), (int)size);

                int result = codec2_fifo_read(cbData->outfifo1, outdata, available);
                if (result == 0) 
                {
                    // write signal to all channels to start. This is so that
                    // the compiler can optimize for the most common case.
                    for(size_t i = 0; i < available; i++, audioData += dev.getNumChannels()) 
                    {
                        for (auto j = 0; j < dev.getNumChannels(); j++)
                        {
                            audioData[j] = outdata[i];
                        }
                    }
                    
                    // If VOX tone is enabled, go back through and add the VOX tone
                    // on the left channel.
                    if (cbData->leftChannelVoxTone)
                    {
                        for(size_t i = 0; i < size; i++, audioData += dev.getNumChannels())
                        {
                            cbData->voxTonePhase += 2.0*M_PI*VOX_TONE_FREQ/wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.sampleRate;
                            cbData->voxTonePhase -= 2.0*M_PI*floor(cbData->voxTonePhase/(2.0*M_PI));
                            audioData[0] = VOX_TONE_AMP*cos(cbData->voxTonePhase);
                        }
                    }
                    
                    if (size != available)
                    {
                        g_outfifo1_empty++;
                    }
                }
                else 
                {
                    g_outfifo1_empty++;
                }
            }, g_rxUserdata);
        
            txOutSoundDevice->setOnAudioOverflow([](IAudioDevice& dev, void* state)
            {
                g_AEstatus1[3]++;
            }, nullptr);
        
            txOutSoundDevice->setOnAudioUnderflow([](IAudioDevice& dev, void* state)
            {
                g_AEstatus1[2]++;
            }, nullptr);
            
            txInSoundDevice->setOnAudioError(errorCallback, nullptr);
            txOutSoundDevice->setOnAudioError(errorCallback, nullptr);
        }
        else
        {
            rxOutSoundDevice->setOnAudioData([](IAudioDevice& dev, void* data, size_t size, void* state) {
                paCallBackData* cbData = static_cast<paCallBackData*>(state);
                short* audioData = static_cast<short*>(data);
                short  outdata[size];

                int result = codec2_fifo_read(cbData->outfifo1, outdata, size);
                if (result == 0) 
                {
                    for (size_t i = 0; i < size; i++)
                    {
                        for (int j = 0; j < dev.getNumChannels(); j++)
                        {
                            *audioData++ = outdata[i];
                        }
                    }
                }
                else 
                {
                    g_outfifo1_empty++;
                }
            }, g_rxUserdata);
            
            rxOutSoundDevice->setOnAudioOverflow([](IAudioDevice& dev, void* state)
            {
                g_AEstatus1[3]++;
            }, nullptr);
        
            rxOutSoundDevice->setOnAudioUnderflow([](IAudioDevice& dev, void* state)
            {
                g_AEstatus1[2]++;
            }, nullptr);
        }
        
        // Create link to allow monitoring TX/VK audio
        wxGetApp().linkStep = std::make_shared<LinkStep>(rxOutSoundDevice->getSampleRate());
        
        // start tx/rx processing thread
        if (txInSoundDevice && txOutSoundDevice)
        {
            m_txThread = new TxRxThread(true, txInSoundDevice->getSampleRate(), txOutSoundDevice->getSampleRate(), wxGetApp().linkStep.get());
            if ( m_txThread->Create() != wxTHREAD_NO_ERROR )
            {
                wxLogError(wxT("Can't create TX thread!"));
            }
            if (wxGetApp().m_txRxThreadHighPriority) {
                m_txThread->SetPriority(WXTHREAD_MAX_PRIORITY);
            }
            
            txInSoundDevice->start();
            if (!txInSoundDevice->isRunning())
            {
                rxInSoundDevice.reset();
                rxOutSoundDevice.reset();
                txInSoundDevice.reset();
                txOutSoundDevice.reset();
                m_RxRunning = false;
                return;
            }

            txOutSoundDevice->start();
            if (!txOutSoundDevice->isRunning())
            {
                txInSoundDevice->stop();

                rxInSoundDevice.reset();
                rxOutSoundDevice.reset();
                txInSoundDevice.reset();
                txOutSoundDevice.reset();
                m_RxRunning = false;
                return;
            }
            
            if ( m_txThread->Run() != wxTHREAD_NO_ERROR )
            {
                wxLogError(wxT("Can't start TX thread!"));
            }
        }

        m_rxThread = new TxRxThread(false, rxInSoundDevice->getSampleRate(), rxOutSoundDevice->getSampleRate(), wxGetApp().linkStep.get());
        if ( m_rxThread->Create() != wxTHREAD_NO_ERROR )
        {
            wxLogError(wxT("Can't create RX thread!"));
        }

        if (wxGetApp().m_txRxThreadHighPriority) {
            m_rxThread->SetPriority(WXTHREAD_MAX_PRIORITY);
        }

        rxInSoundDevice->start();
        if (!rxInSoundDevice->isRunning())
        {
            if (txInSoundDevice) txInSoundDevice->stop();
            if (txOutSoundDevice) txOutSoundDevice->stop();
            
            rxInSoundDevice.reset();
            rxOutSoundDevice.reset();
            txInSoundDevice.reset();
            txOutSoundDevice.reset();
            m_RxRunning = false;
            return;
        }

        rxOutSoundDevice->start();
        if (!rxOutSoundDevice->isRunning())
        {
            if (txInSoundDevice) txInSoundDevice->stop();
            if (txOutSoundDevice) txOutSoundDevice->stop();
            rxInSoundDevice->stop();

            rxInSoundDevice.reset();
            rxOutSoundDevice.reset();
            txInSoundDevice.reset();
            txOutSoundDevice.reset();
            m_RxRunning = false;
            return;
        }

        if ( m_rxThread->Run() != wxTHREAD_NO_ERROR )
        {
            wxLogError(wxT("Can't start RX thread!"));
        }

        log_debug("starting tx/rx processing thread");

        // Work around an issue where the buttons stay disabled even if there
        // is an error opening one or more audio device(s).
        bool txDevicesRunning = 
            (!txInSoundDevice || txInSoundDevice->isRunning()) &&
            (!txOutSoundDevice || txOutSoundDevice->isRunning());
        bool rxDevicesRunning = 
            (rxInSoundDevice && rxInSoundDevice->isRunning()) &&
            (rxOutSoundDevice && rxOutSoundDevice->isRunning());
        m_RxRunning = txDevicesRunning && rxDevicesRunning;
    }
}

bool MainFrame::validateSoundCardSetup()
{
    bool canRun = true;
    
    // Translate device names to IDs
    auto engine = AudioEngineFactory::GetAudioEngine();
    engine->setOnEngineError([&](IAudioEngine&, std::string error, void* state) {
        CallAfter([&]() {
            wxMessageBox(wxString::Format(
                "Error encountered while initializing the audio engine: %s.", 
                error), wxT("Error"), wxOK, this); 
        });
    }, nullptr);
    engine->start();
    
    auto defaultInputDevice = engine->getDefaultAudioDevice(IAudioEngine::AUDIO_ENGINE_IN);
    auto defaultOutputDevice = engine->getDefaultAudioDevice(IAudioEngine::AUDIO_ENGINE_OUT);
    
    bool hasSoundCard1InDevice = wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName != "none";
    bool hasSoundCard1OutDevice = wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName != "none";
    bool hasSoundCard2InDevice = wxGetApp().appConfiguration.audioConfiguration.soundCard2In.deviceName != "none";
    bool hasSoundCard2OutDevice = wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.deviceName != "none";
    
    g_nSoundCards = 0;
    if (hasSoundCard1InDevice && hasSoundCard1OutDevice) {
        g_nSoundCards = 1;
        if (hasSoundCard2InDevice && hasSoundCard2OutDevice)
            g_nSoundCards = 2;
    }
    
    // For the purposes of validation, number of channels isn't necessary.
    auto soundCard1InDevice = engine->getAudioDevice(wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName, IAudioEngine::AUDIO_ENGINE_IN, wxGetApp().appConfiguration.audioConfiguration.soundCard1In.sampleRate, 1);
    auto soundCard1OutDevice = engine->getAudioDevice(wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName, IAudioEngine::AUDIO_ENGINE_OUT, wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.sampleRate, 1);
    auto soundCard2InDevice = engine->getAudioDevice(wxGetApp().appConfiguration.audioConfiguration.soundCard2In.deviceName, IAudioEngine::AUDIO_ENGINE_IN, wxGetApp().appConfiguration.audioConfiguration.soundCard2In.sampleRate, 1);
    auto soundCard2OutDevice = engine->getAudioDevice(wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.deviceName, IAudioEngine::AUDIO_ENGINE_OUT, wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.sampleRate, 1);

    wxString failedDeviceName;
    if (wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName != "none" && !soundCard1InDevice)
    {
        failedDeviceName = wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName.get();
        canRun = false;
    }
    else if (canRun && wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName != "none" && !soundCard1OutDevice)
    {
        failedDeviceName = wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName.get();
        canRun = false;
    }
    else if (canRun && wxGetApp().appConfiguration.audioConfiguration.soundCard2In.deviceName != "none" && !soundCard2InDevice)
    {
        failedDeviceName = wxGetApp().appConfiguration.audioConfiguration.soundCard2In.deviceName.get();
        canRun = false;
    }
    else if (canRun && wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.deviceName != "none" && !soundCard2OutDevice)
    {
        failedDeviceName = wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.deviceName.get();
        canRun = false;
    }
    
    if (canRun && g_nSoundCards == 0)
    {
        // Initial setup. Display Easy Setup dialog.
        CallAfter([&]() {
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

                // Relayout window so that the changes can take effect.
                m_panel->Layout();
            }
        });
        canRun = false;
    }
    else if (!canRun)
    {
        wxMessageBox(wxString::Format(
            "Your %s device cannot be found and may have been removed from your system. Please reattach this device, close this message box and retry. If this fails, go to Tools->Audio Config... to check your settings.", 
            failedDeviceName), wxT("Sound Device Not Found"), wxOK, this);
    }
    else
    {
        const int MIN_SAMPLE_RATE = 16000;
        int failedSampleRate = 0;
        
        // Validate sample rates
        if (wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName != "none" && wxGetApp().appConfiguration.audioConfiguration.soundCard1In.sampleRate < MIN_SAMPLE_RATE)
        {
            failedDeviceName = wxGetApp().appConfiguration.audioConfiguration.soundCard1In.deviceName.get();
            failedSampleRate = wxGetApp().appConfiguration.audioConfiguration.soundCard1In.sampleRate;
            canRun = false;
        }
        else if (wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName != "none" && wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.sampleRate < MIN_SAMPLE_RATE)
        {
            failedDeviceName = wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.deviceName.get();
            failedSampleRate = wxGetApp().appConfiguration.audioConfiguration.soundCard1Out.sampleRate;
            canRun = false;
        }
        else if (wxGetApp().appConfiguration.audioConfiguration.soundCard2In.deviceName != "none" && wxGetApp().appConfiguration.audioConfiguration.soundCard2In.sampleRate < MIN_SAMPLE_RATE)
        {
            failedDeviceName = wxGetApp().appConfiguration.audioConfiguration.soundCard2In.deviceName.get();
            failedSampleRate = wxGetApp().appConfiguration.audioConfiguration.soundCard2In.sampleRate;
            canRun = false;
        }
        else if (wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.deviceName != "none" && wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.sampleRate < MIN_SAMPLE_RATE)
        {
            failedDeviceName = wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.deviceName.get();
            failedSampleRate = wxGetApp().appConfiguration.audioConfiguration.soundCard2Out.sampleRate;
            canRun = false;
        }
        
        if (!canRun)
        {
            wxMessageBox(wxString::Format(
                "Your %s device is set to use a sample rate of %d, which is less than the minimum of %d. Please go to Tools->Audio Config... to check your settings.", 
                failedDeviceName, failedSampleRate, MIN_SAMPLE_RATE), wxT("Sample Rate Too Low"), wxOK, this);
        }
    }
    
    engine->stop();
    engine->setOnEngineError(nullptr, nullptr);
    
    return canRun;
}

void MainFrame::initializeFreeDVReporter_()
{
    bool receiveOnly = isReceiveOnly();
    
    auto oldReporterObject = wxGetApp().m_sharedReporterObject;
    wxGetApp().m_sharedReporterObject =
        std::make_shared<FreeDVReporter>(
            wxGetApp().appConfiguration.reportingConfiguration.freedvReporterHostname->ToStdString(),
            wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign->ToStdString(), 
            wxGetApp().appConfiguration.reportingConfiguration.reportingGridSquare->ToStdString(),
            std::string("FreeDV ") + FREEDV_VERSION,
            receiveOnly);
    assert(wxGetApp().m_sharedReporterObject);
    
    // If we're running, remove any existing reporter object.
    if (oldReporterObject != nullptr)
    {
        for (auto& ptr : wxGetApp().m_reporters)
        {
            if (ptr == oldReporterObject)
            {
                oldReporterObject = nullptr;
                ptr = wxGetApp().m_sharedReporterObject;
                break;
            }
        }
    }
    
    // Make built in FreeDV Reporter client available.
    if (m_reporterDialog == nullptr)
    {
        m_reporterDialog = new FreeDVReporterDialog(this);
    }
        
    m_reporterDialog->setReporter(wxGetApp().m_sharedReporterObject);
    m_reporterDialog->refreshLayout();
    
    // Set up QSY request handler
    wxGetApp().m_sharedReporterObject->setOnQSYRequestFn([&](std::string callsign, uint64_t freqHz, std::string message) {
        double freqFactor = 1000.0;
        std::string fmtMsg = "%s has requested that you QSY to %.01f kHz.";
        
        if (!wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
        {
            freqFactor *= 1000.0;
            fmtMsg = "%s has requested that you QSY to %.04f MHz.";
        }
        
        double frequencyReadable = freqHz / freqFactor;
        wxString fullMessage = wxString::Format(wxString(fmtMsg), callsign, frequencyReadable);
        int dialogStyle = wxOK | wxICON_INFORMATION | wxCENTRE;
        
        if (wxGetApp().rigFrequencyController != nullptr && 
            (wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqModeChanges || wxGetApp().appConfiguration.rigControlConfiguration.hamlibEnableFreqChangesOnly))
        {
            fullMessage = wxString::Format(_("%s Would you like to change to that frequency now?"), fullMessage);
            dialogStyle = wxYES_NO | wxICON_QUESTION | wxCENTRE;
        }
        
        CallAfter([&, fullMessage, dialogStyle, frequencyReadable]() {
            wxMessageDialog messageDialog(this, fullMessage, wxT("FreeDV Reporter"), dialogStyle);

            if (dialogStyle & wxYES_NO)
            {
                messageDialog.SetYesNoLabels(_("Change Frequency"), _("Cancel"));
            }

            auto answer = messageDialog.ShowModal();
            if (answer == wxID_YES)
            {
                // This will implicitly cause Hamlib to change the frequency and mode.
                if (wxGetApp().appConfiguration.reportingConfiguration.reportingFrequencyAsKhz)
                {
                    m_cboReportFrequency->SetValue(wxString::Format("%.1f", frequencyReadable));
                }
                else
                {
                    m_cboReportFrequency->SetValue(wxString::Format("%.4f", frequencyReadable));
                }
            }
        });
    });
    
    wxGetApp().m_sharedReporterObject->connect();
    if (!freedvInterface.isRunning())
    {
        wxGetApp().m_sharedReporterObject->hideFromView();
    }
    else
    {
        wxGetApp().m_sharedReporterObject->transmit(freedvInterface.getCurrentTxModeStr(), g_tx);
        wxGetApp().m_sharedReporterObject->freqChange(wxGetApp().appConfiguration.reportingConfiguration.reportingFrequency);
    }
}
