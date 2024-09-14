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
    EqualizerStep(int sampleRate, bool* enableFilter, void** bassFilter, void** midFilter, void** trebleFilter, void** volFilter);
    virtual ~EqualizerStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples) override;
    
private:
    int sampleRate_;
    bool* enableFilter_;
    void** bassFilter_;
    void** midFilter_;
    void** trebleFilter_;
    void** volFilter_;
};


#endif // AUDIO_PIPELINE__EQUALIZER_STEP_H