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

FreeDVInterface::FreeDVInterface() :
    textRxFunc_(nullptr),
    singleRxThread_(false),
    txMode_(0),
    rxMode_(0),
    squelchEnabled_(false),
    currentTxMode_(nullptr),
    currentRxMode_(nullptr),
    lastSyncRxMode_(nullptr)
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

void FreeDVInterface::start(int txMode, int fifoSizeMs, bool singleRxThread)
{
    singleRxThread_ = singleRxThread;
    
    int src_error = 0;
    for (auto& mode : enabledModes_)
    {
        struct freedv* dv = nullptr;
        if ((mode == FREEDV_MODE_700D) || (mode == FREEDV_MODE_700E) || (mode == FREEDV_MODE_2020)) {
            // 700 has some init time stuff so treat it special
            struct freedv_advanced adv;
            dv = freedv_open_advanced(mode, &adv);
        } else {
            dv = freedv_open(mode);
        }
        assert(dv != nullptr);
        
        dvObjects_.push_back(dv);
        
        FreeDVTextFnState* fnStateObj = new FreeDVTextFnState;
        fnStateObj->interfaceObj = this;
        fnStateObj->modeObj = dv;
        textFnObjs_.push_back(fnStateObj);
            
        struct FIFO* errFifo = codec2_fifo_create(2*freedv_get_sz_error_pattern(dv) + 1);
        assert(errFifo != nullptr);
        
        errorFifos_.push_back(errFifo);
        
        txFreqOffsetPhaseRectObj_.real = cos(0.0);
        txFreqOffsetPhaseRectObj_.imag = sin(0.0);
        
        auto tmpPtr = new COMP();
        tmpPtr->real = cos(0.0);
        tmpPtr->imag = sin(0.0);
        rxFreqOffsetPhaseRectObjs_.push_back(tmpPtr);
                
        // Assume 48K for input FIFO just to be sure. We can readjust later.
        struct FIFO* inFifo = codec2_fifo_create(fifoSizeMs * 48000/1000);  
        assert(inFifo != nullptr);      
        inputFifos_.push_back(inFifo);
        
        freedv_set_callback_error_pattern(dv, &callback_err_fn, errFifo);
        
        if (mode == txMode)
        {
            currentTxMode_ = dv;
            currentRxMode_ = dv;
            rxMode_ = mode;
            txMode_ = mode;
        }
        
        auto inConvertObj = src_new(SRC_SINC_FASTEST, 1, &src_error);
        assert(inConvertObj != nullptr);
        inRateConvObjs_.push_back(inConvertObj);
        
        auto outConvertObj = src_new(SRC_SINC_FASTEST, 1, &src_error);
        assert(outConvertObj != nullptr);
        outRateConvObjs_.push_back(outConvertObj);
        
        threads_.push_back(new EventHandlerThread<RxAudioThreadState*, RxAudioThreadState*>());
    }
}

void FreeDVInterface::stop()
{
    for (auto& fnState : textFnObjs_)
    {
        delete fnState;
    }
    textFnObjs_.clear();
    
    for (auto& thd : threads_)
    {
        delete thd;
    }
    threads_.clear();
    
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
    
    for (auto& conv : inRateConvObjs_)
    {
        src_delete(conv);
    }
    inRateConvObjs_.clear();
    
    for (auto& conv : outRateConvObjs_)
    {
        src_delete(conv);
    }
    outRateConvObjs_.clear();
    
    for (auto& tmp : rxFreqOffsetPhaseRectObjs_)
    {
        delete tmp;
    }
    rxFreqOffsetPhaseRectObjs_.clear();
        
    enabledModes_.clear();
    
    currentTxMode_ = nullptr;
    currentRxMode_ = nullptr;
    txMode_ = 0;
    rxMode_ = 0;
}

void FreeDVInterface::setRunTimeOptions(int clip, int bpf, int phaseEstBW, int phaseEstDPSK)
{
    for (auto& dv : dvObjects_)
    {
        freedv_set_clip(dv, clip);   // 700D/700E
        freedv_set_tx_bpf(dv, bpf);  // 700D/700E
        freedv_set_phase_est_bandwidth_mode(dv, phaseEstBW); // 700D/2020
        freedv_set_dpsk(dv, phaseEstDPSK);  // 700D/2020
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
    return freedv_get_total_bits(currentRxMode_);
}

int FreeDVInterface::getTotalBitErrors()
{
    return freedv_get_total_bit_errors(currentRxMode_);
}

float FreeDVInterface::getVariance() const
{
    struct CODEC2 *c2 = freedv_get_codec2(currentRxMode_);
    if (c2 != NULL)
        return codec2_get_var(c2);
    else
        return 0.0;
}

int FreeDVInterface::getErrorPattern(short** outputPattern)
{
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
    }
    
    return 0;
}

const char* FreeDVInterface::getCurrentModeStr() const
{
    if (currentRxMode_ == nullptr)
    {
        return "unk";
    }
    else
    {
        switch(rxMode_)
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
            case FREEDV_MODE_800XA:
                return "800XA";
            case FREEDV_MODE_2400B:
                return "2400B";
            default:
                return "unk";
        }
    }
}


void FreeDVInterface::changeTxMode(int txMode)
{
    int index = 0;
    for (auto& mode : enabledModes_)
    {
        if (mode == txMode)
        {
            currentTxMode_ = dvObjects_[index];
            txMode_ = mode;
            
            // Reset RX and TX offsets to help FreeDV stay on target during mode changes.
            auto tmpPtr = &txFreqOffsetPhaseRectObj_;
            tmpPtr->real = cos(0.0);
            tmpPtr->imag = sin(0.0);
        
            tmpPtr = rxFreqOffsetPhaseRectObjs_[index];
            tmpPtr->real = cos(0.0);
            tmpPtr->imag = sin(0.0);
            
            return;
        }
        index++;
    }
    
    // Cannot change to mode we're not already listening for.
    assert(false);
}

void FreeDVInterface::setSync(int val)
{
    for (auto& dv : dvObjects_)
    {
        // TBD: do it only for 700*.
        freedv_set_sync(dv, val);
    }
}

int FreeDVInterface::getSync() const
{
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
    assert(currentTxMode_ != nullptr);
    return freedv_get_modem_sample_rate(currentTxMode_);
}

int FreeDVInterface::getTxSpeechSampleRate() const
{
    assert(currentTxMode_ != nullptr);
    return freedv_get_speech_sample_rate(currentTxMode_);
}

int FreeDVInterface::getTxNumSpeechSamples() const
{
    assert(currentTxMode_ != nullptr);
    return freedv_get_n_speech_samples(currentTxMode_);   
}

int FreeDVInterface::getTxNNomModemSamples() const
{
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
    int result = 0;
    for (auto& dv : dvObjects_)
    {
        int tmp = freedv_get_speech_sample_rate(dv);
        if (tmp > result) result = tmp;
    }
    return result;
}

void FreeDVInterface::setSquelch(int enable, float level)
{
    squelchEnabled_ = enable;
    
    for (auto& dv : dvObjects_)
    {
        freedv_set_squelch_en(dv, enable);
        freedv_set_snr_squelch_thresh(dv, level);
    }
}

int FreeDVInterface::processRxAudio(
    short input[], int numFrames, struct FIFO* outputFifo, bool channelNoise, int noiseSnr, 
    float rxFreqOffsetHz, struct MODEM_STATS* stats, float* sig_pwr_av)
{
    int   state = getSync();
    bool  done = false;
    std::deque<std::shared_future<RxAudioThreadState*>> rxFutures;
    for (auto index = 0; (size_t)index < dvObjects_.size(); index++)
    {
        if (singleRxThread_ && state != 0 && dvObjects_[index] != currentRxMode_) 
        {
            // Skip processing of all except for the current receiving mdoe if in sync.
            done = true;
            continue;
        }
        
        // Initialize task inputs and outputs
        RxAudioThreadState* inpState = new RxAudioThreadState();
        inpState->dvObj = dvObjects_[index];
        inpState->inRateConvObj = inRateConvObjs_[index];
        inpState->ownInput = inputFifos_[index];
        inpState->rxFreqOffsetPhaseRectObj = rxFreqOffsetPhaseRectObjs_[index];
        inpState->modeIndex = index;
        inpState->inputBlock = new short[numFrames];
        memcpy(inpState->inputBlock, input, sizeof(short)*numFrames);
        inpState->numFrames = numFrames;
        inpState->channelNoise = channelNoise;
        inpState->noiseSnr = noiseSnr;
        inpState->rxFreqOffsetHz = rxFreqOffsetHz;
        inpState->rxModemSampleRate = getRxModemSampleRate();
        inpState->rxSpeechSampleRate = getRxSpeechSampleRate();
        inpState->outRateConvObj = outRateConvObjs_[index];
        
        // 1s of audio at 48kbps; we shouldn't get anywhere close due to how often 
        // this function is called.
        inpState->ownOutput = codec2_fifo_create(48000); 
        inpState->sig_pwr_av = *sig_pwr_av;
        
        // Wrap the below in an async task.
        auto fut = threads_[index]->enqueue([this](RxAudioThreadState* st) {
            short infreedv[10*N48];
            int   nfreedv;
                
            // Resample from maximum sample rate to the one the current codec expects.
            auto& convertObj = st->inRateConvObj;
            auto& dv = st->dvObj;
            nfreedv = resample(convertObj, infreedv, st->inputBlock, freedv_get_modem_sample_rate(dv), st->rxModemSampleRate, 10*N48, st->numFrames);
            assert(nfreedv <= 10*N48);
            delete[] st->inputBlock;
            
            // Push resampled data into appropriate fifo.
            auto& inFifo = st->ownInput;
            codec2_fifo_write(inFifo, infreedv, nfreedv);
        
            // Begin processing using the current codec.
            int numSpeechSamples = getRxNumSpeechSamples();
            short input_buf[freedv_get_n_max_modem_samples(dv)];
            short output_buf[freedv_get_n_speech_samples(dv)];
            short output_resample_buf[numSpeechSamples];
            COMP  rx_fdm[freedv_get_n_max_modem_samples(dv)];
            COMP  rx_fdm_offset[freedv_get_n_max_modem_samples(dv)];
            int   nin = freedv_nin(dv);
            int   nout = 0;
            while (codec2_fifo_read(inFifo, input_buf, nin) == 0) 
            {
                assert(nin <= freedv_get_n_max_modem_samples(dv));

                // demod per frame processing
                for(int i=0; i<nin; i++) {
                    rx_fdm[i].real = (float)input_buf[i];
                    rx_fdm[i].imag = 0.0;
                }

                // Optional channel noise
                if (st->channelNoise) {
                    fdmdv_simulate_channel(&st->sig_pwr_av, rx_fdm, nin, st->noiseSnr);
                }

                // Optional frequency shifting
                freq_shift_coh(rx_fdm_offset, rx_fdm, st->rxFreqOffsetHz, freedv_get_modem_sample_rate(dv), st->rxFreqOffsetPhaseRectObj, nin);
                nout = freedv_comprx(dv, output_buf, rx_fdm_offset);
            
                // Resample output to the current mode's rate if needed.
                {
                    nout = resample(st->outRateConvObj, output_resample_buf, output_buf, st->rxSpeechSampleRate, freedv_get_speech_sample_rate(dv), numSpeechSamples, nout);
                }
                
                codec2_fifo_write(st->ownOutput, output_resample_buf, nout);
            
                nin = freedv_nin(dv);
            }
                    
            return st;
        }, inpState);
        
        if (singleRxThread_)
        {
            fut.wait();
        }
        
        rxFutures.push_back(fut);
        
        if (singleRxThread_ && done) break;
    }
    
    // Loop through each future and wait for the result, then determine
    // which mode is most likely to have sync. Default to the last mode
    // in the list if we don't find any.
    int futIndexWithSync = rxFutures.size() - 1;
    int futIndex = 0;
    int maxSyncFound = -25;
    std::deque<RxAudioThreadState*> results;
    for(auto& fut : rxFutures)
    {
        if (!singleRxThread_)
        {
            fut.wait();
        }
        
        RxAudioThreadState* res = fut.get();
        results.push_back(res);
        
        struct MODEM_STATS tmpStats;
        freedv_get_modem_extended_stats(res->dvObj, &tmpStats);
        
        int snr = -10;
        if (!isnan(tmpStats.snr_est) && !isinf(tmpStats.snr_est))
            snr = tmpStats.snr_est + 0.5;
        
        if (lastSyncRxMode_ == res->dvObj)
        {
            if (stats->sync != 0)
            {
                futIndexWithSync = futIndex;
                break;
            }
            
            lastSyncRxMode_ = nullptr;
        }
        
        if (snr > maxSyncFound && tmpStats.sync != 0)
        {
            maxSyncFound = snr;
            futIndexWithSync = futIndex;
        }
        
        futIndex++;
    }
    
    futIndex = 0;
    for(auto& res : results)
    {
        // Write to output FIFO on one of the following conditions:
        //   a) We're on the last mode to check (meaning that we didn't get sync on any other mode)
        //   b) We got sync on the current mode at some point.
        if (futIndex == futIndexWithSync)
        {            
            // grab extended stats so we can plot spectrum, scatter diagram etc
            freedv_get_modem_extended_stats(res->dvObj, stats);
        
            // Update sync as it may have gone stale during decode
            state = stats->sync != 0;
                        
            if (state)
            {
                rxMode_ = enabledModes_[res->modeIndex];  
                currentRxMode_ = res->dvObj;
                lastSyncRxMode_ = currentRxMode_;
            } 
            
            if (res->channelNoise) 
            {
                *sig_pwr_av = res->sig_pwr_av;
            }
            
            int usedFifo = codec2_fifo_used(res->ownOutput);
            if (usedFifo > 0 && (!squelchEnabled_ || state)) /* suppresses random pops */
            {
                short input_buf[48000];
                while(codec2_fifo_read(res->ownOutput, input_buf, usedFifo) == 0)
                {
                    codec2_fifo_write(outputFifo, input_buf, usedFifo);
                }
            }
        }
     
        codec2_fifo_free(res->ownOutput);
        delete res;
        
        futIndex++;
    }
    
    if (!state) 
    {
        currentRxMode_ = currentTxMode_;
        rxMode_ = txMode_;
        lastSyncRxMode_ = nullptr;
    }
    
    return state;
}

void FreeDVInterface::transmit(short mod_out[], short speech_in[])
{
    freedv_tx(currentTxMode_, mod_out, speech_in);
}

void FreeDVInterface::complexTransmit(short mod_out[], short speech_in[], float txOffset, int nfreedv)
{
    COMP tx_fdm[nfreedv];
    COMP tx_fdm_offset[nfreedv];

    freedv_comptx(currentTxMode_, tx_fdm, speech_in);

    freq_shift_coh(tx_fdm_offset, tx_fdm, txOffset, getTxModemSampleRate(), &txFreqOffsetPhaseRectObj_, nfreedv);
    for(int i = 0; i<nfreedv; i++)
        mod_out[i] = tx_fdm_offset[i].real;
}