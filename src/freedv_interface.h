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
#include "codec2.h"

class FreeDVInterface
{
public:
    FreeDVInterface();
    virtual ~FreeDVInterface();
    
    void start(int txMode, int fifoSizeMs);
    void stop();
    void changeTxMode(int txMode);
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
        float rxFreqOffsetHz, COMP* rxFreqOffsetPhaseRect, struct MODEM_STATS* stats, float* sig_pwr_av);
    
    void transmit(short mod_out[], short speech_in[]);
    void complexTransmit(COMP mod_out[], short speech_in[]);
private:
    int txMode_;
    int rxMode_;
    std::deque<int> enabledModes_;
    std::deque<struct freedv*> dvObjects_;
    std::deque<struct FIFO*> errorFifos_;
    std::deque<struct FIFO*> inputFifos_;
    std::deque<SRC_STATE*> rateConvObjs_;
    struct freedv* currentTxMode_;
    struct freedv* currentRxMode_; 
    SRC_STATE* soundOutRateConv_;
    
};

#endif // CODEC2_INTERFACE_H