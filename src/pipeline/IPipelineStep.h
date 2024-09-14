//=========================================================================
// Name:            IPipelineStep.h
// Purpose:         Describes a step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__I_PIPELINE_STEP_H
#define AUDIO_PIPELINE__I_PIPELINE_STEP_H

#include <memory>

class IPipelineStep
{
public:
    virtual ~IPipelineStep();
    
    // Returns required input sample rate.
    virtual int getInputSampleRate() const = 0;
    
    // Returns output sample rate after performing the pipeline step.
    virtual int getOutputSampleRate() const = 0;
    
    // Executes pipeline step.
    // Required parameters:
    //     inputSamples: Array of int16 values corresponding to input audio.
    //     numInputSamples: Number of samples in the input array.
    //     numOutputSamples: Location to store number of output samples.
    // Returns: Array of int16 values corresponding to result audio.
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples) = 0;

    virtual void dump(int indentLevel = 0);
};


#endif // AUDIO_PIPELINE__I_PIPELINE_STEP_H
