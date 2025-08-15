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
#include <thread>
#include "../util/GenericFIFO.h"
#include "../util/Semaphore.h"

#include "IPipelineStep.h"

// Creates a separate thread for each TapStep instance if uncommented.
//#define TAP_STEP_USE_THREADING

class TapStep : public IPipelineStep
{
public:
    TapStep(int inputSampleRate, IPipelineStep* tapStep);
    virtual ~TapStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) override;
    
private:
    std::unique_ptr<IPipelineStep> tapStep_;
    int sampleRate_;

#if defined(TAP_STEP_USE_THREADING)
    std::thread tapThread_;
    bool endingTapThread_;
    GenericFIFO<short> tapThreadInput_;
    Semaphore sem_;
#endif // defined(TAP_STEP_USE_THREADING)
};

#endif // AUDIO_PIPELINE__TAP_STEP_H
