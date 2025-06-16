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

#include "../defines.h"
#include "ResamplePlotStep.h"

// TBD - maybe include code for function here?
extern void resample_for_plot(struct FIFO *plotFifo, short buf[], short* dec_samples, int length, int fs);

ResampleForPlotStep::ResampleForPlotStep(struct FIFO* fifo)
    : fifo_(fifo)
{
    decSamples_ = AllocRealtime_<short>(FS);
    assert(decSamples_ != nullptr);
}

ResampleForPlotStep::~ResampleForPlotStep()
{
    FreeRealtime_(decSamples_);
}

int ResampleForPlotStep::getInputSampleRate() const
{
    return FS;
}

int ResampleForPlotStep::getOutputSampleRate() const
{
    return FS;
}

std::shared_ptr<short> ResampleForPlotStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    resample_for_plot(fifo_, inputSamples.get(), decSamples_, numInputSamples, FS);
    
    *numOutputSamples = 0;    
    return std::shared_ptr<short>(nullptr);
}
