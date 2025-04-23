//=========================================================================
// Name:            LinkStep.h
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

#ifndef AUDIO_PIPELINE__LINK_STEP_H
#define AUDIO_PIPELINE__LINK_STEP_H

#include "IPipelineStep.h"
#include "codec2_fifo.h"

class LinkStep
{
public:
    LinkStep(int outputSampleRate, size_t numSamples = 48000);
    virtual ~LinkStep();
    
    // Get the constituent pipeline steps.
    std::shared_ptr<IPipelineStep> getInputPipelineStep()
    {
        return inputPipelineStep_;
    }
    
    std::shared_ptr<IPipelineStep> getOutputPipelineStep()
    {
        return outputPipelineStep_;
    }
    
    int getSampleRate() const { return sampleRate_; }
    FIFO* getFifo() { return fifo_; }
    
    void clearFifo();
    
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
        virtual int getInputSampleRate() const override { return parent_->getSampleRate(); }
    
        // Returns output sample rate after performing the pipeline step.
        virtual int getOutputSampleRate() const override { return parent_->getSampleRate(); }
    
        // Executes pipeline step.
        // Required parameters:
        //     inputSamples: Array of int16 values corresponding to input audio.
        //     numInputSamples: Number of samples in the input array.
        //     numOutputSamples: Location to store number of output samples.
        // Returns: Array of int16 values corresponding to result audio.
        virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples) override;
        
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
            auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
            outputSamples_ = std::shared_ptr<short>(
                new short[maxSamples], 
                std::default_delete<short[]>());
            assert(outputSamples_ != nullptr);
        }
        
        virtual ~OutputStep() = default;
        
        // Returns required input sample rate.
        virtual int getInputSampleRate() const override { return parent_->getSampleRate(); }
    
        // Returns output sample rate after performing the pipeline step.
        virtual int getOutputSampleRate() const override { return parent_->getSampleRate(); }
    
        // Executes pipeline step.
        // Required parameters:
        //     inputSamples: Array of int16 values corresponding to input audio.
        //     numInputSamples: Number of samples in the input array.
        //     numOutputSamples: Location to store number of output samples.
        // Returns: Array of int16 values corresponding to result audio.
        virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples) override;
        
    private:
        LinkStep* parent_;
        std::shared_ptr<short> outputSamples_;
    };
    
    int sampleRate_;
    std::shared_ptr<IPipelineStep> inputPipelineStep_;
    std::shared_ptr<IPipelineStep> outputPipelineStep_;
    FIFO* fifo_;

    short* tmpBuffer_;
};

#endif // AUDIO_PIPELINE__LINK_STEP_H