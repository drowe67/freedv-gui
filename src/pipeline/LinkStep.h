//=========================================================================
// Name:            LinkStep.h
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

#ifndef AUDIO_PIPELINE__LINK_STEP_H
#define AUDIO_PIPELINE__LINK_STEP_H

#include <memory>

#include "IPipelineStep.h"
#include "../util/GenericFIFO.h"

class LinkStep
{
public:
    LinkStep(int outputSampleRate, size_t numSamples = 48000);
    virtual ~LinkStep();
    
    // Get the constituent pipeline steps.
    IPipelineStep* getInputPipelineStep()
    {
        return new InputStep(this);
    }
    
    IPipelineStep* getOutputPipelineStep()
    {
        return new OutputStep(this);
    }
    
    int getSampleRate() const { return sampleRate_; }
    GenericFIFO<short>& getFifo() { return fifo_; }
    
    void clearFifo() FREEDV_NONBLOCKING;
    
private:
    class InputStep : public IPipelineStep
    {
    public:
        InputStep(LinkStep* parent)
            : parent_(parent)
        {
            // empty
        }
        
        virtual ~InputStep() = default;
        
        // Returns required input sample rate.
        virtual int getInputSampleRate() const FREEDV_NONBLOCKING override { return parent_->getSampleRate(); }
    
        // Returns output sample rate after performing the pipeline step.
        virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override { return parent_->getSampleRate(); }
    
        // Executes pipeline step.
        // Required parameters:
        //     inputSamples: Array of int16 values corresponding to input audio.
        //     numInputSamples: Number of samples in the input array.
        //     numOutputSamples: Location to store number of output samples.
        // Returns: Array of int16 values corresponding to result audio.
        virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
        
    private:
        LinkStep* parent_;
    };
    
    class OutputStep : public IPipelineStep
    {
    public:
        OutputStep(LinkStep* parent)
            : parent_(parent)
        {
            // Pre-allocate buffers so we don't have to do so during real-time operation.
            outputSamples_ = std::make_unique<short[]>(parent_->getSampleRate());
            assert(outputSamples_ != nullptr);
        }
        
        virtual ~OutputStep() = default;
        
        // Returns required input sample rate.
        virtual int getInputSampleRate() const FREEDV_NONBLOCKING override { return parent_->getSampleRate(); }
    
        // Returns output sample rate after performing the pipeline step.
        virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override { return parent_->getSampleRate(); }
    
        // Executes pipeline step.
        // Required parameters:
        //     inputSamples: Array of int16 values corresponding to input audio.
        //     numInputSamples: Number of samples in the input array.
        //     numOutputSamples: Location to store number of output samples.
        // Returns: Array of int16 values corresponding to result audio.
        virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
        
    private:
        LinkStep* parent_;
        std::unique_ptr<short[]> outputSamples_;
    };
    
    short* tmpBuffer_;
    int sampleRate_;
    GenericFIFO<short> fifo_;
};

#endif // AUDIO_PIPELINE__LINK_STEP_H
