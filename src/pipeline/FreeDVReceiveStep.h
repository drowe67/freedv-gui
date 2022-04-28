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

class FreeDVReceiveStep : public IPipelineStep
{
public:
    FreeDVReceiveStep(
        FreeDVInterface& iface,
        std::function<int*()> getRxStateFn,
        std::function<struct FIFO*()> getRxFifoFn, // may not be needed, TBD
        std::function<int()> getChannelNoiseFn,
        std::function<int()> getChannelNoiseSnrFn,
        std::function<float()> getFreqOffsetFn,
        std::function<float*()> getSigPwrAvgFn);
    virtual ~FreeDVReceiveStep();
    
    virtual int getInputSampleRate() const;
    virtual int getOutputSampleRate() const;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples);
    
private:
    FreeDVInterface& interface_;
    std::function<int*()> getRxStateFn_;
    std::function<struct FIFO*()> getRxFifoFn_;
    std::function<int()> getChannelNoiseFn_;
    std::function<int()> getChannelNoiseSnrFn_;
    std::function<float()> getFreqOffsetFn_;
    std::function<float*()> getSigPwrAvgFn_;
};

#endif // AUDIO_PIPELINE__FREEDV_RECEIVE_STEP_H
