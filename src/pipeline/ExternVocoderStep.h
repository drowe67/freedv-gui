//=========================================================================
// Name:            ExternVocoderStep.h
// Purpose:         Describes a demodulation step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__EXTERN_VOCODER_STEP_H
#define AUDIO_PIPELINE__EXTERN_VOCODER_STEP_H

#include <unistd.h>
#include <string>
#include "IPipelineStep.h"

class ExternVocoderStep : public IPipelineStep
{
public:
    ExternVocoderStep(std::string scriptPath, int workingSampleRate);
    virtual ~ExternVocoderStep();
    
    virtual int getInputSampleRate() const;
    virtual int getOutputSampleRate() const;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples);
    
private:
    int sampleRate_;
    pid_t recvProcessId_;
    int receiveStdoutFd_;
    int receiveStdinFd_;
};

#endif // AUDIO_PIPELINE__EXTERN_VOCODER_STEP_H
