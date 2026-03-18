//=========================================================================
// Name:            ComputeRfSpectrumStep.cpp
// Purpose:         Describes a RF spectrum computation step step in the audio pipeline.
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

#include "ComputeRfSpectrumStep.h"
#include "../defines.h"

ComputeRfSpectrumStep::ComputeRfSpectrumStep(
    realtime_fp<struct MODEM_STATS*()> const& modemStatsFn,
    realtime_fp<GenericFIFO<float>*()> const& getAvMagFn)
    : modemStatsFn_(modemStatsFn)
    , getAvMagFn_(getAvMagFn)
{
    rxSpectrum_ = new float[MODEM_STATS_NSPEC];
    assert(rxSpectrum_ != nullptr);

    rxFdm_ = new COMP[FS];
    assert(rxFdm_ != nullptr);
}

ComputeRfSpectrumStep::~ComputeRfSpectrumStep()
{
    delete[] rxSpectrum_;
    delete[] rxFdm_;
}

int ComputeRfSpectrumStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return FS;
}

int ComputeRfSpectrumStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return FS;
}

short* ComputeRfSpectrumStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    auto inputSamplesPtr = inputSamples;
    for (int i = 0; i < numInputSamples; i++)
    {
        rxFdm_[i].real = inputSamplesPtr[i];
    }
    
    modem_stats_get_rx_spectrum(modemStatsFn_(), rxSpectrum_, rxFdm_, numInputSamples);
    
    // Average rx spectrum data using a simple IIR low pass filter
    auto avMagPtr = getAvMagFn_();
    avMagPtr->write(rxSpectrum_, MODEM_STATS_NSPEC);
    
    // Tap only, no output.
    *numOutputSamples = 0;

    return nullptr;
}
