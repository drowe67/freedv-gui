//=========================================================================
// Name:            TxRxThread.h
// Purpose:         Implements the main processing thread for audio I/O.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
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
//=========================================================================

// This forces us to use freedv-gui's version rather than another one.
// TBD -- may not be needed once we fully switch over to the audio pipeline.
#include "../defines.h"

#include "TxRxThread.h"
#include "paCallbackData.h"

#include "PlaybackStep.h"
#include "EitherOrStep.h"
#include "SpeexStep.h"
#include "EqualizerStep.h"
#include "ResamplePlotStep.h"
#include "ResampleStep.h"
#include "TapStep.h"
#include "LevelAdjustStep.h"
#include "FreeDVTransmitStep.h"
#include "RecordStep.h"

#include <wx/stopwatch.h>

// External globals
// TBD -- work on fully removing the need for these.
extern paCallBackData* g_rxUserdata;
extern int g_analog;
extern int g_nSoundCards;
extern bool g_half_duplex;
extern int g_tx;
extern int g_soundCard1SampleRate;
extern int g_soundCard2SampleRate;
extern int g_dump_fifo_state;
extern int g_verbose;
extern bool endingTx;
extern bool g_playFileToMicIn;
extern int g_sfTxFs;
extern bool g_loopPlayFileToMicIn;
extern float g_TxFreqOffsetHz;
extern struct FIFO* g_plotSpeechInFifo;
extern struct FIFO* g_plotDemodInFifo;
extern struct FIFO* g_plotSpeechOutFifo;
extern int g_mode;
extern bool g_recFileFromModulator;
extern int g_recFromModulatorSamples;
extern int g_txLevel;
extern int g_dump_timing;
extern bool g_queueResync;
extern int g_resyncs;
extern bool g_recFileFromRadio;
extern unsigned int g_recFromRadioSamples;
extern bool g_playFileFromRadio;
extern int g_sfFs;
extern bool g_loopPlayFileFromRadio;
extern int g_SquelchActive;
extern float g_SquelchLevel;
extern float g_tone_phase;
extern float g_avmag[MODEM_STATS_NSPEC];
extern int g_State;
extern int g_channel_noise;
extern float g_RxFreqOffsetHz;
extern float g_sig_pwr_av;

#include <speex/speex_preprocess.h>
extern SpeexPreprocessState* g_speex_st;

#include "../freedv_interface.h"
extern FreeDVInterface freedvInterface;

#include <wx/wx.h>
#include "../main.h"
extern wxMutex txModeChangeMutex;
extern wxMutex g_mutexProtectingCallbackData;
extern wxWindow* g_parent;

#include <samplerate.h>
extern SRC_STATE* g_spec_src;

#include <sndfile.h>
extern SNDFILE* g_sfPlayFile;
extern SNDFILE* g_sfRecFileFromModulator;
extern SNDFILE* g_sfRecFile;
extern SNDFILE* g_sfPlayFileFromRadio;

// TBD -- shouldn't be needed once we've fully converted over
extern int resample(SRC_STATE *src,
            short      output_short[],
            short      input_short[],
            int        output_sample_rate,
            int        input_sample_rate,
            int        length_output_short, // maximum output array length in samples
            int        length_input_short
            );
#include "sox_biquad.h"

void TxRxThread::initializePipeline_()
{
    if (m_tx)
    {
        pipeline_ = std::shared_ptr<AudioPipeline>(new AudioPipeline(g_soundCard2SampleRate, g_soundCard1SampleRate));
        
        // Mic In playback step (optional)
        auto eitherOrBypassPlay = new AudioPipeline(g_soundCard2SampleRate, g_soundCard2SampleRate);
        auto eitherOrPlayMicIn = new AudioPipeline(g_soundCard2SampleRate, g_soundCard2SampleRate);
        auto playMicIn = new PlaybackStep(
            g_soundCard2SampleRate, 
            []() { return g_sfTxFs; },
            []() { return g_sfPlayFile; },
            []() {
                if (g_loopPlayFileToMicIn)
                    sf_seek(g_sfPlayFile, 0, SEEK_SET);
                else {
                    printf("playFileFromRadio finished, issuing event!\n");
                    g_parent->CallAfter(&MainFrame::StopPlayFileToMicIn);
                }
            }
            );
        eitherOrPlayMicIn->appendPipelineStep(std::shared_ptr<IPipelineStep>(playMicIn));
        
        auto eitherOrPlayStep = new EitherOrStep(
            []() { return g_playFileToMicIn && (g_sfPlayFile != NULL); },
            std::shared_ptr<IPipelineStep>(eitherOrPlayMicIn),
            std::shared_ptr<IPipelineStep>(eitherOrBypassPlay));
            
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrPlayStep));
        
        // Speex step (optional based on g_speex_st)
        auto speexStep = new SpeexStep(g_soundCard2SampleRate, &g_speex_st);
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(speexStep));
        
        // Equalizer step (optional based on filter state)
        auto equalizerStep = new EqualizerStep(
            g_soundCard2SampleRate, 
            &g_rxUserdata->sbqMicInBass,
            &g_rxUserdata->sbqMicInMid,
            &g_rxUserdata->sbqMicInTreble,
            &g_rxUserdata->sbqMicInVol);
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(equalizerStep));
        
        // Resample for plot step
        auto resampleForPlotStep = new ResampleForPlotStep(g_soundCard2SampleRate, g_plotSpeechInFifo);
        auto resampleForPlotTap = new TapStep(g_soundCard2SampleRate, resampleForPlotStep);
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(resampleForPlotTap));
        
        // FreeDV TX step (analog leg)
        auto doubleLevelStep = new LevelAdjustStep(g_soundCard2SampleRate, []() { return 2.0; });
        auto analogTxPipeline = new AudioPipeline(g_soundCard2SampleRate, g_soundCard1SampleRate);
        analogTxPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(doubleLevelStep));
        
        auto digitalTxStep = new FreeDVTransmitStep(freedvInterface, []() { return g_TxFreqOffsetHz; });
        auto digitalTxPipeline = new AudioPipeline(g_soundCard2SampleRate, g_soundCard1SampleRate); 
        digitalTxPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(digitalTxStep));
        
        auto eitherOrDigitalAnalog = new EitherOrStep(
            []() { return g_analog; },
            std::shared_ptr<IPipelineStep>(analogTxPipeline),
            std::shared_ptr<IPipelineStep>(digitalTxPipeline));
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrDigitalAnalog));
        
        // Record modulated output (optional)
        auto recordModulatedStep = new RecordStep(
            g_soundCard1SampleRate, 
            []() { return g_sfRecFileFromModulator; }, 
            [](int numSamples) {
                g_recFromModulatorSamples -= numSamples;
                if (g_recFromModulatorSamples <= 0)
                {
                    // call stop record menu item, should be thread safe
                    g_parent->CallAfter(&MainFrame::StopRecFileFromModulator);
                
                    wxPrintf("write mod output to file complete\n", g_recFromModulatorSamples);  // consider a popup
                }
            });
        auto recordModulatedTap = new TapStep(g_soundCard1SampleRate, recordModulatedStep);
        auto bypassRecordModulated = new AudioPipeline(g_soundCard1SampleRate, g_soundCard2SampleRate);
        auto eitherOrRecordModulated = new EitherOrStep(
            []() { return g_recFileFromModulator && (g_sfRecFileFromModulator != NULL); },
            std::shared_ptr<IPipelineStep>(recordModulatedTap),
            std::shared_ptr<IPipelineStep>(bypassRecordModulated));
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrRecordModulated));
        
        // TX attenuation step
        auto txAttenuationStep = new LevelAdjustStep(g_soundCard1SampleRate, []() {
            double dbLoss = g_txLevel / 10.0;
            double scaleFactor = exp(dbLoss/20.0 * log(10.0));
            return scaleFactor; }
        );
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(txAttenuationStep));
    }
    else
    {
        // TBD
    }
}

//---------------------------------------------------------------------------------------------
// Main real time processing for tx and rx of FreeDV signals, run in its own threads
//---------------------------------------------------------------------------------------------

void TxRxThread::txProcessing()
{
    wxStopWatch sw;

    paCallBackData  *cbData = g_rxUserdata;

    // Buffers re-used by tx and rx processing.  We take samples from
    // the sound card, and resample them for the freedv modem input
    // sample rate.  Typically the sound card is running at 48 or 44.1
    // kHz, and the modem at 8kHz, however some modems such as FreeDV
    // 2400A/B run at 48 kHz.

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
    }
    //fprintf(stderr, "sample rate: %d\n", freedv_samplerate);

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

        unsigned int nsam_one_modem_frame = g_soundCard1SampleRate * freedvInterface.getTxNNomModemSamples()/freedv_samplerate;

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
            
            // There may be recorded audio left to encode while ending TX. To handle this,
            // we keep reading from the FIFO until we have less than nsam_in_48 samples available.
            int nread = codec2_fifo_read(cbData->infifo2, insound_card, nsam_in_48);            
            if (nread != 0 && endingTx) break;
            
#if 0
            // optionally use file for mic input signal
            if (g_playFileToMicIn && (g_sfPlayFile != NULL)) {
                unsigned int nsf = nsam_in_48*g_sfTxFs/g_soundCard2SampleRate;
                short        insf[nsf];
                                
                int n = sf_read_short(g_sfPlayFile, insf, nsf);
                nout = resample(cbData->insrctxsf, insound_card, insf, g_soundCard2SampleRate, g_sfTxFs, nsam_in_48, n);
                
                if (nout == 0) {
                    if (g_loopPlayFileToMicIn)
                        sf_seek(g_sfPlayFile, 0, SEEK_SET);
                    else {
                        printf("playFileFromRadio finished, issuing event!\n");
                        g_parent->CallAfter(&MainFrame::StopPlayFileToMicIn);
                    }
                }
            }
            
            nout = resample(cbData->insrc2, infreedv, insound_card, freedvInterface.getTxSpeechSampleRate(), g_soundCard2SampleRate, 10*N48, nsam_in_48);
                 
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
#endif
            
            short* inputSamples = new short[nsam_in_48];
            memcpy(inputSamples, insound_card, nsam_in_48 * sizeof(short));
            
            auto inputSamplesPtr = std::shared_ptr<short>(inputSamples, std::default_delete<short[]>());
            auto outputSamples = pipeline_->execute(inputSamplesPtr, nsam_in_48, &nout);
            
            if (g_dump_fifo_state) {
                fprintf(stderr, "  nout: %d\n", nout);
            }
            
            codec2_fifo_write(cbData->outfifo1, outputSamples.get(), nout);
        }
        
        txModeChangeMutex.Unlock();
    }

    if (g_dump_timing) {
        fprintf(stderr, "%4ld", sw.Time());
    }
}

void TxRxThread::rxProcessing()
{
    wxStopWatch sw;

    paCallBackData  *cbData = g_rxUserdata;

    // Buffers re-used by tx and rx processing.  We take samples from
    // the sound card, and resample them for the freedv modem input
    // sample rate.  Typically the sound card is running at 48 or 44.1
    // kHz, and the modem at 8kHz, however some modems such as FreeDV
    // 2400A/B run at 48 kHz.

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
    }
    //fprintf(stderr, "sample rate: %d\n", freedv_samplerate);

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
    
    // Attempt to read one processing frame (about 20ms) of receive samples,  we 
    // keep this frame duration constant across modes and sound card sample rates
    int nsam = (int)(g_soundCard1SampleRate * FRAME_DURATION);
    assert(nsam <= 10*N48);
    assert(nsam != 0);

    // while we have enough input samples available ... 
    while (codec2_fifo_read(cbData->infifo1, insound_card, nsam) == 0 && ((g_half_duplex && !g_tx) || !g_half_duplex)) {
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

        modem_stats_get_rx_spectrum(freedvInterface.getCurrentRxModemStats(), rx_spec, rx_fdm, nspec);

        // Average rx spectrum data using a simple IIR low pass filter

        for(i = 0; i<MODEM_STATS_NSPEC; i++) {
            g_avmag[i] = BETA * g_avmag[i] + (1.0 - BETA) * rx_spec[i];
        }

        // Get some audio to send to headphones/speaker.  If in analog
        // mode we pass through the "from radio" audio to the
        // headphones/speaker.
        
        int speechOutbufferSize = (int)(FRAME_DURATION * freedvInterface.getRxSpeechSampleRate());

        if (g_analog) {
            memcpy(outfreedv, infreedv, sizeof(short)*nfreedv);
        }
        else {
            // Write 20ms chunks of input samples for modem rx processing
            g_State = freedvInterface.processRxAudio(
                infreedv, nfreedv, cbData->rxoutfifo, g_channel_noise, wxGetApp().m_noise_snr, 
                g_RxFreqOffsetHz, freedvInterface.getCurrentRxModemStats(), &g_sig_pwr_av);
  
            // Read 20ms chunk of samples from modem rx processing,
            // this will typically be decoded output speech, and is
            // (currently at least) fixed at a sample rate of 8 kHz

            memset(outfreedv, 0, sizeof(short)*speechOutbufferSize);
            codec2_fifo_read(cbData->rxoutfifo, outfreedv, speechOutbufferSize);
        }

        // Optional Spk Out EQ Filtering, need mutex as filter can change at run time from another thread

        g_mutexProtectingCallbackData.Lock();
        if (cbData->spkOutEQEnable) {
            sox_biquad_filter(cbData->sbqSpkOutBass,   outfreedv, outfreedv, speechOutbufferSize);
            sox_biquad_filter(cbData->sbqSpkOutTreble, outfreedv, outfreedv, speechOutbufferSize);
            sox_biquad_filter(cbData->sbqSpkOutMid,    outfreedv, outfreedv, speechOutbufferSize);
            if (cbData->sbqSpkOutVol) sox_biquad_filter(cbData->sbqSpkOutVol,    outfreedv, outfreedv, speechOutbufferSize);
        }
        g_mutexProtectingCallbackData.Unlock();

        resample_for_plot(g_plotSpeechOutFifo, outfreedv, speechOutbufferSize, freedvInterface.getRxSpeechSampleRate());

        // resample to output sound card rate

        if (g_nSoundCards == 1) {
            if (g_analog) /* special case */
                nout = resample(cbData->outsrc2, outsound_card, outfreedv, g_soundCard1SampleRate, freedv_samplerate, N48, nfreedv);
            else
                nout = resample(cbData->outsrc2, outsound_card, outfreedv, g_soundCard1SampleRate, freedvInterface.getRxSpeechSampleRate(), N48, speechOutbufferSize);
            
            codec2_fifo_write(cbData->outfifo1, outsound_card, nout);
        }
        else {
            if (g_analog) /* special case */
                nout = resample(cbData->outsrc2, outsound_card, outfreedv, g_soundCard2SampleRate, freedv_samplerate, N48, nfreedv);
            else
                nout = resample(cbData->outsrc2, outsound_card, outfreedv, g_soundCard2SampleRate, freedvInterface.getRxSpeechSampleRate(), N48, speechOutbufferSize);

            codec2_fifo_write(cbData->outfifo2, outsound_card, nout);
        }
    }
}