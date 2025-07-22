//=========================================================================
// Name:            EitherOrStep.h
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

#ifndef AUDIO_PIPELINE__EITHER_OR_STEP_H
#define AUDIO_PIPELINE__EITHER_OR_STEP_H

#include <functional>
#include "IPipelineStep.h"
#include "ResampleStep.h"

class EitherOrStep : public IPipelineStep
{
public:
    EitherOrStep(std::function<bool()> conditionalFn, std::shared_ptr<IPipelineStep> trueStep, std::shared_ptr<IPipelineStep> falseStep);
    virtual ~EitherOrStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) override;
    virtual void reset() override;
    
private:
    std::function<bool()> conditionalFn_;
    std::shared_ptr<IPipelineStep> falseStep_;
    std::shared_ptr<IPipelineStep> trueStep_;
};

#endif // AUDIO_PIPELINE__EITHER_OR_STEP_H
