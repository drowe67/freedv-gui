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

#include <time.h>
#include <deque>
#include "main.h"
#include "osx_interface.h"
#include "callsign_encoder.h"
#include "freedv_interface.h"

#define wxUSE_FILEDLG   1
#define wxUSE_LIBPNG    1
#define wxUSE_LIBJPEG   1
#define wxUSE_GIF       1
#define wxUSE_PCX       1
#define wxUSE_LIBTIFF   1

extern "C" {
    extern void golay23_init(void);
}

//-------------------------------------------------------------------
// Bunch of globals used for communication with sound card call
// back functions
// ------------------------------------------------------------------

// freedv states
int                 g_verbose;
int                 g_Nc;
int                 g_mode;

FreeDVInterface     freedvInterface;
struct MODEM_STATS  g_stats;
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
int   g_split;
int   g_tx;
float g_snr;
bool  g_half_duplex;
bool  g_modal;
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
int                 g_soundCard1InDeviceNum;
int                 g_soundCard1OutDeviceNum;
int                 g_soundCard1SampleRate;
int                 g_soundCard2InDeviceNum;
int                 g_soundCard2OutDeviceNum;
int                 g_soundCard2SampleRate;

// PortAudio over/underflow counters

int                 g_infifo1_full;
int                 g_outfifo1_empty;
int                 g_infifo2_full;
int                 g_outfifo2_empty;
int                 g_PAstatus1[4];
int                 g_PAstatus2[4];
int                 g_PAframesPerBuffer1;
int                 g_PAframesPerBuffer2;

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
extern bool                g_loopPlayFileFromRadio;
extern int                 g_playFileFromRadioEventId;
extern float               g_blink;

extern SNDFILE            *g_sfRecFileFromModulator;
extern bool                g_recFileFromModulator;
extern int                 g_recFromModulatorSamples;
extern int                 g_recFileFromModulatorEventId;

wxWindow           *g_parent;

// Click to tune rx and tx frequency offset states
float               g_RxFreqOffsetHz;
float               g_TxFreqOffsetHz;

// buffer sizes dependent upon sample rate
int                 g_modemInbufferSize;
int                 g_speechOutbufferSize;

// experimental mutex to make sound card callbacks mutually exclusive
// TODO: review code and see if we need this any more, as fifos should
// now be thread safe

wxMutex g_mutexProtectingCallbackData;
 
// Speex pre-processor states
SpeexPreprocessState *g_speex_st;

// TX mode change mutex
wxMutex txModeChangeMutex;

// End of TX state control
bool endingTx;

// Option test file to log samples

FILE *ftest;
FILE *g_logfile;

// UDP socket available to send messages

extern wxDatagramSocket *g_sock;

// WxWidgets - initialize the application

IMPLEMENT_APP(MainApp);

//-------------------------------------------------------------------------
// OnInit()
//-------------------------------------------------------------------------
bool MainApp::OnInit()
{
    m_pskReporter = NULL;
    m_callsignEncoder = NULL;
    
    if(!wxApp::OnInit())
    {
        return false;
    }
    SetVendorName(wxT("CODEC2-Project"));
    SetAppName(wxT("FreeDV"));      // not needed, it's the default value

#ifdef FILE_RATHER_THAN_REGISTRY
    // Force use of file-based configuration persistance on Windows platforma
    wxConfig *pConfig = new wxConfig();
    wxFileConfig *pFConfig = new wxFileConfig(wxT("FreeDV"), wxT("CODEC2-Project"), wxT("FreeDV.conf"), wxT("FreeDV.conf"),  wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_RELATIVE_PATH);
    pConfig->Set(pFConfig);
    pConfig->SetRecordDefaults();
#else
    wxConfigBase *pConfig = wxConfigBase::Get();
    pConfig->SetRecordDefaults();
#endif

    golay23_init();
    
    m_rTopWindow = wxRect(0, 0, 0, 0);
    m_strRxInAudio.Empty();
    m_strRxOutAudio.Empty();
    m_textVoiceInput.Empty();
    m_textVoiceOutput.Empty();
    m_strSampleRate.Empty();
    m_strBitrate.Empty();

     // Create the main application window

    frame = new MainFrame(NULL);
    SetTopWindow(frame);

    // Should guarantee that the first plot tab defined is the one
    // displayed. But it doesn't when built from command line.  Why?

    frame->m_auiNbookCtrl->ChangeSelection(0);
    frame->Layout();
    frame->Show();
    g_parent =frame;

    return true;
}

//-------------------------------------------------------------------------
// OnExit()
//-------------------------------------------------------------------------
int MainApp::OnExit()
{
    return 0;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class MainFrame(wxFrame* pa->ent) : TopFrame(parent)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
MainFrame::MainFrame(wxWindow *parent) : TopFrame(parent)
{
    m_zoom              = 1.;

    #ifdef __WXMSW__
    g_logfile = stderr;
    #else
    g_logfile = stderr;
    #endif

    // Init Hamlib library, but we dont start talking to any rigs yet

    wxGetApp().m_hamlib = new Hamlib();

    // Init Serialport library, but as for Hamlib we dont start talking to any rigs yet

    wxGetApp().m_serialport = new Serialport();

    // Check for AVX support in the processor.  If it's not present, 2020 won't be processed
    // fast enough
    checkAvxSupport();
    if(!isAvxPresent)
        m_rb2020->Disable();

    tools->AppendSeparator();
    wxMenuItem* m_menuItemToolsConfigDelete;
    m_menuItemToolsConfigDelete = new wxMenuItem(tools, wxID_ANY, wxString(_("&Restore defaults")) , wxT("Delete config file/keys and restore defaults"), wxITEM_NORMAL);
    this->Connect(m_menuItemToolsConfigDelete->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MainFrame::OnDeleteConfig));

    tools->Append(m_menuItemToolsConfigDelete);

    wxConfigBase *pConfig = wxConfigBase::Get();

    // restore frame position and size
    int x = pConfig->Read(wxT("/MainFrame/left"),       20);
    int y = pConfig->Read(wxT("/MainFrame/top"),        20);
    int w = pConfig->Read(wxT("/MainFrame/width"),     800);
    int h = pConfig->Read(wxT("/MainFrame/height"),    695);

    // sanitise frame position as a first pass at Win32 registry bug

    //fprintf(g_logfile, "x = %d y = %d w = %d h = %d\n", x,y,w,h);
    if (x < 0 || x > 2048) x = 20;
    if (y < 0 || y > 2048) y = 20;
    if (w < 0 || w > 2048) w = 800;
    if (h < 0 || h > 2048) h = 695;

    wxGetApp().m_show_wf            = pConfig->Read(wxT("/MainFrame/show_wf"),           1);
    wxGetApp().m_show_spect         = pConfig->Read(wxT("/MainFrame/show_spect"),        1);
    wxGetApp().m_show_scatter       = pConfig->Read(wxT("/MainFrame/show_scatter"),      1);
    wxGetApp().m_show_timing        = pConfig->Read(wxT("/MainFrame/show_timing"),       1);
    wxGetApp().m_show_freq          = pConfig->Read(wxT("/MainFrame/show_freq"),         1);
    wxGetApp().m_show_speech_in     = pConfig->Read(wxT("/MainFrame/show_speech_in"),    1);
    wxGetApp().m_show_speech_out    = pConfig->Read(wxT("/MainFrame/show_speech_out"),   1);
    wxGetApp().m_show_demod_in      = pConfig->Read(wxT("/MainFrame/show_demod_in"),     1);
    wxGetApp().m_show_test_frame_errors = pConfig->Read(wxT("/MainFrame/show_test_frame_errors"),     1);
    wxGetApp().m_show_test_frame_errors_hist = pConfig->Read(wxT("/MainFrame/show_test_frame_errors_hist"),     1);

    wxGetApp().m_rxNbookCtrl        = pConfig->Read(wxT("/MainFrame/rxNbookCtrl"),    (long)0);

    g_SquelchActive = pConfig->Read(wxT("/Audio/SquelchActive"), (long)0);
    g_SquelchLevel = pConfig->Read(wxT("/Audio/SquelchLevel"), (int)(SQ_DEFAULT_SNR*2));
    g_SquelchLevel /= 2.0;

    Move(x, y);
    SetClientSize(w, h);

    if(wxGetApp().m_show_wf)
    {
        // Add Waterfall Plot window
        m_panelWaterfall = new PlotWaterfall((wxFrame*) m_auiNbookCtrl, false, 0);
        m_panelWaterfall->SetToolTip(_("Left click to tune"));
        m_auiNbookCtrl->AddPage(m_panelWaterfall, _("Waterfall"), true, wxNullBitmap);
    }
    if(wxGetApp().m_show_spect)
    {
        // Add Spectrum Plot window
        m_panelSpectrum = new PlotSpectrum((wxFrame*) m_auiNbookCtrl, g_avmag,
                                           MODEM_STATS_NSPEC*((float)MAX_F_HZ/MODEM_STATS_MAX_F_HZ));
        m_panelSpectrum->SetToolTip(_("Left click to tune"));
        m_auiNbookCtrl->AddPage(m_panelSpectrum, _("Spectrum"), true, wxNullBitmap);
    }
    if(wxGetApp().m_show_scatter)
    {
        // Add Scatter Plot window
        m_panelScatter = new PlotScatter((wxFrame*) m_auiNbookCtrl);
        m_auiNbookCtrl->AddPage(m_panelScatter, _("Scatter"), true, wxNullBitmap);
    }
    if(wxGetApp().m_show_demod_in)
    {
        // Add Demod Input window
        m_panelDemodIn = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, WAVEFORM_PLOT_TIME, 1.0/WAVEFORM_PLOT_FS, -1, 1, 1, 0.2, "%2.1f", 0);
        m_auiNbookCtrl->AddPage(m_panelDemodIn, _("Frm Radio"), true, wxNullBitmap);
        g_plotDemodInFifo = codec2_fifo_create(2*WAVEFORM_PLOT_BUF);
    }

    if(wxGetApp().m_show_speech_in)
    {
        // Add Speech Input window
        m_panelSpeechIn = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, WAVEFORM_PLOT_TIME, 1.0/WAVEFORM_PLOT_FS, -1, 1, 1, 0.2, "%2.1f", 0);
        m_auiNbookCtrl->AddPage(m_panelSpeechIn, _("Frm Mic"), true, wxNullBitmap);
        g_plotSpeechInFifo = codec2_fifo_create(4*WAVEFORM_PLOT_BUF);
    }

    if(wxGetApp().m_show_speech_out)
    {
        // Add Speech Output window
        m_panelSpeechOut = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, WAVEFORM_PLOT_TIME, 1.0/WAVEFORM_PLOT_FS, -1, 1, 1, 0.2, "%2.1f", 0);
        m_auiNbookCtrl->AddPage(m_panelSpeechOut, _("To Spkr/Hdphns"), true, wxNullBitmap);
        g_plotSpeechOutFifo = codec2_fifo_create(2*WAVEFORM_PLOT_BUF);
    }

    if(wxGetApp().m_show_timing)
    {
        // Add Timing Offset window
        m_panelTimeOffset = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, 5.0, DT, -0.5, 0.5, 1, 0.1, "%2.1f", 0);
        m_auiNbookCtrl->AddPage(m_panelTimeOffset, L"Timing \u0394", true, wxNullBitmap);
    }
    if(wxGetApp().m_show_freq)
    {
        // Add Frequency Offset window
        m_panelFreqOffset = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, 5.0, DT, -200, 200, 1, 50, "%3.0fHz", 0);
        m_auiNbookCtrl->AddPage(m_panelFreqOffset, L"Frequency \u0394", true, wxNullBitmap);
    }

    if(wxGetApp().m_show_test_frame_errors)
    {
        // Add Test Frame Errors window
        m_panelTestFrameErrors = new PlotScalar((wxFrame*) m_auiNbookCtrl, 2*MODEM_STATS_NC_MAX, 30.0, DT, 0, 2*MODEM_STATS_NC_MAX+2, 1, 1, "", 1);
        m_auiNbookCtrl->AddPage(m_panelTestFrameErrors, L"Test Frame Errors", true, wxNullBitmap);
    }

    if(wxGetApp().m_show_test_frame_errors_hist)
    {
        // Add Test Frame Historgram window.  1 column for every bit, 2 bits per carrier
        m_panelTestFrameErrorsHist = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, 1.0, 1.0/(2*MODEM_STATS_NC_MAX), 0.001, 0.1, 1.0/MODEM_STATS_NC_MAX, 0.1, "%0.0E", 0);
        m_auiNbookCtrl->AddPage(m_panelTestFrameErrorsHist, L"Test Frame Histogram", true, wxNullBitmap);
        m_panelTestFrameErrorsHist->setBarGraph(1);
        m_panelTestFrameErrorsHist->setLogY(1);
    }

    wxGetApp().m_framesPerBuffer = pConfig->Read(wxT("/Audio/framesPerBuffer"), (int)PA_FPB);
    wxGetApp().m_fifoSize_ms = pConfig->Read(wxT("/Audio/fifoSize_ms"), (int)FIFO_SIZE);

    wxGetApp().m_soundCard1InDeviceName = pConfig->Read(wxT("/Audio/soundCard1InDeviceName"), _("none"));
    wxGetApp().m_soundCard1OutDeviceName = pConfig->Read(wxT("/Audio/soundCard1OutDeviceName"), _("none"));
    wxGetApp().m_soundCard2InDeviceName = pConfig->Read(wxT("/Audio/soundCard2InDeviceName"), _("none"));	
    wxGetApp().m_soundCard2OutDeviceName = pConfig->Read(wxT("/Audio/soundCard2OutDeviceName"), _("none"));	

    g_txLevel = pConfig->Read(wxT("/Audio/transmitLevel"), (int)0);
    char fmt[15];
    m_sliderTxLevel->SetValue(g_txLevel);
    sprintf(fmt, "%0.1f dB", (double)g_txLevel / 10.0);
    wxString fmtString(fmt);
    m_txtTxLevelNum->SetLabel(fmtString);
    
    // Get sound card sample rates
    g_soundCard1SampleRate   = pConfig->Read(wxT("/Audio/soundCard1SampleRate"),          -1);
    g_soundCard2SampleRate   = pConfig->Read(wxT("/Audio/soundCard2SampleRate"),          -1);

    validateSoundCardSetup();

    wxGetApp().m_playFileToMicInPath = pConfig->Read("/File/playFileToMicInPath",   wxT(""));
    wxGetApp().m_recFileFromRadioPath = pConfig->Read("/File/recFileFromRadioPath", wxT(""));
    wxGetApp().m_recFileFromRadioSecs = pConfig->Read("/File/recFileFromRadioSecs", 60);
    wxGetApp().m_recFileFromModulatorPath = pConfig->Read("/File/recFileFromModulatorPath", wxT(""));
    wxGetApp().m_recFileFromModulatorSecs = pConfig->Read("/File/recFileFromModulatorSecs", 10);
    wxGetApp().m_playFileFromRadioPath = pConfig->Read("/File/playFileFromRadioPath", wxT(""));

    // PTT -------------------------------------------------------------------

    wxGetApp().m_boolHalfDuplex     = pConfig->ReadBool(wxT("/Rig/HalfDuplex"),     true);
    wxGetApp().m_boolMultipleRx     = pConfig->ReadBool(wxT("/Rig/MultipleRx"),     false);
    wxGetApp().m_leftChannelVoxTone = pConfig->ReadBool("/Rig/leftChannelVoxTone",  false);

    wxGetApp().m_txtVoiceKeyerWaveFilePath = pConfig->Read(wxT("/VoiceKeyer/WaveFilePath"), wxT(""));
    wxGetApp().m_txtVoiceKeyerWaveFile = pConfig->Read(wxT("/VoiceKeyer/WaveFile"), wxT("voicekeyer.wav"));
    wxGetApp().m_intVoiceKeyerRxPause = pConfig->Read(wxT("/VoiceKeyer/RxPause"), 10);
    wxGetApp().m_intVoiceKeyerRepeats = pConfig->Read(wxT("/VoiceKeyer/Repeats"), 5);

    wxGetApp().m_boolHamlibUseForPTT = pConfig->ReadBool("/Hamlib/UseForPTT", false);
    wxGetApp().m_intHamlibIcomCIVHex = pConfig->ReadLong("/Hamlib/IcomCIVHex", 0);
    wxGetApp().m_intHamlibRig = pConfig->ReadLong("/Hamlib/RigName", 0);
    wxGetApp().m_strHamlibSerialPort = pConfig->Read("/Hamlib/SerialPort", "");
    wxGetApp().m_intHamlibSerialRate = pConfig->ReadLong("/Hamlib/SerialRate", 0);

    wxGetApp().m_boolUseSerialPTT   = pConfig->ReadBool(wxT("/Rig/UseSerialPTT"),   false);
    wxGetApp().m_strRigCtrlPort     = pConfig->Read(wxT("/Rig/Port"),               wxT(""));
    wxGetApp().m_boolUseRTS         = pConfig->ReadBool(wxT("/Rig/UseRTS"),         true);
    wxGetApp().m_boolRTSPos         = pConfig->ReadBool(wxT("/Rig/RTSPolarity"),    true);
    wxGetApp().m_boolUseDTR         = pConfig->ReadBool(wxT("/Rig/UseDTR"),         false);
    wxGetApp().m_boolDTRPos         = pConfig->ReadBool(wxT("/Rig/DTRPolarity"),    false);

    assert(wxGetApp().m_serialport != NULL);

    // -----------------------------------------------------------------------

    bool slow = false; // prevents compile error when using default bool
    wxGetApp().m_snrSlow = pConfig->Read("/Audio/snrSlow", slow);

    bool t = true;     // prevents compile error when using default bool
    wxGetApp().m_codec2LPCPostFilterEnable     = pConfig->Read(wxT("/Filter/codec2LPCPostFilterEnable"),    t);
    wxGetApp().m_codec2LPCPostFilterBassBoost  = pConfig->Read(wxT("/Filter/codec2LPCPostFilterBassBoost"), t);
    wxGetApp().m_codec2LPCPostFilterGamma      = (float)pConfig->Read(wxT("/Filter/codec2LPCPostFilterGamma"),     CODEC2_LPC_PF_GAMMA*100)/100.0;
    wxGetApp().m_codec2LPCPostFilterBeta       = (float)pConfig->Read(wxT("/Filter/codec2LPCPostFilterBeta"),      CODEC2_LPC_PF_BETA*100)/100.0;
    //printf("main(): m_codec2LPCPostFilterBeta: %f\n", wxGetApp().m_codec2LPCPostFilterBeta);

    wxGetApp().m_speexpp_enable     = pConfig->Read(wxT("/Filter/speexpp_enable"),    t);
    wxGetApp().m_700C_EQ     = pConfig->Read(wxT("/Filter/700C_EQ"),    t);

    wxGetApp().m_MicInBassFreqHz = (float)pConfig->Read(wxT("/Filter/MicInBassFreqHz"),    1);
    wxGetApp().m_MicInBassGaindB = (float)pConfig->Read(wxT("/Filter/MicInBassGaindB"),    (long)0)/10.0;
    wxGetApp().m_MicInTrebleFreqHz = (float)pConfig->Read(wxT("/Filter/MicInTrebleFreqHz"),    1);
    wxGetApp().m_MicInTrebleGaindB = (float)pConfig->Read(wxT("/Filter/MicInTrebleGaindB"),    (long)0)/10.0;
    wxGetApp().m_MicInMidFreqHz = (float)pConfig->Read(wxT("/Filter/MicInMidFreqHz"),    1);
    wxGetApp().m_MicInMidGaindB = (float)pConfig->Read(wxT("/Filter/MicInMidGaindB"),    (long)0)/10.0;
    wxGetApp().m_MicInMidQ = (float)pConfig->Read(wxT("/Filter/MicInMidQ"),              (long)100)/100.0;

    bool f = false;
    wxGetApp().m_MicInEQEnable = (float)pConfig->Read(wxT("/Filter/MicInEQEnable"), f);

    wxGetApp().m_SpkOutBassFreqHz = (float)pConfig->Read(wxT("/Filter/SpkOutBassFreqHz"),    1);
    wxGetApp().m_SpkOutBassGaindB = (float)pConfig->Read(wxT("/Filter/SpkOutBassGaindB"),    (long)0)/10.0;
    wxGetApp().m_SpkOutTrebleFreqHz = (float)pConfig->Read(wxT("/Filter/SpkOutTrebleFreqHz"),    1);
    wxGetApp().m_SpkOutTrebleGaindB = (float)pConfig->Read(wxT("/Filter/SpkOutTrebleGaindB"),    (long)0)/10.0;
    wxGetApp().m_SpkOutMidFreqHz = (float)pConfig->Read(wxT("/Filter/SpkOutMidFreqHz"),    1);
    wxGetApp().m_SpkOutMidGaindB = (float)pConfig->Read(wxT("/Filter/SpkOutMidGaindB"),    (long)0)/10.0;
    wxGetApp().m_SpkOutMidQ = (float)pConfig->Read(wxT("/Filter/SpkOutMidQ"),                (long)100)/100.0;

    wxGetApp().m_SpkOutEQEnable = (float)pConfig->Read(wxT("/Filter/SpkOutEQEnable"), f);

    wxGetApp().m_callSign = pConfig->Read("/Data/CallSign", wxT(""));
    wxGetApp().m_textEncoding = pConfig->Read("/Data/TextEncoding", 1);

    wxGetApp().m_events = pConfig->Read("/Events/enable", f);
    wxGetApp().m_events_spam_timer = (int)pConfig->Read(wxT("/Events/spam_timer"), 10);
    wxGetApp().m_events_regexp_match = pConfig->Read("/Events/regexp_match", wxT("s=(.*)"));
    wxGetApp().m_events_regexp_replace = pConfig->Read("/Events/regexp_replace",
                                                       wxT("curl http://qso.freedv.org/cgi-bin/onspot.cgi?s=\\1"));
    // make sure regexp lists are terminated by a \n

    if (wxGetApp().m_events_regexp_match.Last() != '\n') {
        wxGetApp().m_events_regexp_match = wxGetApp().m_events_regexp_match+'\n';
    }
    if (wxGetApp().m_events_regexp_replace.Last() != '\n') {
        wxGetApp().m_events_regexp_replace = wxGetApp().m_events_regexp_replace+'\n';
    }

    wxGetApp().m_udp_enable = (float)pConfig->Read(wxT("/UDP/enable"), f);
    wxGetApp().m_udp_port = (int)pConfig->Read(wxT("/UDP/port"), 3000);

    wxGetApp().m_FreeDV700txClip = (float)pConfig->Read(wxT("/FreeDV700/txClip"), t);
    wxGetApp().m_FreeDV700txBPF = (float)pConfig->Read(wxT("/FreeDV700/txBPF"), t);
    wxGetApp().m_FreeDV700Combine = 1;
    wxGetApp().m_FreeDV700ManualUnSync = (float)pConfig->Read(wxT("/FreeDV700/manualUnSync"), f);

    wxGetApp().m_PhaseEstBW = (float)pConfig->Read(wxT("/OFDM/PhaseEstBW"), f);
    wxGetApp().m_PhaseEstDPSK = (float)pConfig->Read(wxT("/OFDM/PhaseEstDPSK"), f);

    wxGetApp().m_noise_snr = (float)pConfig->Read(wxT("/Noise/noise_snr"), 2);

    wxGetApp().m_debug_console = (float)pConfig->Read(wxT("/Debug/console"), f);
    g_verbose = pConfig->Read(wxT("/Debug/verbose"), (long)0);
    g_freedv_verbose = pConfig->Read(wxT("/Debug/APIverbose"), (long)0);

    wxGetApp().m_attn_carrier_en = 0;
    wxGetApp().m_attn_carrier    = 0;

    wxGetApp().m_tone = 0;
    wxGetApp().m_tone_freq_hz = 1000;
    wxGetApp().m_tone_amplitude = 500;
    
    wxGetApp().m_psk_enable = pConfig->ReadBool(wxT("/PSKReporter/Enable"), false);
    wxGetApp().m_psk_callsign = pConfig->Read(wxT("/PSKReporter/Callsign"), wxT(""));
    wxGetApp().m_psk_grid_square = pConfig->Read(wxT("/PSKReporter/GridSquare"), wxT(""));
    
    // Waterfall configuration
    wxGetApp().m_waterfallColor = (int)pConfig->Read(wxT("/Waterfall/Color"), (int)0); // 0-2
    
    int mode  = pConfig->Read(wxT("/Audio/mode"), (long)0);
    if (mode == 0)
        m_rb1600->SetValue(1);
    if (mode == 3)
        m_rb700c->SetValue(1);
    if (mode == 4)
        m_rb700d->SetValue(1);
    if (mode == 5)
        m_rb700e->SetValue(1);
    if (mode == 6)
        m_rb800xa->SetValue(1);
    if (mode == 7)
        m_rb2400b->SetValue(1);
    if ((mode == 9) && isAvxPresent)
        m_rb2020->SetValue(1);
    pConfig->SetPath(wxT("/"));

//    this->Connect(m_menuItemHelpUpdates->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnHelpCheckUpdatesUI));
     m_togBtnOnOff->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnOnOffUI), NULL, this);
    m_togBtnSplit->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnSplitClickUI), NULL, this);
    m_togBtnAnalog->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnAnalogClickUI), NULL, this);
   // m_btnTogPTT->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnPTT_UI), NULL, this);

    m_togBtnSplit->Disable();
    m_togBtnAnalog->Disable();
    m_btnTogPTT->Disable();
    m_togBtnVoiceKeyer->Disable();

    // squelch settings
    char sqsnr[15];
    m_sliderSQ->SetValue((int)((g_SquelchLevel+5.0)*2.0));
    sprintf(sqsnr, "%4.1f", g_SquelchLevel);
    wxString sqsnr_string(sqsnr);
    m_textSQ->SetLabel(sqsnr_string);
    m_ckboxSQ->SetValue(g_SquelchActive);

    // SNR settings

    m_ckboxSNR->SetValue(wxGetApp().m_snrSlow);
    setsnrBeta(wxGetApp().m_snrSlow);

#ifdef _USE_TIMER
    Bind(wxEVT_TIMER, &MainFrame::OnTimer, this);       // ID_MY_WINDOW);
    m_plotTimer.SetOwner(this, ID_TIMER_WATERFALL);
    m_pskReporterTimer.SetOwner(this, ID_TIMER_PSKREPORTER);
    //m_panelWaterfall->Refresh();
#endif

    m_RxRunning = false;

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

    // init click-tune states

    g_RxFreqOffsetHz = 0.0;
    m_panelWaterfall->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);
    m_panelSpectrum->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);

    g_TxFreqOffsetHz = 0.0;

    g_tx = 0;
    g_split = 0;

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

    g_modal = false;

    optionsDlg = new OptionsDlg(NULL);
    m_schedule_restore = false;

    vk_state = VK_IDLE;

    // Init optional Windows debug console so we can see all those printfs

#ifdef __WXMSW__
    if (wxGetApp().m_debug_console) {
        // somewhere to send printfs while developing
        int ret = AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        fprintf(stderr, "AllocConsole: %d m_debug_console: %d\n", ret, wxGetApp().m_debug_console);
    }
#endif

    //#define FTEST
    #ifdef FTEST
    ftest = fopen("ftest.raw", "wb");
    assert(ftest != NULL);
    #endif

    /* experimental checkbox control of thread priority, used
       to helpo debug 700D windows sound break up */

    wxGetApp().m_txRxThreadHighPriority = true;
    g_dump_timing = g_dump_fifo_state = 0;

    UDPInit();
}

//-------------------------------------------------------------------------
// ~MainFrame()
//-------------------------------------------------------------------------
MainFrame::~MainFrame()
{
    int x;
    int y;
    int w;
    int h;

    //fprintf(stderr, "MainFrame::~MainFrame()\n");
    #ifdef FTEST
    fclose(ftest);
    #endif

    #ifdef __WXMSW__
    fclose(g_logfile);
    #endif

    if (wxGetApp().m_serialport)
    {
        delete wxGetApp().m_serialport;
    }

    wxConfigBase *pConfig = wxConfigBase::Get();
    if(pConfig)
    {
        if (!IsIconized()) {
            GetClientSize(&w, &h);
            GetPosition(&x, &y);
            //fprintf(stderr, "x = %d y = %d w = %d h = %d\n", x,y,w,h);
            pConfig->Write(wxT("/MainFrame/left"),               (long) x);
            pConfig->Write(wxT("/MainFrame/top"),                (long) y);
            pConfig->Write(wxT("/MainFrame/width"),              (long) w);
            pConfig->Write(wxT("/MainFrame/height"),             (long) h);
        }
        pConfig->Write(wxT("/MainFrame/show_wf"),           wxGetApp().m_show_wf);
        pConfig->Write(wxT("/MainFrame/show_spect"),        wxGetApp().m_show_spect);
        pConfig->Write(wxT("/MainFrame/show_scatter"),      wxGetApp().m_show_scatter);
        pConfig->Write(wxT("/MainFrame/show_timing"),       wxGetApp().m_show_timing);
        pConfig->Write(wxT("/MainFrame/show_freq"),         wxGetApp().m_show_freq);
        pConfig->Write(wxT("/MainFrame/show_speech_in"),    wxGetApp().m_show_speech_in);
        pConfig->Write(wxT("/MainFrame/show_speech_out"),   wxGetApp().m_show_speech_out);
        pConfig->Write(wxT("/MainFrame/show_demod_in"),     wxGetApp().m_show_demod_in);
        pConfig->Write(wxT("/MainFrame/show_test_frame_errors"), wxGetApp().m_show_test_frame_errors);
        pConfig->Write(wxT("/MainFrame/show_test_frame_errors_hist"), wxGetApp().m_show_test_frame_errors_hist);

        pConfig->Write(wxT("/MainFrame/rxNbookCtrl"), wxGetApp().m_rxNbookCtrl);

        pConfig->Write(wxT("/Audio/SquelchActive"),         g_SquelchActive);
        pConfig->Write(wxT("/Audio/SquelchLevel"),          (int)(g_SquelchLevel*2.0));

        pConfig->Write(wxT("/Audio/framesPerBuffer"),       wxGetApp().m_framesPerBuffer);
        pConfig->Write(wxT("/Audio/fifoSize_ms"),              wxGetApp().m_fifoSize_ms);

        pConfig->Write(wxT("/Audio/soundCard1InDeviceName"), wxGetApp().m_soundCard1InDeviceName);
        pConfig->Write(wxT("/Audio/soundCard1OutDeviceName"), wxGetApp().m_soundCard1OutDeviceName);
        pConfig->Write(wxT("/Audio/soundCard2InDeviceName"), wxGetApp().m_soundCard2InDeviceName);	
        pConfig->Write(wxT("/Audio/soundCard2OutDeviceName"), wxGetApp().m_soundCard2OutDeviceName);	

        pConfig->Write(wxT("/Audio/soundCard1SampleRate"),    g_soundCard1SampleRate );
        pConfig->Write(wxT("/Audio/soundCard2SampleRate"),    g_soundCard2SampleRate );

        pConfig->Write(wxT("/Audio/transmitLevel"), g_txLevel);
        
        pConfig->Write(wxT("/VoiceKeyer/WaveFilePath"), wxGetApp().m_txtVoiceKeyerWaveFilePath);
        pConfig->Write(wxT("/VoiceKeyer/WaveFile"), wxGetApp().m_txtVoiceKeyerWaveFile);
        pConfig->Write(wxT("/VoiceKeyer/RxPause"), wxGetApp().m_intVoiceKeyerRxPause);
        pConfig->Write(wxT("/VoiceKeyer/Repeats"), wxGetApp().m_intVoiceKeyerRepeats);

        pConfig->Write(wxT("/Rig/HalfDuplex"),              wxGetApp().m_boolHalfDuplex);
        pConfig->Write(wxT("/Rig/MultipleRx"), wxGetApp().m_boolMultipleRx);
        pConfig->Write(wxT("/Rig/leftChannelVoxTone"),      wxGetApp().m_leftChannelVoxTone);
        pConfig->Write("/Hamlib/UseForPTT", wxGetApp().m_boolHamlibUseForPTT);
        pConfig->Write("/Hamlib/RigName", wxGetApp().m_intHamlibRig);
        pConfig->Write("/Hamlib/SerialPort", wxGetApp().m_strHamlibSerialPort);
        pConfig->Write("/Hamlib/SerialRate", wxGetApp().m_intHamlibSerialRate);
        pConfig->Write("/Hamlib/IcomCIVHex", wxGetApp().m_intHamlibIcomCIVHex);


        pConfig->Write(wxT("/File/playFileToMicInPath"),    wxGetApp().m_playFileToMicInPath);
        pConfig->Write(wxT("/File/recFileFromRadioPath"),   wxGetApp().m_recFileFromRadioPath);
        pConfig->Write(wxT("/File/recFileFromRadioSecs"),   wxGetApp().m_recFileFromRadioSecs);
        pConfig->Write(wxT("/File/recFileFromModulatorPath"),   wxGetApp().m_recFileFromModulatorPath);
        pConfig->Write(wxT("/File/recFileFromModulatorSecs"),   wxGetApp().m_recFileFromModulatorSecs);
        pConfig->Write(wxT("/File/playFileFromRadioPath"),  wxGetApp().m_playFileFromRadioPath);

        pConfig->Write(wxT("/Audio/snrSlow"), wxGetApp().m_snrSlow);

        pConfig->Write(wxT("/Data/CallSign"), wxGetApp().m_callSign);
        pConfig->Write(wxT("/Data/TextEncoding"), wxGetApp().m_textEncoding);
        pConfig->Write(wxT("/Events/enable"), wxGetApp().m_events);
        pConfig->Write(wxT("/Events/spam_timer"), wxGetApp().m_events_spam_timer);
        pConfig->Write(wxT("/Events/regexp_match"), wxGetApp().m_events_regexp_match);
        pConfig->Write(wxT("/Events/regexp_replace"), wxGetApp().m_events_regexp_replace);

        pConfig->Write(wxT("/UDP/enable"), wxGetApp().m_udp_enable);
        pConfig->Write(wxT("/UDP/port"),  wxGetApp().m_udp_port);

        pConfig->Write(wxT("/Filter/MicInEQEnable"), wxGetApp().m_MicInEQEnable);
        pConfig->Write(wxT("/Filter/SpkOutEQEnable"), wxGetApp().m_SpkOutEQEnable);

        pConfig->Write(wxT("/FreeDV700/txClip"), wxGetApp().m_FreeDV700txClip);
        pConfig->Write(wxT("/OFDM/PhaseEstBW"), wxGetApp().m_PhaseEstBW);
        pConfig->Write(wxT("/OFDM/PhaseEstDPSK"), wxGetApp().m_PhaseEstDPSK);
        pConfig->Write(wxT("/Noise/noise_snr"), wxGetApp().m_noise_snr);

        pConfig->Write(wxT("/Debug/console"), wxGetApp().m_debug_console);

        pConfig->Write(wxT("/PSKReporter/Enable"), wxGetApp().m_psk_enable);
        pConfig->Write(wxT("/PSKReporter/Callsign"), wxGetApp().m_psk_callsign);
        pConfig->Write(wxT("/PSKReporter/GridSquare"), wxGetApp().m_psk_grid_square);
        
        // Waterfall configuration
        pConfig->Write(wxT("/Waterfall/Color"), wxGetApp().m_waterfallColor);
        
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
        if (m_rb2400b->GetValue())
            mode = 7;
        if (m_rb2020->GetValue())
            mode = 9;
       pConfig->Write(wxT("/Audio/mode"), mode);
       pConfig->Flush();
    }

    m_togBtnOnOff->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnOnOffUI), NULL, this);
    m_togBtnSplit->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnSplitClickUI), NULL, this);
    m_togBtnAnalog->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnAnalogClickUI), NULL, this);
 
    sox_biquad_finish();

    if (m_RxRunning)
    {
        stopRxStream();
    }
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

    delete wxConfigBase::Set((wxConfigBase *) NULL);

    if (optionsDlg != NULL) {
        delete optionsDlg;
        optionsDlg = NULL;
    }
}


#ifdef _USE_ONIDLE
void MainFrame::OnIdle(wxIdleEvent &evt) {
}
#endif


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
    if (evt.GetTimer().GetId() == ID_TIMER_PSKREPORTER)
    {
        // PSK Reporter timer fired; send in-progress packet.
        wxGetApp().m_pskReporter->send();
    }
    
    int r,c;

    if (m_panelWaterfall->checkDT()) {
        m_panelWaterfall->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);
        m_panelWaterfall->m_newdata = true;
        m_panelWaterfall->setColor(wxGetApp().m_waterfallColor);
        m_panelWaterfall->Refresh();
    }

    m_panelSpectrum->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);
    m_panelSpectrum->m_newdata = true;
    m_panelSpectrum->Refresh();

    /* update scatter/eye plot ------------------------------------------------------------*/

    if (freedvInterface.isRunning()) {
        int currentMode = freedvInterface.getCurrentMode();
        if ((currentMode == FREEDV_MODE_800XA) || (currentMode == FREEDV_MODE_2400B) ) {

            /* FSK Mode - eye diagram ---------------------------------------------------------*/

            /* add samples row by row */

            int i;
            for (i=0; i<g_stats.neyetr; i++) {
                m_panelScatter->add_new_samples_eye(&g_stats.rx_eye[i][0], g_stats.neyesamp);
            }
        }
        else {
            /* PSK Modes - scatter plot -------------------------------------------------------*/
            for (r=0; r<g_stats.nr; r++) {

                if ((currentMode == FREEDV_MODE_1600) ||
                    (currentMode == FREEDV_MODE_700D) ||
                    (currentMode == FREEDV_MODE_700E) ||
                    (currentMode == FREEDV_MODE_2020)) {
                    m_panelScatter->add_new_samples_scatter(&g_stats.rx_symbols[r][0]);
                }

                if (currentMode == FREEDV_MODE_700C) {

                    if (wxGetApp().m_FreeDV700Combine) {
                        m_panelScatter->setNc(g_Nc/2); /* m_FreeDV700Combine may have changed at run time */

                        /*
                           FreeDV 700C uses diversity, so optionaly combine
                           symbols for scatter plot, as combined symbols are
                           used for demodulation.  Note we need to use a copy
                           of the symbols, as we are not sure when the stats
                           will be updated.
                        */

                        COMP rx_symbols_copy[g_Nc/2];

                        for(c=0; c<g_Nc/2; c++)
                            rx_symbols_copy[c] = fcmult(0.5, cadd(g_stats.rx_symbols[r][c], g_stats.rx_symbols[r][c+g_Nc/2]));
                        m_panelScatter->add_new_samples_scatter(rx_symbols_copy);
                    }
                    else {
                        m_panelScatter->setNc(g_Nc); /* m_FreeDV700Combine may have changed at run time */
                        /*
                          Sometimes useful to plot carriers separately, e.g. to determine if tx carrier power is constant
                          across carriers.
                        */
                        m_panelScatter->add_new_samples_scatter(&g_stats.rx_symbols[r][0]);
                    }
                }

            }
        }
    }

    m_panelScatter->Refresh();

    // Oscilliscope type speech plots -------------------------------------------------------

    short speechInPlotSamples[WAVEFORM_PLOT_BUF];
    if (codec2_fifo_read(g_plotSpeechInFifo, speechInPlotSamples, WAVEFORM_PLOT_BUF)) {
        memset(speechInPlotSamples, 0, WAVEFORM_PLOT_BUF*sizeof(short));
        //fprintf(stderr, "empty!\n");
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

    m_panelTimeOffset->add_new_sample(0, (float)g_stats.rx_timing/FDMDV_NOM_SAMPLES_PER_FRAME);
    m_panelTimeOffset->Refresh();

    m_panelFreqOffset->add_new_sample(0, g_stats.foff);
    m_panelFreqOffset->Refresh();

    // SNR text box and gauge ------------------------------------------------------------

    // LP filter g_stats.snr_est some more to stabilise the
    // display. g_stats.snr_est already has some low pass filtering
    // but we need it fairly fast to activate squelch.  So we
    // optionally perform some further filtering for the display
    // version of SNR.  The "Slow" checkbox controls the amount of
    // filtering.  The filtered snr also controls the squelch

    float snr_limited;
    // some APIs pass us invalid values, so lets trap it rather than bombing
    if (!(isnan(g_stats.snr_est) || isinf(g_stats.snr_est))) {
        g_snr = m_snrBeta*g_snr + (1.0 - m_snrBeta)*g_stats.snr_est;
    }
    snr_limited = g_snr;
    if (snr_limited < -5.0) snr_limited = -5.0;
    if (snr_limited > 20.0) snr_limited = 20.0;
    char snr[15];
    int snr_val = (int)(g_snr+0.5);
    sprintf(snr, "%d", snr_val); // round to nearest dB

    //fprintf(stderr, "g_mode: %d snr_est: %f m_snrBeta: %f g_snr: %f snr_limited: %f\n", g_mode, g_stats.snr_est,  m_snrBeta, g_snr, snr_limited);

    wxString snr_string(snr);
    m_textSNR->SetLabel(snr_string);
    m_gaugeSNR->SetValue((int)(snr_limited+5));


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
    //printf("maxScaled: %d\n", maxScaled);
    if (((float)maxScaled/100) > tooHighThresh)
        m_textLevel->SetLabel("Too High");
    else
        m_textLevel->SetLabel("");

    m_maxLevel *= LEVEL_BETA;

    // sync LED (Colours don't work on Windows) ------------------------

    //fprintf(stderr, "g_State: %d  m_rbSync->GetValue(): %d\n", g_State, m_rbSync->GetValue());
    if (g_State) {
        if (g_prev_State == 0) {
            g_resyncs++;
            if (wxGetApp().m_callsignEncoder) wxGetApp().m_callsignEncoder->clearReceivedText();
            
            // Clear RX text to reduce the incidence of incorrect callsigns extracted with
            // the PSK Reporter callsign extraction logic.
            m_txtCtrlCallSign->SetValue(wxT(""));
            memset(m_callsign, 0, MAX_CALLSIGN);
            m_pcallsign = m_callsign;
            
            // Get current time to enforce minimum sync time requirement for PSK Reporter.
            g_sync_time = time(0);
        }
        m_textSync->SetForegroundColour( wxColour( 0, 255, 0 ) ); // green
	m_textSync->SetLabel("Modem");
     }
    else {
        m_textSync->SetForegroundColour( wxColour( 255, 0, 0 ) ); // red
	m_textSync->SetLabel("Modem");
     }
    g_prev_State = g_State;

    // send Callsign ----------------------------------------------------

    char callsign[MAX_CALLSIGN];
    
    // Convert to 6 bit character set for use with Golay encoding.
    memset(callsign, 0, MAX_CALLSIGN);
    
    if (wxGetApp().m_callsignEncoder)
    {
        strncpy(callsign, (const char*) wxGetApp().m_psk_callsign.mb_str(wxConvUTF8), MAX_CALLSIGN/2 - 2);
        if (strlen(callsign) < MAX_CALLSIGN/2 - 1)
        {
            strncat(callsign, "\r", 1);
        }    
        wxGetApp().m_callsignEncoder->setCallsign(&callsign[0]);
    }
    else
    {
        strncpy(callsign, (const char*) wxGetApp().m_callSign.mb_str(wxConvUTF8), MAX_CALLSIGN - 2);
        if (strlen(callsign) < MAX_CALLSIGN - 1)
        {
            strncat(callsign, "\r", 1);
        }    
    }   
     
    // buffer 1 txt message to ensure tx data fifo doesn't "run dry"
    char* sendBuffer = &callsign[0];
    if (wxGetApp().m_callsignEncoder) sendBuffer = (char*)wxGetApp().m_callsignEncoder->getEncodedCallsign();
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
        
        if (wxGetApp().m_callsignEncoder)
        {
            wxGetApp().m_callsignEncoder->pushReceivedByte(incomingChar);    
            if (wxGetApp().m_callsignEncoder->isInSync())
            {
                m_txtCtrlCallSign->SetValue(wxGetApp().m_callsignEncoder->getReceivedText());
            }        
        }
        else
        {
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

    // We should only report to PSK Reporter when all of the following are true:
    // a) The callsign encoder indicates a valid callsign has been received.
    // b) We detect a valid format callsign in the text (see https://en.wikipedia.org/wiki/Amateur_radio_call_signs).
    // c) We don't currently have a pending report to add to the outbound list for the active callsign.
    // When the above is true, capture the callsign and current SNR and add to the PSK Reporter object's outbound list.
    if (wxGetApp().m_pskReporter != NULL && wxGetApp().m_callsignEncoder != NULL)
    {
        if (wxGetApp().m_callsignEncoder->isCallsignValid())
        {
            wxRegEx callsignFormat("(([A-Za-z0-9]+/)?[A-Za-z0-9]{1,3}[0-9][A-Za-z0-9]*[A-Za-z](/[A-Za-z0-9]+)?)");
            wxString wxCallsign = m_txtCtrlCallSign->GetValue();
            if (callsignFormat.Matches(wxCallsign) && wxGetApp().m_pskPendingCallsign != callsignFormat.GetMatch(wxCallsign, 1).ToStdString())
            {
                wxString rxCallsign = callsignFormat.GetMatch(wxCallsign, 1);
                wxGetApp().m_pskPendingCallsign = rxCallsign.ToStdString();
                wxGetApp().m_pskPendingSnr = snr_val;
            }
        }
        else if (wxGetApp().m_pskPendingCallsign != "")
        {
            wxGetApp().m_hamlib->update_frequency_and_mode();
            fprintf(
                stderr, 
                "Adding callsign %s @ SNR %d, freq %d to PSK Reporter.\n", 
                wxGetApp().m_pskPendingCallsign.c_str(), 
                wxGetApp().m_pskPendingSnr,
                (unsigned int)wxGetApp().m_hamlib->get_frequency());
            
            wxGetApp().m_pskReporter->addReceiveRecord(
                wxGetApp().m_pskPendingCallsign,
                wxGetApp().m_hamlib->get_frequency(),
                wxGetApp().m_pskPendingSnr);
            wxGetApp().m_pskPendingCallsign = "";
            wxGetApp().m_pskPendingSnr = 0;
        }
    }
    
    // Run time update of EQ filters -----------------------------------

    if (m_newMicInFilter || m_newSpkOutFilter) {
        g_mutexProtectingCallbackData.Lock();
        deleteEQFilters(g_rxUserdata);
        designEQFilters(g_rxUserdata);
        g_mutexProtectingCallbackData.Unlock();
        m_newMicInFilter = m_newSpkOutFilter = false;
    }
    g_rxUserdata->micInEQEnable = wxGetApp().m_MicInEQEnable;
    g_rxUserdata->spkOutEQEnable = wxGetApp().m_SpkOutEQEnable;

    // set some run time options (if applicable)
    freedvInterface.setRunTimeOptions(
        (int)wxGetApp().m_FreeDV700txClip,
        (int)wxGetApp().m_FreeDV700txBPF,
        (int)wxGetApp().m_PhaseEstBW,
        (int)wxGetApp().m_PhaseEstDPSK);

    // Test Frame Bit Error Updates ------------------------------------

    // Toggle test frame mode at run time

    if (!freedvInterface.usingTestFrames() && wxGetApp().m_testFrames) {
        // reset stats on check box off to on transition
        freedvInterface.resetTestFrameStats();
    }
    freedvInterface.setTestFrames(wxGetApp().m_testFrames, wxGetApp().m_FreeDV700Combine);
    g_channel_noise = wxGetApp().m_channel_noise;

    // update stats on main page

    char mode[80], bits[80], errors[80], ber[80], resyncs[80], clockoffset[80], freqoffset[80], syncmetric[80];
    sprintf(mode, "Mode: %s", freedvInterface.getCurrentModeStr()); wxString modeString(mode); m_textCurrentDecodeMode->SetLabel(modeString);
    sprintf(bits, "Bits: %d", freedvInterface.getTotalBits()); wxString bits_string(bits); m_textBits->SetLabel(bits_string);
    sprintf(errors, "Errs: %d", freedvInterface.getTotalBitErrors()); wxString errors_string(errors); m_textErrors->SetLabel(errors_string);
    float b = (float)freedvInterface.getTotalBitErrors()/(1E-6+freedvInterface.getTotalBits());
    sprintf(ber, "BER: %4.3f", b); wxString ber_string(ber); m_textBER->SetLabel(ber_string);
    sprintf(resyncs, "Resyncs: %d", g_resyncs); wxString resyncs_string(resyncs); m_textResyncs->SetLabel(resyncs_string);

    sprintf(freqoffset, "FrqOff: %3.1f", g_stats.foff);
    wxString freqoffset_string(freqoffset); m_textFreqOffset->SetLabel(freqoffset_string);
    sprintf(syncmetric, "Sync: %3.2f", g_stats.sync_metric);
    wxString syncmetric_string(syncmetric); m_textSyncMetric->SetLabel(syncmetric_string);

    // Codec 2 700C VQ "auto EQ" equaliser variance
    int currentMode = freedvInterface.getCurrentMode();
    if ((currentMode == FREEDV_MODE_700C) || (currentMode == FREEDV_MODE_700D) || (currentMode == FREEDV_MODE_700E)) {
        auto var = freedvInterface.getVariance();
        char var_str[80]; sprintf(var_str, "Var: %4.1f", var);
        wxString var_string(var_str); m_textCodec2Var->SetLabel(var_string);
    }

    if (g_State) {

        sprintf(clockoffset, "ClkOff: %+-d", (int)round(g_stats.clock_offset*1E6) % 10000);
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
                    //if (b%2)
                    //    printf("g_error_hist[%d]: %d\n", b/2, g_error_hist[b/2]);
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
                //fprintf(stderr, "after g_error_pattern_fifo read 2\n");

                /*
                   FreeDV 700 mapping from error pattern to bit on each carrier, see
                   data bit to carrier mapping in:

                      codec2-dev/octave/cohpsk_frame_design.ods

                   We can plot a histogram of the errors/carrier before or after diversity
                   recombination.  Actually one bar for each IQ bit in carrier order.
                */

                int hist_Nc = sz_error_pattern/4;
                //fprintf(stderr, "hist_Nc: %d\n", hist_Nc);

                for(i=0; i<sz_error_pattern; i++) {
                    /* maps to IQ bits from each symbol to a "carrier" (actually one line for each IQ bit in carrier order) */
                    c = floor(i/4);
                    /* this will clock in 4 bits/carrier to plot */
                    m_panelTestFrameErrors->add_new_sample(c, c + 0.8*error_pattern[i]);
                    g_error_hist[c] += error_pattern[i];
                    g_error_histn[c]++;
                    //printf("i: %d c: %d\n", i, c);
                }
                for(; i<2*MODEM_STATS_NC_MAX*4; i++) {
                    c = floor(i/4);
                    m_panelTestFrameErrors->add_new_sample(c, c);
                    //printf("i: %d c: %d\n", i, c);
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

    // Blink file playback status line

    if (g_playFileFromRadio) {
        g_blink += DT;
        //fprintf(g_logfile, "g_blink: %f\n", g_blink);
        if ((g_blink >= 1.0) && (g_blink < 2.0))
            SetStatusText(wxT("Playing into from radio"), 0);
        if (g_blink >= 2.0) {
            SetStatusText(wxT(""), 0);
            g_blink = 0.0;
        }
    }
    
    // Voice Keyer state machine
    VoiceKeyerProcessEvent(VK_DT);

    // Detect Sync state machine
    DetectSyncProcessEvent();
}
#endif


//-------------------------------------------------------------------------
// OnExit()
//-------------------------------------------------------------------------
void MainFrame::OnExit(wxCommandEvent& event)
{
    // Note: sideband detection needs to be disabled here instead
    // of in the destructor due to its need to touch the UI.
    if (wxGetApp().m_hamlib)
    {
        wxGetApp().m_hamlib->disable_mode_detection();
        wxGetApp().m_hamlib->close();
    }

    if (wxGetApp().m_pskReporter)
    {
        delete wxGetApp().m_pskReporter;
        wxGetApp().m_pskReporter = NULL;
    }
    
    if (wxGetApp().m_callsignEncoder)
    {
        delete wxGetApp().m_callsignEncoder;
        wxGetApp().m_callsignEncoder = NULL;
    }
    
    //fprintf(stderr, "MainFrame::OnExit\n");
    wxUnusedVar(event);
#ifdef _USE_TIMER
    m_plotTimer.Stop();
    m_pskReporterTimer.Stop();
#endif // _USE_TIMER
    if(g_sfPlayFile != NULL)
    {
        sf_close(g_sfPlayFile);
        g_sfPlayFile = NULL;
    }
    if(g_sfRecFile != NULL)
    {
        sf_close(g_sfRecFile);
        g_sfRecFile = NULL;
    }
    if(m_RxRunning)
    {
        stopRxStream();
    }
    m_togBtnSplit->Disable();
    m_togBtnAnalog->Disable();
    //m_btnTogPTT->Disable();

    Pa_Terminate();
    Destroy();
}

void MainFrame::OnChangeTxMode( wxCommandEvent& event )
{
    txModeChangeMutex.Lock();
    
    if (m_rb1600->GetValue()) 
    {
        g_mode = FREEDV_MODE_1600;
        g_Nc = 16;
        m_panelScatter->setNc(g_Nc+1);  /* +1 for BPSK pilot */
    }
    else if (m_rb700c->GetValue()) 
    {
        g_mode = FREEDV_MODE_700C;
        g_Nc = 14;
        if (wxGetApp().m_FreeDV700Combine) {
            m_panelScatter->setNc(g_Nc/2);  /* diversity combnation */
        }
        else {
            m_panelScatter->setNc(g_Nc);
        }
    }
    else if (m_rb700d->GetValue()) 
    {
        g_mode = FREEDV_MODE_700D;
        g_Nc = 17;                         /* TODO: be nice if we didn't have to hard code this, maybe API call? */
        m_panelScatter->setNc(g_Nc);
    }
    else if (m_rb700e->GetValue()) 
    {
        g_mode = FREEDV_MODE_700E;
        g_Nc = 17;
        m_panelScatter->setNc(g_Nc);
    }
    else if (m_rb800xa->GetValue()) 
    {
        g_mode = FREEDV_MODE_800XA;
    }
    else if (m_rb2400b->GetValue()) 
    {
        g_mode = FREEDV_MODE_2400B;
    }
    else if (m_rb2020->GetValue()) 
    {
        assert(isAvxPresent);
        
        g_mode = FREEDV_MODE_2020;
        g_Nc = 31;                         /* TODO: be nice if we didn't have to hard code this, maybe API call? */
        m_panelScatter->setNc(g_Nc);
    }
    
    if (freedvInterface.isRunning())
    {
        // Need to change the TX interface live.
        freedvInterface.changeTxMode(g_mode);
    }
    
    // Re-initialize Speex since the sample rate's changing
    if (wxGetApp().m_speexpp_enable && freedvInterface.isRunning())
    {
        if (g_speex_st)
        {
            speex_preprocess_state_destroy(g_speex_st);
        }
        g_speex_st = speex_preprocess_state_init(freedvInterface.getTxNumSpeechSamples(), freedvInterface.getTxSpeechSampleRate());
    }
    
    txModeChangeMutex.Unlock();
}

//-------------------------------------------------------------------------
// OnTogBtnOnOff()
//-------------------------------------------------------------------------
void MainFrame::OnTogBtnOnOff(wxCommandEvent& event)
{
    wxString startStop = m_togBtnOnOff->GetLabel();

    // we are attempting to start

    if (startStop.IsSameAs("Start"))
    {
        if (g_verbose) fprintf(stderr, "Start .....\n");
        g_queueResync = false;
        endingTx = false;
        
        //
        // Start Running -------------------------------------------------
        //

        // modify some button states when running

        m_togBtnOnOff->SetLabel(wxT("Stop"));

        vk_state = VK_IDLE;

        m_textSync->Enable();
        m_textCurrentDecodeMode->Enable();

        // determine what mode we are using
        OnChangeTxMode(event);

        // init freedv states
        m_togBtnSplit->Enable();
        m_togBtnAnalog->Enable();
        m_btnTogPTT->Enable();
        m_togBtnVoiceKeyer->Enable();

        if (g_mode == FREEDV_MODE_2400B || g_mode == FREEDV_MODE_800XA || !wxGetApp().m_boolMultipleRx)
        {
            m_rb1600->Disable();
            m_rb700c->Disable();
            m_rb700d->Disable();
            m_rb700e->Disable();
            m_rb800xa->Disable();
            m_rb2400b->Disable();
            m_rb2020->Disable();
            freedvInterface.addRxMode(g_mode);
        }
        else
        {
            if(isAvxPresent)
            {
                freedvInterface.addRxMode(FREEDV_MODE_2020);
            }
            
            int rxModes[] = {
                FREEDV_MODE_1600,
                FREEDV_MODE_700E,
                FREEDV_MODE_700C,
                FREEDV_MODE_700D,
                
                // These modes require more CPU than typical (and will drive at least one core to 100% if
                // used with the other five above), so we're excluding them from multi-RX. They also aren't
                // selectable during a session when multi-RX is enabled.
                //FREEDV_MODE_2400B,
                //FREEDV_MODE_800XA
            };
            for (auto& mode : rxModes)
            {
                freedvInterface.addRxMode(mode);
            }
            
            m_rb800xa->Disable();
            m_rb2400b->Disable();
        }
        
        freedvInterface.start(g_mode, wxGetApp().m_fifoSize_ms);
        if (wxGetApp().m_FreeDV700ManualUnSync) {
            freedvInterface.setSync(FREEDV_SYNC_MANUAL);
        } else {
            freedvInterface.setSync(FREEDV_SYNC_AUTO);
        }

        // Codec 2 VQ Equaliser
        freedvInterface.setEq(wxGetApp().m_700C_EQ);

        // Codec2 verbosity setting
        freedvInterface.setVerbose(g_freedv_verbose);

        // Text field/callsign callbacks.
        freedvInterface.setTextCallbackFn(&my_put_next_rx_char, &my_get_next_tx_char);

        g_error_hist = new short[MODEM_STATS_NC_MAX*2];
        g_error_histn = new short[MODEM_STATS_NC_MAX*2];
        int i;
        for(i=0; i<2*MODEM_STATS_NC_MAX; i++) {
            g_error_hist[i] = 0;
            g_error_histn[i] = 0;
        }

        // Set processing buffer sizes, these are FRAME_DURATION (20ms) chunks of modem and speech that are a useful size for the
        // various operations we do before and after passing to the freedv_api layer.
        g_modemInbufferSize = (int)(FRAME_DURATION * freedvInterface.getRxModemSampleRate());
        g_speechOutbufferSize = (int)(FRAME_DURATION * freedvInterface.getRxSpeechSampleRate());

        // init Codec 2 LPC Post Filter (FreeDV 1600)
        freedvInterface.setLpcPostFilter(
                                       wxGetApp().m_codec2LPCPostFilterEnable,
                                       wxGetApp().m_codec2LPCPostFilterBassBoost,
                                       wxGetApp().m_codec2LPCPostFilterBeta,
                                       wxGetApp().m_codec2LPCPostFilterGamma);

        // Init Speex pre-processor states
        // by inspecting Speex source it seems that only denoiser is on by default

        if (g_verbose) fprintf(stderr, "freedv_get_n_speech_samples(tx): %d\n", freedvInterface.getTxNumSpeechSamples());
        if (g_verbose) fprintf(stderr, "freedv_get_speech_sample_rate(tx): %d\n", freedvInterface.getTxSpeechSampleRate());

        if (wxGetApp().m_speexpp_enable)
            g_speex_st = speex_preprocess_state_init(freedvInterface.getTxNumSpeechSamples(), freedvInterface.getTxSpeechSampleRate());
        
        // adjust spectrum and waterfall freq scaling base on mode

        m_panelSpectrum->setFreqScale(MODEM_STATS_NSPEC*((float)MAX_F_HZ/(freedvInterface.getTxModemSampleRate()/2)));
        m_panelWaterfall->setFs(freedvInterface.getTxModemSampleRate());

        // Init text msg decoding
        freedvInterface.setTextVaricodeNum(wxGetApp().m_textEncoding);

        // scatter plot (PSK) or Eye (FSK) mode

        if ((g_mode == FREEDV_MODE_800XA) || (g_mode == FREEDV_MODE_2400A) || (g_mode == FREEDV_MODE_2400B)) {
            m_panelScatter->setEyeScatter(PLOT_SCATTER_MODE_EYE);
        }
        else {
            m_panelScatter->setEyeScatter(PLOT_SCATTER_MODE_SCATTER);
        }

        modem_stats_open(&g_stats);
        int src_error;
        g_spec_src = src_new(SRC_SINC_FASTEST, 1, &src_error);
        assert(g_spec_src != NULL);
        g_State = g_prev_State = 0;
        g_snr = 0.0;
        g_half_duplex = wxGetApp().m_boolHalfDuplex;

        m_pcallsign = m_callsign;
        memset(m_callsign, 0, sizeof(m_callsign));

        m_maxLevel = 0;
        m_textLevel->SetLabel(wxT(""));
        m_gaugeLevel->SetValue(0);

        //printf("m_textEncoding = %d\n", wxGetApp().m_textEncoding);
        //printf("g_stats.snr: %f\n", g_stats.snr_est);

        // attempt to start sound cards and tx/rx processing
        if (VerifyMicrophonePermissions())
        {
            if (validateSoundCardSetup())
            {
                startRxStream();
            }
        }
        else
        {
            wxMessageBox(wxString("Microphone permissions must be granted to FreeDV for it to function properly."), wxT("Error"), wxOK | wxICON_ERROR, this);
        }

        // Clear existing TX text, if any.
        codec2_fifo_destroy(g_txDataInFifo);
        g_txDataInFifo = codec2_fifo_create(MAX_CALLSIGN*FREEDV_VARICODE_MAX_BITS);
        
        // attempt to start PTT ......
        wxGetApp().m_pskReporter = NULL;
        wxGetApp().m_callsignEncoder = NULL;
        if (wxGetApp().m_boolHamlibUseForPTT)
            OpenHamlibRig();
        else if (wxGetApp().m_psk_enable)
        {
            wxMessageBox("Hamlib support must be enabled to report to PSK Reporter. PSK Reporter reporting will be disabled.", wxT("Error"), wxOK | wxICON_ERROR, this);
        }
        
        if (wxGetApp().m_boolUseSerialPTT) {
            OpenSerialPort();
        }

        if (m_RxRunning)
        {
#ifdef _USE_TIMER
            m_plotTimer.Start(_REFRESH_TIMER_PERIOD, wxTIMER_CONTINUOUS);
#endif // _USE_TIMER
        }
        
    }

    // Stop was pressed or start up failed

    if (startStop.IsSameAs("Stop") || !m_RxRunning ) {
        if (g_verbose) fprintf(stderr, "Stop .....\n");
        
        //
        // Stop Running -------------------------------------------------
        //

#ifdef _USE_TIMER
        m_plotTimer.Stop();
        m_pskReporterTimer.Stop();
#endif // _USE_TIMER

        // ensure we are not transmitting and shut down audio processing

        if (wxGetApp().m_boolHamlibUseForPTT) {
            Hamlib *hamlib = wxGetApp().m_hamlib;
            wxString hamlibError;
            if (wxGetApp().m_boolHamlibUseForPTT && hamlib != NULL) {
                if (hamlib->isActive())
                {
                    if (hamlib->ptt(false, hamlibError) == false) {
                        wxMessageBox(wxString("Hamlib PTT Error: ") + hamlibError, wxT("Error"), wxOK | wxICON_ERROR, this);
                    }
                    hamlib->disable_mode_detection();
                    hamlib->close();
                }
            }
        }

        if (wxGetApp().m_pskReporter)
        {
            delete wxGetApp().m_pskReporter;
            wxGetApp().m_pskReporter = NULL;
        }

        if (wxGetApp().m_callsignEncoder)
        {
            delete wxGetApp().m_callsignEncoder;
            wxGetApp().m_callsignEncoder = NULL;
        }
        
        if (wxGetApp().m_boolUseSerialPTT) {
            CloseSerialPort();
        }

        m_btnTogPTT->SetValue(false);
        VoiceKeyerProcessEvent(VK_SPACE_BAR);

        stopRxStream();
        src_delete(g_spec_src);
        modem_stats_close(&g_stats);

        // FreeDV clean up
        delete[] g_error_hist;
        delete[] g_error_histn;
        freedvInterface.stop();
        
        if (wxGetApp().m_speexpp_enable)
            speex_preprocess_state_destroy(g_speex_st);
        
        m_newMicInFilter = m_newSpkOutFilter = true;

        m_textSync->Disable();
        m_textCurrentDecodeMode->Disable();

        m_togBtnSplit->Disable();
        m_togBtnAnalog->Disable();
        m_btnTogPTT->Disable();
        m_togBtnVoiceKeyer->Disable();
        m_togBtnOnOff->SetLabel(wxT("Start"));
        
        m_rb1600->Enable();
        m_rb700c->Enable();
        m_rb700d->Enable();
        m_rb700e->Enable();
        m_rb800xa->Enable();
        m_rb2400b->Enable();
        if(isAvxPresent)
            m_rb2020->Enable();
   }
    
    optionsDlg->setSessionActive(m_RxRunning);
}

//-------------------------------------------------------------------------
// stopRxStream()
//-------------------------------------------------------------------------
void MainFrame::stopRxStream()
{
    if(m_RxRunning)
    {
        m_RxRunning = false;

        //fprintf(stderr, "waiting for thread to stop\n");
        m_txRxThread->m_run = 0;
        m_txRxThread->Wait();
        //fprintf(stderr, "thread stopped\n");

        m_rxInPa->stop();
        m_rxInPa->streamClose();
        delete m_rxInPa;
        if(m_rxOutPa != m_rxInPa) {
			m_rxOutPa->stop();
			m_rxOutPa->streamClose();
			delete m_rxOutPa;
		}

        if (g_nSoundCards == 2) {
            m_txInPa->stop();
            m_txInPa->streamClose();
            delete m_txInPa;
            if(m_txInPa != m_txOutPa) {
				m_txOutPa->stop();
				m_txOutPa->streamClose();
				delete m_txOutPa;
			}
        }

        destroy_fifos();
        destroy_src();
        deleteEQFilters(g_rxUserdata);
        delete g_rxUserdata;
        
        Pa_Terminate();
    }
}

void MainFrame::destroy_fifos(void)
{
    codec2_fifo_destroy(g_rxUserdata->infifo1);
    codec2_fifo_destroy(g_rxUserdata->outfifo1);
    codec2_fifo_destroy(g_rxUserdata->infifo2);
    codec2_fifo_destroy(g_rxUserdata->outfifo2);
    codec2_fifo_destroy(g_rxUserdata->rxinfifo);
    codec2_fifo_destroy(g_rxUserdata->rxoutfifo);
}

void MainFrame::destroy_src(void)
{
    src_delete(g_rxUserdata->insrc1);
    src_delete(g_rxUserdata->outsrc1);
    src_delete(g_rxUserdata->insrc2);
    src_delete(g_rxUserdata->outsrc2);
    src_delete(g_rxUserdata->insrcsf);
}

void  MainFrame::initPortAudioDevice(PortAudioWrap *pa, int inDevice, int outDevice,
                                     int soundCard, int sampleRate, int inputChannels,
                                     int outputChannels)
{
    // Note all of the wrapper functions below just set values in a
    // portaudio struct so can't return any errors. So no need to trap
    // any errors in this function.

    // init input params

    pa->setInputDevice(inDevice);
    if(inDevice != paNoDevice) {
        pa->setInputChannelCount(inputChannels);           // stereo input
        pa->setInputSampleFormat(PA_SAMPLE_TYPE);
        pa->setInputLatency(pa->getInputDefaultHighLatency());
        //fprintf(stderr,"PA in; low: %f high: %f\n", pa->getInputDefaultLowLatency(), pa->getInputDefaultHighLatency());
        pa->setInputHostApiStreamInfo(NULL);
    }

    pa->setOutputDevice(paNoDevice);

    // init output params

    pa->setOutputDevice(outDevice);
    if(outDevice != paNoDevice) {
        pa->setOutputChannelCount(outputChannels);                      // stereo output
        pa->setOutputSampleFormat(PA_SAMPLE_TYPE);
        pa->setOutputLatency(pa->getOutputDefaultHighLatency());
        //fprintf(stderr,"PA out; low: %f high: %f\n", pa->getOutputDefaultLowLatency(), pa->getOutputDefaultHighLatency());
        pa->setOutputHostApiStreamInfo(NULL);
    }

    // init params that affect input and output

    /*
      DR 2013:

      On Linux, setting this to wxGetApp().m_framesPerBuffer caused
      intermittant break up on the audio from my IC7200 on Ubuntu 14.
      After a day of bug hunting I found that 0, as recommended by the
      PortAudio documentation, fixed the problem.

      DR 2018:

      During 700D testing some break up in from radio audio, so made
      this adjustable again.
    */

    pa->setFramesPerBuffer(wxGetApp().m_framesPerBuffer);

    pa->setSampleRate(sampleRate);
    pa->setStreamFlags(paClipOff);
}

//-------------------------------------------------------------------------
// startRxStream()
//-------------------------------------------------------------------------
void MainFrame::startRxStream()
{
    int   src_error;
    const PaDeviceInfo *deviceInfo1a = NULL, *deviceInfo1b = NULL, *deviceInfo2a = NULL, *deviceInfo2b = NULL;
    int   inputChannels1, outputChannels1, inputChannels2, outputChannels2;
    bool  two_rx=false;
    bool  two_tx=false;

    if (g_verbose) fprintf(stderr, "startRxStream .....\n");
    if(!m_RxRunning) {
        m_RxRunning = true;
        if(Pa_Initialize())
        {
            wxMessageBox(wxT("Port Audio failed to initialize"), wxT("Pa_Initialize"), wxOK);
        }

        m_rxInPa = new PortAudioWrap();
        if(g_soundCard1InDeviceNum != g_soundCard1OutDeviceNum)
            two_rx=true;
        if(g_soundCard2InDeviceNum != g_soundCard2OutDeviceNum)
            two_tx=true;

        if (g_verbose) fprintf(stderr, "two_rx: %d two_tx: %d\n", two_rx, two_tx);
        if(two_rx)
            m_rxOutPa = new PortAudioWrap();
        else
            m_rxOutPa = m_rxInPa;

        if (g_nSoundCards == 0) {
            wxMessageBox(wxT("No Sound Cards configured, use Tools - Audio Config to configure"), wxT("Error"), wxOK);
            delete m_rxInPa;
            if(two_rx)
                delete m_rxOutPa;
            m_RxRunning = false;
            Pa_Terminate();
            return;
        }

        // Init Sound card 1 ----------------------------------------------
        // sanity check on sound card device numbers

        if ((m_rxInPa->getDeviceCount() <= g_soundCard1InDeviceNum) ||
            (m_rxOutPa->getDeviceCount() <= g_soundCard1OutDeviceNum)) {
            wxMessageBox(wxT("Sound Card 1 not present"), wxT("Error"), wxOK);
            delete m_rxInPa;
            if(two_rx)
                delete m_rxOutPa;
            m_RxRunning = false;
            Pa_Terminate();
            return;
        }

        // work out how many input channels this device supports.

        deviceInfo1a = Pa_GetDeviceInfo(g_soundCard1InDeviceNum);
        if (deviceInfo1a == NULL) {
            wxMessageBox(wxT("Couldn't get input device info from Port Audio for Sound Card 1"), wxT("Error"), wxOK);
            delete m_rxInPa;
            if(two_rx)
				delete m_rxOutPa;
            m_RxRunning = false;
            Pa_Terminate();
            return;
        }
        if (deviceInfo1a->maxInputChannels == 1)
            inputChannels1 = 1;
        else
            inputChannels1 = 2;

        // Grab the info for the FreeDV->speaker device as well and ensure we're using
        // the smallest number of common channels (1 or 2).
        deviceInfo1b = Pa_GetDeviceInfo(g_soundCard1OutDeviceNum);
        if (deviceInfo1b == NULL) {
            wxMessageBox(wxT("Couldn't get output device info from Port Audio for Sound Card 1"), wxT("Error"), wxOK);
            delete m_rxInPa;
            if(two_rx)
				delete m_rxOutPa;
            m_RxRunning = false;
            Pa_Terminate();
            return;
        }
        if (deviceInfo1b->maxOutputChannels == 1)
            outputChannels1 = 1;
        else
            outputChannels1 = 2;
        
        if(two_rx) {        
            initPortAudioDevice(m_rxInPa, g_soundCard1InDeviceNum, paNoDevice, 1,
                            g_soundCard1SampleRate, inputChannels1, inputChannels1);
            initPortAudioDevice(m_rxOutPa, paNoDevice, g_soundCard1OutDeviceNum, 1,
                            g_soundCard1SampleRate, outputChannels1, outputChannels1);
		}
        else
        {                    
            initPortAudioDevice(m_rxInPa, g_soundCard1InDeviceNum, g_soundCard1OutDeviceNum, 1,
                            g_soundCard1SampleRate, inputChannels1, outputChannels1);
        }
        
        // Init Sound Card 2 ------------------------------------------------

        if (g_nSoundCards == 2) {

            m_txInPa = new PortAudioWrap();
            if(two_tx)
                m_txOutPa = new PortAudioWrap();
            else
                m_txOutPa = m_txInPa;

            // sanity check on sound card device numbers

            //printf("m_txInPa->getDeviceCount(): %d\n", m_txInPa->getDeviceCount());
            //printf("g_soundCard2InDeviceNum: %d\n", g_soundCard2InDeviceNum);
            //printf("g_soundCard2OutDeviceNum: %d\n", g_soundCard2OutDeviceNum);

            if ((m_txInPa->getDeviceCount() <= g_soundCard2InDeviceNum) ||
                (m_txOutPa->getDeviceCount() <= g_soundCard2OutDeviceNum)) {
                wxMessageBox(wxT("Sound Card 2 not present"), wxT("Error"), wxOK);
                delete m_rxInPa;
                if(two_rx)
                    delete m_rxOutPa;
                delete m_txInPa;
                if(two_tx)
                    delete m_txOutPa;
                m_RxRunning = false;
                Pa_Terminate();
                return;
            }

            deviceInfo2a = Pa_GetDeviceInfo(g_soundCard2InDeviceNum);
            if (deviceInfo2a == NULL) {
                wxMessageBox(wxT("Couldn't get device info from Port Audio for Sound Card 2"), wxT("Error"), wxOK);
                delete m_rxInPa;
                if(two_rx)
					delete m_rxOutPa;
                delete m_txInPa;
                if(two_tx)
					delete m_txOutPa;
                m_RxRunning = false;
                Pa_Terminate();
                return;
            }
            if (deviceInfo2a->maxInputChannels == 1)
                inputChannels2 = 1;
            else
                inputChannels2 = 2;
            
            // Grab info for FreeDV->radio device.
            deviceInfo2b = Pa_GetDeviceInfo(g_soundCard2OutDeviceNum);
            if (deviceInfo2b == NULL) {
                wxMessageBox(wxT("Couldn't get device info from Port Audio for Sound Card 2"), wxT("Error"), wxOK);
                delete m_rxInPa;
                if(two_rx)
					delete m_rxOutPa;
                delete m_txInPa;
                if(two_tx)
					delete m_txOutPa;
                m_RxRunning = false;
                Pa_Terminate();
                return;
            }
            if (deviceInfo2b->maxOutputChannels == 1)
                outputChannels2 = 1;
            else
                outputChannels2 = 2;

            if(two_tx) {
				initPortAudioDevice(m_txInPa, g_soundCard2InDeviceNum, paNoDevice, 2,
                                g_soundCard2SampleRate, inputChannels2, inputChannels2);
				initPortAudioDevice(m_txOutPa, paNoDevice, g_soundCard2OutDeviceNum, 2,
                                g_soundCard2SampleRate, outputChannels2, outputChannels2);
			}
			else
				initPortAudioDevice(m_txInPa, g_soundCard2InDeviceNum, g_soundCard2OutDeviceNum, 2,
                                g_soundCard2SampleRate, inputChannels2, outputChannels2);
        }

        // Init call back data structure ----------------------------------------------

        g_rxUserdata = new paCallBackData;
        g_rxUserdata->inputChannels1 = inputChannels1;
        g_rxUserdata->outputChannels1 = outputChannels1;
        if (deviceInfo2a != NULL)
        {
            g_rxUserdata->inputChannels2 = inputChannels2;
            g_rxUserdata->outputChannels2 = outputChannels2;
        }
        
        // init sample rate conversion states

        g_rxUserdata->insrc1 = src_new(SRC_SINC_FASTEST, 1, &src_error);
        assert(g_rxUserdata->insrc1 != NULL);
        g_rxUserdata->outsrc1 = src_new(SRC_SINC_FASTEST, 1, &src_error);
        assert(g_rxUserdata->outsrc1 != NULL);
        g_rxUserdata->insrc2 = src_new(SRC_SINC_FASTEST, 1, &src_error);
        assert(g_rxUserdata->insrc2 != NULL);
        g_rxUserdata->outsrc2 = src_new(SRC_SINC_FASTEST, 1, &src_error);
        assert(g_rxUserdata->outsrc2 != NULL);

        g_rxUserdata->insrcsf = src_new(SRC_SINC_FASTEST, 1, &src_error);
        assert(g_rxUserdata->insrcsf != NULL);

        // create FIFOs used to interface between Port Audio and txRx
        // processing loop, which iterates about once every 20ms.
        // Sample rate conversion, stats for spectral plots, and
        // transmit processng are all performed in the txRxProcessing
        // loop.

        int m_fifoSize_ms = wxGetApp().m_fifoSize_ms;
        int soundCard1FifoSizeSamples = m_fifoSize_ms*g_soundCard1SampleRate/1000;
        int soundCard2FifoSizeSamples = m_fifoSize_ms*g_soundCard2SampleRate/1000;

        g_rxUserdata->infifo1 = codec2_fifo_create(soundCard1FifoSizeSamples);
        g_rxUserdata->outfifo1 = codec2_fifo_create(soundCard1FifoSizeSamples);
        g_rxUserdata->outfifo2 = codec2_fifo_create(soundCard2FifoSizeSamples);
        g_rxUserdata->infifo2 = codec2_fifo_create(soundCard2FifoSizeSamples);

        if (g_verbose) fprintf(stderr, "fifoSize_ms: %d infifo1/outfilo1: %d infifo2/outfilo2: %d\n",
                wxGetApp().m_fifoSize_ms, soundCard1FifoSizeSamples, soundCard2FifoSizeSamples);

        // reset debug stats for FIFOs

        g_infifo1_full = g_outfifo1_empty = g_infifo2_full = g_outfifo2_empty = 0;
        g_infifo1_full = g_outfifo1_empty = g_infifo2_full = g_outfifo2_empty = 0;
        for (int i=0; i<4; i++) {
            g_PAstatus1[i] = g_PAstatus2[i] = 0;
        }

        // These FIFOs interface between the 20ms txRxProcessing()
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

        if (g_verbose) fprintf(stderr, "rxInFifoSizeSamples: %d rxOutFifoSizeSamples: %d\n", rxInFifoSizeSamples, rxOutFifoSizeSamples);

        // Init Equaliser Filters ------------------------------------------------------

        m_newMicInFilter = m_newSpkOutFilter = true;
        designEQFilters(g_rxUserdata);
        g_rxUserdata->micInEQEnable = wxGetApp().m_MicInEQEnable;
        g_rxUserdata->spkOutEQEnable = wxGetApp().m_SpkOutEQEnable;

        // optional tone in left channel to reliably trigger vox

        g_rxUserdata->leftChannelVoxTone = wxGetApp().m_leftChannelVoxTone;
        g_rxUserdata->voxTonePhase = 0;

        // Start sound card 1 ----------------------------------------------------------

        m_rxInPa->setUserData(g_rxUserdata);
        m_rxErr = m_rxInPa->setCallback(rxCallback);

        m_rxErr = m_rxInPa->streamOpen();

        if(m_rxErr != paNoError) {
            wxMessageBox(wxT("Sound Card 1 Open/Setup error."), wxT("Error"), wxOK);
			delete m_rxInPa;
			if(two_rx)
				delete m_rxOutPa;
			delete m_txInPa;
			if(two_tx)
				delete m_txOutPa;
            destroy_fifos();
            destroy_src();
            deleteEQFilters(g_rxUserdata);
            delete g_rxUserdata;
            m_RxRunning = false;
            Pa_Terminate();
            return;
        }

        m_rxErr = m_rxInPa->streamStart();
        if(m_rxErr != paNoError) {
            wxMessageBox(wxT("Sound Card 1 Stream Start Error."), wxT("Error"), wxOK);
			delete m_rxInPa;
			if(two_rx)
				delete m_rxOutPa;
			delete m_txInPa;
			if(two_tx)
				delete m_txOutPa;
            destroy_fifos();
            destroy_src();
            deleteEQFilters(g_rxUserdata);
            delete g_rxUserdata;
            m_RxRunning = false;
            Pa_Terminate();
            return;
        }

        // Start separate output stream if needed

        if(two_rx) {
            m_rxOutPa->setUserData(g_rxUserdata);
            m_rxErr = m_rxOutPa->setCallback(rxCallback);

            m_rxErr = m_rxOutPa->streamOpen();

            if(m_rxErr != paNoError) {
                wxMessageBox(wxT("Sound Card 1 Second Stream Open/Setup error."), wxT("Error"), wxOK);
                delete m_rxInPa;
                delete m_rxOutPa;
                if(two_tx)
                    delete m_txOutPa;
                destroy_fifos();
                destroy_src();
                deleteEQFilters(g_rxUserdata);
                delete g_rxUserdata;
                m_RxRunning = false;
                Pa_Terminate();
                return;
            }

            m_rxErr = m_rxOutPa->streamStart();
            if(m_rxErr != paNoError) {
                wxMessageBox(wxT("Sound Card 1 Second Stream Start Error."), wxT("Error"), wxOK);
                m_rxInPa->stop();
                m_rxInPa->streamClose();
                delete m_rxInPa;
                delete m_rxOutPa;
                if(two_tx)
                    delete m_txOutPa;
                destroy_fifos();
                destroy_src();
                deleteEQFilters(g_rxUserdata);
                delete g_rxUserdata;
                m_RxRunning = false;
                Pa_Terminate();
                return;
            }
        }

        if (g_verbose) fprintf(stderr, "started stream 1\n");

        // Start sound card 2 ----------------------------------------------------------

        if (g_nSoundCards == 2) {

            // question: can we use same callback data
            // (g_rxUserdata)or both sound card callbacks?  Is there a
            // chance of them both being called at the same time?  We
            // could need a mutex ...

            m_txInPa->setUserData(g_rxUserdata);
            m_txErr = m_txInPa->setCallback(txCallback);
            m_txErr = m_txInPa->streamOpen();

            if(m_txErr != paNoError) {
                if (g_verbose) fprintf(stderr, "Err: %d\n", m_txErr);
                wxMessageBox(wxT("Sound Card 2 Open/Setup error."), wxT("Error"), wxOK);
                m_rxInPa->stop();
                m_rxInPa->streamClose();
                delete m_rxInPa;
                if(two_rx) {
                    m_rxOutPa->stop();
                    m_rxOutPa->streamClose();
                    delete m_rxOutPa;
                }
                delete m_txInPa;
                if(two_tx)
                    delete m_txOutPa;
                destroy_fifos();
                destroy_src();
                deleteEQFilters(g_rxUserdata);
                delete g_rxUserdata;
                m_RxRunning = false;
                Pa_Terminate();
                return;
            }
            m_txErr = m_txInPa->streamStart();
            if(m_txErr != paNoError) {
                wxMessageBox(wxT("Sound Card 2 Start Error."), wxT("Error"), wxOK);
                m_rxInPa->stop();
                m_rxInPa->streamClose();
                delete m_rxInPa;
                if(two_rx) {
                    m_rxOutPa->stop();
                    m_rxOutPa->streamClose();
                    delete m_rxOutPa;
                }
                delete m_txInPa;
                if(two_tx)
                    delete m_txOutPa;
                destroy_fifos();
                destroy_src();
                deleteEQFilters(g_rxUserdata);
                delete g_rxUserdata;
                m_RxRunning = false;
                Pa_Terminate();
                return;
            }

            // Start separate output stream if needed

            if (two_tx) {

                // question: can we use same callback data
                // (g_rxUserdata)or both sound card callbacks?  Is there a
                // chance of them both being called at the same time?  We
                // could need a mutex ...

                m_txOutPa->setUserData(g_rxUserdata);
                m_txErr = m_txOutPa->setCallback(txCallback);
                m_txErr = m_txOutPa->streamOpen();

                if(m_txErr != paNoError) {
                    wxMessageBox(wxT("Sound Card 2 Second Stream Open/Setup error."), wxT("Error"), wxOK);
                    m_rxInPa->stop();
                    m_rxInPa->streamClose();
                    delete m_rxInPa;
                    if(two_rx) {
                        m_rxOutPa->stop();
                        m_rxOutPa->streamClose();
                        delete m_rxOutPa;
                    }
                    m_txInPa->stop();
                    m_txInPa->streamClose();
                    delete m_txInPa;
                    delete m_txOutPa;
                    destroy_fifos();
                    destroy_src();
                    deleteEQFilters(g_rxUserdata);
                    delete g_rxUserdata;
                    m_RxRunning = false;
                    Pa_Terminate();
                    return;
                }
                m_txErr = m_txOutPa->streamStart();
                if(m_txErr != paNoError) {
                    wxMessageBox(wxT("Sound Card 2 Second Stream Start Error."), wxT("Error"), wxOK);
                    m_rxInPa->stop();
                    m_rxInPa->streamClose();
                    m_txInPa->stop();
                    m_txInPa->streamClose();
                    delete m_txInPa;
                    if(two_rx) {
                        m_rxOutPa->stop();
                        m_rxOutPa->streamClose();
                        delete m_rxOutPa;
                    }
                    delete m_txInPa;
                    delete m_txOutPa;
                    destroy_fifos();
                    destroy_src();
                    deleteEQFilters(g_rxUserdata);
                    delete g_rxUserdata;
                    m_RxRunning = false;
                    Pa_Terminate();
                    return;
                }
            }
        }

        if (g_verbose) fprintf(stderr, "starting tx/rx processing thread\n");

        // start tx/rx processing thread

        m_txRxThread = new txRxThread;

        if ( m_txRxThread->Create() != wxTHREAD_NO_ERROR )
        {
            wxLogError(wxT("Can't create thread!"));
        }

        if (wxGetApp().m_txRxThreadHighPriority) {
            m_txRxThread->SetPriority(WXTHREAD_MAX_PRIORITY);
        }

        if ( m_txRxThread->Run() != wxTHREAD_NO_ERROR )
        {
            wxLogError(wxT("Can't start thread!"));
        }

    }
}


//---------------------------------------------------------------------------------------------
// Main real time procesing for tx and rx of FreeDV signals, run in its own thread
//---------------------------------------------------------------------------------------------

void txRxProcessing()
{
    wxStopWatch sw;

    paCallBackData  *cbData = g_rxUserdata;

    // Buffers re-used by tx and rx processing.  We take samples from
    // the sound card, and resample them for the freedv modem input
    // sample rate.  Typically the sound card is running at 48 or 44.1
    // kHz, and the modem at 8kHz, however some modems such as FreeDV
    // 2400A/B and the horus baloon telemetry run at 48 kHz.

    // allocate enough room for 20ms processing buffers at maximum
    // sample rate of 48 kHz.  Note these buffer are used by rx and tx
    // side processing

    short           infreedv[10*N48];
    short           insound_card[10*N48];
    short           outfreedv[10*N48];
    short           outsound_card[10*N48];
    int             nout, freedv_samplerate;
    int             nfreedv;

    // analog mode runs at the standard FS = 8000 Hz

    if (g_analog) {
        freedv_samplerate = FS;
    }
    else {
        // Use the maximum modem sample rate. Any needed downconversion
        // just prior to sending to Codec2 will happen in FreeDVInterface.
        freedv_samplerate = freedvInterface.getRxModemSampleRate();
        //fprintf(stderr, "sample rate: %d\n", freedv_samplerate);
    }

    //
    //  RX side processing --------------------------------------------
    //
    
    if (g_queueResync)
    {
        if (g_verbose) fprintf(stderr, "Unsyncing per user request.\n");
        g_queueResync = false;
        freedvInterface.setSync(FREEDV_SYNC_UNSYNC);
        g_resyncs++;
    }
    
    // while we have enough input samples available ...

    // attempt to read one processing buffer of receive samples
    // (20ms), number of samples is scaled for the sound card sample
    // rate, so we get the right number of samples for the output
    // decoded audio

    int nsam = g_soundCard1SampleRate * (float)g_modemInbufferSize/freedv_samplerate;
    assert(nsam <= 10*N48);
    assert(nsam != 0);
    while ((codec2_fifo_read(cbData->infifo1, insound_card, nsam) == 0) && ((g_half_duplex && !g_tx) || !g_half_duplex)) {

        /* convert sound card sample rate FreeDV input sample rate */

        nfreedv = resample(cbData->insrc1, infreedv, insound_card, freedv_samplerate, g_soundCard1SampleRate, N48, nsam);
        assert(nfreedv <= N48);

        // optionally save "from radio" signal (write demod input to file) ----------------------------
        // Really useful for testing and development as it allows us
        // to repeat tests using off air signals

        if (g_recFileFromRadio && (g_sfRecFile != NULL)) {
            //printf("g_recFromRadioSamples: %d  n8k: %d \n", g_recFromRadioSamples);
            if (g_recFromRadioSamples < (unsigned)nfreedv) {
                sf_write_short(g_sfRecFile, infreedv, g_recFromRadioSamples);
                // call stop/start record menu item, should be thread safe
                g_parent->CallAfter(&MainFrame::StopRecFileFromRadio);
                g_recFromRadioSamples = 0;
            }
            else {
                sf_write_short(g_sfRecFile, infreedv, nfreedv);
                g_recFromRadioSamples -= nfreedv;
            }
        }

        // optionally read "from radio" signal from file (read demod input from file) -----------------

        if (g_playFileFromRadio && (g_sfPlayFileFromRadio != NULL)) {
            unsigned int nsf = nfreedv*g_sfFs/freedv_samplerate;
            short        insf[nsf];
            unsigned int n = sf_read_short(g_sfPlayFileFromRadio, insf, nsf);
            //fprintf(stderr, "resample %d to %d\n", g_sfFs, freedv_samplerate);
            nfreedv = resample(cbData->insrcsf, infreedv, insf, freedv_samplerate, g_sfFs, N48, nsf);
            assert(nfreedv <= N48);

            if (n == 0) {
                if (g_loopPlayFileFromRadio)
                    sf_seek(g_sfPlayFileFromRadio, 0, SEEK_SET);
                else {
                    printf("playFileFromRadio finished, issuing event!\n");
                    g_parent->CallAfter(&MainFrame::StopPlaybackFileFromRadio);
                }
            }
        }

        resample_for_plot(g_plotDemodInFifo, infreedv, nfreedv, freedv_samplerate);

        // send latest squelch level to FreeDV API, as it handles squelch internally
        freedvInterface.setSquelch(g_SquelchActive, g_SquelchLevel);

        // Optional tone interferer -----------------------------------------------------

        if (wxGetApp().m_tone) {
            float w = 2.0*M_PI*wxGetApp().m_tone_freq_hz/freedv_samplerate;
            float s;
            int i;
            for(i=0; i<nfreedv; i++) {
                s = (float)wxGetApp().m_tone_amplitude*cos(g_tone_phase);
                infreedv[i] += (int)s;
                g_tone_phase += w;
                //fprintf(stderr, "%f\n", s);
            }
            g_tone_phase -= 2.0*M_PI*floor(g_tone_phase/(2.0*M_PI));
        }

        // compute rx spectrum - do here so update rate is constant across modes -------

        // if necc, resample to Fs = 8kHz for spectrum and waterfall
        // TODO: for some future modes (like 2400A), it might be
        // useful to have different Fs spectrum

        COMP  rx_fdm[nfreedv];
        float rx_spec[MODEM_STATS_NSPEC];
        int i, nspec;
        for(i=0; i<nfreedv; i++) {
            rx_fdm[i].real = infreedv[i];
        }
        if (freedv_samplerate == FS) {
            for(i=0; i<nfreedv; i++) {
                rx_fdm[i].real = infreedv[i];
            }
            nspec = nfreedv;
        } else {
            int   nfreedv_8kHz = nfreedv*FS/freedv_samplerate;
            short infreedv_8kHz[nfreedv_8kHz];
            nout = resample(g_spec_src, infreedv_8kHz, infreedv, FS, freedv_samplerate, nfreedv_8kHz, nfreedv);
            //fprintf(stderr, "resampling, nfreedv: %d nout: %d nfreedv_8kHz: %d \n", nfreedv, nout, nfreedv_8kHz);
            assert(nout <= nfreedv_8kHz);
            for(i=0; i<nout; i++) {
                rx_fdm[i].real = infreedv_8kHz[i];
            }
            nspec = nout;
        }

        modem_stats_get_rx_spectrum(&g_stats, rx_spec, rx_fdm, nspec);

        // Average rx spectrum data using a simple IIR low pass filter

        for(i = 0; i<MODEM_STATS_NSPEC; i++) {
            g_avmag[i] = BETA * g_avmag[i] + (1.0 - BETA) * rx_spec[i];
        }

        // Get some audio to send to headphones/speaker.  If in analog
        // mode we pass thru the "from radio" audio to the
        // headphones/speaker.

        if (g_analog) {
            memcpy(outfreedv, infreedv, sizeof(short)*nfreedv);
        }
        else {
            // Write 20ms chunks of input samples for modem rx processing
            g_State = freedvInterface.processRxAudio(
                infreedv, nfreedv, cbData->rxoutfifo, g_channel_noise, wxGetApp().m_noise_snr, 
                g_RxFreqOffsetHz, &g_stats, &g_sig_pwr_av);
  
            // Read 20ms chunk of samples from modem rx processing,
            // this will typically be decoded output speech, and is
            // (currently at least) fixed at a sample rate of 8 kHz

            memset(outfreedv, 0, sizeof(short)*g_speechOutbufferSize);
            codec2_fifo_read(cbData->rxoutfifo, outfreedv, g_speechOutbufferSize);
        }

        // Optional Spk Out EQ Filtering, need mutex as filter can change at run time from another thread

        g_mutexProtectingCallbackData.Lock();
        if (cbData->spkOutEQEnable) {
            sox_biquad_filter(cbData->sbqSpkOutBass,   outfreedv, outfreedv, g_speechOutbufferSize);
            sox_biquad_filter(cbData->sbqSpkOutTreble, outfreedv, outfreedv, g_speechOutbufferSize);
            sox_biquad_filter(cbData->sbqSpkOutMid,    outfreedv, outfreedv, g_speechOutbufferSize);
        }
        g_mutexProtectingCallbackData.Unlock();

        resample_for_plot(g_plotSpeechOutFifo, outfreedv, g_speechOutbufferSize, freedvInterface.getRxSpeechSampleRate());

        // resample to output sound card rate

        if (g_nSoundCards == 1) {
            if (g_analog) /* special case */
                nout = resample(cbData->outsrc2, outsound_card, outfreedv, g_soundCard1SampleRate, freedvInterface.getRxModemSampleRate(), N48, nfreedv);
            else
                nout = resample(cbData->outsrc2, outsound_card, outfreedv, g_soundCard1SampleRate, freedvInterface.getRxSpeechSampleRate(), N48, g_speechOutbufferSize);
            codec2_fifo_write(cbData->outfifo1, outsound_card, nout);
        }
        else {
            if (g_analog) /* special case */
                nout = resample(cbData->outsrc2, outsound_card, outfreedv, g_soundCard2SampleRate, freedvInterface.getRxModemSampleRate(), N48, nfreedv);
            else
                nout = resample(cbData->outsrc2, outsound_card, outfreedv, g_soundCard2SampleRate, freedvInterface.getRxSpeechSampleRate(), N48, g_speechOutbufferSize);
            codec2_fifo_write(cbData->outfifo2, outsound_card, nout);
        }
    }

    //
    //  TX side processing --------------------------------------------
    //

    if (((g_nSoundCards == 2) && ((g_half_duplex && g_tx) || !g_half_duplex))) {
        // Lock the mode mutex so that TX state doesn't change on us during processing.
        txModeChangeMutex.Lock();
        
        // This while loop locks the modulator to the sample rate of
        // sound card 1.  We want to make sure that modulator samples
        // are uninterrupted by differences in sample rate between
        // this sound card and sound card 2.

        // Run code inside this while loop as soon as we have enough
        // room for one frame of modem samples.  Aim is to keep
        // outfifo1 nice and full so we don't have any gaps in tx
        // signal.

        unsigned int nsam_one_modem_frame = g_soundCard2SampleRate * freedvInterface.getTxNNomModemSamples()/freedv_samplerate;

     	if (g_dump_fifo_state) {
    	  // If this drops to zero we have a problem as we will run out of output samples
    	  // to send to the sound driver via PortAudio
    	  if (g_verbose) fprintf(stderr, "outfifo1 used: %6d free: %6d nsam_one_modem_frame: %d\n",
                      codec2_fifo_used(cbData->outfifo1), codec2_fifo_free(cbData->outfifo1), nsam_one_modem_frame);
    	}

        int nsam_in_48 = g_soundCard2SampleRate * freedvInterface.getTxNumSpeechSamples()/freedvInterface.getTxSpeechSampleRate();
        assert(nsam_in_48 < 10*N48);
        while((unsigned)codec2_fifo_free(cbData->outfifo1) >= nsam_one_modem_frame) {

            // OK to generate a frame of modem output samples we need
            // an input frame of speech samples from the microphone.

            // infifo2 is written to by another sound card so it may
            // over or underflow, but we don't really care.  It will
            // just result in a short interruption in audio being fed
            // to codec2_enc, possibly making a click every now and
            // again in the decoded audio at the other end.

            // zero speech input just in case infifo2 underflows
            memset(insound_card, 0, nsam_in_48*sizeof(short));
            int nread = codec2_fifo_read(cbData->infifo2, insound_card, nsam_in_48);

            // special case - if we are ending Tx then only process if we have samples available from mic
            if (endingTx && nread != 0)
            {
                if (codec2_fifo_used(cbData->infifo2) == 0) break;
                
                short* tmp = (short*)&insound_card;
                while (codec2_fifo_used(cbData->infifo2) > 0)
                {
                    codec2_fifo_read(cbData->infifo2, tmp++, 1);
                }
            }

            nout = resample(cbData->insrc2, infreedv, insound_card, freedvInterface.getTxSpeechSampleRate(), g_soundCard2SampleRate, 10*N48, nsam_in_48);

            // optionally use file for mic input signal
            if (g_playFileToMicIn && (g_sfPlayFile != NULL)) {
                int n = sf_read_short(g_sfPlayFile, infreedv, nout);
                //fprintf(stderr, "n: %d nout: %d\n", n, nout);
                if (n != nout) {
                    if (g_loopPlayFileToMicIn)
                        sf_seek(g_sfPlayFile, 0, SEEK_SET);
                    else {
                        // call stop/start play menu item, should be thread safe
                        g_parent->CallAfter(&MainFrame::StopPlayFileToMicIn);
                    }
                }
            }

            // Optional Speex pre-processor for acoustic noise reduction
            if (wxGetApp().m_speexpp_enable) {
                speex_preprocess_run(g_speex_st, infreedv);
            }

            // Optional Mic In EQ Filtering, need mutex as filter can change at run time

            g_mutexProtectingCallbackData.Lock();
            if (cbData->micInEQEnable) {
                sox_biquad_filter(cbData->sbqMicInBass, infreedv, infreedv, nout);
                sox_biquad_filter(cbData->sbqMicInTreble, infreedv, infreedv, nout);
                sox_biquad_filter(cbData->sbqMicInMid, infreedv, infreedv, nout);
            }
            g_mutexProtectingCallbackData.Unlock();

            resample_for_plot(g_plotSpeechInFifo, infreedv, nout, freedvInterface.getTxSpeechSampleRate());

            nfreedv = freedvInterface.getTxNNomModemSamples();

            if (g_analog) {
                nfreedv = freedvInterface.getTxNumSpeechSamples();

                // Boost the "from mic" -> "to radio" audio in analog
                // mode.  The need for the gain was found by
                // experiment - analog SSB sounded too quiet compared
                // to digital. With digital voice we generally drive
                // the "to radio" (SSB radio mic input) at about 25%
                // of the peak level for normal SSB voice. So we
                // introduce 6dB gain to make analog SSB sound the
                // same level as the digital.  Watch out for clipping.
                for(int i=0; i<nfreedv; i++) {
                    float out = (float)infreedv[i]*2.0;
                    if (out > 32767) out = 32767.0;
                    if (out < -32767) out = -32767.0;
                    outfreedv[i] = out;
                }
            }
            else {
                if (g_mode == FREEDV_MODE_800XA || g_mode == FREEDV_MODE_2400B) {
                    /* 800XA doesn't support complex output just yet */
                    freedvInterface.transmit(outfreedv, infreedv);
                }
                else {
                    freedvInterface.complexTransmit(outfreedv, infreedv, g_TxFreqOffsetHz, nfreedv);
                }
            }

            // Save modulated output file if requested
            if (g_recFileFromModulator && (g_sfRecFileFromModulator != NULL)) {
                if (g_recFromModulatorSamples < nfreedv) {
                    sf_write_short(g_sfRecFileFromModulator, outfreedv, g_recFromModulatorSamples);  // try infreedv to bypass codec and modem, was outfreedv
                    
                    // call stop record menu item, should be thread safe
                    g_parent->CallAfter(&MainFrame::StopRecFileFromModulator);
                    
                    wxPrintf("write mod output to file complete\n", g_recFromModulatorSamples);  // consider a popup
                }
                else {
                    sf_write_short(g_sfRecFileFromModulator, outfreedv, nfreedv);
                    g_recFromModulatorSamples -= nfreedv;
                }
            }
            
            // output one frame of modem signal

            if (g_analog)
                nout = resample(cbData->outsrc1, outsound_card, outfreedv, g_soundCard1SampleRate, freedvInterface.getTxSpeechSampleRate(), 10*N48, nfreedv);
            else
                nout = resample(cbData->outsrc1, outsound_card, outfreedv, g_soundCard1SampleRate, freedvInterface.getTxModemSampleRate(), 10*N48, nfreedv);
            
            // Attenuate signal prior to output
            double dbLoss = g_txLevel / 10.0;
            double scaleFactor = exp(dbLoss/20.0 * log(10.0));
            
            for (int i = 0; i < nout; i++)
            {
                outsound_card[i] *= scaleFactor;
            }
            
            if (g_dump_fifo_state) {
                fprintf(stderr, "  nout: %d\n", nout);
            }
            codec2_fifo_write(cbData->outfifo1, outsound_card, nout);
        }
        
        txModeChangeMutex.Unlock();
    }

    if (g_dump_timing) {
        fprintf(stderr, "%4ld", sw.Time());
    }
}

//-------------------------------------------------------------------------
// rxCallback()
//
// Sound card 1 callback from PortAudio, that is used for processing rx
// side:
//
// + infifo1 is the "from radio" off air modem signal from the SSB rx that we send to the demod.
// + In single sound card mode outfifo1 is the "to speaker/headphones" decoded speech output.
// + In dual sound card mode outfifo1 is the "to radio" modulator signal to the SSB tx.
//
//-------------------------------------------------------------------------

int MainFrame::rxCallback(
                            const void      *inputBuffer,
                            void            *outputBuffer,
                            unsigned long   framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void            *userData
                         )
{
    paCallBackData  *cbData = (paCallBackData*)userData;
    short           *rptr    = (short*)inputBuffer;
    short           *wptr    = (short*)outputBuffer;

    short           indata[MAX_FPB];
    short           outdata[MAX_FPB];

    unsigned int    i;

    (void) timeInfo;
    (void) statusFlags;

    if (statusFlags & 0x1) {  // input underflow
        g_PAstatus1[0]++;
    }
    if (statusFlags & 0x2) {  // input overflow
        g_PAstatus1[1]++;
    }
    if (statusFlags & 0x4) {  // output underflow
        g_PAstatus1[2]++;
    }
    if (statusFlags & 0x8) {  // output overflow
        g_PAstatus1[3]++;
    }

    g_PAframesPerBuffer1 = framesPerBuffer;

    //
    //  RX side processing --------------------------------------------
    //

    // assemble a mono buffer and write to FIFO

    assert(framesPerBuffer < MAX_FPB);

    if (rptr) {
        for(i = 0; i < framesPerBuffer; i++, rptr += cbData->inputChannels1)
            indata[i] = rptr[0];
        if (codec2_fifo_write(cbData->infifo1, indata, framesPerBuffer)) {
            g_infifo1_full++;
        }
    }

    // OK now set up output samples for this callback

    if (wptr) {
         if (codec2_fifo_read(cbData->outfifo1, outdata, framesPerBuffer) == 0) {

            // write signal to both channels if the device can support two channels.
            // Otherwise, we assume we're only dealing with one channel and write
            // only to that channel.
            if (cbData->outputChannels1 == 2)
            {
                for(i = 0; i < framesPerBuffer; i++, wptr += 2) 
                {
                    if (cbData->leftChannelVoxTone)
                    {
                        cbData->voxTonePhase += 2.0*M_PI*VOX_TONE_FREQ/g_soundCard1SampleRate;
                        cbData->voxTonePhase -= 2.0*M_PI*floor(cbData->voxTonePhase/(2.0*M_PI));
                        wptr[0] = VOX_TONE_AMP*cos(cbData->voxTonePhase);
                    }
                    else
                        wptr[0] = outdata[i];

                    wptr[1] = outdata[i];
                }
            }
            else
            {
                for(i = 0; i < framesPerBuffer; i++, wptr++) 
                {
                    wptr[0] = outdata[i];
                }
            }
        }
        else 
        {
            g_outfifo1_empty++;
            
            // zero output if no data available
            if (cbData->outputChannels1 == 2)
            {
                for(i = 0; i < framesPerBuffer; i++, wptr += 2) {
                    wptr[0] = 0;
                    wptr[1] = 0;
                }
            }
            else
            {
                for(i = 0; i < framesPerBuffer; i++, wptr++)
                {
                    wptr[0] = 0;
                }
            }
        }
    }

    return paContinue;
}


//-------------------------------------------------------------------------
// txCallback()
//-------------------------------------------------------------------------
int MainFrame::txCallback(
                            const void *inputBuffer,
                            void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo *outTime,
                            PaStreamCallbackFlags statusFlags,
                            void *userData
                        )
{
    paCallBackData  *cbData = (paCallBackData*)userData;
    unsigned int    i;
    short           *rptr    = (short*)inputBuffer;
    short           *wptr    = (short*)outputBuffer;
    short           indata[MAX_FPB];
    short           outdata[MAX_FPB];

    if (statusFlags & 0x1) { // input underflow
        g_PAstatus2[0]++;
    }
    if (statusFlags & 0x2) { // input overflow
        g_PAstatus2[1]++;
    }
    if (statusFlags & 0x4) { // output underflow
        g_PAstatus2[2]++;
    }
    if (statusFlags & 0x8) { // output overflow
        g_PAstatus2[3]++;
    }

    g_PAframesPerBuffer2 = framesPerBuffer;

    // assemble a mono buffer and write to FIFO

    assert(framesPerBuffer < MAX_FPB);

    if (rptr && !endingTx) {
        for(i = 0; i < framesPerBuffer; i++, rptr += cbData->inputChannels2)
            indata[i] = rptr[0];
        if (codec2_fifo_write(cbData->infifo2, indata, framesPerBuffer)) {
            g_infifo2_full++;
        }
    }

    // OK now set up output samples for this callback

    if (wptr) {
        if (codec2_fifo_read(cbData->outfifo2, outdata, framesPerBuffer) == 0) {

            // write signal to both channels */
            for(i = 0; i < framesPerBuffer; i++, wptr += 2) {
                wptr[0] = outdata[i];
                wptr[1] = outdata[i];
            }
        }
        else {
            g_outfifo2_empty++;
            // zero output if no data available
            for(i = 0; i < framesPerBuffer; i++, wptr += 2) {
                wptr[0] = 0;
                wptr[1] = 0;
            }
        }
    }

    return paContinue;
}

int MainFrame::getSoundCardIDFromName(wxString& name, bool input)
{
    int result = -1;
    
    if (name != "none")
    {
        PaError paResult = Pa_Initialize();
        if (paResult == paNoError)
        {
            for (PaDeviceIndex index = 0; index < Pa_GetDeviceCount(); index++)
            {
                const PaDeviceInfo* device = Pa_GetDeviceInfo(index);
                wxString deviceName = device->name;
                deviceName = deviceName.Trim();
                if (name == deviceName)
                {
                    PaStreamParameters baseParams;
                    baseParams.device = index;
                    baseParams.channelCount = input ? device->maxInputChannels : device->maxOutputChannels;
                    baseParams.sampleFormat = paInt16;
                    baseParams.suggestedLatency = 0;
                    baseParams.hostApiSpecificStreamInfo = NULL;
                    
                    if (baseParams.channelCount == 0) continue;
                    
                    bool supported = false;
                    for(int sampleIndex = 0; PortAudioWrap::standardSampleRates[sampleIndex] > 0; sampleIndex++)
                    {
                        paResult = Pa_IsFormatSupported(input ? &baseParams : NULL, !input ? &baseParams : NULL, PortAudioWrap::standardSampleRates[sampleIndex]);
                        if (paResult == paFormatIsSupported)
                        {
                            supported = true;
                            break;
                        }
                    }
                    
                    if (supported)
                    {
                        result = index;
                        break;
                    }
                }
            }
        }
        else
        {
            fprintf(stderr, "WARNING: could not initialize PortAudio (err=%d, txt=%s)\n", paResult, Pa_GetErrorText(paResult));
        }
        Pa_Terminate();
    }
    return result;
}

bool MainFrame::validateSoundCardSetup()
{
    bool canRun = true;
    
    // Translate device names to IDs
    g_soundCard1InDeviceNum = getSoundCardIDFromName(wxGetApp().m_soundCard1InDeviceName, true);
    g_soundCard1OutDeviceNum = getSoundCardIDFromName(wxGetApp().m_soundCard1OutDeviceName, false);
    g_soundCard2InDeviceNum = getSoundCardIDFromName(wxGetApp().m_soundCard2InDeviceName, true);
    g_soundCard2OutDeviceNum = getSoundCardIDFromName(wxGetApp().m_soundCard2OutDeviceName, false);

    if (wxGetApp().m_soundCard1InDeviceName != "none" && g_soundCard1InDeviceNum == -1)
    {
        wxMessageBox(wxString::Format(
            "Your %s device cannot be found and may have been removed from your system. Please go to Tools->Audio Config... to confirm your audio setup.", 
            wxGetApp().m_soundCard1InDeviceName), wxT("Sound Device Removed"), wxOK, this);
        canRun = false;
    }
    else if (canRun && wxGetApp().m_soundCard1OutDeviceName != "none" && g_soundCard1OutDeviceNum == -1)
    {
        wxMessageBox(wxString::Format(
            "Your %s device cannot be found and may have been removed from your system. Please go to Tools->Audio Config... to confirm your audio setup.", 
            wxGetApp().m_soundCard1OutDeviceName), wxT("Sound Device Removed"), wxOK, this);
        canRun = false;
    }
    else if (canRun && wxGetApp().m_soundCard2InDeviceName != "none" && g_soundCard2InDeviceNum == -1)
    {
        wxMessageBox(wxString::Format(
            "Your %s device cannot be found and may have been removed from your system. Please go to Tools->Audio Config... to confirm your audio setup.", 
            wxGetApp().m_soundCard2InDeviceName), wxT("Sound Device Removed"), wxOK, this);
        canRun = false;
    }
    else if (canRun && wxGetApp().m_soundCard2OutDeviceName != "none" && g_soundCard2OutDeviceNum == -1)
    {
        wxMessageBox(wxString::Format(
            "Your %s device cannot be found and may have been removed from your system. Please go to Tools->Audio Config... to confirm your audio setup.", 
            wxGetApp().m_soundCard2OutDeviceName), wxT("Sound Device Removed"), wxOK, this);
        canRun = false;
    }
    
    g_nSoundCards = 0;
    if ((g_soundCard1InDeviceNum > -1) && (g_soundCard1OutDeviceNum > -1)) {
        g_nSoundCards = 1;
        if ((g_soundCard2InDeviceNum > -1) && (g_soundCard2OutDeviceNum > -1))
            g_nSoundCards = 2;
    }
    
    if (canRun && g_nSoundCards == 0)
    {
        // Initial setup. Remind user to configure sound cards first.
        wxMessageBox(wxString("It looks like this is your first time running FreeDV. Please go to Tools->Audio Config... to choose your sound card(s) before using."), wxT("First Time Setup"), wxOK, this);
        canRun = false;
    }

    return canRun;
}


#ifdef __UDP_SUPPORT__

//----------------------------------------------------------------
// PollUDP() - see if any commands on UDP port
//----------------------------------------------------------------

// test this puppy with netcat:
//   $ echo "hello" | nc -u -q1 localhost 3000

int MainFrame::PollUDP(void)
{
    // this will block until message received, so we put it in it's own thread

    char buf[1024];
    char reply[80];
    size_t n = m_udp_sock->RecvFrom(m_udp_addr, buf, sizeof(buf)).LastCount();

    if (n) {
        wxString bufstr = wxString::From8BitData(buf, n);
        bufstr.Trim();
        wxString ipaddr = m_udp_addr.IPAddress();
        printf("Received: \"%s\" from %s:%u\n",
               (const char *)bufstr.c_str(),
               (const char *)ipaddr.c_str(), m_udp_addr.Service());

        // for security only accept commands from local host

        sprintf(reply,"nope\n");
        if (ipaddr.Cmp(_("127.0.0.1")) == 0) {

            // process commands

            if (bufstr.Cmp(_("restore")) == 0) {
                m_schedule_restore = true;  // Make Restore happen in main thread to avoid crashing
                sprintf(reply,"ok\n");
            }

            wxString itemToSet, val;
            if (bufstr.StartsWith(_("set "), &itemToSet)) {
                if (itemToSet.StartsWith("txtmsg ", &val)) {
                    // note: if options dialog is open this will get overwritten
                    wxGetApp().m_callSign = val;
                }
                sprintf(reply,"ok\n");
            }
            if (bufstr.StartsWith(_("ptton"), &itemToSet)) {
                // note: if options dialog is open this will get overwritten
                m_btnTogPTT->SetValue(true);
                togglePTT();
                sprintf(reply,"ok\n");
            }
            if (bufstr.StartsWith(_("pttoff"), &itemToSet)) {
                // note: if options dialog is open this will get overwritten
                m_btnTogPTT->SetValue(false);
                togglePTT();
                sprintf(reply,"ok\n");
            }

        }
        else {
            printf("We only accept messages from locahost!\n");
        }

       if ( m_udp_sock->SendTo(m_udp_addr, reply, strlen(reply)).LastCount() != strlen(reply)) {
           printf("ERROR: failed to send data\n");
        }
    }

    return n;
}

void MainFrame::startUDPThread(void) {
    fprintf(stderr, "starting UDP thread!\n");
    m_UDPThread = new UDPThread;
    m_UDPThread->mf = this;
    if (m_UDPThread->Create() != wxTHREAD_NO_ERROR ) {
        wxLogError(wxT("Can't create thread!"));
    }
    if (m_UDPThread->Run() != wxTHREAD_NO_ERROR ) {
        wxLogError(wxT("Can't start thread!"));
        delete m_UDPThread;
    }
}

void MainFrame::stopUDPThread(void) {
    printf("stopping UDP thread!\n");
    if ((m_UDPThread != NULL) && m_UDPThread->m_run) {
        m_UDPThread->m_run = 0;
        m_UDPThread->Wait();
        m_UDPThread = NULL;
    }
}

void *UDPThread::Entry() {
    //fprintf(stderr, "UDP thread started!\n");
    while (m_run) {
        if (wxGetApp().m_udp_enable) {
            printf("m_udp_enable\n");
            mf->m_udp_addr.Service(wxGetApp().m_udp_port);
            mf->m_udp_sock = new wxDatagramSocket(mf->m_udp_addr, wxSOCKET_NOWAIT);

            while (m_run && wxGetApp().m_udp_enable) {
                if (mf->PollUDP() == 0) {
                    wxThread::Sleep(20);
                }
            }

            delete mf->m_udp_sock;
        }
        wxThread::Sleep(20);
    }
    return NULL;
}

#endif
