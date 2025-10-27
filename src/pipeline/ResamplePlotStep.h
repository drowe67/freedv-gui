//=========================================================================
// Name:            ResamplePlotStep.h
// Purpose:         Describes a resample for plot step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__RESAMPLE_PLOT_STEP_H
#define AUDIO_PIPELINE__RESAMPLE_PLOT_STEP_H

#include "IPipelineStep.h"

#include "util/GenericFIFO.h"

class ResampleForPlotStep : public IPipelineStep
{
public:
    // Locked to 8Khz. Wrap around AudioPipeline as needed.
    ResampleForPlotStep(GenericFIFO<short>* fifo);
    virtual ~ResampleForPlotStep();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    
private:
    GenericFIFO<short>* fifo_;
    short* decSamples_;
};

#endif // AUDIO_PIPELINE__RESAMPLE_PLOT_STEP_H
