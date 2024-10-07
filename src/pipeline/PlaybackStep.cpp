//=========================================================================
// Name:            PlaybackStep.cpp
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

#include "PlaybackStep.h"
#include <cassert>

#include <cassert>

PlaybackStep::PlaybackStep(
    int inputSampleRate, std::function<int()> fileSampleRateFn, 
    std::function<SNDFILE*()> getSndFileFn, std::function<void()> fileCompleteFn)
: inputSampleRate_(inputSampleRate)
, fileSampleRateFn_(fileSampleRateFn)
, getSndFileFn_(getSndFileFn)
, fileCompleteFn_(fileCompleteFn)
{
    // empty
}

PlaybackStep::~PlaybackStep()
{
    // empty
}

int PlaybackStep::getInputSampleRate() const
{
    return inputSampleRate_;
}

int PlaybackStep::getOutputSampleRate() const
{
    return fileSampleRateFn_();
}

std::shared_ptr<short> PlaybackStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    auto playFile = getSndFileFn_();
    assert(playFile != nullptr);

    unsigned int nsf = numInputSamples * getOutputSampleRate()/getInputSampleRate();
    short* outputSamples = nullptr;
    *numOutputSamples = 0;
    if (nsf > 0)
    {
        outputSamples = new short[nsf];
        assert(outputSamples != nullptr);
    
        *numOutputSamples = sf_read_short(playFile, outputSamples, nsf);
        if ((unsigned)*numOutputSamples < nsf)
        {
            fileCompleteFn_();
        }
    }

    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
