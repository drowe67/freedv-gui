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
#include "codec2_fifo.h"

SpeexStep::SpeexStep(int sampleRate)
    : sampleRate_(sampleRate)
{
    numSamplesPerSpeexRun_ = FRAME_DURATION * sampleRate_;
    assert(numSamplesPerSpeexRun_ > 0);
    
    speexStateObj_ = speex_preprocess_state_init(
                numSamplesPerSpeexRun_,
                sampleRate_);
    assert(speexStateObj_ != nullptr);
    
    // Set FIFO to be 2x the number of samples per run so we don't lose anything.
    inputSampleFifo_ = codec2_fifo_create(numSamplesPerSpeexRun_ * 2);
    assert(inputSampleFifo_ != nullptr);

    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::shared_ptr<short>(
        new short[maxSamples], 
        std::default_delete<short[]>());
    assert(outputSamples_ != nullptr);
}

SpeexStep::~SpeexStep()
{
    speex_preprocess_state_destroy(speexStateObj_);
    codec2_fifo_free(inputSampleFifo_);
}

int SpeexStep::getInputSampleRate() const
{
    return sampleRate_;
}

int SpeexStep::getOutputSampleRate() const
{
    return sampleRate_;
}

std::shared_ptr<short> SpeexStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    *numOutputSamples = 0;
    
    int numSpeexRuns = (codec2_fifo_used(inputSampleFifo_) + numInputSamples) / numSamplesPerSpeexRun_;
    if (numSpeexRuns > 0)
    {
        *numOutputSamples = numSpeexRuns * numSamplesPerSpeexRun_;
        
        short* tmpOutput = outputSamples_.get();
        short* tmpInput = inputSamples.get();
        
        while (numInputSamples > 0 && tmpInput != nullptr)
        {
            codec2_fifo_write(inputSampleFifo_, tmpInput++, 1);
            numInputSamples--;
            
            if (codec2_fifo_used(inputSampleFifo_) >= numSamplesPerSpeexRun_)
            {
                codec2_fifo_read(inputSampleFifo_, tmpOutput, numSamplesPerSpeexRun_);
                speex_preprocess_run(speexStateObj_, tmpOutput);
                tmpOutput += numSamplesPerSpeexRun_;
            }
        }
    }
    else if (numInputSamples > 0 && inputSamples.get() != nullptr)
    {
        codec2_fifo_write(inputSampleFifo_, inputSamples.get(), numInputSamples);
    }
    
    return outputSamples_;
}
