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

#include <chrono>
using namespace std::chrono_literals;

#include "../util/sanitizers.h"

// WebRTC uses FS, which is defined in defines.h. Thus, it needs to be included
// first.
#include "AgcStep.h"

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
#include "ToneInterfererStep.h"
#include "ComputeRfSpectrumStep.h"
#include "FreeDVReceiveStep.h"
#include "MuteStep.h"
#include "LinkStep.h"

#include "util/logging/ulog.h"
#include "os/os_interface.h"

#include "codec2_alloc.h"

// Experimental options for potential future release:
//
// * ENABLE_FASTER_PLOTS: This uses a faster resampling algorithm to reduce the CPU
//   usage required to generate various plots in the user interface. (Tech note: When 
//   enabled, libsamplerate is directed to use SRC_LINEAR for the plot resampling.)
// * ENABLE_PROCESSING_STATS: This causes execution statistics to be collected for RX and TX
//   processing and output in the log after the user pushes Stop. (Define in .h file.)

#define ENABLE_FASTER_PLOTS

// External globals
// TBD -- work on fully removing the need for these.
extern paCallBackData* g_rxUserdata;
extern int g_analog;
extern int g_nSoundCards;
extern std::atomic<bool> g_half_duplex;
extern std::atomic<int> g_tx;
extern int g_dump_fifo_state;
extern std::atomic<bool> endingTx;
extern std::atomic<bool> g_playFileToMicIn;
extern int g_sfTxFs;
extern bool g_loopPlayFileToMicIn;
extern float g_TxFreqOffsetHz;
extern GenericFIFO<short> g_plotSpeechInFifo;
extern GenericFIFO<short> g_plotDemodInFifo;
extern GenericFIFO<short> g_plotSpeechOutFifo;
extern int g_mode;
extern bool g_recFileFromModulator;
extern int g_txLevel;
extern std::atomic<float> g_txLevelScale;
extern int g_dump_timing;
extern std::atomic<bool> g_queueResync;
extern int g_resyncs;
extern bool g_recFileFromRadio;
extern unsigned int g_recFromRadioSamples;
extern std::atomic<bool> g_playFileFromRadio;
extern int g_sfFs;
extern bool g_loopPlayFileFromRadio;
extern int g_SquelchActive;
extern float g_SquelchLevel;
extern float g_tone_phase;
extern GenericFIFO<float> g_avmag;
extern std::atomic<int> g_State;
extern std::atomic<int> g_channel_noise;
extern float g_RxFreqOffsetHz;
extern float g_sig_pwr_av;
extern std::atomic<bool> g_voice_keyer_tx;
extern std::atomic<bool> g_eoo_enqueued;
extern std::atomic<bool> g_agcEnabled;

#include <speex/speex_preprocess.h>

#include "../freedv_interface.h"
extern FreeDVInterface freedvInterface;

#include <wx/wx.h>
#include "../main.h"
extern wxWindow* g_parent;

static auto& NonblockingWxGetApp() FREEDV_NONBLOCKING
{
    // Note: wxWidgets implementation of wxGetApp() only returns the App object
    // and performs no other tasks. Verified RT safe as of wxWidgets version 3.3.1.
    FREEDV_BEGIN_VERIFIED_SAFE
    return wxGetApp();
    FREEDV_END_VERIFIED_SAFE
}

#include <sndfile.h>
extern std::atomic<SNDFILE*> g_sfPlayFile;
extern SNDFILE* g_sfRecFileFromModulator;
extern SNDFILE* g_sfRecFile;
extern SNDFILE* g_sfRecMicFile;
extern std::atomic<SNDFILE*> g_sfPlayFileFromRadio;

extern bool g_recFileFromMic;
extern bool g_recVoiceKeyerFile;

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
        pipeline_ = std::make_unique<AudioPipeline>(inputSampleRate_, outputSampleRate_);

        // Record from mic step (optional)
        auto recordMicStep = new RecordStep(
            inputSampleRate_, 
            []() { return g_sfRecMicFile; }, 
            [](int) {
                // Recording stops when the user explicitly tells us to,
                // no action required here.
            }
        );
        auto recordMicPipeline = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        recordMicPipeline->appendPipelineStep(recordMicStep);
        
        auto recordMicTap = new TapStep(inputSampleRate_, recordMicPipeline);
        auto bypassRecordMic = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        
        auto eitherOrRecordMic = new EitherOrStep(
            +[]() FREEDV_NONBLOCKING { return (g_recVoiceKeyerFile || g_recFileFromMic) && (g_sfRecMicFile != NULL); },
            recordMicTap,
            bypassRecordMic
        );
        pipeline_->appendPipelineStep(eitherOrRecordMic);
        
        // Mic In playback step (optional)
        auto eitherOrBypassPlay = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto eitherOrPlayMicIn = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto playMicIn = new PlaybackStep(
            inputSampleRate_, 
            []() { return g_sfTxFs; },
            []() { return g_playFileToMicIn.load(std::memory_order_acquire) ? g_sfPlayFile.load(std::memory_order_acquire) : nullptr; },
            []() {
                if (g_loopPlayFileToMicIn)
                    sf_seek(g_sfPlayFile.load(std::memory_order_acquire), 0, SEEK_SET);
                else {
                    log_info("playFileFromRadio finished, issuing event!");
                    ((MainFrame*)g_parent)->executeOnUiThreadAndWait_([]() { ((MainFrame*)g_parent)->StopPlayFileToMicIn();});
                }
            }
            );
        eitherOrPlayMicIn->appendPipelineStep(playMicIn);
        
        auto eitherOrPlayStep = new EitherOrStep(
            +[]() FREEDV_NONBLOCKING { return g_playFileToMicIn.load(std::memory_order_acquire) && (g_sfPlayFile.load(std::memory_order_acquire) != NULL); },
            eitherOrPlayMicIn,
            eitherOrBypassPlay);
        pipeline_->appendPipelineStep(eitherOrPlayStep);
        
        // Speex step (optional)
        auto eitherOrProcessSpeex = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto eitherOrBypassSpeex = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        
        auto speexStep = new SpeexStep(inputSampleRate_);
        eitherOrProcessSpeex->appendPipelineStep(speexStep);
        
        auto eitherOrSpeexStep = new EitherOrStep(
            +[]() FREEDV_NONBLOCKING { return (bool)NonblockingWxGetApp().appConfiguration.filterConfiguration.speexppEnable.getWithoutProcessing(); },
            eitherOrProcessSpeex,
            eitherOrBypassSpeex);
        pipeline_->appendPipelineStep(eitherOrSpeexStep);

        // AGC step (optional)
        auto eitherOrProcessAgc = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto eitherOrBypassAgc = new AudioPipeline(inputSampleRate_, inputSampleRate_);

        auto agcStep = new AgcStep(inputSampleRate_);
        eitherOrProcessAgc->appendPipelineStep(agcStep);

        auto eitherOrAgcStep = new EitherOrStep(
            +[]() FREEDV_NONBLOCKING { return g_agcEnabled.load(std::memory_order_acquire); },
            eitherOrProcessAgc,
            eitherOrBypassAgc);
        pipeline_->appendPipelineStep(eitherOrAgcStep); 

        // Equalizer step (optional based on filter state)
        auto equalizerStep = new EqualizerStep(
            inputSampleRate_, 
            &g_rxUserdata->micInEQEnable,
            &g_rxUserdata->sbqMicInBass,
            &g_rxUserdata->sbqMicInMid,
            &g_rxUserdata->sbqMicInTreble,
            &g_rxUserdata->sbqMicInVol,
            g_rxUserdata->micEqLock);
        pipeline_->appendPipelineStep(equalizerStep);
        
        // Take TX audio post-equalizer and send it to RX for possible monitoring use.
        if (equalizedMicAudioLink_ != nullptr)
        {
            auto micAudioPipeline = new AudioPipeline(inputSampleRate_, equalizedMicAudioLink_->getSampleRate());
            micAudioPipeline->appendPipelineStep(equalizedMicAudioLink_->getInputPipelineStep());
        
            auto micAudioTap = new TapStep(inputSampleRate_, micAudioPipeline);
            pipeline_->appendPipelineStep(micAudioTap);
        }
                
        // Resample for plot step
        auto resampleForPlotStep = new ResampleForPlotStep(&g_plotSpeechInFifo);
        auto resampleForPlotPipeline = new AudioPipeline(inputSampleRate_, resampleForPlotStep->getOutputSampleRate());
#if defined(ENABLE_FASTER_PLOTS)
        auto resampleForPlotResampler = new ResampleStep(inputSampleRate_, resampleForPlotStep->getInputSampleRate(), true); // need to create manually to get access to "plot only" optimizations
        resampleForPlotPipeline->appendPipelineStep(resampleForPlotResampler);
#endif // defined(ENABLE_FASTER_PLOTS)
        resampleForPlotPipeline->appendPipelineStep(resampleForPlotStep);

        auto resampleForPlotTap = new TapStep(inputSampleRate_, resampleForPlotPipeline);
        pipeline_->appendPipelineStep(resampleForPlotTap);
      
        // FreeDV TX step (analog leg)
        auto doubleLevelStep = new LevelAdjustStep(inputSampleRate_, +[]() FREEDV_NONBLOCKING { return (float)2.0; });
        auto analogTxPipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_);
        analogTxPipeline->appendPipelineStep(doubleLevelStep);
        
        auto digitalTxStep = freedvInterface.createTransmitPipeline(
            inputSampleRate_, 
            outputSampleRate_, 
            +[]() FREEDV_NONBLOCKING { return g_TxFreqOffsetHz; },
            helper_);
        auto digitalTxPipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_); 
        digitalTxPipeline->appendPipelineStep(digitalTxStep);
        
        auto eitherOrDigitalAnalog = new EitherOrStep(
            +[]() FREEDV_NONBLOCKING { return g_analog != 0; },
            analogTxPipeline,
            digitalTxPipeline);
        pipeline_->appendPipelineStep(eitherOrDigitalAnalog);

        // Record modulated output (optional)
        auto recordModulatedStep = new RecordStep(
            RECORD_FILE_SAMPLE_RATE, 
            []() { return g_sfRecFileFromModulator; }, 
            [](int) {
                // empty
            });
        auto recordModulatedPipeline = new AudioPipeline(outputSampleRate_, recordModulatedStep->getOutputSampleRate());
        recordModulatedPipeline->appendPipelineStep(recordModulatedStep);
        
        auto recordModulatedTap = new TapStep(outputSampleRate_, recordModulatedPipeline);
        auto recordModulatedTapPipeline = new AudioPipeline(outputSampleRate_, outputSampleRate_);
        recordModulatedTapPipeline->appendPipelineStep(recordModulatedTap);
        
        auto bypassRecordModulated = new AudioPipeline(outputSampleRate_, outputSampleRate_);
        
        auto eitherOrRecordModulated = new EitherOrStep(
            +[]() FREEDV_NONBLOCKING { return g_recFileFromModulator && (g_sfRecFileFromModulator != NULL); },
            recordModulatedTapPipeline,
            bypassRecordModulated);
        pipeline_->appendPipelineStep(eitherOrRecordModulated);
        
        // TX attenuation step
        auto txAttenuationStep = new LevelAdjustStep(outputSampleRate_, +[]() FREEDV_NONBLOCKING {
            return g_txLevelScale.load(std::memory_order_acquire);
        });
        pipeline_->appendPipelineStep(txAttenuationStep);
    }
    else
    {
        pipeline_ = std::make_unique<AudioPipeline>(inputSampleRate_, outputSampleRate_);
        // Record from radio step (optional)
        auto recordRadioStep = new RecordStep(
            RECORD_FILE_SAMPLE_RATE, 
            []() { return g_sfRecFile; }, 
            [](int numSamples) {
                g_recFromRadioSamples -= numSamples;
                if (g_recFromRadioSamples <= 0)
                {
                    // call stop record menu item, should be thread safe
                    g_parent->CallAfter(&MainFrame::StopRecFileFromRadio);
                }
            }
        );
        auto recordRadioPipeline = new AudioPipeline(inputSampleRate_, recordRadioStep->getOutputSampleRate());
        recordRadioPipeline->appendPipelineStep(recordRadioStep);
        
        auto recordRadioTap = new TapStep(inputSampleRate_, recordRadioPipeline);
        auto bypassRecordRadio = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        
        auto eitherOrRecordRadio = new EitherOrStep(
            +[]() FREEDV_NONBLOCKING { return g_recFileFromRadio && (g_sfRecFile != NULL); },
            recordRadioTap,
            bypassRecordRadio
        );
        pipeline_->appendPipelineStep(eitherOrRecordRadio);
        
        // Play from radio step (optional)
        auto eitherOrBypassPlayRadio = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto eitherOrPlayRadio = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto playRadio = new PlaybackStep(
            inputSampleRate_, 
            []() { return g_sfFs; },
            []() { return g_sfPlayFileFromRadio.load(std::memory_order_acquire); },
            []() {
                if (g_loopPlayFileFromRadio)
                    sf_seek(g_sfPlayFileFromRadio.load(std::memory_order_acquire), 0, SEEK_SET);
                else {
                    log_info("playFileFromRadio finished, issuing event!");
                    ((MainFrame*)g_parent)->executeOnUiThreadAndWait_([]() { ((MainFrame*)g_parent)->StopPlaybackFileFromRadio();});
                }
            }
        );
        eitherOrPlayRadio->appendPipelineStep(playRadio);
        
        auto eitherOrPlayRadioStep = new EitherOrStep(
            +[]() FREEDV_NONBLOCKING { 
                auto result = g_playFileFromRadio.load(std::memory_order_acquire) && (g_sfPlayFileFromRadio.load(std::memory_order_acquire) != NULL);
                return result;
            },
            eitherOrPlayRadio,
            eitherOrBypassPlayRadio);
        pipeline_->appendPipelineStep(eitherOrPlayRadioStep);
        
        // Resample for plot step (demod in)
        auto resampleForPlotStep = new ResampleForPlotStep(&g_plotDemodInFifo);
        auto resampleForPlotPipeline = new AudioPipeline(inputSampleRate_, resampleForPlotStep->getOutputSampleRate());
#if defined(ENABLE_FASTER_PLOTS)
        auto resampleForPlotResampler = new ResampleStep(inputSampleRate_, resampleForPlotStep->getInputSampleRate(), true); // need to create manually to get access to "plot only" optimizations
        resampleForPlotPipeline->appendPipelineStep(resampleForPlotResampler);
#endif // defined(ENABLE_FASTER_PLOTS)
        resampleForPlotPipeline->appendPipelineStep(resampleForPlotStep);

        auto resampleForPlotTap = new TapStep(inputSampleRate_, resampleForPlotPipeline);
        pipeline_->appendPipelineStep(resampleForPlotTap);

        // Tone interferer step (optional)
        auto bypassToneInterferer = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto toneInterfererStep = new ToneInterfererStep(
            inputSampleRate_,
            +[]() FREEDV_NONBLOCKING { return (float)NonblockingWxGetApp().m_tone_freq_hz; },
            +[]() FREEDV_NONBLOCKING { return (float)NonblockingWxGetApp().m_tone_amplitude; },
            +[]() FREEDV_NONBLOCKING { return (float*)&g_tone_phase; }
        );
        auto eitherOrToneInterferer = new EitherOrStep(
            +[]() FREEDV_NONBLOCKING { return NonblockingWxGetApp().m_tone; },
            toneInterfererStep,
            bypassToneInterferer
        );
        pipeline_->appendPipelineStep(eitherOrToneInterferer);
        
        // RF spectrum computation step
        auto computeRfSpectrumStep = new ComputeRfSpectrumStep(
            +[]() FREEDV_NONBLOCKING { return freedvInterface.getCurrentRxModemStats(); },
            +[]() FREEDV_NONBLOCKING { return &g_avmag; }
        );
        auto computeRfSpectrumPipeline = new AudioPipeline(
            inputSampleRate_, computeRfSpectrumStep->getOutputSampleRate());
#if defined(ENABLE_FASTER_PLOTS)
        auto resampleForRfSpectrum = new ResampleStep(inputSampleRate_, computeRfSpectrumStep->getInputSampleRate(), true); // need to create manually to get access to "plot only" optimizations
        computeRfSpectrumPipeline->appendPipelineStep(resampleForRfSpectrum);
#endif // defined(ENABLE_FASTER_PLOTS)
        computeRfSpectrumPipeline->appendPipelineStep(computeRfSpectrumStep);
        
        auto computeRfSpectrumTap = new TapStep(inputSampleRate_, computeRfSpectrumPipeline);
        pipeline_->appendPipelineStep(computeRfSpectrumTap);
        
        // RX demodulation step
        auto bypassRfDemodulationPipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_);
        auto rfDemodulationPipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_);
        auto rfDemodulationStep = freedvInterface.createReceivePipeline(
            inputSampleRate_, outputSampleRate_,
            +[]() FREEDV_NONBLOCKING { return &g_State; },
            +[]() FREEDV_NONBLOCKING { return g_channel_noise.load(std::memory_order_acquire); },
            +[]() FREEDV_NONBLOCKING { return NonblockingWxGetApp().appConfiguration.noiseSNR.getWithoutProcessing(); },
            +[]() FREEDV_NONBLOCKING { return g_RxFreqOffsetHz; },
            +[]() FREEDV_NONBLOCKING { return &g_sig_pwr_av; },
            helper_
        );
        rfDemodulationPipeline->appendPipelineStep(rfDemodulationStep);

        // Resample for plot step (speech out)
        auto resampleForPlotOutStep = new ResampleForPlotStep(&g_plotSpeechOutFifo);
        auto resampleForPlotOutPipeline = new AudioPipeline(outputSampleRate_, resampleForPlotOutStep->getOutputSampleRate());
#if defined(ENABLE_FASTER_PLOTS)
        auto resampleForPlotOutResampler = new ResampleStep(outputSampleRate_, resampleForPlotOutStep->getInputSampleRate(), true); // need to create manually to get access to "plot only" optimizations
        resampleForPlotOutPipeline->appendPipelineStep(resampleForPlotOutResampler);
#endif // defined(ENABLE_FASTER_PLOTS)
        resampleForPlotOutPipeline->appendPipelineStep(resampleForPlotOutStep);

        auto resampleForPlotOutTap = new TapStep(outputSampleRate_, resampleForPlotOutPipeline);
        rfDemodulationPipeline->appendPipelineStep(resampleForPlotOutTap);
        
        // Replace received audio with microphone audio if we're monitoring TX/voice keyer recording.
        if (equalizedMicAudioLink_ != nullptr)
        {
            auto bypassMonitorAudio = new AudioPipeline(inputSampleRate_, outputSampleRate_);
            auto mutePipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_);
            
            auto monitorPipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_);
            monitorPipeline->appendPipelineStep(equalizedMicAudioLink_->getOutputPipelineStep());
            
            auto monitorLevelStep = new LevelAdjustStep(outputSampleRate_, +[]() FREEDV_NONBLOCKING {
                float volInDb = 0;
                if (g_voice_keyer_tx.load(std::memory_order_acquire) && NonblockingWxGetApp().appConfiguration.monitorVoiceKeyerAudio.getWithoutProcessing())
                {
                    volInDb = NonblockingWxGetApp().appConfiguration.monitorVoiceKeyerAudioVol.getWithoutProcessing();
                }
                else
                {
                    volInDb = NonblockingWxGetApp().appConfiguration.monitorTxAudioVol.getWithoutProcessing();
                }
                
                return std::exp(volInDb/20.0f * std::log(10.0f));
            });
            monitorPipeline->appendPipelineStep(monitorLevelStep);

            auto muteStep = new MuteStep(outputSampleRate_);
            
            mutePipeline->appendPipelineStep(muteStep);
            
            auto eitherOrMuteStep = new EitherOrStep(
                +[]() FREEDV_NONBLOCKING { return g_recVoiceKeyerFile; },
                mutePipeline,
                bypassMonitorAudio
            );

            auto eitherOrMicMonitorStep = new EitherOrStep(
                +[]() FREEDV_NONBLOCKING { return 
                    (g_voice_keyer_tx.load(std::memory_order_acquire) && NonblockingWxGetApp().appConfiguration.monitorVoiceKeyerAudio.getWithoutProcessing()) || 
                    (g_tx.load(std::memory_order_acquire) && NonblockingWxGetApp().appConfiguration.monitorTxAudio.getWithoutProcessing()); },
                monitorPipeline,
                eitherOrMuteStep
            );
            bypassRfDemodulationPipeline->appendPipelineStep(eitherOrMicMonitorStep);
        }
        
        EitherOrStep* eitherOrRfDemodulationStep = nullptr;
        if (equalizedMicAudioLink_ != nullptr)
        {
            eitherOrRfDemodulationStep = new EitherOrStep(
                +[]() FREEDV_NONBLOCKING { return g_analog ||
                    (
                        (g_recVoiceKeyerFile) ||
                        (g_voice_keyer_tx.load(std::memory_order_acquire) && NonblockingWxGetApp().appConfiguration.monitorVoiceKeyerAudio.getWithoutProcessing()) || 
                        (g_tx.load(std::memory_order_acquire) && NonblockingWxGetApp().appConfiguration.monitorTxAudio.getWithoutProcessing())
                    ); },
                bypassRfDemodulationPipeline,
                rfDemodulationPipeline);
        }
        else
        {
            eitherOrRfDemodulationStep = new EitherOrStep(
                +[]() FREEDV_NONBLOCKING { return g_analog != 0; },
                bypassRfDemodulationPipeline,
                rfDemodulationPipeline);
        }

        pipeline_->appendPipelineStep(eitherOrRfDemodulationStep);

        // Equalizer step (optional based on filter state)
        auto equalizerStep = new EqualizerStep(
            outputSampleRate_, 
            &g_rxUserdata->spkOutEQEnable,
            &g_rxUserdata->sbqSpkOutBass,
            &g_rxUserdata->sbqSpkOutMid,
            &g_rxUserdata->sbqSpkOutTreble,
            &g_rxUserdata->sbqSpkOutVol,
            g_rxUserdata->spkEqLock);
        pipeline_->appendPipelineStep(equalizerStep);
        
        // Clear anything in the FIFO before resuming decode.
        clearFifos_();
    }
}

void* TxRxThread::Entry() noexcept
{
    // Get raw pointer so we don't need to constantly access the shared_ptr
    // and thus constantly increment/decrement refcounts.
    IRealtimeHelper* helper = helper_.get();

    // Ensure that O(1) memory allocator is used for Codec2
    // instead of standard malloc().
    codec2_initialize_realtime(CODEC2_REAL_TIME_MEMORY_SIZE);
    
    initializePipeline_();
    
    // Request real-time scheduling from the operating system.
    helper->setHelperRealTime();

#if defined(ENABLE_PROCESSING_STATS)
    resetStats_();
#endif // defined(ENABLE_PROCESSING_STATS)

    // Set thread name for debugging
    const char* threadName = nullptr;
    if (m_tx) threadName = "txThread";
    else threadName = "rxThread";
    SetThreadName(threadName);

    // Make sure we don't start processing until
    // the main thread is ready.
    readySem_.signal();
    startSem_.wait();
    clearFifos_();

    while (m_run)
    {
        if (!m_run) break;
        
        //log_info("thread woken up: m_tx=%d", (int)m_tx);
        helper->startRealTimeWork();

        if (m_tx) txProcessing_(helper);
        else rxProcessing_(helper);

        // Determine whether we need to pause for a shorter amount
        // of time to avoid dropouts.
        paCallBackData  *cbData = g_rxUserdata;
        auto outFifo = cbData->outfifo1;
        if (!m_tx)
        {
            outFifo = (g_nSoundCards == 1) ? cbData->outfifo1 : cbData->outfifo2;
        }
        auto totalFifoCapacity = outFifo->capacity();
        auto fifoUsed = outFifo->numUsed();
        helper->stopRealTimeWork(fifoUsed < totalFifoCapacity / 2);
    }

#if defined(ENABLE_PROCESSING_STATS)
    reportStats_();
#endif // defined(ENABLE_PROCESSING_STATS)

    // Force pipeline to delete itself when we're done with the thread.
    pipeline_ = nullptr;
    
    // Return to normal scheduling
    helper->clearHelperRealTime();
    
    codec2_disable_realtime();
    
    return NULL;
}

#if defined(ENABLE_PROCESSING_STATS)
void TxRxThread::resetStats_()
{
    numTimeSamples_ = 0;
    minDuration_ = 1e9;
    maxDuration_ = 0;
    sumDuration_ = 0;
    sumDoubleDuration_ = 0; 
}

void TxRxThread::startTimer_()
{
    timeStart_ = std::chrono::high_resolution_clock::now();
}

void TxRxThread::endTimer_()
{
    auto e = std::chrono::high_resolution_clock::now();
    auto d = std::chrono::duration_cast<std::chrono::nanoseconds>(e - timeStart_).count();
    numTimeSamples_++; 
    if (d < minDuration_)
    {
        minDuration_ = d;
        minTime_ = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }
    if (d > maxDuration_)
    {
        maxDuration_ = d;
        maxTime_ = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }
    sumDuration_ += d; sumDoubleDuration_ += pow(d, 2);
}

void TxRxThread::reportStats_()
{
    if (numTimeSamples_ > 0)
    {
        std::tm * minTm = std::localtime(&minTime_);
        std::tm * maxTm = std::localtime(&maxTime_);
        char bufMin[32];
        char bufMax[32];
        std::strftime(bufMin, 32, "%H:%M:%S", minTm);
        std::strftime(bufMax, 32, "%H:%M:%S", maxTm);
        
        log_info("m_tx = %d, min = %f ns [%s], max = %f ns [%s], mean = %f ns, stdev = %f ns (n = %d)", m_tx, minDuration_, bufMin, maxDuration_, bufMax, sumDuration_ / numTimeSamples_, sqrt((sumDoubleDuration_ - pow(sumDuration_, 2)/numTimeSamples_) / (numTimeSamples_ - 1)), numTimeSamples_);
    }
}
#endif // defined(ENABLE_PROCESSING_STATS)

void TxRxThread::clearFifos_() FREEDV_NONBLOCKING
{
    paCallBackData  *cbData = g_rxUserdata;
    
    if (equalizedMicAudioLink_ != nullptr && !g_tx.load(std::memory_order_acquire))
    {
        equalizedMicAudioLink_->clearFifo();
    }
    
    if (m_tx)
    {
        cbData->outfifo1->reset();
        cbData->infifo2->reset();
    }
    else
    {
        cbData->infifo1->reset();

        auto outFifo = (g_nSoundCards == 1) ? cbData->outfifo1 : cbData->outfifo2;
        outFifo->reset();
    }

    if (equalizedMicAudioLink_)
    {
        equalizedMicAudioLink_->getFifo().reset();
    }
}

//---------------------------------------------------------------------------------------------
// Main real time processing for tx and rx of FreeDV signals, run in its own threads
//---------------------------------------------------------------------------------------------

void TxRxThread::txProcessing_(IRealtimeHelper* helper) FREEDV_NONBLOCKING
{
    paCallBackData  *cbData = g_rxUserdata;

    // Buffers re-used by tx and rx processing.  We take samples from
    // the sound card, and resample them for the freedv modem input
    // sample rate.  Typically the sound card is running at 48 or 44.1
    // kHz, and the modem at 8kHz

    //
    //  TX side processing --------------------------------------------
    //

    bool tmpHalfDuplex = g_half_duplex.load(std::memory_order_acquire);
    if (((g_nSoundCards == 2) && ((tmpHalfDuplex && g_tx.load(std::memory_order_acquire)) || !tmpHalfDuplex || g_voice_keyer_tx.load(std::memory_order_acquire) || g_recVoiceKeyerFile || g_recFileFromMic))) {        
        if (deferReset_)
        {
            // We just entered TX from RX.
            // Reset pipeline and wipe anything in the FIFO.
            deferReset_ = false;
            pipeline_->reset();
            clearFifos_();
        }

        // This while loop locks the modulator to the sample rate of
        // the input sound card.  We want to make sure that modulator samples
        // are uninterrupted by differences in sample rate between
        // this sound card and the output sound card.

        // Run code inside this while loop as soon as we have enough
        // room for one frame of modem samples.  Aim is to keep
        // outfifo1 nice and full so we don't have any gaps in tx
        // signal.

        unsigned int nsam_one_modem_frame = (freedvInterface.getTxNNomModemSamples() * outputSampleRate_) / freedvInterface.getTxModemSampleRate();

     	if (g_dump_fifo_state) {
    	  // If this drops to zero we have a problem as we will run out of output samples
    	  // to send to the sound driver
          FREEDV_BEGIN_VERIFIED_SAFE
    	  log_debug("outfifo1 used: %6d free: %6d nsam_one_modem_frame: %d",
                      cbData->outfifo1->numUsed(), cbData->outfifo1->numFree(), nsam_one_modem_frame);
          FREEDV_END_VERIFIED_SAFE
    	}

        int nsam_in_48 = (inputSampleRate_ * FRAME_DURATION_MS) / MS_TO_SEC;
        assert(nsam_in_48 > 0);

        int             nout;

        while(!helper->mustStopWork() && (unsigned)cbData->outfifo1->numFree() >= nsam_one_modem_frame) {        
            // OK to generate a frame of modem output samples we need
            // an input frame of speech samples from the microphone.
            
#if defined(ENABLE_PROCESSING_STATS)
            startTimer_();
#endif // defined(ENABLE_PROCESSING_STATS)

            // infifo2 is written to by another sound card so it may
            // over or underflow, but we don't really care.  It will
            // just result in a short interruption in audio being fed
            // to codec2_enc, possibly making a click every now and
            // again in the decoded audio at the other end.

            // There may be recorded audio left to encode while ending TX. To handle this,
            // we keep reading from the FIFO until we have less than nsam_in_48 samples available.
            auto inputPtr = inputSamples_.get();
            int nread = cbData->infifo2->read(inputPtr, nsam_in_48);            
            if (nread != 0)
            {
                inputPtr = inputSamplesZeros_.get();
            }
            if (nread != 0 && endingTx.load(std::memory_order_acquire))
            {
                if (freedvInterface.getCurrentMode() >= FREEDV_MODE_RADE)
                {
                    if (!hasEooBeenSent_)
                    {
                        // Special case for handling RADE EOT
                        freedvInterface.restartTxVocoder();
                        hasEooBeenSent_ = true;
                    }

                    auto outputSamples = pipeline_->execute(inputPtr, 0, &nout);
                    if (nout > 0 && outputSamples != nullptr)
                    {
                        if (cbData->outfifo1->write(outputSamples, nout) != 0)
                        {
                            FREEDV_BEGIN_VERIFIED_SAFE
                            log_warn("Could not inject resampled EOO samples (space remaining in FIFO = %d)", cbData->outfifo1->numFree());
                            FREEDV_END_VERIFIED_SAFE
                        }
                    }
                    else
                    {
                        g_eoo_enqueued.store(true, std::memory_order_release);
                    }
                }
                break;
            }
            else
            {
                g_eoo_enqueued.store(false, std::memory_order_release);
                hasEooBeenSent_ = false;
            }
            
            auto outputSamples = pipeline_->execute(inputPtr, nsam_in_48, &nout);
            
            if (g_dump_fifo_state) {
                FREEDV_BEGIN_VERIFIED_SAFE
                log_info("  nout: %d", nout);
                FREEDV_END_VERIFIED_SAFE
            }
            
            if (outputSamples != nullptr)
            {
                cbData->outfifo1->write(outputSamples, nout);
            }
            
#if defined(ENABLE_PROCESSING_STATS)
            endTimer_();
#endif // defined(ENABLE_PROCESSING_STATS)
        }
    }
    else
    {
        // Defer reset until next time we go into TX.
        deferReset_ = true;
    }
}

void TxRxThread::rxProcessing_(IRealtimeHelper* helper) FREEDV_NONBLOCKING
{
    paCallBackData  *cbData = g_rxUserdata;

    // Buffers re-used by tx and rx processing.  We take samples from
    // the sound card, and resample them for the freedv modem input
    // sample rate.  Typically the sound card is running at 48 or 44.1
    // kHz, and the modem at 8kHz.

    //
    //  RX side processing --------------------------------------------
    //
    
    if (g_queueResync.load(std::memory_order_acquire))
    {
        g_queueResync.store(std::memory_order_release);
        freedvInterface.setSync(FREEDV_SYNC_UNSYNC);
        g_resyncs++;
    }
    
    // Attempt to read one processing frame (about 20ms) of receive samples,  we 
    // keep this frame duration constant across modes and sound card sample rates
    int nsam = (inputSampleRate_ * FRAME_DURATION_MS) / MS_TO_SEC;
    assert(nsam > 0);

    int             nout;


    bool tmpTx = g_tx.load(std::memory_order_acquire);
    bool tmpVkTx = g_voice_keyer_tx.load(std::memory_order_acquire);
    bool tmpHalfDuplex = g_half_duplex.load(std::memory_order_acquire);
    bool processInputFifo = 
        (tmpVkTx && NonblockingWxGetApp().appConfiguration.monitorVoiceKeyerAudio.getWithoutProcessing()) ||
        (tmpTx && NonblockingWxGetApp().appConfiguration.monitorTxAudio.getWithoutProcessing()) ||
        (!tmpVkTx && ((tmpHalfDuplex && !tmpTx) || !tmpHalfDuplex));
    if (!processInputFifo)
    {
        clearFifos_();
    }

    int nsam_one_speech_frame = (freedvInterface.getRxNumSpeechSamples() * outputSampleRate_) / freedvInterface.getRxSpeechSampleRate();
    auto outFifo = (g_nSoundCards == 1) ? cbData->outfifo1 : cbData->outfifo2;

    // while we have enough input samples available and enough space in the output FIFO ... 
    while (!helper->mustStopWork() && processInputFifo && outFifo->numFree() >= nsam_one_speech_frame && cbData->infifo1->read(inputSamples_.get(), nsam) == 0) {
        
#if defined(ENABLE_PROCESSING_STATS)
        startTimer_();
#endif // defined(ENABLE_PROCESSING_STATS)
        
        // send latest squelch level to FreeDV API, as it handles squelch internally
        freedvInterface.setSquelch(g_SquelchActive, g_SquelchLevel);

        auto outputSamples = pipeline_->execute(inputSamples_.get(), nsam, &nout);
        
        if (nout > 0 && outputSamples != nullptr)
        {
            outFifo->write(outputSamples, nout);
        }
        
        tmpTx = g_tx.load(std::memory_order_acquire);
        tmpVkTx = g_voice_keyer_tx.load(std::memory_order_acquire);
        tmpHalfDuplex = g_half_duplex.load(std::memory_order_acquire);
        processInputFifo = 
            (tmpVkTx && NonblockingWxGetApp().appConfiguration.monitorVoiceKeyerAudio.getWithoutProcessing()) ||
            (tmpTx && NonblockingWxGetApp().appConfiguration.monitorTxAudio.getWithoutProcessing()) ||
            (!tmpVkTx && ((tmpHalfDuplex && !tmpTx) || !tmpHalfDuplex));
        
#if defined(ENABLE_PROCESSING_STATS)
        endTimer_();
#endif // defined(ENABLE_PROCESSING_STATS)
    }
}
