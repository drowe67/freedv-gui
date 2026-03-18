//=========================================================================
// Name:            EitherOrStep.cpp
// Purpose:         Describes an either/or step in the audio pipeline.
//
// Authors:         Mooneer Salem
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================

#include <assert.h>
#include "EitherOrStep.h"

EitherOrStep::EitherOrStep(realtime_fp<bool()> const& conditionalFn, IPipelineStep* trueStep, IPipelineStep* falseStep)
    : conditionalFn_(conditionalFn)
    , falseStep_(std::unique_ptr<IPipelineStep>(falseStep))
    , trueStep_(std::unique_ptr<IPipelineStep>(trueStep))
{
    // empty
}

EitherOrStep::~EitherOrStep()
{
    // empty, shared_ptr will automatically clean up members
}

int EitherOrStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    assert(falseStep_->getInputSampleRate() == trueStep_->getInputSampleRate());
    return trueStep_->getInputSampleRate();
}

int EitherOrStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    assert(falseStep_->getOutputSampleRate() == trueStep_->getOutputSampleRate());
    return trueStep_->getOutputSampleRate();
}

short* EitherOrStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
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

void EitherOrStep::reset() FREEDV_NONBLOCKING
{
    trueStep_->reset();
    falseStep_->reset();
}
