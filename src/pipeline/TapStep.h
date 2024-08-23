//=========================================================================
// Name:            TapStep.h
// Purpose:         Describes a tap step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__TAP_STEP_H
#define AUDIO_PIPELINE__TAP_STEP_H

#include <memory>

#include "IPipelineStep.h"

class TapStep : public IPipelineStep
{
public:
    TapStep(int inputSampleRate, IPipelineStep* tapStep);
    virtual ~TapStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples) override;

    virtual void dump(int indentLevel = 0) override;
    
private:
    std::shared_ptr<IPipelineStep> tapStep_;
    int sampleRate_;
};

#endif // AUDIO_PIPELINE__TAP_STEP_H
