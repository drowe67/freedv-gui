//==========================================================================
// Name:            main.h
//
// Purpose:         Declares simple wxWidgets application with GUI.
// Created:         Apr. 9, 2012
// Authors:         David Rowe, David Witten
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================
#ifndef __FDMDV2_MAIN__
#define __FDMDV2_MAIN__

#include "config.h"
#include <wx/wx.h>

#include <wx/tglbtn.h>
#include <wx/app.h>
#include "wx/rawbmp.h"
#include "wx/file.h"
#include "wx/filename.h"
#include "wx/config.h"
#include <wx/fileconf.h>
#include "wx/graphics.h"
#include "wx/mstream.h"
#include "wx/wfstream.h"
#include "wx/quantize.h"
#include "wx/scopedptr.h"
#include "wx/stopwatch.h"
#include "wx/versioninfo.h"
#include <wx/sound.h>
#include <wx/url.h>
#include <wx/sstream.h>
#include <wx/listbox.h>
#include <wx/textdlg.h>
#include <wx/regex.h>
#include <wx/socket.h>

#include <samplerate.h>

#include <stdint.h>
#include <speex/speex_preprocess.h>
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#include <cpuid.h>
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#endif

#ifdef _MSC_VER
// used for AVX checking
#include <intrin.h>
#endif

#include "codec2.h"
#include "codec2_fifo.h"
#include "modem_stats.h"

#include "topFrame.h"
#include "dlg_easy_setup.h"
#include "dlg_ptt.h"
#include "dlg_options.h"
#include "plot.h"
#include "plot_scalar.h"
#include "plot_scatter.h"
#include "plot_waterfall.h"
#include "plot_spectrum.h"
#include "sndfile.h"
#include "dlg_audiooptions.h"
#include "dlg_filter.h"
#include "dlg_options.h"
#include "sox_biquad.h"
#include "comp_prim.h"
#include "hamlib.h"
#include "serialport.h" 
#include "pskreporter.h"
#include "freedv_interface.h"
#include "audio/AudioEngineFactory.h"
#include "audio/IAudioDevice.h"
#include "pipeline/paCallbackData.h"

#define _USE_TIMER              1
#define _USE_ONIDLE             1
#define _DUMMY_DATA             1
//#define _AUDIO_PASSTHROUGH    1
#define _REFRESH_TIMER_PERIOD   (DT*1000)

//#define _USE_ABOUT_DIALOG       1

enum {
        ID_START = wxID_HIGHEST,
        ID_TIMER_WATERFALL,
        ID_TIMER_SPECTRUM,
        ID_TIMER_SCATTER,
        ID_TIMER_SCALAR,
        ID_TIMER_PSKREPORTER,
        ID_TIMER_UPD_FREQ,
     };

#define EXCHANGE_DATA_IN    0
#define EXCHANGE_DATA_OUT   1

extern int                 g_verbose;
extern int                 g_nSoundCards;

// Voice Keyer Constants

#define VK_SYNC_WAIT_TIME 5.0

// Voice Keyer States

#define VK_IDLE      0
#define VK_TX        1
#define VK_RX        2
#define VK_SYNC_WAIT 3

// Voice Keyer Events

#define VK_START         0
#define VK_SPACE_BAR     1
#define VK_PLAY_FINISHED 2
#define VK_DT            3
#define VK_SYNC          4

// "Detect Sync" state machine states and constants

#define DS_IDLE           0
#define DS_SYNC_WAIT      1
#define DS_UNSYNC_WAIT    2
#define DS_SYNC_WAIT_TIME 5.0

class MainFrame;
class FilterDlg;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class MainApp
//
// @class $(Name)
// @author $(User)
// @date $(Date)
// @file $(CurrentFileName).$(CurrentFileExt)
// @brief
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class MainApp : public wxApp
{
    public:
        virtual bool        OnInit();
        virtual void        OnInitCmdLine(wxCmdLineParser& parser);
        virtual bool        OnCmdLineParsed(wxCmdLineParser& parser);
        virtual int         OnExit();


        bool                    CanAccessSerialPort(std::string portName);
        
        wxString            m_strVendName;
        wxString            m_StrAppName;

        wxString            m_textNumChOut;
        wxString            m_textNumChIn;

        wxString            m_strRxInAudio;
        wxString            m_strRxOutAudio;
        wxString            m_textVoiceInput;
        wxString            m_textVoiceOutput;
        wxString            m_strSampleRate;
        wxString            m_strBitrate;

        // Sound card
        wxString m_soundCard1InDeviceName;
        int m_soundCard1InSampleRate;
        wxString m_soundCard2InDeviceName;
        int m_soundCard2InSampleRate;
        wxString m_soundCard1OutDeviceName;
        int m_soundCard1OutSampleRate;
        wxString m_soundCard2OutDeviceName;
        int m_soundCard2OutSampleRate;
        
        // PTT -----------------------------------

        bool                m_boolHalfDuplex;
        bool                m_boolMultipleRx;
        bool                m_boolSingleRxThread;
        
        wxString            m_txtVoiceKeyerWaveFilePath;
        wxString            m_txtVoiceKeyerWaveFile;
        int                 m_intVoiceKeyerRxPause;
        int                 m_intVoiceKeyerRepeats;

        wxString            m_txtQuickRecordPath;
    
        bool                m_boolHamlibUseForPTT;
        unsigned int        m_intHamlibRig;
        wxString            m_strHamlibRigName;
        wxString            m_strHamlibSerialPort;
        unsigned int        m_intHamlibSerialRate;
        unsigned int        m_intHamlibIcomCIVHex;
        Hamlib              *m_hamlib;

        bool                m_boolUseSerialPTT;
        wxString            m_strRigCtrlPort;
        bool                m_boolUseRTS;
        bool                m_boolRTSPos;
        bool                m_boolUseDTR;
        bool                m_boolDTRPos;
        Serialport         *m_serialport;

        // PTT Input
        bool                m_boolUseSerialPTTInput;
        wxString            m_strPTTInputPort;
        bool                m_boolCTSPos;
        Serialport         *m_pttInSerialPort;
        
        // Play/Rec files

        wxString            m_playFileToMicInPath;
        wxString            m_recFileFromRadioPath;
        wxString            m_recFileFromModulatorPath;
        unsigned int        m_recFileFromRadioSecs;
        unsigned int        m_recFileFromModulatorSecs;
        wxString            m_playFileFromRadioPath;

        // Options dialog

        wxString            m_callSign;
        unsigned int        m_textEncoding;
        bool                m_snrSlow;
        unsigned int        m_statsResetTimeSec;

        // LPC Post Filter
        bool                m_codec2LPCPostFilterEnable;
        bool                m_codec2LPCPostFilterBassBoost;
        float               m_codec2LPCPostFilterGamma;
        float               m_codec2LPCPostFilterBeta;

        // Speex Pre-Processor
        bool                m_speexpp_enable;
        // Codec 2 700C Equaliser
        bool                m_700C_EQ;

        // Mic In Equaliser
        float               m_MicInBassFreqHz;
        float               m_MicInBassGaindB;
        float               m_MicInTrebleFreqHz;
        float               m_MicInTrebleGaindB;
        float               m_MicInMidFreqHz;
        float               m_MicInMidGaindB;
        float               m_MicInMidQ;
        bool                m_MicInEQEnable;
        float               m_MicInVolInDB;

        // Spk Out Equaliser
        float               m_SpkOutBassFreqHz;
        float               m_SpkOutBassGaindB;
        float               m_SpkOutTrebleFreqHz;
        float               m_SpkOutTrebleGaindB;
        float               m_SpkOutMidFreqHz;
        float               m_SpkOutMidGaindB;
        float               m_SpkOutMidQ;
        bool                m_SpkOutEQEnable;
        float               m_SpkOutVolInDB;

        // optional vox trigger tone
        bool                m_leftChannelVoxTone;

        // UDP control port
        bool                m_udp_enable;
        int                 m_udp_port;

        // notebook display after tx->rxtransition
        int                 m_rxNbookCtrl;

        wxRect              m_rTopWindow;

        int                 m_fifoSize_ms;

        // PSK Reporter configuration
        bool                m_psk_enable;
        wxString            m_psk_callsign;
        wxString            m_psk_grid_square;
        uint64_t            m_psk_freq;
        
        // Callsign list configuration
        bool                m_useUTCTime;

        PskReporter*            m_pskReporter;
        
        // Waterfall display
        int                 m_waterfallColor;
        
        bool                loadConfig();
        bool                saveConfig();

        // misc

        bool       m_testFrames;
        bool       m_channel_noise;
        float      m_channel_snr_dB;

        int        FilterEvent(wxEvent& event);
        MainFrame *frame;

        // 700 options

        bool       m_FreeDV700txClip;
        bool       m_FreeDV700txBPF;
        bool       m_FreeDV700Combine;

        // Noise simulation

        int        m_noise_snr;

        // carrier attenuation

        bool       m_attn_carrier_en;
        int        m_attn_carrier;

        // tone interferer simulation

        bool       m_tone;
        int        m_tone_freq_hz;
        int        m_tone_amplitude;

        // Windows debug console

        bool       m_debug_console;

        // debugging 700D audio break up

        bool       m_txRxThreadHighPriority;

        int        m_prevMode;
        
        bool       m_firstTimeUse;
        bool       m_2020Allowed;

    protected:
};

// declare global static function wxGetApp()
DECLARE_APP(MainApp)

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// panel with custom loop checkbox for play file dialog
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class MyExtraPlayFilePanel : public wxPanel
{
public:
    MyExtraPlayFilePanel(wxWindow *parent);
    void setLoopPlayFileToMicIn(bool checked) { m_cb->SetValue(checked); }
    bool getLoopPlayFileToMicIn(void) { return m_cb->GetValue(); }
private:
    wxCheckBox *m_cb;
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// panel with custom Seconds-to-record control for record file dialog
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class MyExtraRecFilePanel : public wxPanel
{
public:
    MyExtraRecFilePanel(wxWindow *parent);
    ~MyExtraRecFilePanel()
    {
        wxLogDebug("Destructor\n");
    }
    void setSecondsToRecord(wxString value) { m_secondsToRecord->SetValue(value); }
    wxString getSecondsToRecord(void)
    {
        wxLogDebug("getSecondsToRecord: %s\n",m_secondsToRecord->GetValue());
        return m_secondsToRecord->GetValue();
    }
private:
    wxTextCtrl *m_secondsToRecord;
};

class TxRxThread;
class UDPThread;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class MainFrame
//
// @class $(Name)
// @author $(User)
// @date $(Date)
// @file $(CurrentFileName).$(CurrentFileExt)
// @brief
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class MainFrame : public TopFrame
{
    public:
        MainFrame(wxWindow *parent);
        virtual ~MainFrame();

        FilterDlg*              m_filterDialog;
        PlotSpectrum*           m_panelSpectrum;
        PlotWaterfall*          m_panelWaterfall;
        PlotScatter*            m_panelScatter;
        PlotScalar*             m_panelTimeOffset;
        PlotScalar*             m_panelFreqOffset;
        PlotScalar*             m_panelSpeechIn;
        PlotScalar*             m_panelSpeechOut;
        PlotScalar*             m_panelDemodIn;
        PlotScalar*             m_panelTestFrameErrors;
        PlotScalar*             m_panelTestFrameErrorsHist;

        bool                    m_RxRunning;

        TxRxThread*             m_txThread;
        TxRxThread*             m_rxThread;
        
        bool                    OpenHamlibRig();
        void                    OpenSerialPort(void);
        void                    CloseSerialPort(void);
        void                    OpenPTTInPort(void);
        void                    ClosePTTInPort(void);

        bool                    m_modal;

#ifdef _USE_TIMER
        wxTimer                 m_plotTimer;
        
        // Not sure why we have the option to disable timers. TBD?
        wxTimer                 m_pskReporterTimer;
        wxTimer                 m_updFreqStatusTimer; //[UP]
#endif

    void destroy_fifos(void);

    void togglePTT(void);

    wxIPV4address           m_udp_addr;
    wxDatagramSocket       *m_udp_sock;
    UDPThread              *m_UDPThread;
    void                    startUDPThread(void);
    void                    stopUDPThread(void);
    int                     PollUDP();
    bool                    m_schedule_restore;

    // Voice Keyer state machine

    int                     vk_state;
    void VoiceKeyerProcessEvent(int vk_event);

    // Detect Sync state machine

    int                     ds_state;
    float                   ds_rx_time;
    void DetectSyncProcessEvent(void);

        void StopPlayFileToMicIn(void);
        void StopPlaybackFileFromRadio();
        void StopRecFileFromModulator();
        void StopRecFileFromRadio();
        
    protected:

        void setsnrBeta(bool snrSlow);

        // protected event handlers
        virtual void topFrame_OnSize( wxSizeEvent& event );
        virtual void OnCloseFrame(wxCloseEvent& event);
        void OnExitClick(wxCommandEvent& event);

        void startTxStream();
        void startRxStream();
        void stopTxStream();
        void stopRxStream();
        void abortTxStream();
        void abortRxStream();

        void OnTop(wxCommandEvent& event);
        void OnExit( wxCommandEvent& event );

        void OnToolsEasySetup( wxCommandEvent& event );
        void OnToolsEasySetupUI( wxUpdateUIEvent& event );
        void OnToolsAudio( wxCommandEvent& event );
        void OnToolsAudioUI( wxUpdateUIEvent& event );
        void OnToolsComCfg( wxCommandEvent& event );
        void OnToolsComCfgUI( wxUpdateUIEvent& event );
        void OnToolsFilter( wxCommandEvent& event );
        void OnToolsOptions(wxCommandEvent& event);
        void OnToolsOptionsUI(wxUpdateUIEvent& event);

        void OnPlayFileToMicIn( wxCommandEvent& event );
        void OnRecFileFromRadio( wxCommandEvent& event );
        void OnRecFileFromModulator( wxCommandEvent& event);
        void OnPlayFileFromRadio( wxCommandEvent& event );

        void OnHelpCheckUpdates( wxCommandEvent& event );
        void OnHelpCheckUpdatesUI( wxUpdateUIEvent& event );
        void OnHelpAbout( wxCommandEvent& event );
        void OnHelpManual( wxCommandEvent& event );
        void OnCmdSliderScroll( wxScrollEvent& event );
        void OnCheckSQClick( wxCommandEvent& event );
        void OnCheckSNRClick( wxCommandEvent& event );

        // Toggle Buttons
        void OnTogBtnSplitClick(wxCommandEvent& event);
        void OnTogBtnAnalogClick(wxCommandEvent& event);
        void OnTogBtnPTT( wxCommandEvent& event );
        void OnTogBtnVoiceKeyerClick (wxCommandEvent& event);
        void OnTogBtnOnOff( wxCommandEvent& event );
        void OnTogBtnRecord( wxCommandEvent& event );

        void OnCallSignReset( wxCommandEvent& event );
        void OnBerReset( wxCommandEvent& event );
        void OnReSync( wxCommandEvent& event );

        //System Events
        void OnPaint(wxPaintEvent& event);
        void OnSize( wxSizeEvent& event );
        void OnUpdateUI( wxUpdateUIEvent& event );
        void OnDeleteConfig(wxCommandEvent&);
        void OnDeleteConfigUI( wxUpdateUIEvent& event );
#ifdef _USE_TIMER
        void OnTimer(wxTimerEvent &evt);
#endif
#ifdef _USE_ONIDLE
        void OnIdle(wxIdleEvent &evt);
#endif

        int VoiceKeyerStartTx(void);

        void OnChangeTxMode( wxCommandEvent& event );
        
        void OnChangeTxLevel( wxScrollEvent& event );
        
        void OnChangeReportFrequency( wxCommandEvent& event );
    private:
        std::shared_ptr<IAudioDevice> rxInSoundDevice;
        std::shared_ptr<IAudioDevice> rxOutSoundDevice;
        std::shared_ptr<IAudioDevice> txInSoundDevice;
        std::shared_ptr<IAudioDevice> txOutSoundDevice;
        
        unsigned int         m_timeSinceSyncLoss;
        bool        m_useMemory;
        wxTextCtrl* m_tc;
        int         m_zoom;
        float       m_snrBeta;

        // Callsign/text messaging
        char        m_callsign[MAX_CALLSIGN];
        char       *m_pcallsign;

        // Events
        void        processTxtEvent(char event[]);
        class OptionsDlg *optionsDlg;

        // level Gauge
        float       m_maxLevel;

        // flags to indicate when new EQ filters need to be designed

        bool        m_newMicInFilter;
        bool        m_newSpkOutFilter;

        void*       designAnEQFilter(const char filterType[], float freqHz, float gaindB, float Q = 0.0, int sampleRate = 8000);
        void        designEQFilters(paCallBackData *cb, int rxSampleRate, int txSampleRate);
        void        deleteEQFilters(paCallBackData *cb);

        // Voice Keyer States

        int        vk_rx_pause;
        int        vk_repeats, vk_repeat_counter;
        float      vk_rx_time;
        float      vk_rx_sync_time;
        
        int         getSoundCardIDFromName(wxString& name, bool input);
        bool        validateSoundCardSetup();
        
        void loadConfiguration_();
        void resetStats_();

#if defined(FREEDV_MODE_2020)
        void test2020Mode_();
#endif // defined(FREEDV_MODE_2020)
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// class UDPThread - waits for UDP messages
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class UDPThread : public wxThread
{
public:
    UDPThread(void) : wxThread(wxTHREAD_JOINABLE) { m_run = 1; }

    // thread execution starts here
    void *Entry();

    // called when the thread exits - whether it terminates normally or is
    // stopped with Delete() (but not when it is Kill()ed!)
    void OnExit() { }

public:
    MainFrame              *mf;
    bool                    m_run;
};

void resample_for_plot(struct FIFO *plotFifo, short buf[], int length, int fs);

int resample(SRC_STATE *src,
             short      output_short[],
             short      input_short[],
             int        output_sample_rate,
             int        input_sample_rate,
             int        length_output_short, // maximum output array length in samples
             int        length_input_short
             );
void txRxProcessing();

// FreeDv API calls this when there is a test frame that needs a-plottin'

void my_freedv_put_error_pattern(void *state, short error_pattern[], int sz_error_pattern);

// FreeDv API calls these puppies when it needs/receives a text char

char my_get_next_tx_char(void *callback_state);
void my_put_next_rx_char(void *callback_state, char c);

// helper complex freq shift function

void freq_shift_coh(COMP rx_fdm_fcorr[], COMP rx_fdm[], float foff, float Fs, COMP *foff_phase_rect, int nin);

void UDPSend(int port, char *buf, unsigned int n);
void UDPInit(void);

#endif //__FDMDV2_MAIN__
