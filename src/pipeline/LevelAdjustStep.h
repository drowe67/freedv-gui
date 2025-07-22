//=========================================================================
// Name:            LevelAdjustStep.h
// Purpose:         Describes a level adjust step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__LEVEL_ADJUST_STEP_H
#define AUDIO_PIPELINE__LEVEL_ADJUST_STEP_H

#include <functional>
#include <memory>

#include "IPipelineStep.h"

class LevelAdjustStep : public IPipelineStep
{
public:
    LevelAdjustStep(int sampleRate, std::function<double()> scaleFactorFn);
    virtual ~LevelAdjustStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) override;
    
private:
    std::function<double()> scaleFactorFn_;
    int sampleRate_;
    std::unique_ptr<short[]> outputSamples_;
};

#endif // AUDIO_PIPELINE__LEVEL_ADJUST_STEP_H
