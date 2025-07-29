//==========================================================================
// Name:            freedv_interface.h
// Purpose:         Implements a wrapper around the Codec2 FreeDV interface.
// Created:         March 31, 2021
// Authors:         Mooneer Salem
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

#ifndef FREEDV_INTERFACE_H
#define FREEDV_INTERFACE_H

#include <pthread.h>

#include <deque>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <queue>
#include <future>
#include <atomic>

// Codec2 required include files.
#include "codec2.h"
#include "comp.h"
#include "modem_stats.h"
#include "reliable_text.h"

// RADE required include files
#include "rade_api.h"
#include "pipeline/rade_text.h"

// TBD - need to wrap in "extern C" to avoid linker errors
extern "C" 
{
    #include "fargan_config.h"
    #include "fargan.h"
    #include "lpcnet.h"
}

#include "util/IRealtimeHelper.h"

#include <samplerate.h>

class IPipelineStep;
class ParallelStep;
class RADETransmitStep;

// Anything above 255 is a RADE mode. There's only one right now,
// this is just for future expansion.
#define FREEDV_MODE_RADE ((1 << 8) + 1)

class FreeDVInterface
{
public:
    FreeDVInterface();
    virtual ~FreeDVInterface();
    
    void start(int txMode, int fifoSizeMs, bool singleRxThread, bool usingReliableText);
    void stop();
    void changeTxMode(int txMode);
    int getTxMode() const { return txMode_; }
    bool isRunning() const { return rade_ != nullptr || dvObjects_.size() > 0; }
    bool isModeActive(int mode) const { return std::find(enabledModes_.begin(), enabledModes_.end(), mode) != enabledModes_.end(); }
    void setRunTimeOptions(bool clip, bool bpf);
    
    const char* getCurrentModeStr() const;
    const char* getCurrentTxModeStr() const;
    bool usingTestFrames() const;
    void resetTestFrameStats();
    void resetBitStats();
    void setTestFrames(bool testFrames, bool combine);
    
    int getTotalBits();
    int getTotalBitErrors();
    
    int getCurrentMode() const { return rxMode_; }
    float getVariance() const;
    
    int getErrorPattern(short** outputPattern);
    
    int getSync() const;
    void setSync(int val);
    void setEq(int val);
    void setVerbose(bool val);
    
    void setTextCallbackFn(void (*rxFunc)(void *, char), char (*txFunc)(void *));
    
    void addRxMode(int mode) { enabledModes_.push_back(mode); }
    
    int getTxModemSampleRate() const;
    int getTxSpeechSampleRate() const;
    int getTxNumSpeechSamples() const;
    int getTxNNomModemSamples() const;
    
    int getRxModemSampleRate() const;
    int getRxNumModemSamples() const;
    int getRxNumSpeechSamples() const;
    int getRxSpeechSampleRate() const;
    
    void setLpcPostFilter(int enable, int bassBoost, float beta, float gamma);
    
    void setTextVaricodeNum(int num);
    
    void setSquelch(bool enable, float level);
    
    void setCarrierAmplitude(int c, float amp);
    
    struct MODEM_STATS* getCurrentRxModemStats() { return &modemStatsList_[modemStatsIndex_]; }
    
    void resetReliableText();
    const char* getReliableText();
    void setReliableText(const char* callsign);
    
    IPipelineStep* createTransmitPipeline(
        int inputSampleRate, 
        int outputSampleRate, 
        std::function<float()> getFreqOffsetFn,
        std::shared_ptr<IRealtimeHelper> realtimeHelper);
    IPipelineStep* createReceivePipeline(
        int inputSampleRate, int outputSampleRate,
        std::function<int*()> getRxStateFn,
        std::function<int()> getChannelNoiseFn,
        std::function<int()> getChannelNoiseSnrFn,
        std::function<float()> getFreqOffsetFn,
        std::function<float*()> getSigPwrAvgFn,
        std::shared_ptr<IRealtimeHelper> realtimeHelper
    );

    void restartTxVocoder();
 
    float getSNREstimate();
    
private:
    struct ReceivePipelineState
    {
        std::function<int*()> getRxStateFn;
        std::function<int()> getChannelNoiseFn;
        std::function<int()> getChannelNoiseSnrFn;
        std::function<float()> getFreqOffsetFn;
        std::function<float*()> getSigPwrAvgFn;
        std::function<int(ParallelStep*)> preProcessFn;
        std::function<int(ParallelStep*)> postProcessFn;
    };

    struct FreeDVTextFnState
    {
        FreeDVInterface* interfaceObj;
        struct freedv* modeObj;
    };
    
    static void FreeDVTextRxFn_(void *callback_state, char c);
    static void OnReliableTextRx_(reliable_text_t rt, const char* txt_ptr, int length, void* state);
    static void OnRadeTextRx_(rade_text_t rt, const char* txt_ptr, int length, void* state);
    
    static float GetMinimumSNR_(int mode);
    
    void (*textRxFunc_)(void *, char);
    bool singleRxThread_;
    int txMode_;
    int rxMode_;
    bool squelchEnabled_;
    std::deque<int> enabledModes_;
    std::deque<struct freedv*> dvObjects_;
    std::deque<FreeDVTextFnState*> textFnObjs_;
    std::deque<struct FIFO*> errorFifos_;
    std::deque<float> snrVals_;
    std::deque<float> squelchVals_;
    
    // Amount to add to squelch SNR to take into account different mode characteristics.
    // For example, if multi-RX is on, 700E's squelch would be the user's selection + 3.0dB
    // since 700E's minimum is 1.0dB and 700D's is -2.0dB.
    //
    // More info on minimum SNRs is at https://github.com/drowe67/codec2/blob/master/README_freedv.md.
    std::deque<float> snrAdjust_; 
    
    struct MODEM_STATS* modemStatsList_;
    int modemStatsIndex_;
    
    struct freedv* currentTxMode_;
    struct freedv* currentRxMode_; 
    struct freedv* lastSyncRxMode_;
    
    std::deque<reliable_text_t> reliableText_;
    std::string receivedReliableText_;
    std::mutex reliableTextMutex_;
   
    struct rade* rade_;
    FARGANState fargan_;
    LPCNetEncState *lpcnetEncState_; 
    RADETransmitStep *radeTxStep_;
    std::atomic<int> sync_;
    std::atomic<int> radeSnr_;
    rade_text_t radeTextPtr_;
    
    int preProcessRxFn_(ParallelStep* ps);
    int postProcessRxFn_(ParallelStep* ps);
};

#endif // FREEDV_INTERFACE_H
