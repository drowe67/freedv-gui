//=========================================================================
// Name:            LinkStep.cpp
// Purpose:         Allows one pipeline to grab audio from another.
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

#include <cassert>
#include <algorithm>
#include "LinkStep.h"

LinkStep::LinkStep(int outputSampleRate, size_t numSamples)
    : sampleRate_(outputSampleRate)
    , fifo_(numSamples)
{
    // Create pipeline steps
    inputPipelineStep_ = std::make_shared<InputStep>(this);
    outputPipelineStep_ = std::make_shared<OutputStep>(this);

    tmpBuffer_ = new short[numSamples];
    assert(tmpBuffer_ != nullptr);
}

LinkStep::~LinkStep()
{
    delete[] tmpBuffer_;
}

void LinkStep::clearFifo()
{    
    // Read data and then promptly throw it out.
    fifo_.reset();
}

std::shared_ptr<short> LinkStep::InputStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    auto fifo = parent_->getFifo();
    auto samplePtr = inputSamples.get();
    if (numInputSamples > 0 && samplePtr != nullptr)
    {
        fifo.write(samplePtr, numInputSamples);
    }

    // Since we short circuited to the output step, don't return any samples here.
    *numOutputSamples = 0;
    return nullptr;
}

std::shared_ptr<short> LinkStep::OutputStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    auto fifo = parent_->getFifo();
    *numOutputSamples = std::min(fifo.numUsed(), numInputSamples);
    
    if (*numOutputSamples > 0)
    {
        fifo.read(outputSamples_.get(), *numOutputSamples);
        return outputSamples_;
    }
    else
    {
        return nullptr;
    }
}
