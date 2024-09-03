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
{
    fifo_ = codec2_fifo_create(numSamples);
    assert(fifo_ != nullptr);
    
    // Create pipeline steps
    inputPipelineStep_ = std::make_shared<InputStep>(this);
    outputPipelineStep_ = std::make_shared<OutputStep>(this);
}

LinkStep::~LinkStep()
{
    codec2_fifo_destroy(fifo_);
    fifo_ = nullptr;
}

void LinkStep::clearFifo()
{
    int numUsed = codec2_fifo_used(fifo_);
    short tmp[numUsed];
    
    // Read data and then promptly throw it out.
    codec2_fifo_read(fifo_, tmp, numUsed);
}

std::shared_ptr<short> LinkStep::InputStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    auto fifo = parent_->getFifo();
    if (numInputSamples > 0)
    {
        codec2_fifo_write(fifo, inputSamples.get(), numInputSamples);
    }
 
    // Since we short circuited to the output step, don't return any samples here.
    *numOutputSamples = 0;
    return nullptr;
}

std::shared_ptr<short> LinkStep::OutputStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    auto fifo = parent_->getFifo();
    *numOutputSamples = std::min(codec2_fifo_used(fifo), numInputSamples);
    
    if (*numOutputSamples > 0)
    {
        short* outputSamples = new short[*numOutputSamples];
        assert(outputSamples != nullptr);
    
        codec2_fifo_read(fifo, outputSamples, *numOutputSamples);
        return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
    }
    else
    {
        return nullptr;
    }
}
