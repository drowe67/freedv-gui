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

#include <cassert>
#include <cstring>
#include "MuteStep.h"

MuteStep::MuteStep(int outputSampleRate)
    : sampleRate_(outputSampleRate)
{
    // empty
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
        short* outputSamples = new short[*numOutputSamples];
        assert(outputSamples != nullptr);

        memset(outputSamples, 0, sizeof(short) * (*numOutputSamples));

        return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
    }
    else
    {
        return nullptr;
    }
}
