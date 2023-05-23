/*
    util.h
    
    Miscellaneous utility functions
*/

#include "main.h"
#include "codec2_fdmdv.h"

#ifdef _WIN32
#include <strsafe.h>
#endif

// Callback from plot_spectrum & plot_waterfall.  would be nice to
// work out a way to do this without globals.
extern float           g_RxFreqOffsetHz;
extern float           g_TxFreqOffsetHz;
extern FreeDVInterface freedvInterface;
extern int             g_tx;

void clickTune(float freq) {

    // The demod is hard-wired to expect a centre frequency of
    // FDMDV_FCENTRE.  So we want to take the signal centered on the
    // click tune freq and re-centre it on FDMDV_FCENTRE.  For example
    // if the click tune freq is 1500Hz, and FDMDV_CENTRE is 1200 Hz,
    // we need to shift the input signal centred on 1500Hz down to
    // 1200Hz, an offset of -300Hz.

    // Bit of an "indent" as we are often trying to get it back
    // exactly in the centre

    if (fabs(FDMDV_FCENTRE - freq) < 10.0) {
        freq = FDMDV_FCENTRE;
        fprintf(stderr, "indent!\n");
    }

    g_TxFreqOffsetHz = freq - FDMDV_FCENTRE;
    g_RxFreqOffsetHz = FDMDV_FCENTRE - freq;
    fprintf(stderr, "g_TxFreqOffsetHz: %f g_RxFreqOffsetHz: %f\n", g_TxFreqOffsetHz, g_RxFreqOffsetHz);
}

bool MainApp::CanAccessSerialPort(std::string portName)
{
    bool couldOpen = true;
    com_handle_t com_handle = COM_HANDLE_INVALID;
    
#ifdef _WIN32
    {
        if (portName.substr(0, 3) != "COM")
        {
            // assume we can open if we don't have a valid port name.
            return couldOpen;
        }
        
        TCHAR  nameWithStrangePrefix[100];
        StringCchPrintf(nameWithStrangePrefix, 100, TEXT("\\\\.\\%hs"), portName.c_str());
	
        if((com_handle=CreateFile(nameWithStrangePrefix
                                   ,GENERIC_READ | GENERIC_WRITE/* Access */
                                   ,0				/* Share mode */
                                   ,NULL		 	/* Security attributes */
                                   ,OPEN_EXISTING		/* Create access */
                                   ,0                           /* File attributes */
                                   ,NULL		        /* Template */
                                   ))==INVALID_HANDLE_VALUE) {
           couldOpen = false;
    	}
        else
        {
            CloseHandle(com_handle);
        }
    }
#else
	{
        if (portName.substr(0, 5) != "/dev/")
        {
            // assume we can open if we don't have a valid port name.
            return couldOpen;
        }
        
		if((com_handle=open(portName.c_str(), O_NONBLOCK|O_RDWR))== COM_HANDLE_INVALID)
        {
            couldOpen = false;
        }
        else
        {
            close(com_handle);
        }
	}
#endif
    
    if (!couldOpen)
    {
        std::string errorMessage = "Could not open serial port " + portName + ".";
        
        #ifdef _WIN32
        errorMessage += " Please ensure that no other applications are accessing the port.";
        #elif __linux
        errorMessage += " Please ensure that you have permission to access the port. Adding yourself to the 'dialout' group (and logging out/back in) along with reattaching your radio to your PC will typically ensure this.";
        #else
        errorMessage += " Please ensure that you have permission to access the port.";
        #endif
         
        CallAfter([&, errorMessage]() {
            wxMessageBox(
                errorMessage, 
                wxT("Error"), wxOK | wxICON_ERROR, GetTopWindow());
        });
    }
    
    return couldOpen;
}

//----------------------------------------------------------------
// OpenSerialPort()
//----------------------------------------------------------------

void MainFrame::OpenSerialPort(void)
{
    Serialport *serialport = wxGetApp().m_serialport;

    if(!wxGetApp().m_strRigCtrlPort.IsEmpty()) 
    {
        if (wxGetApp().CanAccessSerialPort((const char*)wxGetApp().m_strRigCtrlPort.ToUTF8()))
        {
            serialport->openport(wxGetApp().m_strRigCtrlPort.c_str(),
                    wxGetApp().m_boolUseRTS,
                    wxGetApp().m_boolRTSPos,
                    wxGetApp().m_boolUseDTR,
                    wxGetApp().m_boolDTRPos);
            if (serialport->isopen()) 
            {
                // always start PTT in Rx state
                serialport->ptt(false);
            }
            else 
            {
                CallAfter([&]() 
                {
                    wxMessageBox("Couldn't open serial port for PTT output", wxT("Error"), wxOK | wxICON_ERROR, this);
                });
            }
        }
    }
}


//----------------------------------------------------------------
// CloseSerialPort()
//----------------------------------------------------------------

void MainFrame::CloseSerialPort(void)
{
    Serialport *serialport = wxGetApp().m_serialport;
    if (serialport->isopen()) {
        // always end with PTT in rx state

        serialport->ptt(false);
        serialport->closeport();
    }
}


//----------------------------------------------------------------
// OpenPTTInPort()
//----------------------------------------------------------------

void MainFrame::OpenPTTInPort(void)
{
    Serialport *serialport = wxGetApp().m_pttInSerialPort;

    if(!wxGetApp().m_strPTTInputPort.IsEmpty()) 
    {
        if (wxGetApp().CanAccessSerialPort((const char*)wxGetApp().m_strPTTInputPort.ToUTF8()))
        {
            serialport->openport(
                wxGetApp().m_strPTTInputPort.c_str(),
                false,
                false,
                false,
                false);
            if (!serialport->isopen()) 
            {
                CallAfter([&]() 
                {
                    wxMessageBox("Couldn't open PTT input port", wxT("Error"), wxOK | wxICON_ERROR, this);
                });
            } 
            else 
            {
                // Set up PTT monitoring. When PTT state changes, we should also change 
                // the PTT state in th app.
                serialport->enablePttInputMonitoring(wxGetApp().m_boolCTSPos, [&](bool pttState) {
                    fprintf(stderr, "PTT input state is now %d\n", pttState);
                    GetEventHandler()->CallAfter([&]() { 
                        m_btnTogPTT->SetValue(pttState); 
                        togglePTT(); 
                    });
                });
            }
        }
    }
}


//----------------------------------------------------------------
// ClosePTTInPort()
//----------------------------------------------------------------

void MainFrame::ClosePTTInPort(void)
{
    Serialport *serialport = wxGetApp().m_pttInSerialPort;
    if (serialport->isopen()) {
        serialport->closeport();
    }
}

struct FIFO extern  *g_txDataInFifo;
struct FIFO extern *g_rxDataOutFifo;

char my_get_next_tx_char(void *callback_state) {
    short ch = 0;

    codec2_fifo_read(g_txDataInFifo, &ch, 1);
    //fprintf(stderr, "get_next_tx_char: %c\n", (char)ch);
    return (char)ch;
}

void my_put_next_rx_char(void *callback_state, char c) {
    short ch = (short)((unsigned char)c);
    //fprintf(stderr, "put_next_rx_char: %c\n", (char)c);
    codec2_fifo_write(g_rxDataOutFifo, &ch, 1);
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

    /* normalise digital oscillator as the magnitude can drift over time */

    mag = cabsolute(*foff_phase_rect);
    foff_phase_rect->real /= mag;
    foff_phase_rect->imag /= mag;
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
    float    input[length_input_short];
    float    output[length_output_short];
    int      ret;

    assert(src != NULL);

    src_short_to_float_array(input_short, input, length_input_short);

    src_data.data_in = input;
    src_data.data_out = output;
    src_data.input_frames = length_input_short;
    src_data.output_frames = length_output_short;
    src_data.end_of_input = 0;
    src_data.src_ratio = (float)output_sample_rate/input_sample_rate;

    ret = src_process(src, &src_data);
    if (ret != 0)
    {
        fprintf(stderr, "WARNING: resampling failed: %s\n", src_strerror(ret));
    }
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
    codec2_fifo_write(plotFifo, dec_samples, nSamples);
}

// State machine to detect sync and send a UDP message

void MainFrame::DetectSyncProcessEvent(void) {
    int next_state = ds_state;

    switch(ds_state) {

    case DS_IDLE:
        if (freedvInterface.getSync() == 1) {
            next_state = DS_SYNC_WAIT;
            ds_rx_time = 0;
        }
        break;

    case DS_SYNC_WAIT:

        // In this state we wait for a few seconds of valid sync, then
        // send UDP message

        if (freedvInterface.getSync() == 0) {
            next_state = DS_IDLE;
        } else {
            ds_rx_time += DT;
        }

        if (ds_rx_time >= DS_SYNC_WAIT_TIME) {
            char s[100]; snprintf(s, 100, "rx sync");
            if (wxGetApp().m_udp_enable) {
                UDPSend(wxGetApp().m_udp_port, s, strlen(s)+1);
            }
            ds_rx_time = 0;
            next_state = DS_UNSYNC_WAIT;
        }
        break;

    case DS_UNSYNC_WAIT:

        // In this state we wait for sync to end

        if (freedvInterface.getSync() == 0) {
            ds_rx_time += DT;
            if (ds_rx_time >= DS_SYNC_WAIT_TIME) {
                next_state = DS_IDLE;
            }
        } else {
            ds_rx_time = 0;
        }
        break;

    default:
        // catch anything we missed

        next_state = DS_IDLE;
    }

    ds_state = next_state;
}


