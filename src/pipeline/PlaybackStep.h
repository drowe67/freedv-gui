//=========================================================================
// Name:            PlaybackStep.h
// Purpose:         Describes a playback step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__PLAYBACK_STEP_H
#define AUDIO_PIPELINE__PLAYBACK_STEP_H

#include "IPipelineStep.h"
#include <functional>
#include <sndfile.h>

class PlaybackStep : public IPipelineStep
{
public:
    PlaybackStep(
        int inputSampleRate, std::function<int()> fileSampleRateFn, 
        std::function<SNDFILE*()> getSndFileFn, std::function<void()> fileCompleteFn);
    virtual ~PlaybackStep();
    
    virtual int getInputSampleRate() const;
    virtual int getOutputSampleRate() const;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples);
    
private:
    int inputSampleRate_;
    std::function<int()> fileSampleRateFn_;
    std::function<SNDFILE*()> getSndFileFn_;
    std::function<void()> fileCompleteFn_;
    std::shared_ptr<short> outputSamples_;
};

#endif // AUDIO_PIPELINE__PLAYBACK_STEP_H