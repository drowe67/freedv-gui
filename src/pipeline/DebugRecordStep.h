//=========================================================================
// Name:            DebugRecordStep.h
// Purpose:         Records last X seconds of audio to file for debugging
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

#ifndef AUDIO_PIPELINE__DEBUG_RECORD_STEP_H
#define AUDIO_PIPELINE__DEBUG_RECORD_STEP_H

#include "IPipelineStep.h"
#include "../util/GenericFIFO.h"

#include <memory>
#include <string>

class DebugRecordStep : public IPipelineStep
{
public:
    DebugRecordStep(int sampleRate, int numSecondsToRecord, std::string filename);
    virtual ~DebugRecordStep();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    virtual void reset() FREEDV_NONBLOCKING override;
    
private:
    int sampleRate_;
    GenericFIFO<short> inputSampleFifo_;
    std::string fileName_;
};


#endif // AUDIO_PIPELINE__DEBUG_RECORD_STEP_H
