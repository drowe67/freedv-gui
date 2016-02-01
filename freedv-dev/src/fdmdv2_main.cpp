//==========================================================================
// Name:            fdmdv2_main.cpp
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

#include "fdmdv2_main.h"

#define wxUSE_FILEDLG   1
#define wxUSE_LIBPNG    1
#define wxUSE_LIBJPEG   1
#define wxUSE_GIF       1
#define wxUSE_PCX       1
#define wxUSE_LIBTIFF   1

//-------------------------------------------------------------------
// Bunch of globals used for communication with sound card call
// back functions
// ------------------------------------------------------------------

int g_in, g_out;

// Global Codec2 & modem states - just one reqd for tx & rx
int                 g_Nc;
int                 g_mode;
struct freedv      *g_pfreedv;
struct MODEM_STATS  g_stats;
float               g_pwr_scale;
int                 g_clip;

// test Frames
int                 g_testFrames;
int                 g_test_frame_sync_state;
int                 g_test_frame_count;
int                 g_total_bits;
int                 g_total_bit_errors;
int                 g_channel_noise;
float               g_sig_pwr_av = 0.0;
struct FIFO        *g_error_pattern_fifo;
short              *g_error_hist;
 
// time averaged magnitude spectrum used for waterfall and spectrum display
float               g_avmag[MODEM_STATS_NSPEC];

// GUI controls that affect rx and tx processes
int   g_SquelchActive;
float g_SquelchLevel;
int   g_analog;
int   g_split;
int   g_tx;
float g_snr;
bool  g_half_duplex;
bool  g_modal;

// sending and receiving Call Sign data
struct FIFO         *g_txDataInFifo;
struct FIFO         *g_rxDataOutFifo;

// tx/rx processing states
int                 g_State;
paCallBackData     *g_rxUserdata;

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

// playing and recording from sound files

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
bool                g_loopPlayFileFromRadio;
int                 g_playFileFromRadioEventId;
float               g_blink;

wxWindow           *g_parent;

// Click to tune rx and tx frequency offset states
float               g_RxFreqOffsetHz;
COMP                g_RxFreqOffsetPhaseRect;
float               g_TxFreqOffsetHz;
COMP                g_TxFreqOffsetPhaseRect;

// experimental mutex to make sound card callbacks mutually exclusive
// TODO: review code and see if we need this any more, as fifos should
// now be thread safe

wxMutex g_mutexProtectingCallbackData;

// Speex pre-processor states

SpeexPreprocessState *g_speex_st;

// WxWidgets - initialize the application
IMPLEMENT_APP(MainApp);

//FILE *ft;
FILE *g_logfile;

//-------------------------------------------------------------------------
// OnInit()
//-------------------------------------------------------------------------
bool MainApp::OnInit()
{
    if(!wxApp::OnInit())
    {
        return false;
    }
    SetVendorName(wxT("CODEC2-Project"));
    SetAppName(wxT("FreeDV"));      // not needed, it's the default value

    // DR - this is wrong define so won't be used on windows build, should be __WXMSW__
    // So registry will be used for Win32
#ifdef _WXMSW_
    // Force use of file-based configuration persistance on Windows platforma
    wxConfig *pConfig = new wxConfig();
    wxFileConfig *pFConfig = new wxFileConfig(wxT("FreeDV"), wxT("CODEC2-Project"), wxT("FreeDV.conf"), wxT("FreeDV.conf"),  wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_RELATIVE_PATH);
    pConfig->Set(pFConfig);
    pConfig->SetRecordDefaults();
#else
    wxConfigBase *pConfig = wxConfigBase::Get();
    pConfig->SetRecordDefaults();
#endif

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

    //ft = fopen("tmp.raw","wb");
    //assert(ft != NULL);

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
    g_logfile = fopen("log.txt","wt");
    #else
    g_logfile = stderr;
    #endif

    SetMinSize(wxSize(400,400));

    // Init Hamlib library, but we dont start talking to any rigs yet

    wxGetApp().m_hamlib = new Hamlib();

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
    int h = pConfig->Read(wxT("/MainFrame/height"),    550);

    // sanitise frame position as a first pass at Win32 registry bug

    fprintf(g_logfile, "x = %d y = %d w = %d h = %d\n", x,y,w,h);
    if (x < 0 || x > 2048) x = 20;
    if (y < 0 || y > 2048) y = 20;
    if (w < 0 || w > 2048) w = 800;
    if (h < 0 || h > 2048) h = 550;

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
        m_panelWaterfall->SetToolTip(_("Left click to tune, Right click to toggle mono/colour"));
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
        g_plotDemodInFifo = fifo_create(2*WAVEFORM_PLOT_BUF);
    }

    if(wxGetApp().m_show_speech_in)
    {
        // Add Speech Input window
        m_panelSpeechIn = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, WAVEFORM_PLOT_TIME, 1.0/WAVEFORM_PLOT_FS, -1, 1, 1, 0.2, "%2.1f", 0);
        m_auiNbookCtrl->AddPage(m_panelSpeechIn, _("Frm Mic"), true, wxNullBitmap);
        g_plotSpeechInFifo = fifo_create(4*WAVEFORM_PLOT_BUF);
    }

    if(wxGetApp().m_show_speech_out)
    {
        // Add Speech Output window
        m_panelSpeechOut = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, WAVEFORM_PLOT_TIME, 1.0/WAVEFORM_PLOT_FS, -1, 1, 1, 0.2, "%2.1f", 0);
        m_auiNbookCtrl->AddPage(m_panelSpeechOut, _("To Spkr/Hdphns"), true, wxNullBitmap);
        g_plotSpeechOutFifo = fifo_create(2*WAVEFORM_PLOT_BUF);
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
        // Add Test Frame Errors window
        m_panelTestFrameErrorsHist = new PlotScalar((wxFrame*) m_auiNbookCtrl, 1, 1.0, 1.0/(2*FDMDV_NC_MAX), 0.0, 1.0, 1.0/FDMDV_NC_MAX, 0.1, "%3.2f", 0);
        m_auiNbookCtrl->AddPage(m_panelTestFrameErrorsHist, L"Test Frame Histogram", true, wxNullBitmap);
    }

    wxGetApp().m_framesPerBuffer = pConfig->Read(wxT("/Audio/framesPerBuffer"), PA_FPB);

    g_soundCard1InDeviceNum  = pConfig->Read(wxT("/Audio/soundCard1InDeviceNum"),         -1);
    g_soundCard1OutDeviceNum = pConfig->Read(wxT("/Audio/soundCard1OutDeviceNum"),        -1);
    g_soundCard1SampleRate   = pConfig->Read(wxT("/Audio/soundCard1SampleRate"),          -1);

    g_soundCard2InDeviceNum  = pConfig->Read(wxT("/Audio/soundCard2InDeviceNum"),         -1);
    g_soundCard2OutDeviceNum = pConfig->Read(wxT("/Audio/soundCard2OutDeviceNum"),        -1);
    g_soundCard2SampleRate   = pConfig->Read(wxT("/Audio/soundCard2SampleRate"),          -1);

    g_nSoundCards = 0;
    if ((g_soundCard1InDeviceNum > -1) && (g_soundCard1OutDeviceNum > -1)) {
        g_nSoundCards = 1;
        if ((g_soundCard2InDeviceNum > -1) && (g_soundCard2OutDeviceNum > -1))
            g_nSoundCards = 2;
    }

    wxGetApp().m_playFileToMicInPath = pConfig->Read("/File/playFileToMicInPath",   wxT(""));
    wxGetApp().m_recFileFromRadioPath = pConfig->Read("/File/recFileFromRadioPath", wxT(""));
    wxGetApp().m_recFileFromRadioSecs = pConfig->Read("/File/recFileFromRadioSecs", 30);
    wxGetApp().m_playFileFromRadioPath = pConfig->Read("/File/playFileFromRadioPath", wxT(""));

    // PTT -------------------------------------------------------------------

    wxGetApp().m_boolHalfDuplex     = pConfig->ReadBool(wxT("/Rig/HalfDuplex"),     true);
    wxGetApp().m_leftChannelVoxTone = pConfig->ReadBool("/Rig/leftChannelVoxTone",  false);
 
    wxGetApp().m_txtVoiceKeyerWaveFilePath = pConfig->Read(wxT("/VoiceKeyer/WaveFilePath"), wxT(""));
    wxGetApp().m_txtVoiceKeyerWaveFile = pConfig->Read(wxT("/VoiceKeyer/WaveFile"), wxT("voicekeyer.wav"));
    wxGetApp().m_intVoiceKeyerRxPause = pConfig->Read(wxT("/VoiceKeyer/RxPause"), 10);
    wxGetApp().m_intVoiceKeyerRepeats = pConfig->Read(wxT("/VoiceKeyer/Repeats"), 5);
 
    wxGetApp().m_boolHamlibUseForPTT = pConfig->ReadBool("/Hamlib/UseForPTT", false);
    wxGetApp().m_intHamlibRig = pConfig->ReadLong("/Hamlib/RigName", 0);
    wxGetApp().m_strHamlibSerialPort = pConfig->Read("/Hamlib/SerialPort", "");
    
    wxGetApp().m_boolUseSerialPTT   = pConfig->ReadBool(wxT("/Rig/UseSerialPTT"),   false);
    wxGetApp().m_strRigCtrlPort     = pConfig->Read(wxT("/Rig/Port"),               wxT(""));
    wxGetApp().m_boolUseRTS         = pConfig->ReadBool(wxT("/Rig/UseRTS"),         true);
    wxGetApp().m_boolRTSPos         = pConfig->ReadBool(wxT("/Rig/RTSPolarity"),    true);
    wxGetApp().m_boolUseDTR         = pConfig->ReadBool(wxT("/Rig/UseDTR"),         false);
    wxGetApp().m_boolDTRPos         = pConfig->ReadBool(wxT("/Rig/DTRPolarity"),    false);
    com_handle = COM_HANDLE_INVALID;

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
    wxGetApp().m_enable_checksum = pConfig->Read("/Data/EnableChecksumOnMsgRx", f);

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

    int mode  = pConfig->Read(wxT("/Audio/mode"), (long)0);
    if (mode == 0)
        m_rb1600->SetValue(1);
    if (mode == 2)
        m_rb700b->SetValue(1);
        
    pConfig->SetPath(wxT("/"));

//    this->Connect(m_menuItemHelpUpdates->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler(TopFrame::OnHelpCheckUpdatesUI));
    //m_togRxID->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnRxIDUI), NULL, this);
    //m_togTxID->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnTxIDUI), NULL, this);
    m_togBtnOnOff->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnOnOffUI), NULL, this);
    m_togBtnSplit->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnSplitClickUI), NULL, this);
    m_togBtnAnalog->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnAnalogClickUI), NULL, this);
    //m_togBtnALC->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnALCClickUI), NULL, this);
   // m_btnTogPTT->Connect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnPTT_UI), NULL, this);

    m_togBtnSplit->Disable();
    //m_togRxID->Disable();
    //m_togTxID->Disable();
    m_togBtnAnalog->Disable();
    m_btnTogPTT->Disable();
    m_togBtnVoiceKeyer->Disable();
    //m_togBtnALC->Disable();

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

    // init click-tune states

    g_RxFreqOffsetHz = 0.0;
    g_RxFreqOffsetPhaseRect.real = cos(0.0);
    g_RxFreqOffsetPhaseRect.imag = sin(0.0);
    m_panelWaterfall->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);
    m_panelSpectrum->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);

    g_TxFreqOffsetHz = 0.0;
    g_TxFreqOffsetPhaseRect.real = cos(0.0);
    g_TxFreqOffsetPhaseRect.imag = sin(0.0);

    g_tx = 0;
    g_split = 0;

    // data states
    g_txDataInFifo = fifo_create(MAX_CALLSIGN*VARICODE_MAX_BITS);
    g_rxDataOutFifo = fifo_create(MAX_CALLSIGN*VARICODE_MAX_BITS);

    sox_biquad_start();

    g_testFrames = 0;
    g_test_frame_sync_state = 0;
    g_total_bit_errors = 0;
    g_total_bits = 0;
    wxGetApp().m_testFrames = false;

    g_modal = false;

    // Start UDP listener thread

    m_UDPThread = NULL;
    startUDPThread();

    optionsDlg = new OptionsDlg(NULL);
    m_schedule_restore = false;

    vk_state = VK_IDLE;
    
    // Look for Plug In

    m_plugIn = false;
    m_plugInHandle = dlopen("/home/david/tmp/modem_api/afreedvplugin.so", RTLD_LAZY);
    
    if (m_plugInHandle) {
        fprintf(stderr, "plugin: .so found\n");

        // lets get some information abt the plugIn

        void (*plugin_namefp)(char s[]);
        void *(*plugin_openfp)(char *param_names[], int *nparams);

        plugin_namefp = (void (*)(char*))dlsym(m_plugInHandle, "plugin_name");
        plugin_openfp = (void* (*)(char**,int *))dlsym(m_plugInHandle, "plugin_open");

        if ((plugin_namefp != NULL) && (plugin_openfp != NULL)) {

            char s[256];
            m_plugIn = true;
            (plugin_namefp)(s);
            fprintf(stderr, "plugin name: %s\n", s);
            m_plugInName = s;

            char param_name1[80], param_name2[80];
            char *param_names[2] = {param_name1, param_name2};
            int  nparams, i;
            m_plugInStates = (plugin_openfp)(param_names, &nparams);
            m_numPlugInParam = nparams;
            for(i=0; i<nparams; i++) {
                fprintf(stderr, "  plugin param name[%d]: %s\n", i, param_names[i]);
                m_plugInParamName[i] = param_names[i];
                wxString configStr = "/" + m_plugInName + "/" + m_plugInParamName[i];
                wxGetApp().m_txtPlugInParam[i] = pConfig->Read(configStr, wxT(""));
            }
        }
        else {
           fprintf(stderr, "plugin: fps not found...\n");           
        }
    }

    //m_plugIn = true;
    //m_plugInName = "MyModem";
    //m_numPlugInParam = 2;
    //m_plugInParamName[0] = "Symbol Rate";
    //m_plugInParamName[1] = "Num Tones";   
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

    if (m_plugIn)
        dlclose(m_plugInHandle);

    //fclose(ft);
    #ifdef __WXMSW__
    fclose(g_logfile);
    #endif

    if (optionsDlg != NULL) {
        delete optionsDlg;
        optionsDlg = NULL;
    }

    stopUDPThread();

    /* TOOD(Joel): the ownership of m_hamlib is probably wrong. */
    if (wxGetApp().m_hamlib) delete wxGetApp().m_hamlib;

    //MainApp *pApp = wxGetApp();
    wxConfigBase *pConfig = wxConfigBase::Get();
    if(pConfig)
    {
        if (!IsIconized()) {
            GetClientSize(&w, &h);

            // big hack - for some reason height shrinks by this much
            // every time FreeDV runs!  I have no idea why

            h += 23;

            GetPosition(&x, &y);
            printf("x = %d y = %d w = %d h = %d\n", x,y,w,h);
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

        pConfig->Write(wxT("/Audio/soundCard1InDeviceNum"),   g_soundCard1InDeviceNum);
        pConfig->Write(wxT("/Audio/soundCard1OutDeviceNum"),  g_soundCard1OutDeviceNum);
        pConfig->Write(wxT("/Audio/soundCard1SampleRate"),    g_soundCard1SampleRate );

        pConfig->Write(wxT("/Audio/soundCard2InDeviceNum"),   g_soundCard2InDeviceNum);
        pConfig->Write(wxT("/Audio/soundCard2OutDeviceNum"),  g_soundCard2OutDeviceNum);
        pConfig->Write(wxT("/Audio/soundCard2SampleRate"),    g_soundCard2SampleRate );

        pConfig->Write(wxT("/VoiceKeyer/WaveFilePath"), wxGetApp().m_txtVoiceKeyerWaveFilePath);
        pConfig->Write(wxT("/VoiceKeyer/WaveFile"), wxGetApp().m_txtVoiceKeyerWaveFile);
        pConfig->Write(wxT("/VoiceKeyer/RxPause"), wxGetApp().m_intVoiceKeyerRxPause);
        pConfig->Write(wxT("/VoiceKeyer/Repeats"), wxGetApp().m_intVoiceKeyerRepeats);

        pConfig->Write(wxT("/Rig/HalfDuplex"),              wxGetApp().m_boolHalfDuplex);
        pConfig->Write(wxT("/Rig/leftChannelVoxTone"),      wxGetApp().m_leftChannelVoxTone);
        pConfig->Write("/Hamlib/UseForPTT", wxGetApp().m_boolHamlibUseForPTT);
        pConfig->Write("/Hamlib/RigName", wxGetApp().m_intHamlibRig);
        pConfig->Write("/Hamlib/SerialPort", wxGetApp().m_strHamlibSerialPort);


        pConfig->Write(wxT("/File/playFileToMicInPath"),    wxGetApp().m_playFileToMicInPath);
        pConfig->Write(wxT("/File/recFileFromRadioPath"),   wxGetApp().m_recFileFromRadioPath);
        pConfig->Write(wxT("/File/recFileFromRadioSecs"),   wxGetApp().m_recFileFromRadioSecs);
        pConfig->Write(wxT("/File/playFileFromRadioPath"),  wxGetApp().m_playFileFromRadioPath);

        pConfig->Write(wxT("/Audio/snrSlow"), wxGetApp().m_snrSlow);

        pConfig->Write(wxT("/Data/CallSign"), wxGetApp().m_callSign);
        pConfig->Write(wxT("/Data/TextEncoding"), wxGetApp().m_textEncoding);
        pConfig->Write(wxT("/Data/EnableChecksumOnMsgRx"), wxGetApp().m_enable_checksum);
        pConfig->Write(wxT("/Events/enable"), wxGetApp().m_events);
        pConfig->Write(wxT("/Events/spam_timer"), wxGetApp().m_events_spam_timer);
        pConfig->Write(wxT("/Events/regexp_match"), wxGetApp().m_events_regexp_match);
        pConfig->Write(wxT("/Events/regexp_replace"), wxGetApp().m_events_regexp_replace);
 
        pConfig->Write(wxT("/UDP/enable"), wxGetApp().m_udp_enable);
        pConfig->Write(wxT("/UDP/port"),  wxGetApp().m_udp_port);

        pConfig->Write(wxT("/Filter/MicInEQEnable"), wxGetApp().m_MicInEQEnable);
        pConfig->Write(wxT("/Filter/SpkOutEQEnable"), wxGetApp().m_SpkOutEQEnable);

        pConfig->Write(wxT("/FreeDV700/txClip"), wxGetApp().m_FreeDV700txClip);

        int mode;
        if (m_rb1600->GetValue())
            mode = 0;
        if (m_rb700b->GetValue())
            mode = 2;
        pConfig->Write(wxT("/Audio/mode"), mode);
    }

    //m_togRxID->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnRxIDUI), NULL, this);
    //m_togTxID->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnTxIDUI), NULL, this);
    m_togBtnOnOff->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnOnOffUI), NULL, this);
    m_togBtnSplit->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnSplitClickUI), NULL, this);
    m_togBtnAnalog->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnAnalogClickUI), NULL, this);
    //m_togBtnALC->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnALCClickUI), NULL, this);
    //m_btnTogPTT->Disconnect(wxEVT_UPDATE_UI, wxUpdateUIEventHandler(MainFrame::OnTogBtnPTT_UI), NULL, this);

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
#ifdef _USE_TIMER
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
}

//----------------------------------------------------------------
// closeComPort() closes the currently open com port
//----------------------------------------------------------------
void MainFrame::closeComPort(void)
{
#ifdef _WIN32
	CloseHandle(com_handle);
#else
	close(com_handle);
#endif
	com_handle = COM_HANDLE_INVALID;
}

//----------------------------------------------------------------
// openComPort() opens the com port specified by the string
// ie: "COM1" on Windows or "/dev/ttyu0" on FreeBSD
//----------------------------------------------------------------
bool MainFrame::openComPort(const char *name)
{
	if(com_handle != COM_HANDLE_INVALID)
		closeComPort();
#ifdef _WIN32
	{
		COMMCONFIG CC;
		DWORD CCsize=sizeof(CC);
		COMMTIMEOUTS timeouts;
		DCB	dcb;

		if(GetDefaultCommConfigA(name, &CC, &CCsize)) {
			CC.dcb.fOutxCtsFlow		= FALSE;
			CC.dcb.fOutxDsrFlow		= FALSE;
			CC.dcb.fDtrControl		= DTR_CONTROL_DISABLE;
			CC.dcb.fDsrSensitivity	= FALSE;
			CC.dcb.fRtsControl		= RTS_CONTROL_DISABLE;
			SetDefaultCommConfigA(name, &CC, CCsize);
		}

		if((com_handle=CreateFileA(name
			,GENERIC_READ|GENERIC_WRITE 	/* Access */
			,0								/* Share mode */
			,NULL							/* Security attributes */
			,OPEN_EXISTING					/* Create access */
			,FILE_ATTRIBUTE_NORMAL			/* File attributes */
			,NULL							/* Template */
			))==INVALID_HANDLE_VALUE)
			return false;

		if(GetCommTimeouts(com_handle, &timeouts)) {
			timeouts.ReadIntervalTimeout=MAXDWORD;
			timeouts.ReadTotalTimeoutMultiplier=0;
			timeouts.ReadTotalTimeoutConstant=0;		// No-wait read timeout
			timeouts.WriteTotalTimeoutMultiplier=0;
			timeouts.WriteTotalTimeoutConstant=5000;	// 5 seconds
			SetCommTimeouts(com_handle,&timeouts);
		}

		/* Force N-8-1 mode: */
		if(GetCommState(com_handle, &dcb)==TRUE) {
			dcb.ByteSize		= 8;
			dcb.Parity			= NOPARITY;
			dcb.StopBits		= ONESTOPBIT;
			dcb.DCBlength		= sizeof(DCB);
			dcb.fBinary			= TRUE;
			dcb.fOutxCtsFlow	= FALSE;
			dcb.fOutxDsrFlow	= FALSE;
			dcb.fDtrControl		= DTR_CONTROL_DISABLE;
			dcb.fDsrSensitivity	= FALSE;
			dcb.fTXContinueOnXoff= TRUE;
			dcb.fOutX			= FALSE;
			dcb.fInX			= FALSE;
			dcb.fRtsControl		= RTS_CONTROL_DISABLE;
			dcb.fAbortOnError	= FALSE;
			SetCommState(com_handle, &dcb);
		}
	}
#else
	{
		struct termios t;

		if((com_handle=open(name, O_NONBLOCK|O_RDWR))==COM_HANDLE_INVALID)
			return false;

		if(tcgetattr(com_handle, &t)==-1) {
			close(com_handle);
			com_handle = COM_HANDLE_INVALID;
			return false;
		}

		t.c_iflag = (
					  IGNBRK   /* ignore BREAK condition */
					| IGNPAR   /* ignore (discard) parity errors */
					);
		t.c_oflag = 0;	/* No output processing */
		t.c_cflag = (
					  CS8         /* 8 bits */
					| CREAD       /* enable receiver */
		/*
		Fun snippet from the FreeBSD manpage:

			 If CREAD is set, the receiver is enabled.  Otherwise, no character is
			 received.  Not all hardware supports this bit.  In fact, this flag is
			 pretty silly and if it were not part of the termios specification it
			 would be omitted.
		*/
					| CLOCAL      /* ignore modem status lines */
					);
		t.c_lflag = 0;	/* No local modes */
		if(tcsetattr(com_handle, TCSANOW, &t)==-1) {
			close(com_handle);
			com_handle = COM_HANDLE_INVALID;
			return false;
		}
		
	}
#endif
	return true;
}

#ifdef _USE_ONIDLE
void MainFrame::OnIdle(wxIdleEvent &evt) {
}
#endif

//----------------------------------------------------------------
// (raise|lower)(RTS|DTR)()
//
// Raises/lowers the specified signal
//----------------------------------------------------------------
void MainFrame::raiseDTR(void)
{
	if(com_handle == COM_HANDLE_INVALID)
		return;
#ifdef _WIN32
	EscapeCommFunction(com_handle, SETDTR);
#else
	{	// For C89 happiness
		int flags = TIOCM_DTR;
		ioctl(com_handle, TIOCMBIS, &flags);
	}
#endif
}
void MainFrame::raiseRTS(void)
{
	if(com_handle == COM_HANDLE_INVALID)
		return;
#ifdef _WIN32
	EscapeCommFunction(com_handle, SETRTS);
#else
	{	// For C89 happiness
		int flags = TIOCM_RTS;
		ioctl(com_handle, TIOCMBIS, &flags);
	}
#endif
}
void MainFrame::lowerDTR(void)
{
	if(com_handle == COM_HANDLE_INVALID)
		return;
#ifdef _WIN32
	EscapeCommFunction(com_handle, CLRDTR);
#else
	{	// For C89 happiness
		int flags = TIOCM_DTR;
		ioctl(com_handle, TIOCMBIC, &flags);
	}
#endif
}
void MainFrame::lowerRTS(void)
{
	if(com_handle == COM_HANDLE_INVALID)
		return;
#ifdef _WIN32
	EscapeCommFunction(com_handle, CLRRTS);
#else
	{	// For C89 happiness
		int flags = TIOCM_RTS;
		ioctl(com_handle, TIOCMBIC, &flags);
	}
#endif
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

    int r,c;

    if (m_panelWaterfall->checkDT()) {
        m_panelWaterfall->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);
        m_panelWaterfall->m_newdata = true;
        m_panelWaterfall->Refresh();
    }

    m_panelSpectrum->setRxFreq(FDMDV_FCENTRE - g_RxFreqOffsetHz);
    m_panelSpectrum->m_newdata = true;
    m_panelSpectrum->Refresh();

    /* update scatter plot -----------------------------------------------------------------*/

    for (r=0; r<g_stats.nr; r++) {
        
        if (freedv_get_mode(g_pfreedv) == FREEDV_MODE_1600) {
            m_panelScatter->add_new_samples(&g_stats.rx_symbols[r][0]);
        }
        
        if (freedv_get_mode(g_pfreedv) == FREEDV_MODE_700B) {
            
            /* 
               FreeDV 700 uses diversity, so combine symbols for
               scatter plot, as combined symbols are used for
               demodulation.  Note we need to use a copy of the
               symbols, as we are not sure when the stats will be
               updated.
            */

            COMP rx_symbols_copy[g_Nc/2];

            for(c=0; c<g_Nc/2; c++)
                rx_symbols_copy[c] = cadd(g_stats.rx_symbols[r][c], g_stats.rx_symbols[r][c+g_Nc/2]);
            m_panelScatter->add_new_samples(rx_symbols_copy);
        }
       
    }

    m_panelScatter->Refresh();

    // Oscilliscope type speech plots -------------------------------------------------------

    short speechInPlotSamples[WAVEFORM_PLOT_BUF];
    if (fifo_read(g_plotSpeechInFifo, speechInPlotSamples, WAVEFORM_PLOT_BUF)) {
        memset(speechInPlotSamples, 0, WAVEFORM_PLOT_BUF*sizeof(short));
        //fprintf(stderr, "empty!\n");
    }
    m_panelSpeechIn->add_new_short_samples(0, speechInPlotSamples, WAVEFORM_PLOT_BUF, 32767);
    m_panelSpeechIn->Refresh();

    short speechOutPlotSamples[WAVEFORM_PLOT_BUF];
    if (fifo_read(g_plotSpeechOutFifo, speechOutPlotSamples, WAVEFORM_PLOT_BUF))
        memset(speechOutPlotSamples, 0, WAVEFORM_PLOT_BUF*sizeof(short));
    m_panelSpeechOut->add_new_short_samples(0, speechOutPlotSamples, WAVEFORM_PLOT_BUF, 32767);
    m_panelSpeechOut->Refresh();

    short demodInPlotSamples[WAVEFORM_PLOT_BUF];
    if (fifo_read(g_plotDemodInFifo, demodInPlotSamples, WAVEFORM_PLOT_BUF)) {
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

    g_snr = m_snrBeta*g_snr + (1.0 - m_snrBeta)*g_stats.snr_est;
    float snr_limited = g_snr;
    if (snr_limited < -5.0) snr_limited = -5.0;
    if (snr_limited > 20.0) snr_limited = 20.0;

    char snr[15];
    sprintf(snr, "%d", (int)(g_snr+0.5)); // round to nearest dB

    //printf("snr_est: %f m_snrBeta: %f g_snr: %f snr_limited: %f\n", g_stats.snr_est,  m_snrBeta, g_snr, snr_limited);

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

    if (g_State) {
        m_rbSync->SetForegroundColour( wxColour( 0, 255, 0 ) ); // green
        m_rbSync->SetValue(true);
    }
    else {
        m_rbSync->SetForegroundColour( wxColour( 255, 0, 0 ) ); // red
        m_rbSync->SetValue(false);
    }

    // send Callsign ----------------------------------------------------

    char callsign[MAX_CALLSIGN];
    strncpy(callsign, (const char*) wxGetApp().m_callSign.mb_str(wxConvUTF8), MAX_CALLSIGN-1);

    // buffer 1 txt message to ensure tx data fifo doesn't "run dry"

    if ((unsigned)fifo_used(g_txDataInFifo) < strlen(callsign)) {
        unsigned int  i;

        //fprintf(g_logfile, "tx callsign: %s.\n", callsign);

        /* optionally append checksum */

        if (wxGetApp().m_enable_checksum) {

            unsigned char checksum = 0;
            char callsign_checksum_cr[MAX_CALLSIGN+1];

            for(i=0; i<strlen(callsign); i++)
                checksum += callsign[i];
            sprintf(callsign_checksum_cr, "%s%2x", callsign, checksum);
            callsign_checksum_cr[strlen(callsign)+2] = 13;
            callsign_checksum_cr[strlen(callsign)+3] = 0;
            strcpy(callsign, callsign_checksum_cr);
        }
        else {
            callsign[strlen(callsign)] = 13;
            callsign[strlen(callsign)+1] = 0;
        }

        //fprintf(g_logfile, "tx callsign: %s.\n", callsign);

        // write chars to tx data fifo

        for(i=0; i<strlen(callsign); i++) {
            short ashort = (short)callsign[i];
            fifo_write(g_txDataInFifo, &ashort, 1);
        }
    }

    // See if any Callsign info received --------------------------------

    short ashort;
    while (fifo_read(g_rxDataOutFifo, &ashort, 1) == 0) {

        if ((ashort == 13) || ((m_pcallsign - m_callsign) > MAX_CALLSIGN-1)) {
            // CR completes line
            *m_pcallsign = 0;
            
            /* Checksums can be disabled, e.g. for compatability with
               older vesions.  In that case we print msg but don't do
               any event processing.  If checksums enabled, only print
               out when checksum is good. */

            if (wxGetApp().m_enable_checksum) {
                // lets see if checksum is OK
            
                unsigned char checksum_rx = 0;
                if (strlen(m_callsign) > 2) {
                    for(unsigned int i=0; i<strlen(m_callsign)-2; i++)
                        checksum_rx += m_callsign[i];
                }
                unsigned int checksum_tx;
                int ret = sscanf(&m_callsign[strlen(m_callsign)-2], "%2x", &checksum_tx);
                //fprintf(g_logfile, "rx callsign: %s.\n  checksum tx: %02x checksum rx: %02x\n", m_callsign, checksum_tx, checksum_rx);

                wxString s;
                if (ret && (checksum_tx == checksum_rx)) {
                    m_callsign[strlen(m_callsign)-2] = 0;
                    s.Printf("%s", m_callsign);
                    m_txtCtrlCallSign->SetValue(s);

                    char s1[MAX_CALLSIGN];
                    sprintf(s1,"rx_txtmsg %s", m_callsign);
                    processTxtEvent(s1);

                    m_checksumGood++;
                    s.Printf("%d", m_checksumGood);
                    m_txtChecksumGood->SetLabel(s);              
                }
                else {
                    m_checksumBad++;
                    s.Printf("%d", m_checksumBad);
                    m_txtChecksumBad->SetLabel(s);        
                }
            }

            //fprintf(g_logfile,"resetting callsign %s %ld\n", m_callsign, m_pcallsign-m_callsign);
            // reset ptr to start of string
            m_pcallsign = m_callsign;
        }
        else {
            //fprintf(g_logfile, "new char %d %c\n", ashort, (char)ashort);
            *m_pcallsign++ = (char)ashort;
        }

        /* If checksums disabled, display txt chars as they arrive */

        if (!wxGetApp().m_enable_checksum) {
            m_txtCtrlCallSign->SetValue(m_callsign);
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

    // Run time update of FreeDV 700 tx clipper

    freedv_set_clip(g_pfreedv, (int)wxGetApp().m_FreeDV700txClip);

    // Test Frame Bit Error Updates ------------------------------------

    // Toggle test frame mode at run time

    if (!freedv_get_test_frames(g_pfreedv) && wxGetApp().m_testFrames) {

        // reset stats on check box off to on transition

        freedv_set_test_frames(g_pfreedv, 1);
        freedv_set_total_bits(g_pfreedv, 0);
        freedv_set_total_bit_errors(g_pfreedv, 0);
    }
    freedv_set_test_frames(g_pfreedv, wxGetApp().m_testFrames);
    g_channel_noise =  wxGetApp().m_channel_noise;

    if (g_State) {
        char bits[80], errors[80], ber[80];

        // update stats on main page

        sprintf(bits, "Bits: %d", freedv_get_total_bits(g_pfreedv)); wxString bits_string(bits); m_textBits->SetLabel(bits_string);
        sprintf(errors, "Errs: %d", freedv_get_total_bit_errors(g_pfreedv)); wxString errors_string(errors); m_textErrors->SetLabel(errors_string);
        float b = (float)freedv_get_total_bit_errors(g_pfreedv)/(1E-6+freedv_get_total_bits(g_pfreedv));
        sprintf(ber, "BER: %4.3f", b); wxString ber_string(ber); m_textBER->SetLabel(ber_string);

        // update error plots

        int sz_error_pattern = freedv_get_sz_error_pattern(g_pfreedv);
        short error_pattern[sz_error_pattern];

        if (fifo_read(g_error_pattern_fifo, error_pattern, sz_error_pattern) == 0) {
            int i,b;

            /* both modes map IQ to alternate bits, but one same carrier */

            if (freedv_get_mode(g_pfreedv) == FREEDV_MODE_1600) {
                /* FreeDV 1600 mapping from error pattern to bit on each carrier */

                for(b=0; b<g_Nc*2; b++) {
                    for(i=b; i<sz_error_pattern; i+= 2*g_Nc) {
                        m_panelTestFrameErrors->add_new_sample(b, b + 0.8*error_pattern[i]);
                        g_error_hist[b] += error_pattern[i];
                    }
                    //if (b%2)
                    //    printf("g_error_hist[%d]: %d\n", b/2, g_error_hist[b/2]);
                }

                int max_hist = 0;
                for(b=0; b<g_Nc; b++)
                    if (g_error_hist[b] > max_hist)
                        max_hist = g_error_hist[b];

                m_panelTestFrameErrorsHist->add_new_short_samples(0, g_error_hist, 2*FDMDV_NC_MAX, max_hist);
            }
       
            if (freedv_get_mode(g_pfreedv) == FREEDV_MODE_700B) {
                int c;

                /* FreeDV 700 mapping from error pattern to bit on each
                   carrier.  Note we don't have access to carriers before
                   diversity re-combination, so this won't give us the full
                   picture, we have to assume Nc/2 carriers. */

                for(i=0; i<sz_error_pattern; i++) {
                    c = i/4;
                    m_panelTestFrameErrors->add_new_sample(c, c + 0.8*error_pattern[i]);
                    g_error_hist[c] += error_pattern[i];
                    //printf("i: %d c: %d\n", i, c);
                }

                int max_hist = 0;
                for(b=0; b<g_Nc; b++)
                    if (g_error_hist[b] > max_hist)
                        max_hist = g_error_hist[b];
                m_panelTestFrameErrorsHist->add_new_short_samples(0, g_error_hist, 2*FDMDV_NC_MAX, max_hist);                
            }
 
            m_panelTestFrameErrors->Refresh();       
            m_panelTestFrameErrorsHist->Refresh();
        }
    }

    // command from UDP thread that is best processed in main thread to avoid seg faults

    if (m_schedule_restore) {
        if (IsIconized())
            Restore();
        m_schedule_restore = false;
    }

    // Light Spam Timer LED if at least one timer is running

    int i;
    optionsDlg->SetSpamTimerLight(false);
    for(i=0; i<MAX_EVENT_RULES; i++)
        if (spamTimer[i].IsRunning())
            optionsDlg->SetSpamTimerLight(true);        

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
}
#endif


//-------------------------------------------------------------------------
// OnCloseFrame()
//-------------------------------------------------------------------------
void MainFrame::OnCloseFrame(wxCloseEvent& event)
{
    Pa_Terminate();
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
    wxConfigBase *pConfig = wxConfigBase::Get();
    if(pConfig->DeleteAll())
    {
        wxLogMessage(wxT("Config file/registry key successfully deleted."));

        delete wxConfigBase::Set(NULL);
        wxConfigBase::DontCreateOnDemand();
    }
    else
    {
        wxLogError(wxT("Deleting config file/registry key failed."));
    }
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
    sprintf(sqsnr, "%4.1f", g_SquelchLevel); // 0.5 dB steps
    wxString sqsnr_string(sqsnr);
    m_textSQ->SetLabel(sqsnr_string);

    event.Skip();
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
    wxGetApp().m_snrSlow = m_ckboxSNR->GetValue();
    setsnrBeta(wxGetApp().m_snrSlow);
    //printf("m_snrSlow: %d\n", (int)wxGetApp().m_snrSlow);
}

// check for space bar press (only when running)

int MainApp::FilterEvent(wxEvent& event)
{
    if ((event.GetEventType() == wxEVT_KEY_DOWN) && 
        (((wxKeyEvent&)event).GetKeyCode() == WXK_SPACE))
        {
            // only use space to toggle PTT if we are running and no modal dialogs (like options) up
            //fprintf(stderr,"frame->m_RxRunning: %d g_modal: %d\n",
            //        frame->m_RxRunning, g_modal);
            if (frame->m_RxRunning && !g_modal) {

                // space bar controls rx/rx if keyer not running
                if (frame->vk_state == VK_IDLE) {
                    if (frame->m_btnTogPTT->GetValue())
                        frame->m_btnTogPTT->SetValue(false);
                    else
                        frame->m_btnTogPTT->SetValue(true);

                    frame->togglePTT();
                }
                else // spavce bar stops keyer
                    frame->VoiceKeyerProcessEvent(VK_SPACE_BAR);
                    
                return true; // absorb space so we don't toggle control with focus (e.g. Start)

            }
        }

    return -1;
}

//-------------------------------------------------------------------------
// OnTogBtnPTT ()
//-------------------------------------------------------------------------
void MainFrame::OnTogBtnPTT (wxCommandEvent& event)
{
    togglePTT();
    event.Skip();
}

void MainFrame::togglePTT(void) {

    // Change tabbed page in centre panel depending on PTT state

    if (g_tx)
    {
        // tx-> rx transition, swap to the page we were on for last rx
        m_auiNbookCtrl->ChangeSelection(wxGetApp().m_rxNbookCtrl);
    }
    else
    {
        // rx-> tx transition, swap to Mic In page to monitor speech
        wxGetApp().m_rxNbookCtrl = m_auiNbookCtrl->GetSelection();
        m_auiNbookCtrl->ChangeSelection(m_auiNbookCtrl->GetPageIndex((wxWindow *)m_panelSpeechIn));
        char e[80]; sprintf(e,"ptt"); processTxtEvent(e);
    }

    g_tx = m_btnTogPTT->GetValue();

    // Hamlib PTT

    if (wxGetApp().m_boolHamlibUseForPTT) {        
        Hamlib *hamlib = wxGetApp().m_hamlib; 
        if (wxGetApp().m_boolHamlibUseForPTT && hamlib != NULL) {
            hamlib->ptt(g_tx);
        }
    }

    // Serial PTT

    /*  Truth table:

          g_tx   RTSPos   RTS
          -------------------
          0      1        0
          1      1        1
          0      0        1
          1      0        0

          exclusive NOR
    */

    if(wxGetApp().m_boolUseSerialPTT && (com_handle != COM_HANDLE_INVALID)) {
        if (wxGetApp().m_boolUseRTS) {
            //fprintf(stderr, "g_tx: %d m_boolRTSPos: %d serialLine: %d\n", g_tx, wxGetApp().m_boolRTSPos, g_tx == wxGetApp().m_boolRTSPos);
            if (g_tx == wxGetApp().m_boolRTSPos)
                raiseRTS();
            else
                lowerRTS();
        }
        if (wxGetApp().m_boolUseDTR) {
            //fprintf(stderr, "g_tx: %d m_boolDTRPos: %d serialLine: %d\n", g_tx, wxGetApp().m_boolDTRPos, g_tx == wxGetApp().m_boolDTRPos);
            if (g_tx == wxGetApp().m_boolDTRPos)
                raiseDTR();
            else
                lowerDTR();
        }
 
    }

    // reset level gauge

    m_maxLevel = 0;
    m_textLevel->SetLabel(wxT(""));
    m_gaugeLevel->SetValue(0);
}

/*
   Voice Keyer:

   + space bar turns keyer off
   + 5 secs of valid sync turns it off

   [X] complete state machine and builds OK
   [ ] file select dialog
   [ ] test all states
   [ ] restore size
*/

void MainFrame::OnTogBtnVoiceKeyerClick (wxCommandEvent& event)
{
    if (vk_state == VK_IDLE)
        VoiceKeyerProcessEvent(VK_START);
    else
        VoiceKeyerProcessEvent(VK_SPACE_BAR);
        
    event.Skip();
}


int MainFrame::VoiceKeyerStartTx(void)
{
    int next_state;

    // start playing wave file or die trying

    SF_INFO sfInfo;
    sfInfo.format = 0;

    g_sfPlayFile = sf_open(wxGetApp().m_txtVoiceKeyerWaveFile, SFM_READ, &sfInfo);
    if(g_sfPlayFile == NULL) {
        wxString strErr = sf_strerror(NULL);
        wxMessageBox(strErr, wxT("Couldn't open:") + wxGetApp().m_txtVoiceKeyerWaveFile, wxOK);
        m_togBtnVoiceKeyer->SetValue(false);
        next_state = VK_IDLE;
    }
    else {
        SetStatusText(wxT("Voice Keyer: Playing File") + wxGetApp().m_txtVoiceKeyerWaveFile + wxT(" to Mic Input") , 0);
        g_loopPlayFileToMicIn = false;
        g_playFileToMicIn = true;

        m_btnTogPTT->SetValue(true); togglePTT();
        next_state = VK_TX;
    }

    return next_state;
}


void MainFrame::VoiceKeyerProcessEvent(int vk_event) {
    int next_state = vk_state;

    switch(vk_state) {

    case VK_IDLE:
        if (vk_event == VK_START) {
            // sample these puppies at start just in case they are changed while VK running
            vk_rx_pause = wxGetApp().m_intVoiceKeyerRxPause;
            vk_repeats = wxGetApp().m_intVoiceKeyerRepeats;
            fprintf(stderr, "vk_rx_pause: %d vk_repeats: %d\n", vk_rx_pause, vk_repeats);

            vk_repeat_counter = 0;
            next_state = VoiceKeyerStartTx();
        }
        break;
        
     case VK_TX:

        // In this state we are transmitting and playing a wave file
        // to Mic In

        if (vk_event == VK_SPACE_BAR) {
            m_btnTogPTT->SetValue(false); togglePTT();
            StopPlayFileToMicIn();
            m_togBtnVoiceKeyer->SetValue(false);
            next_state = VK_IDLE;
        }

        if (vk_event == VK_PLAY_FINISHED) {
            m_btnTogPTT->SetValue(false); togglePTT();
            vk_repeat_counter++;
            if (vk_repeat_counter > vk_repeats) {
                m_togBtnVoiceKeyer->SetValue(false);
                next_state = VK_IDLE;
            }
            else {
                vk_rx_time = 0.0;
                next_state = VK_RX;
            }
        }

        break;

     case VK_RX:

        // in this state we are receiving and waiting for
        // delay timer or valid sync

        if (vk_event == VK_DT) {
            vk_rx_time += DT;
            if (vk_rx_time >= vk_rx_pause) {
                next_state = VoiceKeyerStartTx();
            }
        }

        if (vk_event == VK_SPACE_BAR) {
            m_togBtnVoiceKeyer->SetValue(false);
            next_state = VK_IDLE;
        }

        if (vk_event == VK_SYNC) {
            vk_rx_time = 0;
            next_state = VK_SYNC_WAIT;
        }
        break;

     case VK_SYNC_WAIT:

        // In this state we wait for valid sync to last
        // VK_SYNC_WAIT_TIME seconds

        if (vk_event == VK_SPACE_BAR) {
            m_togBtnVoiceKeyer->SetValue(false);
            next_state = VK_IDLE;
        }

        if (vk_event == VK_DT) {
            vk_rx_time += DT;

            // if we lose sync restart RX state

            if (freedv_get_sync(g_pfreedv) == 0) {
                vk_rx_time = 0.0;
                next_state = VK_RX;
            }

            // drop out of voice keyer if we get a few seconds of valid sync

            if (vk_rx_time >= VK_SYNC_WAIT_TIME) {
                m_togBtnVoiceKeyer->SetValue(false);
                next_state = VK_IDLE;
            }
        }
        break;

    default:
        // catch anything we missed

        m_btnTogPTT->SetValue(false); togglePTT();
        m_togBtnVoiceKeyer->SetValue(false);
        next_state = VK_IDLE;
   }

    if ((vk_event != VK_DT) || (vk_state != next_state))
        fprintf(stderr, "VoiceKeyerProcessEvent: vk_state: %d vk_event: %d next_state: %d  vk_repeat_counter: %d\n", vk_state, vk_event, next_state, vk_repeat_counter);
    vk_state = next_state;
}


//-------------------------------------------------------------------------
// OnTogBtnRxID()
//-------------------------------------------------------------------------
void MainFrame::OnTogBtnRxID(wxCommandEvent& event)
{
    // empty any junk in rx data FIFO
    short junk;
    while(fifo_read(g_rxDataOutFifo,&junk,1) == 0);
    event.Skip();
}

//-------------------------------------------------------------------------
// OnTogBtnTxID()
//-------------------------------------------------------------------------
void MainFrame::OnTogBtnTxID(wxCommandEvent& event)
{
    event.Skip();
}

void MainFrame::OnTogBtnSplitClick(wxCommandEvent& event) {
    if (g_split)
        g_split = 0;
    else
        g_split = 1;
    event.Skip();
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
    }
    else {
        g_analog = 0;
        m_panelSpectrum->setFreqScale(MODEM_STATS_NSPEC*((float)MAX_F_HZ/(freedv_get_modem_sample_rate(g_pfreedv)/2)));
        m_panelWaterfall->setFs(freedv_get_modem_sample_rate(g_pfreedv));
    }

    g_State = 0;
    g_stats.snr_est = 0;

    event.Skip();
}

void MainFrame::OnCallSignReset(wxCommandEvent& event)
{
    m_pcallsign = m_callsign;
    memset(m_callsign, 0, MAX_CALLSIGN);
    wxString s;
    s.Printf("%s", m_callsign);
    m_txtCtrlCallSign->SetValue(s);
    m_checksumGood = m_checksumBad = 0;
    m_txtChecksumGood->SetLabel(_("0"));
    m_txtChecksumBad->SetLabel(_("0"));
}

void MainFrame::OnBerReset(wxCommandEvent& event)
{
    freedv_set_total_bits(g_pfreedv, 0);
    freedv_set_total_bit_errors(g_pfreedv, 0);
    int i;
    for(i=0; i<2*g_Nc; i++)
        g_error_hist[i] = 0;
    
}

#ifdef ALC
//-------------------------------------------------------------------------
// OnTogBtnALCClick()
//-------------------------------------------------------------------------
void MainFrame::OnTogBtnALCClick(wxCommandEvent& event)
{
    wxMessageBox(wxT("Got Click!"), wxT("OnTogBtnALCClick"), wxOK);

    event.Skip();
}
#endif

// extra panel added to file open dialog to add loop checkbox
MyExtraPlayFilePanel::MyExtraPlayFilePanel(wxWindow *parent): wxPanel(parent)
{
    m_cb = new wxCheckBox(this, -1, wxT("Loop"));
    m_cb->SetToolTip(_("When checked file will repeat forever"));
    m_cb->SetValue(g_loopPlayFileToMicIn);

    // bug: I can't this to align right.....
    wxBoxSizer *sizerTop = new wxBoxSizer(wxHORIZONTAL);
    sizerTop->Add(m_cb, 0, wxALIGN_RIGHT, 0);
    SetSizerAndFit(sizerTop);
}

static wxWindow* createMyExtraPlayFilePanel(wxWindow *parent)
{
    return new MyExtraPlayFilePanel(parent);
}

void MainFrame::StopPlayFileToMicIn(void)
{
    g_mutexProtectingCallbackData.Lock();
    g_playFileToMicIn = false;
    sf_close(g_sfPlayFile);
    SetStatusText(wxT(""));
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
        VoiceKeyerProcessEvent(VK_PLAY_FINISHED);
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
                sfInfo.samplerate = FS;
            }
        }
        g_sfPlayFile = sf_open(soundFile.c_str(), SFM_READ, &sfInfo);
        if(g_sfPlayFile == NULL)
        {
            wxString strErr = sf_strerror(NULL);
            wxMessageBox(strErr, wxT("Couldn't open sound file"), wxOK);
            return;
        }

        wxWindow * const ctrl = openFileDialog.GetExtraControl();

        // Huh?! I just copied wxWidgets-2.9.4/samples/dialogs ....
        g_loopPlayFileToMicIn = static_cast<MyExtraPlayFilePanel*>(ctrl)->getLoopPlayFileToMicIn();

        SetStatusText(wxT("Playing File: ") + fileName + wxT(" to Mic Input") , 0);
        g_playFileToMicIn = true;
    }
}

//-------------------------------------------------------------------------
// OnPlayFileFromRadio()
// This puppy "plays" a recorded file into the demodulator input, allowing us
// to replay off air signals.
//-------------------------------------------------------------------------
void MainFrame::OnPlayFileFromRadio(wxCommandEvent& event)
{
    wxUnusedVar(event);

    printf("OnPlayFileFromRadio:: %d\n", (int)g_playFileFromRadio);
    if (g_playFileFromRadio)
    {
        printf("OnPlayFileFromRadio:: Stop\n");
        g_mutexProtectingCallbackData.Lock();
        g_playFileFromRadio = false;
        sf_close(g_sfPlayFileFromRadio);
        SetStatusText(wxT(""),0);
        SetStatusText(wxT(""),1);
        g_mutexProtectingCallbackData.Unlock();
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
                sfInfo.samplerate = freedv_get_modem_sample_rate(g_pfreedv);
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
        fprintf(g_logfile, "OnPlayFileFromRadio:: Playing File\n");
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
    sizerTop->Add(m_secondsToRecord, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    SetSizerAndFit(sizerTop);
}

static wxWindow* createMyExtraRecFilePanel(wxWindow *parent)
{
    return new MyExtraRecFilePanel(parent);
}

//-------------------------------------------------------------------------
// OnRecFileFromRadio()
//-------------------------------------------------------------------------
void MainFrame::OnRecFileFromRadio(wxCommandEvent& event)
{
    wxUnusedVar(event);

    if (g_recFileFromRadio) {
        printf("Stopping Record....\n");
        g_mutexProtectingCallbackData.Lock();
        g_recFileFromRadio = false;
        sf_close(g_sfRecFile);
        SetStatusText(wxT(""));
        g_mutexProtectingCallbackData.Unlock();
    }
    else {

        wxString    soundFile;
        SF_INFO     sfInfo;

         wxFileDialog openFileDialog(
                                    this,
                                    wxT("Record File From Radio"),
                                    wxGetApp().m_recFileFromRadioPath,
                                    wxEmptyString,
                                    wxT("WAV and RAW files (*.wav;*.raw)|*.wav;*.raw|")
                                    wxT("All files (*.*)|*.*"),
                                    wxFD_SAVE
                                    );

        // add the loop check box
        openFileDialog.SetExtraControlCreator(&createMyExtraRecFilePanel);

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

        if(!extension.IsEmpty())
        {
            extension.LowerCase();
            if(extension == wxT("raw"))
            {
                sfInfo.format     = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
                sfInfo.channels   = 1;
                sfInfo.samplerate = freedv_get_modem_sample_rate(g_pfreedv);
            }
            else if(extension == wxT("wav"))
            {
                sfInfo.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
                sfInfo.channels   = 1;
                sfInfo.samplerate = freedv_get_modem_sample_rate(g_pfreedv);
            } else {
                wxMessageBox(wxT("Invalid file format"), wxT("Record File From Radio"), wxOK);
                return;
            }
        }
        else {
            wxMessageBox(wxT("Invalid file format"), wxT("Record File From Radio"), wxOK);
            return;
        }

        // Bug: on Win32 I cant read m_recFileFromRadioSecs, so have hard coded it
#ifdef __WIN32__
        long secs = wxGetApp().m_recFileFromRadioSecs;
        g_recFromRadioSamples = FS*(unsigned int)secs;
#else
        // work out number of samples to record

        wxWindow * const ctrl = openFileDialog.GetExtraControl();
        wxString secsString = static_cast<MyExtraRecFilePanel*>(ctrl)->getSecondsToRecord();
        wxLogDebug("test: %s secsString: %s\n", wxT("testing 123"), secsString);

        long secs;
        if (secsString.ToLong(&secs)) {
            wxGetApp().m_recFileFromRadioSecs = (unsigned int)secs;
            //printf(" secondsToRecord: %d\n",  (unsigned int)secs);
            g_recFromRadioSamples = FS*(unsigned int)secs;
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
        g_recFileFromRadio = true;
    }

}

//-------------------------------------------------------------------------
// OnExit()
//-------------------------------------------------------------------------
void MainFrame::OnExit(wxCommandEvent& event)
{
    wxUnusedVar(event);
#ifdef _USE_TIMER
    m_plotTimer.Stop();
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
    //m_togRxID->Disable();
    //m_togTxID->Disable();
    m_togBtnAnalog->Disable();
    //m_togBtnALC->Disable();
    //m_btnTogPTT->Disable();
    Pa_Terminate();
    Destroy();
}

//-------------------------------------------------------------------------
// OnExitClick()
//-------------------------------------------------------------------------
void MainFrame::OnExitClick(wxCommandEvent& event)
{
    OnExit(event);
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
    FilterDlg *dlg = new FilterDlg(NULL, m_RxRunning, &m_newMicInFilter, &m_newSpkOutFilter);
    dlg->ShowModal();
    delete dlg;
}

//-------------------------------------------------------------------------
// OnToolsOptions()
//-------------------------------------------------------------------------
void MainFrame::OnToolsOptions(wxCommandEvent& event)
{
    wxUnusedVar(event);
    g_modal = true;
    //fprintf(stderr,"g_modal: %d\n", g_modal);
    optionsDlg->Show();
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

    int rv = dlg->ShowModal();

    // test Hamlib/Serial set up

    if(rv == wxID_OK)
    {
        if (wxGetApp().m_boolHamlibUseForPTT) {
            OpenHamlibRig();
            wxGetApp().m_hamlib->close();
        }
        if (wxGetApp().m_boolUseSerialPTT) {
            SetupSerialPort();
            CloseSerialPort();
        }
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
// OnToolsPlugInCfg()
//-------------------------------------------------------------------------
void MainFrame::OnToolsPlugInCfg(wxCommandEvent& event)
{
    wxUnusedVar(event);
    PlugInDlg *dlg = new PlugInDlg(m_plugInName, m_numPlugInParam, m_plugInParamName);
    dlg->ShowModal();
    delete dlg;
}
               
void MainFrame::OnToolsPlugInCfgUI(wxUpdateUIEvent& event)
{
    event.Enable(!m_RxRunning && m_plugIn);
}


//-------------------------------------------------------------------------
// OnHelpCheckUpdates()
//-------------------------------------------------------------------------
void MainFrame::OnHelpCheckUpdates(wxCommandEvent& event)
{
    wxMessageBox("Got Click!", "OnHelpCheckUpdates", wxOK);
    event.Skip();
}

//-------------------------------------------------------------------------
// OnHelpCheckUpdatesUI()
//-------------------------------------------------------------------------
void MainFrame::OnHelpCheckUpdatesUI(wxUpdateUIEvent& event)
{
    event.Enable(false);
}

//-------------------------------------------------------------------------
//OnHelpAbout()
//-------------------------------------------------------------------------
void MainFrame::OnHelpAbout(wxCommandEvent& event)
{
    wxUnusedVar(event);
    wxString msg;
    msg.Printf( wxT("FreeDV %s\n\n")
                wxT("Open Source Digital Voice\n\n")
                wxT("For Help and Support visit: http://freedv.org\n\n")

                wxT("GNU Public License V2.1\n\n")
                wxT("Copyright (c) David Witten KD0EAG and David Rowe VK5DGR\n\n")
                wxT("svn revision: %s\n"), FREEDV_VERSION, SVN_REVISION);

    wxMessageBox(msg, wxT("About"), wxOK | wxICON_INFORMATION, this);
}


// Attempt to talk to rig using Hamlib

bool MainFrame::OpenHamlibRig() {
    if (wxGetApp().m_boolHamlibUseForPTT != true)
       return false;
    if (wxGetApp().m_intHamlibRig == 0)
        return false;
    if (wxGetApp().m_hamlib == NULL)
        return false;

    int rig = wxGetApp().m_intHamlibRig;
    wxString port = wxGetApp().m_strHamlibSerialPort;
    bool status = wxGetApp().m_hamlib->connect(rig, port.mb_str(wxConvUTF8));
    if (status == false)
        wxMessageBox("Couldn't connect to Radio with hamlib", wxT("About"), wxOK | wxICON_ERROR, this);
 
    return status;
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
        //
        // Start Running -------------------------------------------------
        //

        // modify some button states when running

        m_togBtnSplit->Enable();
        m_togBtnAnalog->Enable();
        m_togBtnOnOff->SetLabel(wxT("Stop"));
        m_btnTogPTT->Enable();
        m_togBtnVoiceKeyer->Enable();
        vk_state = VK_IDLE;

        m_rb1600->Disable();
        m_rb700b->Disable();

        // determine what mode we are using

        if (m_rb1600->GetValue()) {
            g_mode = FREEDV_MODE_1600;
            g_Nc = 16;
            m_panelScatter->setNc(g_Nc);
        }
        if (m_rb700b->GetValue()) {
            g_mode = FREEDV_MODE_700B;
            g_Nc = 14;
            m_panelScatter->setNc(g_Nc/2-1);  /* due to diversity, -1 due to no pilot like FreeDV 1600 */
        }
   
        // init freedv states

        g_pfreedv = freedv_open(g_mode);
        freedv_set_callback_txt(g_pfreedv, &my_put_next_rx_char, &my_get_next_tx_char, NULL);

        freedv_set_callback_error_pattern(g_pfreedv, my_freedv_put_error_pattern, (void*)m_panelTestFrameErrors);
        g_error_pattern_fifo = fifo_create(2*freedv_get_sz_error_pattern(g_pfreedv));
        g_error_hist = new short[FDMDV_NC_MAX*2];
        int i;
        for(i=0; i<2*FDMDV_NC_MAX; i++)
            g_error_hist[i] = 0;

        assert(g_pfreedv != NULL);
        modem_stats_open(&g_stats);

        // Init Speex pre-processor states
        // by inspecting Speex source it seems that only denoiser is on be default

        g_speex_st = speex_preprocess_state_init(freedv_get_n_speech_samples(g_pfreedv), FS); 

        // adjust spectrum and waterfall freq scaling base on mode

        m_panelSpectrum->setFreqScale(MODEM_STATS_NSPEC*((float)MAX_F_HZ/(freedv_get_modem_sample_rate(g_pfreedv)/2)));
        m_panelWaterfall->setFs(freedv_get_modem_sample_rate(g_pfreedv));

        // adjust scatter diagram for Number of FDM carriers

        // init Codec 2 LPC Post Filter

        codec2_set_lpc_post_filter(freedv_get_codec2(g_pfreedv),
                                   wxGetApp().m_codec2LPCPostFilterEnable,
                                   wxGetApp().m_codec2LPCPostFilterBassBoost,
                                   wxGetApp().m_codec2LPCPostFilterBeta,
                                   wxGetApp().m_codec2LPCPostFilterGamma);

        g_State = 0;
        g_snr = 0.0;
        g_half_duplex = wxGetApp().m_boolHalfDuplex;

        m_pcallsign = m_callsign;
        memset(m_callsign, 0, sizeof(m_callsign));
        m_checksumGood = m_checksumBad = 0;
        wxString s;
        s.Printf("%d", m_checksumGood);
        m_txtChecksumGood->SetLabel(s);              
        s.Printf("%d", m_checksumBad);
        m_txtChecksumBad->SetLabel(s);        

        m_maxLevel = 0;
        m_textLevel->SetLabel(wxT(""));
        m_gaugeLevel->SetValue(0);

        // Init text msg decoding

        freedv_set_varicode_code_num(g_pfreedv, wxGetApp().m_textEncoding);
        //printf("m_textEncoding = %d\n", wxGetApp().m_textEncoding);
        //printf("g_stats.snr: %f\n", g_stats.snr_est);

        // attempt to start PTT ......
        
        if (wxGetApp().m_boolHamlibUseForPTT)
            OpenHamlibRig();
        if (wxGetApp().m_boolUseSerialPTT) {
            SetupSerialPort();
        }

        // attempt to start sound cards and tx/rx processing

        startRxStream();

        if (m_RxRunning)
        {
#ifdef _USE_TIMER
            m_plotTimer.Start(_REFRESH_TIMER_PERIOD, wxTIMER_CONTINUOUS);
#endif // _USE_TIMER
        }
        char e[80]; sprintf(e,"start"); processTxtEvent(e);
    }

    // Stop was pressed or start up failed

    if (startStop.IsSameAs("Stop") || !m_RxRunning ) {

        //
        // Stop Running -------------------------------------------------
        //

        optionsDlg->SetSpamTimerLight(false);

#ifdef _USE_TIMER
        m_plotTimer.Stop();
#endif // _USE_TIMER

        // ensure we are not transmitting and shut down audio processing

        if (wxGetApp().m_boolHamlibUseForPTT) {
            Hamlib *hamlib = wxGetApp().m_hamlib; 
            if (wxGetApp().m_boolHamlibUseForPTT && hamlib != NULL) {
                hamlib->ptt(false);
                hamlib->close();
            }
        }

        if (wxGetApp().m_boolUseSerialPTT)
            CloseSerialPort();

        m_btnTogPTT->SetValue(false);
        VoiceKeyerProcessEvent(VK_SPACE_BAR);

        stopRxStream();

        // free up states

        modem_stats_close(&g_stats);
        delete g_error_hist;
        fifo_destroy(g_error_pattern_fifo);
        freedv_close(g_pfreedv);
        speex_preprocess_state_destroy(g_speex_st);
        m_newMicInFilter = m_newSpkOutFilter = true;

        m_togBtnSplit->Disable();
        //m_togRxID->Disable();
        //m_togTxID->Disable();
        m_togBtnAnalog->Disable();
        m_btnTogPTT->Disable();
        m_togBtnVoiceKeyer->Disable();
        m_togBtnOnOff->SetLabel(wxT("Start"));
        m_rb1600->Enable();
        m_rb700b->Enable();
#ifdef DISABLED_FEATURE
        m_rb700->Enable();
        m_rb1400old->Enable();
        m_rb1600Wide->Enable();
        m_rb1400->Enable();
        m_rb2000->Enable();
#endif
        char e[80]; sprintf(e,"stop"); processTxtEvent(e);
    }
}

//-------------------------------------------------------------------------
// stopRxStream()
//-------------------------------------------------------------------------
void MainFrame::stopRxStream()
{
    if(m_RxRunning)
    {
        m_RxRunning = false;

        fprintf(g_logfile, "waiting for thread to stop");
        m_txRxThread->m_run = 0;
        m_txRxThread->Wait();
        fprintf(g_logfile, "thread stopped");

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
    }
}

void MainFrame::destroy_fifos(void)
{
    fifo_destroy(g_rxUserdata->infifo1);
    fifo_destroy(g_rxUserdata->outfifo1);
    fifo_destroy(g_rxUserdata->infifo2);
    fifo_destroy(g_rxUserdata->outfifo2);
    fifo_destroy(g_rxUserdata->rxinfifo);
    fifo_destroy(g_rxUserdata->rxoutfifo);
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
                                     int soundCard, int sampleRate, int inputChannels)
{
    // Note all of the wrapper functions below just set values in a
    // portaudio struct so can't return any errors. So no need to trap
    // any errors in this function.

    // init input params

    pa->setInputDevice(inDevice);
    if(inDevice != paNoDevice) {
        pa->setInputChannelCount(inputChannels);           // stereo input
        pa->setInputSampleFormat(PA_SAMPLE_TYPE);
        pa->setInputLatency(pa->getInputDefaultLowLatency());
        pa->setInputHostApiStreamInfo(NULL);
    }

    pa->setOutputDevice(paNoDevice);
    
    // init output params
    
    pa->setOutputDevice(outDevice);
    if(outDevice != paNoDevice) {
        pa->setOutputChannelCount(2);                      // stereo output
        pa->setOutputSampleFormat(PA_SAMPLE_TYPE);
        pa->setOutputLatency(pa->getOutputDefaultLowLatency());
        pa->setOutputHostApiStreamInfo(NULL);
    }

    // init params that affect input and output

    /*
      On Linux, setting this to wxGetApp().m_framesPerBuffer caused
      intermittant break up on the audio from my IC7200 on Ubuntu 14.
      After a day of bug hunting I found that 0, as recommended by the
      PortAudio docunmentation, fixed the problem.
    */

    //pa->setFramesPerBuffer(wxGetApp().m_framesPerBuffer);
    pa->setFramesPerBuffer(0);

    pa->setSampleRate(sampleRate);
    pa->setStreamFlags(paClipOff);
}

//-------------------------------------------------------------------------
// startRxStream()
//-------------------------------------------------------------------------
void MainFrame::startRxStream()
{
    int   src_error;
    const PaDeviceInfo *deviceInfo1 = NULL, *deviceInfo2 = NULL;
    int   inputChannels1, inputChannels2;
    bool  two_rx=false;
    bool  two_tx=false;

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
        
        fprintf(g_logfile, "two_rx: %d two_tx: %d\n", two_rx, two_tx);
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
            return;
        }

        // work out how many input channels this device supports.

        deviceInfo1 = Pa_GetDeviceInfo(g_soundCard1InDeviceNum);
        if (deviceInfo1 == NULL) {
            wxMessageBox(wxT("Couldn't get device info from Port Audio for Sound Card 1"), wxT("Error"), wxOK);
            delete m_rxInPa;
            if(two_rx)
				delete m_rxOutPa;
            m_RxRunning = false;
            return;
        }
        if (deviceInfo1->maxInputChannels == 1)
            inputChannels1 = 1;
        else
            inputChannels1 = 2;

        if(two_rx) {
            initPortAudioDevice(m_rxInPa, g_soundCard1InDeviceNum, paNoDevice, 1,
                            g_soundCard1SampleRate, inputChannels1);
            initPortAudioDevice(m_rxOutPa, paNoDevice, g_soundCard1OutDeviceNum, 1,
                            g_soundCard1SampleRate, inputChannels1);
		}
        else
            initPortAudioDevice(m_rxInPa, g_soundCard1InDeviceNum, g_soundCard1OutDeviceNum, 1,
                            g_soundCard1SampleRate, inputChannels1);

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
                return;
            }

            deviceInfo2 = Pa_GetDeviceInfo(g_soundCard2InDeviceNum);
            if (deviceInfo2 == NULL) {
                wxMessageBox(wxT("Couldn't get device info from Port Audio for Sound Card 1"), wxT("Error"), wxOK);
                delete m_rxInPa;
                if(two_rx)
					delete m_rxOutPa;
                delete m_txInPa;
                if(two_tx)
					delete m_txOutPa;
                m_RxRunning = false;
                return;
            }
            if (deviceInfo2->maxInputChannels == 1)
                inputChannels2 = 1;
            else
                inputChannels2 = 2;

            if(two_tx) {
				initPortAudioDevice(m_txInPa, g_soundCard2InDeviceNum, paNoDevice, 2,
                                g_soundCard2SampleRate, inputChannels2);
				initPortAudioDevice(m_txOutPa, paNoDevice, g_soundCard2OutDeviceNum, 2,
                                g_soundCard2SampleRate, inputChannels2);
			}
			else
				initPortAudioDevice(m_txInPa, g_soundCard2InDeviceNum, g_soundCard2OutDeviceNum, 2,
                                g_soundCard2SampleRate, inputChannels2);
        }

        // Init call back data structure ----------------------------------------------

        g_rxUserdata = new paCallBackData;
        g_rxUserdata->inputChannels1 = inputChannels1;
        if (deviceInfo2 != NULL)
            g_rxUserdata->inputChannels2 = inputChannels2;

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

        // create FIFOs used to interface between different buffer sizes

        g_rxUserdata->infifo1 = fifo_create(8*N48);
        g_rxUserdata->outfifo1 = fifo_create(10*N48);
        g_rxUserdata->outfifo2 = fifo_create(8*N48);
        g_rxUserdata->infifo2 = fifo_create(8*N48);
        printf("N48: %d\n", N48);

        g_rxUserdata->rxinfifo = fifo_create(3 * freedv_get_n_speech_samples(g_pfreedv));
        g_rxUserdata->rxoutfifo = fifo_create(2 * freedv_get_n_speech_samples(g_pfreedv));

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
                delete m_txOutPa;
                if(two_tx)
                    delete m_txOutPa;
                destroy_fifos();
                destroy_src();
                deleteEQFilters(g_rxUserdata);
                delete g_rxUserdata;
                m_RxRunning = false;
                return;
            }

            m_rxErr = m_rxOutPa->streamStart();
            if(m_rxErr != paNoError) {
                wxMessageBox(wxT("Sound Card 1 Second Stream Start Error."), wxT("Error"), wxOK);
                m_rxInPa->stop();
                m_rxInPa->streamClose();
                delete m_rxInPa;
                delete m_rxOutPa;
                delete m_txOutPa;
                if(two_tx)
                    delete m_txOutPa;
                destroy_fifos();
                destroy_src();
                deleteEQFilters(g_rxUserdata);
                delete g_rxUserdata;
                m_RxRunning = false;
                return;
            }
        }

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
                fprintf(stderr, "Err: %d\n", m_txErr);
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
                    return;
                }
            }
        }

        // start tx/rx processing thread

        m_txRxThread = new txRxThread;

        if ( m_txRxThread->Create() != wxTHREAD_NO_ERROR )
        {
            wxLogError(wxT("Can't create thread!"));
        }

        m_txRxThread->SetPriority(WXTHREAD_MAX_PRIORITY);

        if ( m_txRxThread->Run() != wxTHREAD_NO_ERROR )
        {
            wxLogError(wxT("Can't start thread!"));
        }

    }
}


void MainFrame::processTxtEvent(char event[]) {
    int rule = 0;

    //printf("processTxtEvent:\n");
    //printf("  event: %s\n", event);

    // process with regexp and issue system command

    // Each regexp in our list is separated by a newline.  We want to try all of them.

    wxString event_str(event);
    int match_end, replace_end;
    match_end = replace_end = 0;
    wxString regexp_match_list = wxGetApp().m_events_regexp_match;
    wxString regexp_replace_list = wxGetApp().m_events_regexp_replace;

    bool found_match = false;

    while (((match_end = regexp_match_list.Find('\n')) != wxNOT_FOUND) && (rule < MAX_EVENT_RULES)) {
        //printf("match_end: %d\n", match_end);
        if ((replace_end = regexp_replace_list.Find('\n')) != wxNOT_FOUND) {
            //printf("replace_end = %d\n", replace_end);

            // candidate match and replace regexps strings exist, so lets try them

            wxString regexp_match = regexp_match_list.SubString(0, match_end-1);
            wxString regexp_replace = regexp_replace_list.SubString(0, replace_end-1);
            //printf("match: %s replace: %s\n", (const char *)regexp_match.c_str(), (const char *)regexp_replace.c_str());
            wxRegEx re(regexp_match);
            //printf("  checking for match against: %s\n", (const char *)regexp_match.c_str());

            // if we found a match, lets run the replace regexp and issue the system command

            wxString event_str_rep = event_str;
           
            if (re.Replace(&event_str_rep, regexp_replace) != 0) {
                fprintf(stderr, "  found match! event_str: %s\n", (const char *)event_str.c_str());
                found_match = true;

                bool enableSystem = false;
                if (wxGetApp().m_events)
                    enableSystem = true;

                // no syscall if spam timer still running

                if (spamTimer[rule].IsRunning()) {
                    enableSystem = false;
                    fprintf(stderr, "  spam timer running\n");
                }

                const char *event_out = event_str_rep.ToUTF8();
                wxString event_out_with_return_code;

                if (enableSystem) {
                    int ret = wxExecute(event_str_rep);
                    event_out_with_return_code.Printf(_T("%s -> returned %d"), event_out, ret);
                    spamTimer[rule].Start((wxGetApp().m_events_spam_timer)*1000, wxTIMER_ONE_SHOT);
                }
                else
                    event_out_with_return_code.Printf(_T("%s T: %d"), event_out, spamTimer[rule].IsRunning());

                // update event log GUI if currently displayed
                
                if (optionsDlg != NULL) {                  
                    optionsDlg->updateEventLog(wxString(event), event_out_with_return_code);                     
                }
            }
        }
        regexp_match_list = regexp_match_list.SubString(match_end+1, regexp_match_list.length());
        regexp_replace_list = regexp_replace_list.SubString(replace_end+1, regexp_replace_list.length());

        rule++;
    }
 
    if ((optionsDlg != NULL) && !found_match) {                  
        optionsDlg->updateEventLog(wxString(event), _("<no match>"));                     
    }
}


#define SBQ_MAX_ARGS 4

void *MainFrame::designAnEQFilter(const char filterType[], float freqHz, float gaindB, float Q)
{
    char  *arg[SBQ_MAX_ARGS];
    char   argstorage[SBQ_MAX_ARGS][80];
    void  *sbq;
    int    i, argc;

    assert((strcmp(filterType, "bass") == 0)   ||
           (strcmp(filterType, "treble") == 0) ||
           (strcmp(filterType, "equalizer") == 0));

    for(i=0; i<SBQ_MAX_ARGS; i++) {
        arg[i] = &argstorage[i][0];
        arg[i] = &argstorage[i][0];
        arg[i] = &argstorage[i][0];
    }

    argc = 0;

    if ((strcmp(filterType, "bass") == 0) || (strcmp(filterType, "treble") == 0)) {
        sprintf(arg[argc++], "%s", filterType);
        sprintf(arg[argc++], "%f", gaindB+1E-6);
        sprintf(arg[argc++], "%f", freqHz);
    }

    if (strcmp(filterType, "equalizer") == 0) {
        sprintf(arg[argc++], "%s", filterType);
        sprintf(arg[argc++], "%f", freqHz);
        sprintf(arg[argc++], "%f", Q);
        sprintf(arg[argc++], "%f", gaindB+1E-6);
    }

    assert(argc <= SBQ_MAX_ARGS);

    sbq = sox_biquad_create(argc-1, (const char **)arg);

    return sbq;
}

void  MainFrame::designEQFilters(paCallBackData *cb)
{
    // init Mic In Equaliser Filters

    if (m_newMicInFilter) {
        //printf("designing new Min In filters\n");
        cb->sbqMicInBass   = designAnEQFilter("bass", wxGetApp().m_MicInBassFreqHz, wxGetApp().m_MicInBassGaindB);
        cb->sbqMicInTreble = designAnEQFilter("treble", wxGetApp().m_MicInTrebleFreqHz, wxGetApp().m_MicInTrebleGaindB);
        cb->sbqMicInMid    = designAnEQFilter("equalizer", wxGetApp().m_MicInMidFreqHz, wxGetApp().m_MicInMidGaindB, wxGetApp().m_MicInMidQ);
    }

    // init Spk Out Equaliser Filters

    if (m_newSpkOutFilter) {
        //printf("designing new Spk Out filters\n");
        //printf("designEQFilters: wxGetApp().m_SpkOutBassFreqHz: %f\n",wxGetApp().m_SpkOutBassFreqHz);
        cb->sbqSpkOutBass   = designAnEQFilter("bass", wxGetApp().m_SpkOutBassFreqHz, wxGetApp().m_SpkOutBassGaindB);
        cb->sbqSpkOutTreble = designAnEQFilter("treble", wxGetApp().m_SpkOutTrebleFreqHz, wxGetApp().m_SpkOutTrebleGaindB);
        cb->sbqSpkOutMid    = designAnEQFilter("equalizer", wxGetApp().m_SpkOutMidFreqHz, wxGetApp().m_SpkOutMidGaindB, wxGetApp().m_SpkOutMidQ);
    }
}

void  MainFrame::deleteEQFilters(paCallBackData *cb)
{
    if (m_newMicInFilter) {
        sox_biquad_destroy(cb->sbqMicInBass);
        sox_biquad_destroy(cb->sbqMicInTreble);
        sox_biquad_destroy(cb->sbqMicInMid);
    }
    if (m_newSpkOutFilter) {
        sox_biquad_destroy(cb->sbqSpkOutBass);
        sox_biquad_destroy(cb->sbqSpkOutTreble);
        sox_biquad_destroy(cb->sbqSpkOutMid);
    }
}

// returns number of output samples generated by resampling
int resample(SRC_STATE *src,
            short      output_short[],
            short      input_short[],
            int        output_sample_rate,
            int        input_sample_rate,
            int        length_output_short, // maximum output array length in samples
            int        length_input_short
            )
{
    SRC_DATA src_data;
    float    input[N48*4];
    float    output[N48*4];
    int      ret;

    assert(src != NULL);
    assert(length_input_short <= N48*4);
    assert(length_output_short <= N48*4);

    src_short_to_float_array(input_short, input, length_input_short);

    src_data.data_in = input;
    src_data.data_out = output;
    src_data.input_frames = length_input_short;
    src_data.output_frames = length_output_short;
    src_data.end_of_input = 0;
    src_data.src_ratio = (float)output_sample_rate/input_sample_rate;

    ret = src_process(src, &src_data);
    assert(ret == 0);

    assert(src_data.output_frames_gen <= length_output_short);
    src_float_to_short_array(output, output_short, src_data.output_frames_gen);

    return src_data.output_frames_gen;
}


// Decimates samples using an algorithm that produces nice plots of
// speech signals at a low sample rate.  We want a low sample rate so
// we don't hammer the graphics system too hard.  Saves decimated data
// to a fifo for plotting on screen.
void resample_for_plot(struct FIFO *plotFifo, short buf[], int length, int fs)
{
    int decimation = fs/WAVEFORM_PLOT_FS;
    int nSamples, sample;
    int i, st, en, max, min;
    short dec_samples[length];

    nSamples = length/decimation;

    for(sample = 0; sample < nSamples; sample += 2)
    {
        st = decimation*sample;
        en = decimation*(sample+2);
        max = min = 0;
        for(i=st; i<en; i++ )
        {
            if (max < buf[i]) max = buf[i];
            if (min > buf[i]) min = buf[i];
        }
        dec_samples[sample] = max;
        dec_samples[sample+1] = min;
    }
    fifo_write(plotFifo, dec_samples, nSamples);
}

void txRxProcessing()
{

    paCallBackData  *cbData = g_rxUserdata;

    // Buffers re-used by tx and rx processing
    // signals in in48k/out48k are at a maximum sample rate of 48k, could be 44.1kHz
    // depending on sound hardware.

    short           in8k_short[4*N8];
    short           in48k_short[4*N48];
    short           out8k_short[4*N8];
    short           out48k_short[4*N48];
    int             nout, samplerate, n_samples;

    //fprintf(g_logfile, "start infifo1: %5d outfifo2: %5d\n", fifo_used(cbData->infifo1), fifo_used(cbData->outfifo2));

    // FreeDV 700 uses a modem sample rate of 7500 Hz which requires some special treatment

    if (g_analog) 
        samplerate = FS;
    else
        samplerate = freedv_get_modem_sample_rate(g_pfreedv);

    //
    //  RX side processing --------------------------------------------
    //

    // while we have enough input samples available ...

    int nsam = g_soundCard1SampleRate * (float)N8/FS;
    assert(nsam <= N48);
    g_mutexProtectingCallbackData.Lock();
    while ((fifo_read(cbData->infifo1, in48k_short, nsam) == 0) && ((g_half_duplex && !g_tx) || !g_half_duplex)) 
    {
        g_mutexProtectingCallbackData.Unlock();
        unsigned int n8k;

        n8k = resample(cbData->insrc1, in8k_short, in48k_short, samplerate, g_soundCard1SampleRate, N8, nsam);
        assert(n8k <= N8);

        // optionally save "from radio" signal (write demod input to file)
        // Really useful for testing and development as it allows us
        // to repeat tests using off air signals

        g_mutexProtectingCallbackData.Lock();
        if (g_recFileFromRadio && (g_sfRecFile != NULL)) {
            //printf("g_recFromRadioSamples: %d  n8k: %d \n", g_recFromRadioSamples);
            if (g_recFromRadioSamples < n8k) {
                sf_write_short(g_sfRecFile, in8k_short, g_recFromRadioSamples);
                wxCommandEvent event( wxEVT_COMMAND_MENU_SELECTED, g_recFileFromRadioEventId );
                // call stop/start record menu item, should be thread safe
                g_parent->GetEventHandler()->AddPendingEvent( event );
                g_recFromRadioSamples = 0;
                //printf("finished recording g_recFromRadioSamples: %d n8k: %d!\n", g_recFileFromRadio, n8k);
            }
            else {
                sf_write_short(g_sfRecFile, in8k_short, n8k);
                g_recFromRadioSamples -= n8k;
            }
        }
        g_mutexProtectingCallbackData.Unlock();

        // optionally read "from radio" signal from file (read demod input from file)

        g_mutexProtectingCallbackData.Lock();
        if (g_playFileFromRadio && (g_sfPlayFileFromRadio != NULL)) {
            unsigned int nsf = n8k*g_sfFs/samplerate;
            short        insf_short[nsf];
            unsigned int n = sf_read_short(g_sfPlayFileFromRadio, insf_short, nsf);
            n8k = resample(cbData->insrcsf, in8k_short, insf_short, samplerate, g_sfFs, N8, nsf);
            //fprintf(g_logfile, "n: %d nsnf: %d n8k: %d\n", n, nsf, n8k);
            assert(n8k <= N8);

            if (n == 0) {
                if (g_loopPlayFileFromRadio)
                    sf_seek(g_sfPlayFileFromRadio, 0, SEEK_SET);
                else {
                    printf("playFileFromRadio finished, issuing event!\n");
                    wxCommandEvent event( wxEVT_COMMAND_MENU_SELECTED, g_playFileFromRadioEventId );
                    // call stop/start play menu item, should be thread safe
                    g_parent->GetEventHandler()->AddPendingEvent( event );
                }
            }
        }
        g_mutexProtectingCallbackData.Unlock();

        resample_for_plot(g_plotDemodInFifo, in8k_short, n8k, samplerate);

        // send latest squelch level to FreeDV API, as it handles squelch internally

        freedv_set_squelch_en(g_pfreedv, g_SquelchActive);
        freedv_set_snr_squelch_thresh(g_pfreedv, g_SquelchLevel);
        //fprintf(g_logfile, "snr_squelch_thresh: %f\n",  g_pfreedv->snr_squelch_thresh);

        // Get some audio to send to headphones/speaker.  If in analog
        // mode we pass thru the "from radio" audio to the
        // headphones/speaker.

        if (g_analog) {
            memcpy(out8k_short, in8k_short, sizeof(short)*n8k);
            
            // compute rx spectrum 

            COMP  rx_fdm[n8k];
            float rx_spec[MODEM_STATS_NSPEC];
            unsigned int   i;

            for(i=0; i<n8k; i++) {
                rx_fdm[i].real = in8k_short[i];
                rx_fdm[i].imag = 0.0;
            }            
            modem_stats_get_rx_spectrum(&g_stats, rx_spec, rx_fdm, n8k);

            // Average rx spectrum data using a simple IIR low pass filter

            for(i = 0; i<MODEM_STATS_NSPEC; i++) {
                g_avmag[i] = BETA * g_avmag[i] + (1.0 - BETA) * rx_spec[i];
            }
        }
        else {
            fifo_write(cbData->rxinfifo, in8k_short, n8k);
            per_frame_rx_processing(cbData->rxoutfifo, cbData->rxinfifo);
            memset(out8k_short, 0, sizeof(short)*N8);
            fifo_read(cbData->rxoutfifo, out8k_short, N8);
        }


        // Optional Spk Out EQ Filtering, need mutex as filter can change at run time
        g_mutexProtectingCallbackData.Lock();
        if (cbData->spkOutEQEnable) {
            sox_biquad_filter(cbData->sbqSpkOutBass,   out8k_short, out8k_short, N8);
            sox_biquad_filter(cbData->sbqSpkOutTreble, out8k_short, out8k_short, N8);
            sox_biquad_filter(cbData->sbqSpkOutMid,    out8k_short, out8k_short, N8);
        }
        g_mutexProtectingCallbackData.Unlock();

        resample_for_plot(g_plotSpeechOutFifo, out8k_short, N8, FS);

        g_mutexProtectingCallbackData.Lock();
        if (g_nSoundCards == 1) {
            nout = resample(cbData->outsrc2, out48k_short, out8k_short, g_soundCard1SampleRate, FS, N48, N8);
            fifo_write(cbData->outfifo1, out48k_short, nout);
        }
        else {
            nout = resample(cbData->outsrc2, out48k_short, out8k_short, g_soundCard2SampleRate, FS, N48, N8);
            fifo_write(cbData->outfifo2, out48k_short, nout);
        }
    }
    g_mutexProtectingCallbackData.Unlock();

    //
    //  TX side processing --------------------------------------------
    //

    if ((g_nSoundCards == 2) && ((g_half_duplex && g_tx) || !g_half_duplex)) {
        int ret;

        // Make sure we have q few frames of modulator output
        // samples.  This also locks the modulator to the sample rate
        // of sound card 1.  We want to make sure that modulator
        // samples are uninterrupted by differences in sample rate
        // between this sound card and sound card 2.

        g_mutexProtectingCallbackData.Lock();
        while((unsigned)fifo_used(cbData->outfifo1) < 6*N48)
        {
            g_mutexProtectingCallbackData.Unlock();

            int   nsam = g_soundCard2SampleRate * freedv_get_n_speech_samples(g_pfreedv)/FS;
            assert(nsam <= 4*N48);

            // infifo2 is written to by another sound card so it may
            // over or underflow, but we don't realy care.  It will
            // just result in a short interruption in audio being fed
            // to codec2_enc, possibly making a click every now and
            // again in the decoded audio at the other end.

            // zero speech input just in case infifo2 underflows
            memset(in48k_short, 0, nsam*sizeof(short));
            fifo_read(cbData->infifo2, in48k_short, nsam);

            nout = resample(cbData->insrc2, in8k_short, in48k_short, FS, g_soundCard2SampleRate, 4*N8, nsam);

            // optionally use file for mic input signal

            g_mutexProtectingCallbackData.Lock();
            if (g_playFileToMicIn && (g_sfPlayFile != NULL)) {
                int n = sf_read_short(g_sfPlayFile, in8k_short, nout);
                //fprintf(stderr, "n: %d nout: %d\n", n, nout);
                if (n != nout) {
                    if (g_loopPlayFileToMicIn)
                        sf_seek(g_sfPlayFile, 0, SEEK_SET);
                    else {
                        wxCommandEvent event( wxEVT_COMMAND_MENU_SELECTED, g_playFileToMicInEventId );
                        // call stop/start play menu item, should be thread safe
                        g_parent->GetEventHandler()->AddPendingEvent( event );
                    }
                }
            }
            g_mutexProtectingCallbackData.Unlock();

            // Optional Speex pre-processor for acoustic noise reduction

            if (wxGetApp().m_speexpp_enable) {
                speex_preprocess_run(g_speex_st, in8k_short);
            }

            // Optional Mic In EQ Filtering, need mutex as filter can change at run time

            g_mutexProtectingCallbackData.Lock();
            if (cbData->micInEQEnable) {
                sox_biquad_filter(cbData->sbqMicInBass, in8k_short, in8k_short, nout);
                sox_biquad_filter(cbData->sbqMicInTreble, in8k_short, in8k_short, nout);
                sox_biquad_filter(cbData->sbqMicInMid, in8k_short, in8k_short, nout);
            }
            g_mutexProtectingCallbackData.Unlock();

            resample_for_plot(g_plotSpeechInFifo, in8k_short, nout, FS);

            n_samples = freedv_get_n_nom_modem_samples(g_pfreedv);

            if (g_analog) {
                n_samples = freedv_get_n_speech_samples(g_pfreedv);

                // Boost the "from mic" -> "to radio" audio in analog
                // mode.  The need for the gain was found by
                // experiment - analog SSB sounded too quiet compared
                // to digital. With digital voice we generally drive
                // the "to radio" (SSB radio mic input) at about 25%
                // of the peak level for normal SSB voice. So we
                // introduce 6dB gain to make analog SSB sound the
                // same level as the digital.  Watch out for clipping.
                for(int i=0; i<n_samples; i++) {
                    float out = (float)in8k_short[i]*2.0;
                    if (out > 32767) out = 32767.0;
                    if (out < -32767) out = -32767.0;
                    out8k_short[i] = out;
                }
            }
            else {
                COMP tx_fdm[freedv_get_n_nom_modem_samples(g_pfreedv)];
                COMP tx_fdm_offset[freedv_get_n_nom_modem_samples(g_pfreedv)];
                int  i;

                freedv_comptx(g_pfreedv, tx_fdm, in8k_short);
  
                freq_shift_coh(tx_fdm_offset, tx_fdm, g_TxFreqOffsetHz, freedv_get_modem_sample_rate(g_pfreedv), &g_TxFreqOffsetPhaseRect, freedv_get_n_nom_modem_samples(g_pfreedv));
                for(i=0; i<freedv_get_n_nom_modem_samples(g_pfreedv); i++)
                    out8k_short[i] = tx_fdm_offset[i].real;
            }

            // output one frame of modem signal
            nout = resample(cbData->outsrc1, out48k_short, out8k_short, g_soundCard1SampleRate, samplerate, N48*4, n_samples);
            g_mutexProtectingCallbackData.Lock();
            ret = fifo_write(cbData->outfifo1, out48k_short, nout);
            //fprintf(stderr,"nout: %d ret: %d N48*4: %d\n", nout, ret, N48*4);

            assert(ret != -1);
        }
        g_mutexProtectingCallbackData.Unlock();
    }
   
    //fprintf(g_logfile, "  end infifo1: %5d outfifo2: %5d\n", fifo_used(cbData->infifo1), fifo_used(cbData->outfifo2));

}

//----------------------------------------------------------------
// per_frame_rx_processing()
//----------------------------------------------------------------

void per_frame_rx_processing(
                                            FIFO    *output_fifo,   // decoded speech samples
                                            FIFO    *input_fifo
                                        )
{
    short               input_buf[freedv_get_n_max_modem_samples(g_pfreedv)];
    short               output_buf[freedv_get_n_speech_samples(g_pfreedv)];
    COMP                rx_fdm[freedv_get_n_max_modem_samples(g_pfreedv)];
    COMP                rx_fdm_offset[freedv_get_n_max_modem_samples(g_pfreedv)];
    float               rx_spec[MODEM_STATS_NSPEC];
    int                 i;
    int                 nin, nin_prev, nout;

    nin = freedv_nin(g_pfreedv);
    while (fifo_read(input_fifo, input_buf, nin) == 0)
    {
        assert(nin <= freedv_get_n_max_modem_samples(g_pfreedv));

        nin_prev = nin;
        //fwrite(input_buf, sizeof(short), nin, ft);

        // demod per frame processing
        for(i=0; i<nin; i++)
        {
            rx_fdm[i].real = (float)input_buf[i];
            rx_fdm[i].imag = 0.0;
        }
       
        if (g_channel_noise) {
            float snr;

            /* enough noise to get a couple of % errors */

            if (freedv_get_mode(g_pfreedv) == FREEDV_MODE_1600)
                snr = 2.0;
            if (freedv_get_mode(g_pfreedv) == FREEDV_MODE_700B)
                snr = -1.0;           
            fdmdv_simulate_channel(&g_sig_pwr_av, rx_fdm, nin, snr);
        }
        freq_shift_coh(rx_fdm_offset, rx_fdm, g_RxFreqOffsetHz, freedv_get_modem_sample_rate(g_pfreedv), &g_RxFreqOffsetPhaseRect, nin);
        nout = freedv_comprx(g_pfreedv, output_buf, rx_fdm_offset);
        
        fifo_write(output_fifo, output_buf, nout);
        
        nin = freedv_nin(g_pfreedv);
        g_State = freedv_get_sync(g_pfreedv);
        //fprintf(g_logfile, "g_State: %d g_stats.sync: %d snr: %f \n", g_State, g_stats.sync, f->snr);

        // grab extended stats so we can plot spectrum, scatter diagram etc

        freedv_get_modem_extended_stats(g_pfreedv, &g_stats);

        // compute rx spectrum 

        modem_stats_get_rx_spectrum(&g_stats, rx_spec, rx_fdm, nin_prev);
 
        // Average rx spectrum data using a simple IIR low pass filter

        for(i = 0; i<MODEM_STATS_NSPEC; i++) {
            g_avmag[i] = BETA * g_avmag[i] + (1.0 - BETA) * rx_spec[i];
        }
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

    wxMutexLocker lock(g_mutexProtectingCallbackData);

    //fprintf(g_logfile, "cb1 statusFlags: 0x%x framesPerBuffer: %d rptr: 0x%x wptr: 0x%x \n", (int)statusFlags,
    //        framesPerBuffer, rptr, wptr);

    //
    //  RX side processing --------------------------------------------
    //

    // assemble a mono buffer and write to FIFO

    assert(framesPerBuffer < MAX_FPB);

    if(rptr) {
        //fprintf(g_logfile,"in %ld %d\n",  framesPerBuffer, g_in++);
        //g_indata += framesPerBuffer;
        for(i = 0; i < framesPerBuffer; i++, rptr += cbData->inputChannels1)
            indata[i] = rptr[0];                       
        if (fifo_write(cbData->infifo1, indata, framesPerBuffer)) {
            //fprintf(g_logfile, "infifo1 full\n");
        }
        //fifo_write(cbData->outfifo1, indata, framesPerBuffer);
    }

    // OK now set up output samples for this callback

    if(wptr) {
        //fprintf(g_logfile,"out %ld %d\n",  framesPerBuffer, g_out++);
        if (fifo_read(cbData->outfifo1, outdata, framesPerBuffer) == 0) {

            // write signal to both channels

            for(i = 0; i < framesPerBuffer; i++, wptr += 2) {
                if (cbData->leftChannelVoxTone) {
                    cbData->voxTonePhase += 2.0*M_PI*VOX_TONE_FREQ/g_soundCard1SampleRate;
                    cbData->voxTonePhase -= 2.0*M_PI*floor(cbData->voxTonePhase/(2.0*M_PI));
                    wptr[0] = VOX_TONE_AMP*cos(cbData->voxTonePhase);                              
                    //printf("%f %d\n", cbData->voxTonePhase, wptr[0]);
                }
                else
                    wptr[0] = outdata[i];
                               
                wptr[1] = outdata[i];
            }
        }
        else {
            //fprintf(g_logfile, "outfifo1 empty\n");
            // zero output if no data available
            for(i = 0; i < framesPerBuffer; i++, wptr += 2) {
                wptr[0] = 0;
                wptr[1] = 0;
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

    wxMutexLocker lock(g_mutexProtectingCallbackData);

    //    if (statusFlags)
    //    printf("cb2 statusFlags: 0x%x\n", (int)statusFlags);

    // assemble a mono buffer and write to FIFO

    assert(framesPerBuffer < MAX_FPB);

    if(rptr) {
        for(i = 0; i < framesPerBuffer; i++, rptr += cbData->inputChannels2)
            indata[i] = rptr[0];                        
    }

    //#define SC2_LOOPBACK
#ifdef SC2_LOOPBACK
    //TODO: This doesn't work unless using the same soundcard!

    if(wptr) {
        for(i = 0; i < framesPerBuffer; i++, wptr += 2) {
            wptr[0] = indata[i];
            wptr[1] = indata[i];
        }
    }

#else
    if(rptr) {
        if (fifo_write(cbData->infifo2, indata, framesPerBuffer)) {
            //fprintf(g_logfile, "infifo2 full\n");
        }
    }

    // OK now set up output samples for this callback

    if(wptr) {
        if (fifo_read(cbData->outfifo2, outdata, framesPerBuffer) == 0) {
		
            // write signal to both channels */
            for(i = 0; i < framesPerBuffer; i++, wptr += 2) {
                wptr[0] = outdata[i];
                wptr[1] = outdata[i];
            }
        }
        else {
            //fprintf(g_logfile, "outfifo2 empty\n");
            // zero output if no data available
            for(i = 0; i < framesPerBuffer; i++, wptr += 2) {
                wptr[0] = 0;
                wptr[1] = 0;
            }
        }
    }
#endif
    return paContinue;
}

// Callback from plot_spectrum & plot_waterfall.  would be nice to
// work out a way to do this without globals.

void fdmdv2_clickTune(float freq) {

    // The demod is hard-wired to expect a centre frequency of
    // FDMDV_FCENTRE.  So we want to take the signal centered on the
    // click tune freq and re-centre it on FDMDV_FCENTRE.  For example
    // if the click tune freq is 1500Hz, and FDMDV_CENTRE is 1200 Hz,
    // we need to shift the input signal centred on 1500Hz down to
    // 1200Hz, an offset of -300Hz.

    if (g_split) {
        g_RxFreqOffsetHz = FDMDV_FCENTRE - freq;
    }
    else {
        g_TxFreqOffsetHz = freq - FDMDV_FCENTRE;
        g_RxFreqOffsetHz = FDMDV_FCENTRE - freq;
    }
}

//----------------------------------------------------------------
// SetupSerialPort()
//----------------------------------------------------------------
void MainFrame::SetupSerialPort(void)
{
    if(!wxGetApp().m_strRigCtrlPort.IsEmpty())
    {
        if(openComPort(wxGetApp().m_strRigCtrlPort.c_str()))
        {
            // always start PTT in Rx state
            SerialPTTRx();
        }
        else
        {
            wxMessageBox("Couldn't open Serial Port", wxT("About"), wxOK | wxICON_ERROR, this);
        }
    }
}

void MainFrame::SerialPTTRx(void)
{
    printf("m_boolUseRTS: %d m_boolRTSPos: %d m_boolUseDTR: %d m_boolDTRPos: %d\n",
           wxGetApp().m_boolUseRTS, wxGetApp().m_boolRTSPos, wxGetApp().m_boolUseDTR, wxGetApp().m_boolDTRPos);

        if(wxGetApp().m_boolRTSPos) // RTS cleared LOW
            lowerRTS();
        else                        // RTS cleared HIGH
            raiseRTS();

        if(wxGetApp().m_boolDTRPos) // DTR cleared LOW
            lowerDTR();
        else                        // DTR cleared HIGH
            raiseDTR();
}

//----------------------------------------------------------------
// CloseSerialPort()
//----------------------------------------------------------------
void MainFrame::CloseSerialPort(void)
{
    if (com_handle != COM_HANDLE_INVALID) {
        // always end with PTT in rx state

        SerialPTTRx();

		closeComPort();
    }
}


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
    printf("starting UDP thread!\n");
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
    printf("UDP thread started!\n");
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


char my_get_next_tx_char(void *callback_state) {
    short ch = 0;
    
    fifo_read(g_txDataInFifo, &ch, 1);
    //fprintf(stderr, "get_next_tx_char: %c\n", (char)ch);
    return (char)ch;
}

void my_put_next_rx_char(void *callback_state, char c) {
    short ch = (short)c;
    //fprintf(stderr, "put_next_rx_char: %c\n", (char)c);
    fifo_write(g_rxDataOutFifo, &ch, 1);
}

// Callback from FreeDv API to update error plots

void my_freedv_put_error_pattern(void *state, short error_pattern[], int sz_error_pattern) {
    fifo_write(g_error_pattern_fifo, error_pattern, sz_error_pattern);
}

void freq_shift_coh(COMP rx_fdm_fcorr[], COMP rx_fdm[], float foff, float Fs, COMP *foff_phase_rect, int nin)
{
    COMP  foff_rect;
    float mag;
    int   i;

    foff_rect.real = cosf(2.0*M_PI*foff/Fs);
    foff_rect.imag = sinf(2.0*M_PI*foff/Fs);
    for(i=0; i<nin; i++) {
	*foff_phase_rect = cmult(*foff_phase_rect, foff_rect);
	rx_fdm_fcorr[i] = cmult(rx_fdm[i], *foff_phase_rect);
    }

    /* normalise digital oscilator as the magnitude can drfift over time */

    mag = cabsolute(*foff_phase_rect);
    foff_phase_rect->real /= mag;	 
    foff_phase_rect->imag /= mag;	 
}
