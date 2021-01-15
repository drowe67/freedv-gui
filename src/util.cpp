/*
    util.h
    
    Miscellaneous utility functions
*/

#include "main.h"

// Callback from plot_spectrum & plot_waterfall.  would be nice to
// work out a way to do this without globals.
extern float  g_RxFreqOffsetHz;
extern float  g_TxFreqOffsetHz;
extern int *g_split;

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

    if (g_split) {
        g_RxFreqOffsetHz = FDMDV_FCENTRE - freq;
    }
    else {
        g_TxFreqOffsetHz = freq - FDMDV_FCENTRE;
        g_RxFreqOffsetHz = FDMDV_FCENTRE - freq;
    }
    fprintf(stderr, "g_TxFreqOffsetHz: %f g_RxFreqOffsetHz: %f\n", g_TxFreqOffsetHz, g_RxFreqOffsetHz);
}

//----------------------------------------------------------------
// OpenSerialPort()
//----------------------------------------------------------------

void MainFrame::OpenSerialPort(void)
{
    Serialport *serialport = wxGetApp().m_serialport;

    if(!wxGetApp().m_strRigCtrlPort.IsEmpty()) {
       serialport->openport(wxGetApp().m_strRigCtrlPort.c_str(),
                            wxGetApp().m_boolUseRTS,
                            wxGetApp().m_boolRTSPos,
                            wxGetApp().m_boolUseDTR,
                            wxGetApp().m_boolDTRPos);
       if (serialport->isopen()) {
            // always start PTT in Rx state
           serialport->ptt(false);
       }
       else {
           wxMessageBox("Couldn't open Serial Port", wxT("About"), wxOK | wxICON_ERROR, this);
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

//
// checkAvxSupport
//
// Tests the underlying platform for AVX support.  2020 needs AVX support to run
// in real-time, and old processors do not offer AVX support
//

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
void MainFrame::checkAvxSupport(void)
{

    isAvxPresent = false;
    uint32_t eax, ebx, ecx, edx;
    eax = ebx = ecx = edx = 0;
    __cpuid(1, eax, ebx, ecx, edx);

    if (ecx & (1<<27) && ecx & (1<<28)) {
        // CPU supports XSAVE and AVX
        uint32_t xcr0, xcr0_high;
        asm("xgetbv" : "=a" (xcr0), "=d" (xcr0_high) : "c" (0));
        isAvxPresent = (xcr0 & 6) == 6;    // AVX state saving enabled?
    }
}
#elif defined(__APPLE__) && defined(__aarch64__)
void MainFrame::checkAvxSupport(void)
{
    // Force 2020 mode to be enabled on ARM Macs. This is experimental
    // and may cause problems. During preliminary testing it seems to use
    // NEON optimizations enabled in LPCNet and consumes less than 100% of a single
    // core while decoding audio.
    isAvxPresent = true;
}
#else
void MainFrame::checkAvxSupport(void)
{
    isAvxPresent = false;
}
#endif

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

// Callback from FreeDv API to update error plots
struct FIFO extern *g_error_pattern_fifo;
void my_freedv_put_error_pattern(void *state, short error_pattern[], int sz_error_pattern) {
    codec2_fifo_write(g_error_pattern_fifo, error_pattern, sz_error_pattern);
    //fprintf(stderr, "my_freedv_put_error_pattern: sz_error_pattern: %d ret: %d used: %d\n",
    //        sz_error_pattern, ret, codec2_fifo_used(g_error_pattern_fifo) );
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

    /* normalise digital oscilator as the magnitude can drift over time */

    mag = cabsolute(*foff_phase_rect);
    foff_phase_rect->real /= mag;
    foff_phase_rect->imag /= mag;
}

