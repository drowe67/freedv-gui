//=========================================================================
// Name:            EitherOrStep.cpp
// Purpose:         Describes an either/or step in the audio pipeline.
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

#include <assert.h>
#include "EitherOrStep.h"

EitherOrStep::EitherOrStep(std::function<bool()> conditionalFn, std::shared_ptr<IPipelineStep> trueStep, std::shared_ptr<IPipelineStep> falseStep)
    : conditionalFn_(conditionalFn)
    , falseStep_(falseStep)
    , trueStep_(trueStep)
{
    // empty
}

EitherOrStep::~EitherOrStep()
{
    // empty, shared_ptr will automatically clean up members
}

int EitherOrStep::getInputSampleRate() const
{
    assert(falseStep_->getInputSampleRate() == trueStep_->getInputSampleRate());
    return trueStep_->getInputSampleRate();
}

int EitherOrStep::getOutputSampleRate() const
{
    assert(falseStep_->getOutputSampleRate() == trueStep_->getOutputSampleRate());
    return trueStep_->getOutputSampleRate();
}

std::shared_ptr<short> EitherOrStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    bool condResult = conditionalFn_();
    if (condResult)
    {
        return trueStep_->execute(inputSamples, numInputSamples, numOutputSamples);
    }
    else
    {
        return falseStep_->execute(inputSamples, numInputSamples, numOutputSamples);
    }
}

void EitherOrStep::reset()
{
    trueStep_->reset();
    falseStep_->reset();
}