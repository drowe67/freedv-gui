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

#include <wx/stopwatch.h>

// Experimental options for potential future release:
//
// * ENABLE_FASTER_PLOTS: This uses a faster resampling algorithm to reduce the CPU
//   usage required to generate various plots in the user interface. (Tech note: When 
//   enabled, libsamplerate is directed to use SRC_LINEAR for the plot resampling.)
// * ENABLE_PROCESSING_STATS: This causes execution statistics to be collected for RX and TX
//   processing and output in the log after the user pushes Stop. 

#define ENABLE_FASTER_PLOTS
#define ENABLE_PROCESSING_STATS

// External globals
// TBD -- work on fully removing the need for these.
extern paCallBackData* g_rxUserdata;
extern int g_analog;
extern int g_nSoundCards;
extern std::atomic<bool> g_half_duplex;
extern std::atomic<int> g_tx;
extern int g_dump_fifo_state;
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
extern int g_txLevel;
extern std::atomic<float> g_txLevelScale;
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
extern std::atomic<bool> g_voice_keyer_tx;
extern bool g_eoo_enqueued;

#include <speex/speex_preprocess.h>

#include "../freedv_interface.h"
extern FreeDVInterface freedvInterface;

#include <wx/wx.h>
#include "../main.h"
extern wxWindow* g_parent;

#include <sndfile.h>
extern SNDFILE* g_sfPlayFile;
extern SNDFILE* g_sfRecFileFromModulator;
extern SNDFILE* g_sfRecFile;
extern SNDFILE* g_sfRecMicFile;
extern SNDFILE* g_sfPlayFileFromRadio;

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
        pipeline_ = std::shared_ptr<AudioPipeline>(new AudioPipeline(inputSampleRate_, outputSampleRate_));

        // Record from mic step (optional)
        auto recordMicStep = new RecordStep(
            inputSampleRate_, 
            []() { return g_sfRecMicFile; }, 
            [](int numSamples) {
                // Recording stops when the user explicitly tells us to,
                // no action required here.
            }
        );
        auto recordMicPipeline = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        recordMicPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(recordMicStep));
        
        auto recordMicTap = new TapStep(inputSampleRate_, recordMicPipeline);
        auto bypassRecordMic = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        
        auto eitherOrRecordMic = new EitherOrStep(
            []() { return (g_recVoiceKeyerFile || g_recFileFromMic) && (g_sfRecMicFile != NULL); },
            std::shared_ptr<IPipelineStep>(recordMicTap),
            std::shared_ptr<IPipelineStep>(bypassRecordMic)
        );
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrRecordMic));
        
        // Mic In playback step (optional)
        auto eitherOrBypassPlay = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto eitherOrPlayMicIn = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto playMicIn = new PlaybackStep(
            inputSampleRate_, 
            []() { return g_sfTxFs; },
            []() { return g_sfPlayFile; },
            []() {
                if (g_loopPlayFileToMicIn)
                    sf_seek(g_sfPlayFile, 0, SEEK_SET);
                else {
                    log_info("playFileFromRadio finished, issuing event!");
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
        
        // Speex step (optional)
        auto eitherOrProcessSpeex = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto eitherOrBypassSpeex = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        
        auto speexStep = new SpeexStep(inputSampleRate_);
        eitherOrProcessSpeex->appendPipelineStep(std::shared_ptr<IPipelineStep>(speexStep));
        
        auto eitherOrSpeexStep = new EitherOrStep(
            []() { return wxGetApp().appConfiguration.filterConfiguration.speexppEnable; },
            std::shared_ptr<IPipelineStep>(eitherOrProcessSpeex),
            std::shared_ptr<IPipelineStep>(eitherOrBypassSpeex));
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrSpeexStep));
       
        // Equalizer step (optional based on filter state)
        auto equalizerStep = new EqualizerStep(
            inputSampleRate_, 
            &g_rxUserdata->micInEQEnable,
            &g_rxUserdata->sbqMicInBass,
            &g_rxUserdata->sbqMicInMid,
            &g_rxUserdata->sbqMicInTreble,
            &g_rxUserdata->sbqMicInVol);
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(equalizerStep));
        
        // Take TX audio post-equalizer and send it to RX for possible monitoring use.
        if (equalizedMicAudioLink_ != nullptr)
        {
            auto micAudioPipeline = new AudioPipeline(inputSampleRate_, equalizedMicAudioLink_->getSampleRate());
            micAudioPipeline->appendPipelineStep(equalizedMicAudioLink_->getInputPipelineStep());
        
            auto micAudioTap = std::make_shared<TapStep>(inputSampleRate_, micAudioPipeline);
            pipeline_->appendPipelineStep(micAudioTap);
        }
                
        // Resample for plot step
        auto resampleForPlotStep = new ResampleForPlotStep(g_plotSpeechInFifo);
        auto resampleForPlotPipeline = new AudioPipeline(inputSampleRate_, resampleForPlotStep->getOutputSampleRate());
#if defined(ENABLE_FASTER_PLOTS)
        auto resampleForPlotResampler = new ResampleStep(inputSampleRate_, resampleForPlotStep->getInputSampleRate(), true); // need to create manually to get access to "plot only" optimizations
        resampleForPlotPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(resampleForPlotResampler));
#endif // defined(ENABLE_FASTER_PLOTS)
        resampleForPlotPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(resampleForPlotStep));

        auto resampleForPlotTap = new TapStep(inputSampleRate_, resampleForPlotPipeline);
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(resampleForPlotTap));
        
        // FreeDV TX step (analog leg)
        auto doubleLevelStep = new LevelAdjustStep(inputSampleRate_, []() { return 2.0; });
        auto analogTxPipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_);
        analogTxPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(doubleLevelStep));
        
        auto digitalTxStep = freedvInterface.createTransmitPipeline(
            inputSampleRate_, 
            outputSampleRate_, 
            []() { return g_TxFreqOffsetHz; },
            helper_);
        auto digitalTxPipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_); 
        digitalTxPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(digitalTxStep));
        
        auto eitherOrDigitalAnalog = new EitherOrStep(
            []() { return g_analog; },
            std::shared_ptr<IPipelineStep>(analogTxPipeline),
            std::shared_ptr<IPipelineStep>(digitalTxPipeline));
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrDigitalAnalog));

        // Record modulated output (optional)
        auto recordModulatedStep = new RecordStep(
            outputSampleRate_, 
            []() { return g_sfRecFileFromModulator; }, 
            [](int numSamples) {
                // empty
            });
        auto recordModulatedPipeline = new AudioPipeline(outputSampleRate_, recordModulatedStep->getOutputSampleRate());
        recordModulatedPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(recordModulatedStep));
        
        auto recordModulatedTap = new TapStep(outputSampleRate_, recordModulatedPipeline);
        auto recordModulatedTapPipeline = new AudioPipeline(outputSampleRate_, outputSampleRate_);
        recordModulatedTapPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(recordModulatedTap));
        
        auto bypassRecordModulated = new AudioPipeline(outputSampleRate_, outputSampleRate_);
        
        auto eitherOrRecordModulated = new EitherOrStep(
            []() { return g_recFileFromModulator && (g_sfRecFileFromModulator != NULL); },
            std::shared_ptr<IPipelineStep>(recordModulatedTapPipeline),
            std::shared_ptr<IPipelineStep>(bypassRecordModulated));
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrRecordModulated));
        
        // TX attenuation step
        auto txAttenuationStep = new LevelAdjustStep(outputSampleRate_, []() {
            return g_txLevelScale.load(std::memory_order_acquire);
        });
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(txAttenuationStep));
    }
    else
    {
        pipeline_ = std::shared_ptr<AudioPipeline>(new AudioPipeline(inputSampleRate_, outputSampleRate_));
        // Record from radio step (optional)
        auto recordRadioStep = new RecordStep(
            inputSampleRate_, 
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
        auto recordRadioPipeline = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        recordRadioPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(recordRadioStep));
        
        auto recordRadioTap = new TapStep(inputSampleRate_, recordRadioPipeline);
        auto bypassRecordRadio = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        
        auto eitherOrRecordRadio = new EitherOrStep(
            []() { return g_recFileFromRadio && (g_sfRecFile != NULL); },
            std::shared_ptr<IPipelineStep>(recordRadioTap),
            std::shared_ptr<IPipelineStep>(bypassRecordRadio)
        );
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrRecordRadio));
        
        // Play from radio step (optional)
        auto eitherOrBypassPlayRadio = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto eitherOrPlayRadio = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto playRadio = new PlaybackStep(
            inputSampleRate_, 
            []() { return g_sfFs; },
            []() { return g_sfPlayFileFromRadio; },
            []() {
                if (g_loopPlayFileFromRadio)
                    sf_seek(g_sfPlayFileFromRadio, 0, SEEK_SET);
                else {
                    log_info("playFileFromRadio finished, issuing event!");
                    g_parent->CallAfter(&MainFrame::StopPlaybackFileFromRadio);
                }
            }
        );
        eitherOrPlayRadio->appendPipelineStep(std::shared_ptr<IPipelineStep>(playRadio));
        
        auto eitherOrPlayRadioStep = new EitherOrStep(
            []() { 
                auto result = g_playFileFromRadio && (g_sfPlayFileFromRadio != NULL);
                return result;
            },
            std::shared_ptr<IPipelineStep>(eitherOrPlayRadio),
            std::shared_ptr<IPipelineStep>(eitherOrBypassPlayRadio));
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrPlayRadioStep));
        
        // Resample for plot step (demod in)
        auto resampleForPlotStep = new ResampleForPlotStep(g_plotDemodInFifo);
        auto resampleForPlotPipeline = new AudioPipeline(inputSampleRate_, resampleForPlotStep->getOutputSampleRate());
#if defined(ENABLE_FASTER_PLOTS)
        auto resampleForPlotResampler = new ResampleStep(inputSampleRate_, resampleForPlotStep->getInputSampleRate(), true); // need to create manually to get access to "plot only" optimizations
        resampleForPlotPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(resampleForPlotResampler));
#endif // defined(ENABLE_FASTER_PLOTS)
        resampleForPlotPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(resampleForPlotStep));

        auto resampleForPlotTap = new TapStep(inputSampleRate_, resampleForPlotPipeline);
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(resampleForPlotTap));

        // Tone interferer step (optional)
        auto bypassToneInterferer = new AudioPipeline(inputSampleRate_, inputSampleRate_);
        auto toneInterfererStep = new ToneInterfererStep(
            inputSampleRate_,
            []() { return wxGetApp().m_tone_freq_hz; },
            []() { return wxGetApp().m_tone_amplitude; },
            []() { return &g_tone_phase; }
        );
        auto eitherOrToneInterferer = new EitherOrStep(
            []() { return wxGetApp().m_tone; },
            std::shared_ptr<IPipelineStep>(toneInterfererStep),
            std::shared_ptr<IPipelineStep>(bypassToneInterferer)
        );
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrToneInterferer));
        
        // RF spectrum computation step
        auto computeRfSpectrumStep = new ComputeRfSpectrumStep(
            []() { return freedvInterface.getCurrentRxModemStats(); },
            []() { return &g_avmag[0]; }
        );
        auto computeRfSpectrumPipeline = new AudioPipeline(
            inputSampleRate_, computeRfSpectrumStep->getOutputSampleRate());
#if defined(ENABLE_FASTER_PLOTS)
        auto resampleForRfSpectrum = new ResampleStep(inputSampleRate_, computeRfSpectrumStep->getInputSampleRate(), true); // need to create manually to get access to "plot only" optimizations
        computeRfSpectrumPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(resampleForRfSpectrum));
#endif // defined(ENABLE_FASTER_PLOTS)
        computeRfSpectrumPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(computeRfSpectrumStep));
        
        auto computeRfSpectrumTap = new TapStep(inputSampleRate_, computeRfSpectrumPipeline);
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(computeRfSpectrumTap));
        
        // RX demodulation step
        auto bypassRfDemodulationPipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_);
        auto rfDemodulationPipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_);
        auto rfDemodulationStep = freedvInterface.createReceivePipeline(
            inputSampleRate_, outputSampleRate_,
            []() { return &g_State; },
            []() { return g_channel_noise; },
            []() { return wxGetApp().appConfiguration.noiseSNR; },
            []() { return g_RxFreqOffsetHz; },
            []() { return &g_sig_pwr_av; },
            helper_
        );
        rfDemodulationPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(rfDemodulationStep));
        
        // Replace received audio with microphone audio if we're monitoring TX/voice keyer recording.
        if (equalizedMicAudioLink_ != nullptr)
        {
            auto bypassMonitorAudio = new AudioPipeline(inputSampleRate_, outputSampleRate_);
            auto mutePipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_);
            
            auto monitorPipeline = new AudioPipeline(inputSampleRate_, outputSampleRate_);
            monitorPipeline->appendPipelineStep(equalizedMicAudioLink_->getOutputPipelineStep());
            
            auto monitorLevelStep = std::make_shared<LevelAdjustStep>(outputSampleRate_, [&]() {
                float volInDb = 0;
                if (g_voice_keyer_tx && wxGetApp().appConfiguration.monitorVoiceKeyerAudio)
                {
                    volInDb = wxGetApp().appConfiguration.monitorVoiceKeyerAudioVol;
                }
                else
                {
                    volInDb = wxGetApp().appConfiguration.monitorTxAudioVol;
                }
                
                return std::exp(volInDb/20.0f * std::log(10.0f));
            });
            monitorPipeline->appendPipelineStep(monitorLevelStep);

            auto muteStep = new MuteStep(outputSampleRate_);
            
            mutePipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(muteStep));
            
            auto eitherOrMuteStep = new EitherOrStep(
                []() { return g_recVoiceKeyerFile; },
                std::shared_ptr<IPipelineStep>(mutePipeline),
                std::shared_ptr<IPipelineStep>(bypassMonitorAudio)
            );

            auto eitherOrMicMonitorStep = new EitherOrStep(
                []() { return 
                    (g_voice_keyer_tx && wxGetApp().appConfiguration.monitorVoiceKeyerAudio) || 
                    (g_tx.load(std::memory_order_acquire) && wxGetApp().appConfiguration.monitorTxAudio); },
                std::shared_ptr<IPipelineStep>(monitorPipeline),
                std::shared_ptr<IPipelineStep>(eitherOrMuteStep)
            );
            bypassRfDemodulationPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrMicMonitorStep));
        }
        
        auto eitherOrRfDemodulationStep = new EitherOrStep(
            [this]() { return g_analog ||
                (equalizedMicAudioLink_ != nullptr && (
                    (g_recVoiceKeyerFile) ||
                    (g_voice_keyer_tx && wxGetApp().appConfiguration.monitorVoiceKeyerAudio) || 
                    (g_tx.load(std::memory_order_acquire) && wxGetApp().appConfiguration.monitorTxAudio)
                )); },
            std::shared_ptr<IPipelineStep>(bypassRfDemodulationPipeline),
            std::shared_ptr<IPipelineStep>(rfDemodulationPipeline)
        );

        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(eitherOrRfDemodulationStep));

        // Equalizer step (optional based on filter state)
        auto equalizerStep = new EqualizerStep(
            outputSampleRate_, 
            &g_rxUserdata->spkOutEQEnable,
            &g_rxUserdata->sbqSpkOutBass,
            &g_rxUserdata->sbqSpkOutMid,
            &g_rxUserdata->sbqSpkOutTreble,
            &g_rxUserdata->sbqSpkOutVol);
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(equalizerStep));
        
        // Resample for plot step (speech out)
        auto resampleForPlotOutStep = new ResampleForPlotStep(g_plotSpeechOutFifo);
        auto resampleForPlotOutPipeline = new AudioPipeline(outputSampleRate_, resampleForPlotOutStep->getOutputSampleRate());
#if defined(ENABLE_FASTER_PLOTS)
        auto resampleForPlotOutResampler = new ResampleStep(outputSampleRate_, resampleForPlotOutStep->getInputSampleRate(), true); // need to create manually to get access to "plot only" optimizations
        resampleForPlotOutPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(resampleForPlotOutResampler));
#endif // defined(ENABLE_FASTER_PLOTS)
        resampleForPlotOutPipeline->appendPipelineStep(std::shared_ptr<IPipelineStep>(resampleForPlotOutStep));

        auto resampleForPlotOutTap = new TapStep(outputSampleRate_, resampleForPlotOutPipeline);
        pipeline_->appendPipelineStep(std::shared_ptr<IPipelineStep>(resampleForPlotOutTap));
        
        // Clear anything in the FIFO before resuming decode.
        clearFifos_();
    }
}

void* TxRxThread::Entry()
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
    int numTimeSamples = 0;
    double minDuration = 1e9;
    double maxDuration = 0;
    double sumDuration = 0;
    double sumDoubleDuration = 0; 
#endif // defined(ENABLE_PROCESSING_STATS)

    while (m_run)
    {
#if defined(__linux__)
        const char* threadName = nullptr;
        if (m_tx) threadName = "FreeDV txThread";
        else threadName = "FreeDV rxThread";
        pthread_setname_np(pthread_self(), threadName);
#endif // defined(__linux__)

        if (!m_run) break;
        
        //log_info("thread woken up: m_tx=%d", (int)m_tx);
        helper->startRealTimeWork();

#if defined(ENABLE_PROCESSING_STATS)
        auto b = std::chrono::high_resolution_clock::now();
#endif // defined(ENABLE_PROCESSING_STATS)

        if (m_tx) txProcessing_(helper);
        else rxProcessing_(helper);

#if defined(ENABLE_PROCESSING_STATS)
        auto e = std::chrono::high_resolution_clock::now();
        auto d = std::chrono::duration_cast<std::chrono::nanoseconds>(e - b).count();
        numTimeSamples++; 
        if (d < minDuration) minDuration = d;
        if (d > maxDuration) maxDuration = d;
        sumDuration += d; sumDoubleDuration += pow(d, 2);
#endif // defined(ENABLE_PROCESSING_STATS)

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
    log_info("m_tx = %d, min = %f ns, max = %f ns, mean = %f ns, stdev = %f ns", m_tx, minDuration, maxDuration, sumDuration / numTimeSamples, sqrt((sumDoubleDuration - pow(sumDuration, 2)/numTimeSamples) / (numTimeSamples - 1)));
#endif // defined(ENABLE_PROCESSING_STATS)

    // Force pipeline to delete itself when we're done with the thread.
    pipeline_ = nullptr;
    
    // Return to normal scheduling
    helper->clearHelperRealTime();
    
    codec2_disable_realtime();
    
    return NULL;
}

void TxRxThread::OnExit() 
{ 
    // empty
}

void TxRxThread::terminateThread()
{
    m_run = 0;
    notify();
}

void TxRxThread::notify()
{
    // empty
}

void TxRxThread::clearFifos_()
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
}

//---------------------------------------------------------------------------------------------
// Main real time processing for tx and rx of FreeDV signals, run in its own threads
//---------------------------------------------------------------------------------------------

void TxRxThread::txProcessing_(IRealtimeHelper* helper) noexcept
#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
[[clang::nonblocking]]
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)
{
    wxStopWatch sw;
    paCallBackData  *cbData = g_rxUserdata;

    // Buffers re-used by tx and rx processing.  We take samples from
    // the sound card, and resample them for the freedv modem input
    // sample rate.  Typically the sound card is running at 48 or 44.1
    // kHz, and the modem at 8kHz

    //
    //  TX side processing --------------------------------------------
    //

    if (((g_nSoundCards == 2) && ((g_half_duplex && g_tx.load(std::memory_order_acquire)) || !g_half_duplex || g_voice_keyer_tx || g_recVoiceKeyerFile || g_recFileFromMic))) {        
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

        unsigned int nsam_one_modem_frame = freedvInterface.getTxNNomModemSamples() * ((float)outputSampleRate_ / (float)freedvInterface.getTxModemSampleRate());

     	if (g_dump_fifo_state) {
    	  // If this drops to zero we have a problem as we will run out of output samples
    	  // to send to the sound driver
    	  log_debug("outfifo1 used: %6d free: %6d nsam_one_modem_frame: %d",
                      cbData->outfifo1->numUsed(), cbData->outfifo1->numFree(), nsam_one_modem_frame);
    	}

        int nsam_in_48 = (inputSampleRate_ * FRAME_DURATION_MS) / MS_TO_SEC;
        assert(nsam_in_48 > 0);

        int             nout;

        while(!helper->mustStopWork() && (unsigned)cbData->outfifo1->numFree() >= nsam_one_modem_frame) {        
            // OK to generate a frame of modem output samples we need
            // an input frame of speech samples from the microphone.

            // infifo2 is written to by another sound card so it may
            // over or underflow, but we don't really care.  It will
            // just result in a short interruption in audio being fed
            // to codec2_enc, possibly making a click every now and
            // again in the decoded audio at the other end.

            // zero speech input just in case infifo2 underflows
            memset(inputSamples_.get(), 0, nsam_in_48*sizeof(short));
            
            // There may be recorded audio left to encode while ending TX. To handle this,
            // we keep reading from the FIFO until we have less than nsam_in_48 samples available.
            int nread = cbData->infifo2->read(inputSamples_.get(), nsam_in_48);            
            if (nread != 0 && endingTx)
            {
                if (freedvInterface.getCurrentMode() >= FREEDV_MODE_RADE)
                {
                    if (!hasEooBeenSent_)
                    {
                        // Special case for handling RADE EOT
                        freedvInterface.restartTxVocoder();
                        hasEooBeenSent_ = true;
                    }

                    auto outputSamples = pipeline_->execute(inputSamples_.get(), 0, &nout);
                    if (nout > 0 && outputSamples != nullptr)
                    {
                        if (cbData->outfifo1->write(outputSamples, nout) != 0)
                        {
                            log_warn("Could not inject resampled EOO samples (space remaining in FIFO = %d)", cbData->outfifo1->numFree());
                        }
                    }
                    else
                    {
                        g_eoo_enqueued = true;
                    }
                }
                break;
            }
            else
            {
                g_eoo_enqueued = false;
                hasEooBeenSent_ = false;
            }
            
            auto outputSamples = pipeline_->execute(inputSamples_.get(), nsam_in_48, &nout);
            
            if (g_dump_fifo_state) {
                log_info("  nout: %d", nout);
            }
            
            if (outputSamples != nullptr)
            {
                cbData->outfifo1->write(outputSamples, nout);
            }
        }
    }
    else
    {
        // Defer reset until next time we go into TX.
        deferReset_ = true;
    }

    if (g_dump_timing) {
        log_info("%4ld", sw.Time());
    }
}

void TxRxThread::rxProcessing_(IRealtimeHelper* helper) noexcept
#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
[[clang::nonblocking]]
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)
{
    wxStopWatch sw;
    paCallBackData  *cbData = g_rxUserdata;

    // Buffers re-used by tx and rx processing.  We take samples from
    // the sound card, and resample them for the freedv modem input
    // sample rate.  Typically the sound card is running at 48 or 44.1
    // kHz, and the modem at 8kHz.

    //
    //  RX side processing --------------------------------------------
    //
    
    if (g_queueResync)
    {
        log_debug("Unsyncing per user request.");
        g_queueResync = false;
        freedvInterface.setSync(FREEDV_SYNC_UNSYNC);
        g_resyncs++;
    }
    
    // Attempt to read one processing frame (about 20ms) of receive samples,  we 
    // keep this frame duration constant across modes and sound card sample rates
    int nsam = (inputSampleRate_ * FRAME_DURATION_MS) / MS_TO_SEC;
    assert(nsam > 0);

    int             nout;


    bool tmpTx = g_tx.load(std::memory_order_acquire);
    bool processInputFifo = 
        (g_voice_keyer_tx && wxGetApp().appConfiguration.monitorVoiceKeyerAudio) ||
        (tmpTx && wxGetApp().appConfiguration.monitorTxAudio) ||
        (!g_voice_keyer_tx && ((g_half_duplex && !tmpTx) || !g_half_duplex));
    if (!processInputFifo)
    {
        clearFifos_();
    }

    int nsam_one_speech_frame = freedvInterface.getRxNumSpeechSamples() * ((float)outputSampleRate_ / (float)freedvInterface.getRxSpeechSampleRate());
    auto outFifo = (g_nSoundCards == 1) ? cbData->outfifo1 : cbData->outfifo2;

    // while we have enough input samples available and enough space in the output FIFO ... 
    while (!helper->mustStopWork() && processInputFifo && outFifo->numFree() >= nsam_one_speech_frame && cbData->infifo1->read(inputSamples_.get(), nsam) == 0) {
        // send latest squelch level to FreeDV API, as it handles squelch internally
        freedvInterface.setSquelch(g_SquelchActive, g_SquelchLevel);

        auto outputSamples = pipeline_->execute(inputSamples_.get(), nsam, &nout);
        
        if (nout > 0 && outputSamples != nullptr)
        {
            outFifo->write(outputSamples, nout);
        }
        
        tmpTx = g_tx.load(std::memory_order_acquire);
        processInputFifo = 
            (g_voice_keyer_tx && wxGetApp().appConfiguration.monitorVoiceKeyerAudio) ||
            (tmpTx && wxGetApp().appConfiguration.monitorTxAudio) ||
            (!g_voice_keyer_tx && ((g_half_duplex && !tmpTx) || !g_half_duplex));
    }
}
