//=========================================================================
// Name:            RecordStep.cpp
// Purpose:         Describes a record step in the audio pipeline.
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

#include "../defines.h"
#include "ResamplePlotStep.h"

// TBD - maybe include code for function here?
extern void resample_for_plot(GenericFIFO<short> *plotFifo, short buf[], short* dec_samples, int length, int fs) FREEDV_NONBLOCKING;

ResampleForPlotStep::ResampleForPlotStep(GenericFIFO<short>* fifo)
    : fifo_(fifo)
{
    decSamples_ = new short[FS];
    assert(decSamples_ != nullptr);
}

ResampleForPlotStep::~ResampleForPlotStep()
{
    delete[] decSamples_;
}

int ResampleForPlotStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return FS;
}

int ResampleForPlotStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return FS;
}

short* ResampleForPlotStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    resample_for_plot(fifo_, inputSamples, decSamples_, numInputSamples, FS);
    
    *numOutputSamples = 0;    
    return nullptr;
}
