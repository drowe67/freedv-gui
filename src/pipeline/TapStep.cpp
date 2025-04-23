//=========================================================================
// Name:            TapStep.cpp
// Purpose:         Describes a tap step in the audio pipeline.
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

#include "TapStep.h"

#include <assert.h>

TapStep::TapStep(int sampleRate, IPipelineStep* tapStep, bool operateBackground)
    : tapStep_(tapStep)
    , sampleRate_(sampleRate)
    , operateBackground_(operateBackground)
{
    // empty
}

TapStep::~TapStep()
{
    // empty
}

int TapStep::getInputSampleRate() const
{
    return sampleRate_;
}

int TapStep::getOutputSampleRate() const
{
    return sampleRate_;
}

std::shared_ptr<short> TapStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    assert(tapStep_->getInputSampleRate() == sampleRate_);
    
    if (operateBackground_)
    {
        // 5 millisecond timeout to queue to background thread.
        // Since this is likely for the UI, it's fine if the step
        // doesn't execute.
        enqueue_([&, inputSamples, numInputSamples]() {
            int temp = 0;
            tapStep_->execute(inputSamples, numInputSamples, &temp);
        }, 5);
    }
    else
    {
        int temp = 0;
        tapStep_->execute(inputSamples, numInputSamples, &temp);
    }
    
    *numOutputSamples = numInputSamples;
    return inputSamples;
}
