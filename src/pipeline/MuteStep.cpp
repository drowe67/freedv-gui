//=========================================================================
// Name:            MuteStep.cpp
// Purpose:         Zeros out incoming audio.
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

#include <algorithm>
#include <cassert>
#include <cstring>
#include "MuteStep.h"

MuteStep::MuteStep(int outputSampleRate)
    : sampleRate_(outputSampleRate)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::shared_ptr<short>(
        AllocRealtime_<short>(maxSamples), 
        RealtimeDeleter<short>());
    assert(outputSamples_ != nullptr);

    memset(outputSamples_.get(), 0, sizeof(short) * maxSamples);
}
    
// Executes pipeline step.
// Required parameters:
//     inputSamples: Array of int16 values corresponding to input audio.
//     numInputSamples: Number of samples in the input array.
//     numOutputSamples: Location to store number of output samples.
// Returns: Array of int16 values corresponding to result audio.
std::shared_ptr<short> MuteStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    *numOutputSamples = numInputSamples;
    
    if (*numOutputSamples > 0)
    {
        return outputSamples_;
    }
    else
    {
        return nullptr;
    }
}
