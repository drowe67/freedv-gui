//=========================================================================
// Name:            SpeexStep.h
// Purpose:         Describes a noise reduction step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__SPEEX_STEP_H
#define AUDIO_PIPELINE__SPEEX_STEP_H

#include "IPipelineStep.h"

#include <memory>
#include <speex/speex_preprocess.h>

// Forward definition of struct FIFO from Codec2.
extern "C"
{
    struct FIFO;
}

class SpeexStep : public IPipelineStep
{
public:
    SpeexStep(int sampleRate);
    virtual ~SpeexStep();
    
    virtual int getInputSampleRate() const;
    virtual int getOutputSampleRate() const;
    
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples);
    
private:
    int sampleRate_;
    SpeexPreprocessState* speexStateObj_;
    int numSamplesPerSpeexRun_;
    struct FIFO* inputSampleFifo_;

    std::shared_ptr<short> outputSamples_;
};


#endif // AUDIO_PIPELINE__SPEEX_STEP_H