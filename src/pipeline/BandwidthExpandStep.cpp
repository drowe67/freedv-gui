//=========================================================================
// Name:            BandwidthExpandStep.cpp
// Purpose:         Implements BBWENet from the Opus project (which expands 
//                  audio from 16kHz to 48kHz).
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
