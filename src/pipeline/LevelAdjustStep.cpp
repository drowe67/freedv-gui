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

#include <assert.h>

LevelAdjustStep::LevelAdjustStep(int sampleRate, std::function<double()> scaleFactorFn)
    : scaleFactorFn_(scaleFactorFn)
    , sampleRate_(sampleRate)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::shared_ptr<short>(
        new short[maxSamples], 
        std::default_delete<short[]>());
    assert(outputSamples_ != nullptr);
}

LevelAdjustStep::~LevelAdjustStep()
{
    // empty
}

int LevelAdjustStep::getInputSampleRate() const
{
    return sampleRate_;
}

int LevelAdjustStep::getOutputSampleRate() const
{
    return sampleRate_;
}

std::shared_ptr<short> LevelAdjustStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    double scaleFactor = scaleFactorFn_();

    for (int index = 0; index < numInputSamples; index++)
    {
        // outputSamples_.get()[index] = inputSamples.get()[index] * scaleFactor;
        double y = inputSamples.get()[index] * scaleFactor;
        if (y > 32767.0) y = 32767.0;
        else if (y < -32768.0) y = -32768.0;
        outputSamples_.get()[index] = static_cast<short>(y);
    }
    
    *numOutputSamples = numInputSamples;
    return outputSamples_;
}
