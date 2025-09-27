//=========================================================================
// Name:            ToneInterfererStep.h
// Purpose:         Describes a tone interferer step step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__TONE_INTERFERER_STEP_H
#define AUDIO_PIPELINE__TONE_INTERFERER_STEP_H

#include <memory>
#include <functional>

#include "IPipelineStep.h"

class ToneInterfererStep : public IPipelineStep
{
public:
    ToneInterfererStep(
        int sampleRate, std::function<float()> toneFrequencyFn, 
        std::function<float()> toneAmplitudeFn, std::function<float*()> tonePhaseFn);
    virtual ~ToneInterfererStep();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    
private:
    int sampleRate_;
    std::function<float()> toneFrequencyFn_;
    std::function<float()> toneAmplitudeFn_;
    std::function<float*()> tonePhaseFn_;
    std::unique_ptr<short[]> outputSamples_;
};

#endif // AUDIO_PIPELINE__TONE_INTERFERER_STEP_H
