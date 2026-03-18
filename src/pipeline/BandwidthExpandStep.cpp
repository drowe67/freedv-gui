//=========================================================================
// Name:            BandwidthExpandStep.cpp
// Purpose:         Implements BBWENet from the Opus project (which expands 
//                  audio from 16kHz to 48kHz).
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

#include "BandwidthExpandStep.h"

#define INPUT_SAMPLE_RATE 16000
#define OUTPUT_SAMPLE_RATE 48000
#define BWE_FRAME_SIZE 320

BandwidthExpandStep::BandwidthExpandStep()
    : osceBWE_(nullptr)
    , osce_(nullptr)
    , inputSampleFifo_(INPUT_SAMPLE_RATE / 2)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(OUTPUT_SAMPLE_RATE);
    assert(outputSamples_ != nullptr);

    tmpInput_ = std::make_unique<short[]>(BWE_FRAME_SIZE);
    assert(tmpInput_ != nullptr);
    
    osceBWE_ = new silk_OSCE_BWE_struct();
    assert(osceBWE_ != nullptr);
    memset(osceBWE_, 0, sizeof(silk_OSCE_BWE_struct));
    
    osce_ = new OSCEModel();
    assert(osce_ != nullptr);
    memset(osce_, 0, sizeof(OSCEModel));
    
    arch_ = opus_select_arch();
    
    // Make sure OSCE data is initialized.
    resetImpl_();
}

BandwidthExpandStep::~BandwidthExpandStep()
{
    delete osceBWE_;
    delete osce_;
}

int BandwidthExpandStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return INPUT_SAMPLE_RATE;
}

int BandwidthExpandStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return OUTPUT_SAMPLE_RATE;
}

short* BandwidthExpandStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    *numOutputSamples = 0;
    short* outputSamples = outputSamples_.get();

    int numRuns = (inputSampleFifo_.numUsed() + numInputSamples) / BWE_FRAME_SIZE;
    if (numRuns > 0)
    {
        *numOutputSamples = numRuns * BWE_FRAME_SIZE * (OUTPUT_SAMPLE_RATE / INPUT_SAMPLE_RATE);
        
        short* tmpOutput = outputSamples;
        short* tmpInput = tmpInput_.get();

        inputSampleFifo_.write(inputSamples, numInputSamples);
        while (inputSampleFifo_.numUsed() >= BWE_FRAME_SIZE)
        {
            inputSampleFifo_.read(tmpInput, BWE_FRAME_SIZE);

            // Run BBWENet. Expects BWE_FRAME_SIZE * 3 buffer for output.
            osce_bwe(osce_, osceBWE_, tmpOutput, tmpInput, BWE_FRAME_SIZE, arch_);
            
            tmpOutput += BWE_FRAME_SIZE * (OUTPUT_SAMPLE_RATE / INPUT_SAMPLE_RATE);
        }
    }
    else if (numInputSamples > 0 && inputSamples != nullptr)
    {
        inputSampleFifo_.write(inputSamples, numInputSamples);
    }
    
    return outputSamples;
}

void BandwidthExpandStep::reset() FREEDV_NONBLOCKING
{
    resetImpl_();
}

void BandwidthExpandStep::resetImpl_() FREEDV_NONBLOCKING
{
    inputSampleFifo_.reset();
    
    osce_load_models(osce_, nullptr, arch_);
    osce_bwe_reset(osceBWE_);
}
