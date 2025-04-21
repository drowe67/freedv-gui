//=========================================================================
// Name:            ComputeRfSpectrumStep.cpp
// Purpose:         Describes a RF spectrum computation step step in the audio pipeline.
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

#include "ComputeRfSpectrumStep.h"
#include "../defines.h"

ComputeRfSpectrumStep::ComputeRfSpectrumStep(
    std::function<struct MODEM_STATS*()> modemStatsFn,
    std::function<float*()> getAvMagFn)
    : modemStatsFn_(modemStatsFn)
    , getAvMagFn_(getAvMagFn)
{
    // empty
}

ComputeRfSpectrumStep::~ComputeRfSpectrumStep()
{
    // empty
}

int ComputeRfSpectrumStep::getInputSampleRate() const
{
    return FS;
}

int ComputeRfSpectrumStep::getOutputSampleRate() const
{
    return FS;
}

std::shared_ptr<short> ComputeRfSpectrumStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    COMP*  rx_fdm = new COMP[numInputSamples];
    assert(rx_fdm != nullptr);

    float rx_spec[MODEM_STATS_NSPEC];
    
    auto inputSamplesPtr = inputSamples.get();
    for (int i = 0; i < numInputSamples; i++)
    {
        rx_fdm[i].real = inputSamplesPtr[i];
    }
    
    modem_stats_get_rx_spectrum(modemStatsFn_(), rx_spec, rx_fdm, numInputSamples);
    
    // Average rx spectrum data using a simple IIR low pass filter
    auto avMagPtr = getAvMagFn_();
    for(int i = 0; i < MODEM_STATS_NSPEC; i++) 
    {
        avMagPtr[i] = BETA * avMagPtr[i] + (1.0 - BETA) * rx_spec[i];
    }
    
    // Tap only, no output.
    *numOutputSamples = 0;
    delete[] rx_fdm;

    return std::shared_ptr<short>((short*)nullptr, std::default_delete<short[]>());
}
