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

#include <cassert>
#include "FreeDVReceiveStep.h"
#include "freedv_api.h"
#include "codec2_fifo.h"
#include "../defines.h"

FreeDVReceiveStep::FreeDVReceiveStep(
    FreeDVInterface& interface,
    std::function<int*()> getRxStateFn,
    std::function<struct FIFO*()> getRxFifoFn, // may not be needed, TBD
    std::function<int()> getChannelNoiseFn,
    std::function<int()> getChannelNoiseSnrFn,
    std::function<float()> getFreqOffsetFn,
    std::function<float*()> getSigPwrAvgFn)
    : interface_(interface)
    , getRxStateFn_(getRxStateFn)
    , getRxFifoFn_(getRxFifoFn)
    , getChannelNoiseFn_(getChannelNoiseFn)
    , getChannelNoiseSnrFn_(getChannelNoiseSnrFn)
    , getFreqOffsetFn_(getFreqOffsetFn)
    , getSigPwrAvgFn_(getSigPwrAvgFn)
{
    // empty
}

FreeDVReceiveStep::~FreeDVReceiveStep()
{
    // empty
}

int FreeDVReceiveStep::getInputSampleRate() const
{
    return interface_.getRxModemSampleRate();
}

int FreeDVReceiveStep::getOutputSampleRate() const
{
    return interface_.getRxSpeechSampleRate();
}

std::shared_ptr<short> FreeDVReceiveStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    *numOutputSamples = (int)(FRAME_DURATION * getOutputSampleRate());
    short* outputSamples = new short[*numOutputSamples];
    assert(outputSamples != nullptr);
    
    // Write 20ms chunks of input samples for modem rx processing
    auto state = getRxStateFn_();
    auto rxFifo = getRxFifoFn_();
    
    *state = interface_.processRxAudio(
        inputSamples.get(), numInputSamples, rxFifo, getChannelNoiseFn_(), getChannelNoiseSnrFn_(), 
        getChannelNoiseSnrFn_(), interface_.getCurrentRxModemStats(), getSigPwrAvgFn_());

    // Read 20ms chunk of samples from modem rx processing,
    // this will typically be decoded output speech.
    memset(outputSamples, 0, sizeof(short) * *numOutputSamples);
    codec2_fifo_read(rxFifo, outputSamples, *numOutputSamples);
    
    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
