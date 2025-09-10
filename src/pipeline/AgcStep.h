//=========================================================================
// Name:            AgcStep.h
// Purpose:         Describes an AGC step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__AGC_STEP_H
#define AUDIO_PIPELINE__AGC_STEP_H

#include "IPipelineStep.h"
#include "../util/GenericFIFO.h"

#include <memory>
#include "agc.h" // from 3rdparty/WebRTC_AGC

class AgcStep : public IPipelineStep
{
public:
    AgcStep(int sampleRate);
    virtual ~AgcStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) override;
    virtual void reset() override;
    
private:
    int sampleRate_;
    WebRtcAgcConfig agcConfig_;
    void* agcState_;
    int micLevel_;

    int numSamplesPerRun_;
    GenericFIFO<short> inputSampleFifo_;
    std::unique_ptr<short[]> outputSamples_;
    std::unique_ptr<short[]> tmpInput_;
};


#endif // AUDIO_PIPELINE__AGC_STEP_H
