//=========================================================================
// Name:            LevelAdjustStep.cpp
// Purpose:         Describes a level adjust step in the audio pipeline.
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

#include "LevelAdjustStep.h"

#include <functional>
#include <cassert>

LevelAdjustStep::LevelAdjustStep(int sampleRate, realtime_fp<float()> scaleFactorFn)
    : scaleFactorFn_(scaleFactorFn)
    , sampleRate_(sampleRate)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::make_unique<short[]>(maxSamples);
    assert(outputSamples_ != nullptr);
}

LevelAdjustStep::~LevelAdjustStep()
{
    // empty
}

int LevelAdjustStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

int LevelAdjustStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

short* LevelAdjustStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    float scaleFactor = scaleFactorFn_();
    short* outPtr = outputSamples_.get();

    for (int index = 0; index < numInputSamples; index++)
    {
        outPtr[index] = inputSamples[index] * scaleFactor;
    }
    
    *numOutputSamples = numInputSamples;
    return outPtr;
}
