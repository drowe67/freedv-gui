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

// Forward declaration
struct FIFO;

class ResampleForPlotStep : public IPipelineStep
{
public:
    // Locked to 8Khz. Wrap around AudioPipeline as needed.
    ResampleForPlotStep(struct FIFO* fifo);
    virtual ~ResampleForPlotStep();
    
    virtual int getInputSampleRate() const;
    virtual int getOutputSampleRate() const;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples);
    
private:
    struct FIFO* fifo_;
    short* decSamples_;
};

#endif // AUDIO_PIPELINE__RESAMPLE_PLOT_STEP_H
