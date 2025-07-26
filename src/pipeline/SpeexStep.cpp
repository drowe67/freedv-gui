//=========================================================================
// Name:            SpeexStep.cpp
// Purpose:         Describes a noise reduction step in the audio pipeline.
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


#include "SpeexStep.h"
#include "../defines.h"

#include <assert.h>

SpeexStep::SpeexStep(int sampleRate)
    : sampleRate_(sampleRate)
    , numSamplesPerSpeexRun_((FRAME_DURATION_MS * sampleRate_) / MS_TO_SEC)
    , inputSampleFifo_(numSamplesPerSpeexRun_)
{
    assert(numSamplesPerSpeexRun_ > 0);
    
    speexStateObj_ = speex_preprocess_state_init(
                numSamplesPerSpeexRun_,
                sampleRate_);
    assert(speexStateObj_ != nullptr);
    
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::make_unique<short[]>(maxSamples);
    assert(outputSamples_ != nullptr);
}

SpeexStep::~SpeexStep()
{
    outputSamples_ = nullptr;
    speex_preprocess_state_destroy(speexStateObj_);
}

int SpeexStep::getInputSampleRate() const
{
    return sampleRate_;
}

int SpeexStep::getOutputSampleRate() const
{
    return sampleRate_;
}

short* SpeexStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples)
{
    *numOutputSamples = 0;

    short* outputSamples = outputSamples_.get();

    int numSpeexRuns = (inputSampleFifo_.numUsed() + numInputSamples) / numSamplesPerSpeexRun_;
    if (numSpeexRuns > 0)
    {
        *numOutputSamples = numSpeexRuns * numSamplesPerSpeexRun_;
        
        short* tmpOutput = outputSamples;
        
        inputSampleFifo_.write(inputSamples, numInputSamples);
        while (inputSampleFifo_.numUsed() >= numSamplesPerSpeexRun_)
        {
            inputSampleFifo_.read(tmpOutput, numSamplesPerSpeexRun_);
            speex_preprocess_run(speexStateObj_, tmpOutput);
            tmpOutput += numSamplesPerSpeexRun_;
        }
    }
    else if (numInputSamples > 0 && inputSamples != nullptr)
    {
        inputSampleFifo_.write(inputSamples, numInputSamples);
    }
    
    return outputSamples;
}

void SpeexStep::reset()
{
    inputSampleFifo_.reset();
}
