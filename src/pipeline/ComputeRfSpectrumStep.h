//=========================================================================
// Name:            ComputeRfSpectrumStep.h
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

#ifndef AUDIO_PIPELINE__COMPUTE_RF_SPECTRUM_STEP_H
#define AUDIO_PIPELINE__COMPUTE_RF_SPECTRUM_STEP_H

#include <memory>
#include <functional>

#include "modem_stats.h"
#include "IPipelineStep.h"
#include "../util/realtime_fp.h"

class ComputeRfSpectrumStep : public IPipelineStep
{
public:
    // Note: only supports 8 kHz, so needs to be inserted into an AudioPipeline
    // in order to downconvert properly.
    ComputeRfSpectrumStep(
        realtime_fp<struct MODEM_STATS*()> modemStatsFn,
        realtime_fp<float*()> getAvMagFn);
    virtual ~ComputeRfSpectrumStep();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    
private:
    realtime_fp<struct MODEM_STATS*()> modemStatsFn_;
    realtime_fp<float*()> getAvMagFn_;
    float* rxSpectrum_;
    COMP* rxFdm_;
};

#endif // AUDIO_PIPELINE__COMPUTE_RF_SPECTRUM_STEP_H
