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

#include <iostream>
#include <assert.h>

TapStep::TapStep(int sampleRate, IPipelineStep* tapStep)
    : tapStep_(tapStep)
    , sampleRate_(sampleRate)
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
    int temp = 0;
    assert(tapStep_->getInputSampleRate() == sampleRate_);
    tapStep_->execute(inputSamples, numInputSamples, &temp);
    
    *numOutputSamples = numInputSamples;
    return inputSamples;
}

void TapStep::dump(int indentLevel)
{
    IPipelineStep::dump(indentLevel);
    indentLevel += 4;
    std::cout << std::string(indentLevel, ' ');
    tapStep_->dump(indentLevel);
}
