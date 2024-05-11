//=========================================================================
// Name:            ExclusiveAccessStep.cpp
// Purpose:         Describes a step that wraps a mutex lock/unlock around
//                  another step in the audio pipeline.
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

#include <iostream>
#include "ExclusiveAccessStep.h"

ExclusiveAccessStep::ExclusiveAccessStep(IPipelineStep* step, std::function<void()> lockFn, std::function<void()> unlockFn)
    : step_(std::shared_ptr<IPipelineStep>(step))
    , lockFn_(lockFn)
    , unlockFn_(unlockFn)
{
    // empty
}

ExclusiveAccessStep::~ExclusiveAccessStep()
{
    // empty
}
    
int ExclusiveAccessStep::getInputSampleRate() const
{
    return step_->getInputSampleRate();
}

int ExclusiveAccessStep::getOutputSampleRate() const
{
    return step_->getOutputSampleRate();
}
    
std::shared_ptr<short> ExclusiveAccessStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    lockFn_();
    auto result = step_->execute(inputSamples, numInputSamples, numOutputSamples);
    unlockFn_();

    return result;
}

void ExclusiveAccessStep::dump(int indentLevel)
{
    IPipelineStep::dump(indentLevel);
    indentLevel += 4;

    std::cout << std::string(indentLevel, ' ');
    step_->dump(indentLevel);
}
