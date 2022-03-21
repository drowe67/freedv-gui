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
    // empty
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
    short* outputSamples = new short[numInputSamples];
    double scaleFactor = scaleFactorFn_();

    for (int index = 0; index < numInputSamples; index++)
    {
        outputSamples[index] = inputSamples.get()[index] * scaleFactor;
    }
    
    *numOutputSamples = numInputSamples;
    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
