//=========================================================================
// Name:            LinkStep.cpp
// Purpose:         Allows one pipeline to grab audio from another.
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

#include <cassert>
#include <algorithm>
#include "LinkStep.h"

LinkStep::LinkStep(int outputSampleRate, size_t numSamples)
    : tmpBuffer_(nullptr)
    , sampleRate_(outputSampleRate)
    , fifo_(numSamples)
{
    tmpBuffer_ = new short[numSamples];
    assert(tmpBuffer_ != nullptr);
}

LinkStep::~LinkStep()
{
    delete[] tmpBuffer_;
}

void LinkStep::clearFifo() FREEDV_NONBLOCKING
{    
    fifo_.reset();
}

short* LinkStep::InputStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    auto& fifo = parent_->getFifo();
    auto samplePtr = inputSamples;
    
    if (numInputSamples > 0 && samplePtr != nullptr)
    {
        fifo.write(samplePtr, numInputSamples);
    }

    // Since we short circuited to the output step, don't return any samples here.
    *numOutputSamples = 0;
    return nullptr;
}

short* LinkStep::OutputStep::execute(short*, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    auto& fifo = parent_->getFifo();
    *numOutputSamples = numInputSamples > 0 ? std::min(fifo.numUsed(), numInputSamples) : fifo.numUsed();
    
    if (*numOutputSamples > 0)
    {
        fifo.read(outputSamples_.get(), *numOutputSamples);
        return outputSamples_.get();
    }
    else
    {
        return nullptr;
    }
}
