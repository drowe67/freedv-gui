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

#ifndef CODEC2_INTERFACE_H
#define CODEC2_INTERFACE_H

#include <deque>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <queue>
#include <future>

// Codec2 required include files.
#include "codec2.h"
#include "comp.h"
#include "modem_stats.h"
#include "reliable_text.h"

#include <samplerate.h>

class IPipelineStep;

class FreeDVInterface
{
public:
    FreeDVInterface();
    virtual ~FreeDVInterface();
    
    void start(int txMode, int fifoSizeMs, bool singleRxThread, bool usingReliableText);
    void stop();
    void changeTxMode(int txMode);
    int getTxMode() const { return txMode_; }
    bool isRunning() const { return dvObjects_.size() > 0; }
    bool isModeActive(int mode) const { return std::find(enabledModes_.begin(), enabledModes_.end(), mode) != enabledModes_.end(); }
    void setRunTimeOptions(int clip, int bpf, int phaseEstBW, int phaseEstDPSK);
    
    const char* getCurrentModeStr() const;
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
    
    void setSquelch(int enable, float level);
    
    void setCarrierAmplitude(int c, float amp);
    
    int processRxAudio(
        short input[], int numFrames, struct FIFO* outputFifo, bool channelNoise, int noiseSnr, 
        float rxFreqOffsetHz, struct MODEM_STATS* stats, float* sig_pwr_av);
    
    struct MODEM_STATS* getCurrentRxModemStats() { return &modemStatsList_[modemStatsIndex_]; }
    
    void resetReliableText();
    const char* getReliableText();
    void setReliableText(const char* callsign);
    
    IPipelineStep* createTransmitPipeline(int inputSampleRate, int outputSampleRate, std::function<float()> getFreqOffsetFn);
private:
    struct FreeDVTextFnState
    {
        FreeDVInterface* interfaceObj;
        struct freedv* modeObj;
    };
    
    struct RxAudioThreadState
    {
        // Inputs
        struct freedv* dvObj;
        SRC_STATE* inRateConvObj;
        struct FIFO* ownInput;
        COMP* rxFreqOffsetPhaseRectObj;
        int modeIndex;
        short* inputBlock;
        int numFrames;
        bool channelNoise;
        int noiseSnr;
        float rxFreqOffsetHz;
        int rxModemSampleRate;
        int rxSpeechSampleRate;
        SRC_STATE* outRateConvObj;
        
        // Outputs
        struct FIFO* ownOutput;
        float sig_pwr_av;
    };
    
    template<typename R, typename T>
    class EventHandlerThread
    {
    public:
        EventHandlerThread();
        ~EventHandlerThread();
        
        std::shared_future<R> enqueue(std::function<R(T)> fn, T arg);
        
    private:
        struct Args 
        {
            Args(std::packaged_task<R(T)>&& t, T a)
                : task(std::move(t)), arg(a) { }

            Args(Args&& old)
                : task(std::move(old.task)), arg(old.arg) { }
            
            std::packaged_task<R(T)> task;
            T arg;
        };
    
        bool isEnding_;
        std::queue<Args> execQueue_;
        std::mutex execQueueMtx_;
        std::condition_variable_any execQueueCond_;
        std::thread execThread_;
                
        static void ThreadEntry_(EventHandlerThread<R,T>* ptr);
    };
    
    static void FreeDVTextRxFn_(void *callback_state, char c);
    static void OnReliableTextRx_(reliable_text_t rt, const char* txt_ptr, int length, void* state);
    
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
    std::deque<struct FIFO*> inputFifos_;
    std::deque<SRC_STATE*> inRateConvObjs_;
    std::deque<SRC_STATE*> outRateConvObjs_;
    std::deque<float> snrVals_;
    std::deque<float> squelchVals_;
    std::deque<EventHandlerThread<RxAudioThreadState*, RxAudioThreadState*> *> threads_;
    
    // Amount to add to squelch SNR to take into account different mode characteristics.
    // For example, if multi-RX is on, 700E's squelch would be the user's selection + 3.0dB
    // since 700E's minimum is 1.0dB and 700D's is -2.0dB.
    //
    // More info on minimum SNRs is at https://github.com/drowe67/codec2/blob/master/README_freedv.md.
    std::deque<float> snrAdjust_; 
    
    COMP txFreqOffsetPhaseRectObj_;
    std::deque<COMP*> rxFreqOffsetPhaseRectObjs_;
    struct MODEM_STATS* modemStatsList_;
    int modemStatsIndex_;
    
    struct freedv* currentTxMode_;
    struct freedv* currentRxMode_; 
    struct freedv* lastSyncRxMode_;
    
    std::deque<reliable_text_t> reliableText_;
    std::string receivedReliableText_;
};

template<typename R, typename T>
FreeDVInterface::EventHandlerThread<R, T>::EventHandlerThread()
    : isEnding_(false)
    , execThread_(ThreadEntry_, this)
{
    // empty
}

template<typename R, typename T>
FreeDVInterface::EventHandlerThread<R, T>::~EventHandlerThread()
{
    isEnding_ = true;
    execQueueCond_.notify_one();
    execThread_.join();
}

template<typename R, typename T>
std::shared_future<R> FreeDVInterface::EventHandlerThread<R, T>::enqueue(std::function<R(T)> fn, T arg)
{    
    std::packaged_task<R(T)> task(fn);
    std::future<R> fut = task.get_future();
    
    {
        const std::lock_guard<std::mutex> lock(execQueueMtx_);
        
        Args x(std::move(task), arg);
        execQueue_.push(std::move(x));
    }
    
    execQueueCond_.notify_one();
    return fut;
}

template<typename R, typename T>
void FreeDVInterface::EventHandlerThread<R, T>::ThreadEntry_(EventHandlerThread<R,T>* ptr)
{
    while(!ptr->isEnding_)
    {
        std::unique_lock<std::mutex> lock(ptr->execQueueMtx_);
        ptr->execQueueCond_.wait_for(lock, std::chrono::milliseconds(100));

        // No timeout; received request.
        while (!ptr->isEnding_ && ptr->execQueue_.size() > 0)
        {
            auto& fn = ptr->execQueue_.front();
            fn.task(fn.arg);
            ptr->execQueue_.pop();
        }
    }
}

#endif // CODEC2_INTERFACE_H