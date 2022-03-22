//=========================================================================
// Name:            SpeexStep.cpp
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


#include "SpeexStep.h"

#include <assert.h>

// TBD -- use std::mutex instead of wxMutex to remove wxWidgets dependency.
#include <wx/wx.h>
extern wxMutex g_mutexProtectingCallbackData;

SpeexStep::SpeexStep(int sampleRate, SpeexPreprocessState** speexStateObj)
    : sampleRate_(sampleRate)
    , speexStateObj_(speexStateObj)
{
    assert(speexStateObj_ != nullptr);
}

SpeexStep::~SpeexStep()
{
    // empty
}

int SpeexStep::getInputSampleRate() const
{
    return sampleRate_;
}

int SpeexStep::getOutputSampleRate() const
{
    return sampleRate_;
}

std::shared_ptr<short> SpeexStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    short* outputSamples = nullptr;
    if (numInputSamples > 0)
    {
        outputSamples = new short[numInputSamples];
        assert(outputSamples != nullptr);
    
        memcpy(outputSamples, inputSamples.get(), sizeof(short)*numInputSamples);
    
        g_mutexProtectingCallbackData.Lock();
        if (*speexStateObj_)
        {
            speex_preprocess_run(*speexStateObj_, outputSamples);
        }
        g_mutexProtectingCallbackData.Unlock();
    }

    *numOutputSamples = numInputSamples;
    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
