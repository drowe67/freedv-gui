//=========================================================================
// Name:            ResampeStep.cpp
// Purpose:         Describes a resampling step in the audio pipeline.
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

#include "ResampleStep.h"

#include <algorithm>
#include <assert.h>
#include <cstdio>

int r8b::InterpFilterFracs = -1;

ResampleStep::ResampleStep(int inputSampleRate, int outputSampleRate, bool)
    : inputSampleRate_(inputSampleRate)
    , outputSampleRate_(outputSampleRate)
{
    int maxInputLen = inputSampleRate * 10 / 1000;

    // r8brain is fast enough that we don't need special transition bands
    // for plots.
    double reqTransBand = 9.0;

    resampleState_ = new r8b::CDSPResampler24(
        inputSampleRate, outputSampleRate, maxInputLen, reqTransBand);
    assert(resampleState_ != nullptr);

    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(outputSampleRate);
    assert(outputSamples_ != nullptr);

    tempInput_ = new double[maxInputLen];
    assert(tempInput_ != nullptr);

    prewarm_();
}

ResampleStep::~ResampleStep()
{
    delete resampleState_;
    delete[] tempInput_;
}

int ResampleStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return inputSampleRate_;
}

int ResampleStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return outputSampleRate_;
}

short* ResampleStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    if (numInputSamples == 0)
    {
        // Not generating any samples if we haven't gotten any.
        *numOutputSamples = 0;
        return inputSamples;
    }

    if (inputSampleRate_ == outputSampleRate_)
    {
        // shortcut - just return what we got.
        *numOutputSamples = numInputSamples;
        return inputSamples;
    }
    
    *numOutputSamples = 0;

    auto inputPtr = inputSamples;
    auto outputPtr = outputSamples_.get();
    while (numInputSamples > 0)
    {
        int inputSize = std::min(numInputSamples, inputSampleRate_ * 10 / 1000);

        ConvertToFloatSampleType_<double, short>(inputPtr, tempInput_, inputSize);
        double* tempOutputPtr = nullptr;
        auto numSamples = resampleState_->process(tempInput_, inputSize, tempOutputPtr);
        ConvertToIntSampleType_<short, double>(tempOutputPtr, outputPtr, numSamples);

        outputPtr += numSamples;
        inputPtr += inputSize;
        numInputSamples -= inputSize;
        *numOutputSamples += numSamples;
    }
    
    return outputSamples_.get();
}

void ResampleStep::reset() FREEDV_NONBLOCKING
{
    resampleState_->clear();
    prewarm_();
}

void ResampleStep::prewarm_()
{
    if (inputSampleRate_ == outputSampleRate_) return;

    int maxInputLen = inputSampleRate_ * 10 / 1000;
    std::fill_n(tempInput_, maxInputLen, 0.0);

    // Feed zero-valued samples until the resampler produces its first output.
    // r8brain with DoConsumeLatency=true silently discards the first Latency
    // input samples before emitting any output. Pre-warming here consumes that
    // startup latency so that real audio produces output from the first sample,
    // matching libsamplerate's behaviour and keeping feature files aligned.
    double* tempOutputPtr = nullptr;
    int lenRequired = resampleState_->getInLenBeforeOutPos(0);
    while (lenRequired > 0)
    {
        int len = std::min(lenRequired, maxInputLen);
        resampleState_->process(tempInput_, len, tempOutputPtr);
        lenRequired -= len;
    }
}
