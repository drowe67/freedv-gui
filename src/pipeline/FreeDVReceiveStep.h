//=========================================================================
// Name:            FreeDVReceiveStep.h
// Purpose:         Describes a demodulation step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__FREEDV_RECEIVE_STEP_H
#define AUDIO_PIPELINE__FREEDV_RECEIVE_STEP_H

#include <functional>
#include "IPipelineStep.h"
#include "../freedv_interface.h"
#include "freedv_api.h"

// Forward declarations of structs implemented by Codec2 
extern "C"
{
    struct FIFO;
    struct freedv;
}

class FreeDVReceiveStep : public IPipelineStep
{
public:
    FreeDVReceiveStep(struct freedv* dv);
    virtual ~FreeDVReceiveStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples) override;
    
    void setSigPwrAvg(float newVal) { sigPwrAvg_ = newVal; }
    float getSigPwrAvg() const { return sigPwrAvg_; }
    int getSync() const { return freedv_get_sync(dv_); }
    void setChannelNoiseEnable(bool enabled, int snr) 
    { 
        channelNoiseEnabled_ = enabled; 
        channelNoiseSnr_ = snr;
    }
    void setFreqOffset(float freq) { freqOffsetHz_ = freq; }
    
private:
    struct freedv* dv_;
    struct FIFO* inputSampleFifo_;
    COMP rxFreqOffsetPhaseRectObjs_;
    float sigPwrAvg_;
    bool channelNoiseEnabled_;
    int channelNoiseSnr_;
    float freqOffsetHz_;
};

#endif // AUDIO_PIPELINE__FREEDV_RECEIVE_STEP_H
