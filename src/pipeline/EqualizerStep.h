//=========================================================================
// Name:            EqualizerStep.h
// Purpose:         Describes an equalizer step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__EQUALIZER_STEP_H
#define AUDIO_PIPELINE__EQUALIZER_STEP_H

#include "IPipelineStep.h"
#include <memory>

class EqualizerStep : public IPipelineStep
{
public:
    EqualizerStep(int sampleRate, bool* enableFilter, std::shared_ptr<void>* bassFilter, std::shared_ptr<void>* midFilter, std::shared_ptr<void>* trebleFilter, std::shared_ptr<void>* volFilter);
    virtual ~EqualizerStep();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;    
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    
private:
    int sampleRate_;
    bool* enableFilter_;
    std::shared_ptr<void>* bassFilter_;
    std::shared_ptr<void>* midFilter_;
    std::shared_ptr<void>* trebleFilter_;
    std::shared_ptr<void>* volFilter_;
    std::unique_ptr<short[]> outputSamples_;
};


#endif // AUDIO_PIPELINE__EQUALIZER_STEP_H
