//=========================================================================
// Name:            RecordStep.cpp
// Purpose:         Describes a record step in the audio pipeline.
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

#include "RecordStep.h"

RecordStep::RecordStep(
    int inputSampleRate, std::function<SNDFILE*()> getSndFileFn, 
    std::function<void(int)> isFileCompleteFn)
: inputSampleRate_(inputSampleRate)
, getSndFileFn_(getSndFileFn)
, isFileCompleteFn_(isFileCompleteFn)
{
    // empty
}

RecordStep::~RecordStep()
{
    // empty
}

int RecordStep::getInputSampleRate() const
{
    return inputSampleRate_;
}

int RecordStep::getOutputSampleRate() const
{
    return inputSampleRate_;
}

std::shared_ptr<short> RecordStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    auto recordFile = getSndFileFn_();
    sf_write_short(recordFile, inputSamples.get(), numInputSamples);
    
    isFileCompleteFn_(numInputSamples);
    
    *numOutputSamples = 0;    
    return std::shared_ptr<short>((short*)nullptr, std::default_delete<short[]>());
}