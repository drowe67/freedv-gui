//=========================================================================
// Name:            ResampeStep.cpp
// Purpose:         Describes a resampling step in the audio pipeline.
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

#include "ResampleStep.h"

#include <algorithm>
#include <assert.h>

// Copied from util.cpp
// TBD -- move logic into this class?
extern int resample(SRC_STATE *src,
            short      output_short[],
            short      input_short[],
            int        output_sample_rate,
            int        input_sample_rate,
            int        length_output_short, // maximum output array length in samples
            int        length_input_short
            );
            
ResampleStep::ResampleStep(int inputSampleRate, int outputSampleRate)
    : inputSampleRate_(inputSampleRate)
    , outputSampleRate_(outputSampleRate)
{
    int src_error;
    resampleState_ = src_new(SRC_SINC_FASTEST, 1, &src_error);
    assert(resampleState_ != nullptr);
}

ResampleStep::~ResampleStep()
{
    src_delete(resampleState_);
}

int ResampleStep::getInputSampleRate() const
{
    return inputSampleRate_;
}

int ResampleStep::getOutputSampleRate() const
{
    return outputSampleRate_;
}

std::shared_ptr<short> ResampleStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    double scaleFactor = ((double)outputSampleRate_)/((double)inputSampleRate_);
    int outputArraySize = std::max(numInputSamples, (int)(scaleFactor*numInputSamples));
    short* outputSamples = new short[outputArraySize];
    
    *numOutputSamples = resample(
        resampleState_, outputSamples, inputSamples.get(), outputSampleRate_, 
        inputSampleRate_, outputArraySize, numInputSamples);
    
    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
