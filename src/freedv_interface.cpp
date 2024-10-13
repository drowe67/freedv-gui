//==========================================================================
// Name:            freedv_interface.cpp
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

#include <future>
#include "main.h"
#include "codec2_fdmdv.h"
#include "lpcnet.h"
#include "pipeline/ParallelStep.h"
#include "pipeline/FreeDVTransmitStep.h"
#include "pipeline/FreeDVReceiveStep.h"
#include "pipeline/RADEReceiveStep.h"
#include "pipeline/RADETransmitStep.h"
#include "pipeline/AudioPipeline.h"

using namespace std::placeholders;

static const char* GetCurrentModeStrImpl_(int mode)
{
    switch(mode)
    {
        case FREEDV_MODE_700C:
            return "700C";
        case FREEDV_MODE_700D:
            return "700D";
        case FREEDV_MODE_700E:
            return "700E";
        case FREEDV_MODE_1600:
            return "1600";
        case FREEDV_MODE_2020:
            return "2020";
#if defined(FREEDV_MODE_2020B)
        case FREEDV_MODE_2020B:
            return "2020B";
#endif // FREEDV_MODE_2020B
        case FREEDV_MODE_800XA:
            return "800XA";
        case FREEDV_MODE_RADE:
            return "RADE";
        default:
            return "unk";
    }
}

FreeDVInterface::FreeDVInterface() :
    textRxFunc_(nullptr),
    singleRxThread_(false),
    txMode_(0),
    rxMode_(0),
    squelchEnabled_(false),
    modemStatsList_(nullptr),
    modemStatsIndex_(0),
    currentTxMode_(nullptr),
    currentRxMode_(nullptr),
    lastSyncRxMode_(nullptr),
    rade_(nullptr),
    lpcnetEncState_(nullptr),
    radeTxStep_(nullptr)
{
    // empty
}

FreeDVInterface::~FreeDVInterface()
{
    if (isRunning()) stop();
}


static void callback_err_fn(void *fifo, short error_pattern[], int sz_error_pattern)
{
    codec2_fifo_write((struct FIFO*)fifo, error_pattern, sz_error_pattern);
}

void FreeDVInterface::OnReliableTextRx_(reliable_text_t rt, const char* txt_ptr, int length, void* state) 
{
    fprintf(stderr, "FreeDVInterface::OnReliableTextRx_: received %s\n", txt_ptr);
    
    FreeDVInterface* obj = (FreeDVInterface*)state;
    assert(obj != nullptr);
    
    {
        std::unique_lock<std::mutex> lock(obj->reliableTextMutex_);
        obj->receivedReliableText_ = txt_ptr;
    }
    reliable_text_reset(rt);
}

float FreeDVInterface::GetMinimumSNR_(int mode)
{
    switch(mode)
    {
        case FREEDV_MODE_700C:
            return 2.0;
        case FREEDV_MODE_700D:
            return -2.0f;
        case FREEDV_MODE_700E:
            return 1.0f;
        case FREEDV_MODE_1600:
            return 4.0f;
        case FREEDV_MODE_2020:
            return 2.0f;
        case FREEDV_MODE_800XA:
            return 2.0f;
        default:
            return 0.0f;
    }
}

void FreeDVInterface::start(int txMode, int fifoSizeMs, bool singleRxThread, bool usingReliableText)
{
    singleRxThread_ = singleRxThread;

    modemStatsList_ = new MODEM_STATS[enabledModes_.size()];
    for (int index = 0; index < (int)enabledModes_.size(); index++)
    {
        modem_stats_open(&modemStatsList_[index]);
    }
    
    float minimumSnr = 999.0f;
    for (auto& mode : enabledModes_)
    {
        if (mode >= FREEDV_MODE_RADE)
        {
            // Special case for RADE.
            // Note: multi-RX not currently supported.
            rxMode_ = mode;
            txMode_ = mode;
            modemStatsIndex_ = 0;

            rade_ = rade_open("TBD");
            assert(rade_ != nullptr);

            float zeros[320] = {0};
            float in_features[5*NB_TOTAL_FEATURES] = {0};
            fargan_init(&fargan_);
            fargan_cont(&fargan_, zeros, in_features);

            lpcnetEncState_ = lpcnet_encoder_create();
            assert(lpcnetEncState_ != nullptr);
            
            continue;
        }

        struct freedv* dv = freedv_open(mode);
        assert(dv != nullptr);
        
        snrVals_.push_back(-20);
        
        dvObjects_.push_back(dv);
        
        float tempSnr = GetMinimumSNR_(mode);
        if (tempSnr < minimumSnr)
        {
            minimumSnr = tempSnr;
        }
        snrAdjust_.push_back(tempSnr);
        squelchVals_.push_back(0.0f);
        
        FreeDVTextFnState* fnStateObj = new FreeDVTextFnState;
        fnStateObj->interfaceObj = this;
        fnStateObj->modeObj = dv;
        textFnObjs_.push_back(fnStateObj);
            
        struct FIFO* errFifo = codec2_fifo_create(2*freedv_get_sz_error_pattern(dv) + 1);
        assert(errFifo != nullptr);
        
        errorFifos_.push_back(errFifo);
        
        freedv_set_callback_error_pattern(dv, &callback_err_fn, errFifo);
        
        if (mode == txMode)
        {
            currentTxMode_ = dv;
            currentRxMode_ = dv;
            rxMode_ = mode;
            txMode_ = mode;
        }
        
        if (usingReliableText)
        {
            reliable_text_t rt = reliable_text_create();
            assert(rt != nullptr);
            
            reliable_text_use_with_freedv(rt, dv, &FreeDVInterface::OnReliableTextRx_, this);
            reliableText_.push_back(rt);
        }
    }
    
    // Loop back through SNR adjust list and subtract minimum from each entry.
    for (size_t index = 0; index < snrAdjust_.size(); index++)
    {
        snrAdjust_[index] -= minimumSnr;
    }
}

void FreeDVInterface::stop()
{
    snrAdjust_.clear();
    squelchVals_.clear();
    snrVals_.clear();
    
    for (auto& fnState : textFnObjs_)
    {
        delete fnState;
    }
    textFnObjs_.clear();
    
    for (int index = 0; index < (int)enabledModes_.size(); index++)
    {
        modem_stats_close(&modemStatsList_[index]);
    }
    delete[] modemStatsList_;
    modemStatsList_ = nullptr;
    
    for (auto& reliableTextObj : reliableText_)
    {
        reliable_text_destroy(reliableTextObj);
    }
    reliableText_.clear();
    
    for (auto& dv : dvObjects_)
    {
        freedv_close(dv);
    }
    dvObjects_.clear();
    
    for (auto& fifo : errorFifos_)
    {
        codec2_fifo_destroy(fifo);
    }
    errorFifos_.clear();
        
    enabledModes_.clear();
    
    modemStatsList_ = nullptr;
    currentTxMode_ = nullptr;
    currentRxMode_ = nullptr;
    modemStatsIndex_ = 0;
    txMode_ = 0;
    rxMode_ = 0;

    if (rade_ != nullptr)
    {
        rade_close(rade_);
    }
    rade_ = nullptr;

    if (lpcnetEncState_ != nullptr)
    {
        lpcnet_encoder_destroy(lpcnetEncState_);
    }
    lpcnetEncState_ = nullptr;
    radeTxStep_ = nullptr;
}

void FreeDVInterface::setRunTimeOptions(bool clip, bool bpf)
{
    for (auto& dv : dvObjects_)
    {
        freedv_set_clip(dv, clip);   // 700D/700E
        freedv_set_tx_bpf(dv, bpf);  // 700D/700E
    }
}

bool FreeDVInterface::usingTestFrames() const
{
    bool result = false;
    for (auto& dv : dvObjects_)
    {
        result |= freedv_get_test_frames(dv);
    }
    return result;
}

void FreeDVInterface::resetTestFrameStats()
{
    for (auto& dv : dvObjects_)
    {
        freedv_set_test_frames(dv, 1);
    }
    resetBitStats();
}

void FreeDVInterface::resetBitStats()
{
    for (auto& dv : dvObjects_)
    {
        freedv_set_total_bits(dv, 0);
        freedv_set_total_bit_errors(dv, 0);
    }
}

void FreeDVInterface::setTestFrames(bool testFrames, bool combine)
{
    for (auto& dv : dvObjects_)
    {
        freedv_set_test_frames(dv, testFrames);
        freedv_set_test_frames_diversity(dv, combine);
    }
}

int FreeDVInterface::getTotalBits()
{
    // Special case for RADE.
    if (currentRxMode_ == nullptr) return 1;

    return freedv_get_total_bits(currentRxMode_);
}

int FreeDVInterface::getTotalBitErrors()
{
    // Special case for RADE.
    if (currentRxMode_ == nullptr) return 0;

    return freedv_get_total_bit_errors(currentRxMode_);
}

float FreeDVInterface::getVariance() const
{
    // Special case for RADE.
    if (currentRxMode_ == nullptr) return 0.0;

    struct CODEC2 *c2 = freedv_get_codec2(currentRxMode_);
    if (c2 != NULL)
        return codec2_get_var(c2);
    else
        return 0.0;
}

int FreeDVInterface::getErrorPattern(short** outputPattern)
{
    // Special case for RADE.
    if (currentRxMode_ == nullptr) return 0;

    int size = freedv_get_sz_error_pattern(currentRxMode_);
    if (size > 0)
    {
        *outputPattern = new short[size];
        int index = 0;
        for (auto& dv : dvObjects_)
        {
            if (dv == currentRxMode_)
            {
                struct FIFO* currentErrFifo = errorFifos_[index];
                if (codec2_fifo_read(currentErrFifo, *outputPattern, size) == 0)
                {
                    return size;
                }
            }
            index++;
        }
        
        delete[] *outputPattern;
        *outputPattern = nullptr;
    }
    
    return 0;
}

const char* FreeDVInterface::getCurrentModeStr() const
{
    return GetCurrentModeStrImpl_(rxMode_);
}

const char* FreeDVInterface::getCurrentTxModeStr() const
{
    return GetCurrentModeStrImpl_(txMode_);
}

void FreeDVInterface::changeTxMode(int txMode)
{
    if (txMode >= FREEDV_MODE_RADE)
    {
        txMode_ = txMode;
        return;
    }

    int index = 0;
    for (auto& mode : enabledModes_)
    {
        if (mode == txMode)
        {
            currentTxMode_ = dvObjects_[index];
            txMode_ = mode;            
            return;
        }
        index++;
    }
    
    // Cannot change to mode we're not already listening for.
    assert(false);
}

void FreeDVInterface::setSync(int val)
{
    // Special case for RADE.
    if (currentRxMode_ == nullptr) return;

    for (auto& dv : dvObjects_)
    {
        // TBD: do it only for 700*.
        freedv_set_sync(dv, val);
    }
}

int FreeDVInterface::getSync() const
{
    // Special case for RADE.
    if (currentRxMode_ == nullptr)
    {
        return rade_sync(rade_);
    }

    return freedv_get_sync(currentRxMode_);
}

void FreeDVInterface::setEq(int val)
{
    for (auto& dv : dvObjects_)
    {
        freedv_set_eq(dv, val);
    }
}

void FreeDVInterface::setCarrierAmplitude(int c, float amp)
{
    for (auto& dv : dvObjects_)
    {
        freedv_set_carrier_ampl(dv, c, amp);
    }
}

void FreeDVInterface::setVerbose(bool val)
{
    for (auto& dv : dvObjects_)
    {
        freedv_set_verbose(dv, val ? 2 : 0);
    }
}

void FreeDVInterface::FreeDVTextRxFn_(void *callback_state, char c)
{
    FreeDVTextFnState* fnState = (FreeDVTextFnState*)callback_state;
    if (fnState->modeObj == fnState->interfaceObj->currentRxMode_)
    {
        // Only forward text onto RX function if this is the currently sync'd mode.
        fnState->interfaceObj->textRxFunc_(callback_state, c);
    }
}

void FreeDVInterface::setTextCallbackFn(void (*rxFunc)(void *, char), char (*txFunc)(void *))
{
    textRxFunc_ = rxFunc;
    
    int index = 0;
    for (auto& dv : dvObjects_)
    {
        freedv_set_callback_txt(dv, &FreeDVTextRxFn_, txFunc, textFnObjs_[index++]);
    }
}

int FreeDVInterface::getTxModemSampleRate() const
{
    if (txMode_ >= FREEDV_MODE_RADE)
    {
        return RADE_MODEM_SAMPLE_RATE;
    }

    assert(currentTxMode_ != nullptr);
    return freedv_get_modem_sample_rate(currentTxMode_);
}

int FreeDVInterface::getTxSpeechSampleRate() const
{
    if (txMode_ >= FREEDV_MODE_RADE)
    {
        return RADE_SPEECH_SAMPLE_RATE;
    }

    assert(currentTxMode_ != nullptr);
    return freedv_get_speech_sample_rate(currentTxMode_);
}

int FreeDVInterface::getTxNumSpeechSamples() const
{
    if (txMode_ >= FREEDV_MODE_RADE)
    {
        return 1920;
    }

    assert(currentTxMode_ != nullptr);
    return freedv_get_n_speech_samples(currentTxMode_);   
}

int FreeDVInterface::getTxNNomModemSamples() const
{
    if (txMode_ >= FREEDV_MODE_RADE)
    {
        return 960;
    }

    assert(currentTxMode_ != nullptr);
    return freedv_get_n_nom_modem_samples(currentTxMode_);   
}

void FreeDVInterface::setLpcPostFilter(int enable, int bassBoost, float beta, float gamma)
{
    for (auto& dv : dvObjects_)
    {
        struct CODEC2 *c2 = freedv_get_codec2(dv);
        if (c2 != NULL) 
        {
            codec2_set_lpc_post_filter(c2, enable, bassBoost, beta, gamma);
        }
    }
}

void FreeDVInterface::setTextVaricodeNum(int num)
{
    for (auto& dv : dvObjects_)
    {
        freedv_set_varicode_code_num(dv, num);
    }
}

int FreeDVInterface::getRxModemSampleRate() const
{
    if (rxMode_ >= FREEDV_MODE_RADE)
    {
        return RADE_MODEM_SAMPLE_RATE;
    }

    int result = 0;
    for (auto& dv : dvObjects_)
    {
        int tmp = freedv_get_modem_sample_rate(dv);
        if (tmp > result) result = tmp;
    }
    return result;
}

int FreeDVInterface::getRxNumModemSamples() const
{
    if (rxMode_ >= FREEDV_MODE_RADE)
    {
        return 960;
    }

    int result = 0;
    for (auto& dv : dvObjects_)
    {
        int tmp = freedv_get_n_max_modem_samples(dv);
        if (tmp > result) result = tmp;
    }
    return result;
}

int FreeDVInterface::getRxNumSpeechSamples() const
{
    if (rxMode_ >= FREEDV_MODE_RADE)
    {
        return 1920;
    }

    int result = 0;
    for (auto& dv : dvObjects_)
    {
        int tmp = freedv_get_n_speech_samples(dv);
        if (tmp > result) result = tmp;
    }
    return result;
}

int FreeDVInterface::getRxSpeechSampleRate() const
{
    if (rxMode_ >= FREEDV_MODE_RADE)
    {
        return RADE_SPEECH_SAMPLE_RATE;
    }

    int result = 0;
    for (auto& dv : dvObjects_)
    {
        int tmp = freedv_get_speech_sample_rate(dv);
        if (tmp > result) result = tmp;
    }
    return result;
}

void FreeDVInterface::setSquelch(bool enable, float level)
{
    int index = 0;
    
    squelchEnabled_ = enable;
    
    for (auto& dv : dvObjects_)
    {
        freedv_set_squelch_en(dv, enable);
        squelchVals_[index] = level + snrAdjust_[index];
        freedv_set_snr_squelch_thresh(dv, squelchVals_[index++]);
    }
}

void FreeDVInterface::resetReliableText()
{
    for (auto& rt : reliableText_)
    {
        reliable_text_reset(rt);
    }
    
    {
        std::unique_lock<std::mutex> lock(reliableTextMutex_);
        receivedReliableText_ = "";
    }
}

const char* FreeDVInterface::getReliableText()
{
    std::unique_lock<std::mutex> lock(reliableTextMutex_);
    char* ret = new char[receivedReliableText_.size() + 1];
    assert(ret != nullptr);
    
    if (receivedReliableText_.size() > 0)
    {
        strncpy(ret, receivedReliableText_.c_str(), receivedReliableText_.size());
    }
    ret[receivedReliableText_.size()] = 0;
    return ret;
}

void FreeDVInterface::setReliableText(const char* callsign)
{
    for (auto& rt : reliableText_)
    {
        reliable_text_set_string(rt, callsign, strlen(callsign));
    }
}

IPipelineStep* FreeDVInterface::createTransmitPipeline(int inputSampleRate, int outputSampleRate, std::function<float()> getFreqOffsetFn)
{
    std::vector<IPipelineStep*> parallelSteps;

    if (txMode_ >= FREEDV_MODE_RADE)
    {
        // Special handling for RADE
        radeTxStep_ = new RADETransmitStep(rade_, lpcnetEncState_);
        parallelSteps.push_back(radeTxStep_);
    }
 
    for (auto& dv : dvObjects_)
    {
        parallelSteps.push_back(new FreeDVTransmitStep(dv, getFreqOffsetFn));
    }
    
    std::function<int(ParallelStep*)> modeFn = 
        [&](ParallelStep*) {
            int index = 0;

            // Special handling for RADE.
            if (txMode_ >= FREEDV_MODE_RADE) return 0;

            for (auto& dv : dvObjects_)
            {
                if (dv == currentTxMode_) return index;
                index++;
            }
            return -1;
        };
        
    auto parallelStep = new ParallelStep(
        inputSampleRate,
        outputSampleRate,
        false,
        modeFn,
        modeFn,
        parallelSteps,
        nullptr
    );
    
    return parallelStep;
}

IPipelineStep* FreeDVInterface::createReceivePipeline(
    int inputSampleRate, int outputSampleRate,
    std::function<int*()> getRxStateFn,
    std::function<int()> getChannelNoiseFn,
    std::function<int()> getChannelNoiseSnrFn,
    std::function<float()> getFreqOffsetFn,
    std::function<float*()> getSigPwrAvgFn)
{
    std::vector<IPipelineStep*> parallelSteps;

    std::shared_ptr<ReceivePipelineState> state = std::make_shared<ReceivePipelineState>();
    assert(state != nullptr);

    state->getRxStateFn = getRxStateFn;
    state->getChannelNoiseFn = getChannelNoiseFn;
    state->getChannelNoiseSnrFn = getChannelNoiseSnrFn;
    state->getFreqOffsetFn = getFreqOffsetFn;
    state->getSigPwrAvgFn = getSigPwrAvgFn;
   
    if (txMode_ >= FREEDV_MODE_RADE)
    {
        // special handling for RADE
        parallelSteps.push_back(new RADEReceiveStep(rade_, &fargan_));

        state->preProcessFn = [&](ParallelStep*) { return 0; };
        state->postProcessFn = [&](ParallelStep*) { return 0; };
    }
    else
    { 
        for (auto& dv : dvObjects_)
        {
            auto recvStep = new FreeDVReceiveStep(dv);
            assert(recvStep != nullptr);
        
            parallelSteps.push_back(recvStep);
        }

        state->preProcessFn = std::bind(&FreeDVInterface::preProcessRxFn_, this, _1);
        state->postProcessFn = std::bind(&FreeDVInterface::postProcessRxFn_, this, _1);
    } 
        
    auto parallelStep = new ParallelStep(
        inputSampleRate,
        outputSampleRate,
        !singleRxThread_,
        state->preProcessFn,
        state->postProcessFn,
        parallelSteps,
        state
    );
    
    return parallelStep;
}

void FreeDVInterface::restartTxVocoder() 
{ 
    radeTxStep_->restartVocoder(); 
}

int FreeDVInterface::preProcessRxFn_(ParallelStep* stepObj)
{
    int rxIndex = 0;
    std::shared_ptr<ReceivePipelineState> state = std::static_pointer_cast<ReceivePipelineState>(stepObj->getState());

    // Set initial state for each step prior to execution.
    for (auto& step : stepObj->getParallelSteps())
    {
        assert(step != nullptr);
        
        FreeDVReceiveStep* castedStep = (FreeDVReceiveStep*)step.get();
        castedStep->setSigPwrAvg(*state->getSigPwrAvgFn());
        castedStep->setChannelNoiseEnable(state->getChannelNoiseFn(), state->getChannelNoiseSnrFn());
        castedStep->setFreqOffset(state->getFreqOffsetFn());
    }
    
    // If the current RX mode is still sync'd, only process through that one.
    for (auto& dv : dvObjects_)
    {
        if (dv == currentRxMode_ && freedv_get_sync(currentRxMode_))
        {
            return rxIndex;
        }
        rxIndex++;
    }
    
    return -1;
};

int FreeDVInterface::postProcessRxFn_(ParallelStep* stepObj)
{
    std::shared_ptr<ReceivePipelineState> state = std::static_pointer_cast<ReceivePipelineState>(stepObj->getState());

    // If the current RX mode is still sync'd, only let that one out.
    int rxIndex = 0;
    int indexWithSync = 0;
    int maxSyncFound = -25;
    struct freedv* dvWithSync = nullptr;

    if (dvObjects_.size() == 0) goto skipSyncCheck;

    for (auto& dv : dvObjects_)
    {
        if (dv == currentRxMode_ && freedv_get_sync(currentRxMode_))
        {
            dvWithSync = dv;
            indexWithSync = rxIndex;
            goto skipSyncCheck;
        }
        rxIndex++;
    }
    
    // Otherwise, find the mode that's sync'd and has the highest SNR.
    rxIndex = 0;
    for (auto& dv : dvObjects_)
    {
        struct MODEM_STATS *tmpStats = &modemStatsList_[rxIndex];
        freedv_get_modem_extended_stats(dv, tmpStats);
    
        if (!(isnan(tmpStats->snr_est) || isinf(tmpStats->snr_est))) {
            snrVals_[rxIndex] = 0.95*snrVals_[rxIndex] + (1.0 - 0.95)*tmpStats->snr_est;
        }
        int snr = (int)(snrVals_[rxIndex]+0.5);
        
        bool canUnsquelch = !squelchEnabled_ ||
            (squelchEnabled_ && snr >= squelchVals_[rxIndex]);
        
        if (snr > maxSyncFound && tmpStats->sync != 0 && canUnsquelch)
        {
            maxSyncFound = snr;
            indexWithSync = rxIndex;
            dvWithSync = dv;
        }
        
        rxIndex++;
    }
    
    if (dvWithSync == nullptr)
    {
        // Default to the TX DV object if there's no sync.
        int index = 0;
        indexWithSync = index;
        for (auto& mode : enabledModes_)
        {
            if (mode == txMode_)
            {
                indexWithSync = index;
                break;
            }
            index++;
        }
        dvWithSync = dvObjects_[indexWithSync];
    }

skipSyncCheck:        
    struct MODEM_STATS* stats = getCurrentRxModemStats();

    if (dvWithSync != nullptr)
    {    
        // grab extended stats so we can plot spectrum, scatter diagram etc
        freedv_get_modem_extended_stats(dvWithSync, stats);

        // Update sync as it may have gone stale during decode
        *state->getRxStateFn() = stats->sync != 0;
            
        if (*state->getRxStateFn())
        {
            rxMode_ = enabledModes_[indexWithSync];  
            currentRxMode_ = dvWithSync;
            lastSyncRxMode_ = currentRxMode_;
        } 

        *state->getSigPwrAvgFn() = ((FreeDVReceiveStep*)stepObj->getParallelSteps()[indexWithSync].get())->getSigPwrAvg();
    }
    else
    {
        *state->getRxStateFn() = rade_sync(rade_);
    }

    return indexWithSync;
};
