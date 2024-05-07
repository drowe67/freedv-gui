//=========================================================================
// Name:            ExclusiveAccessStep.h
// Purpose:         Describes a step that wraps a mutex lock/unlock around
//                  another step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__EXCLUSIVE_ACCESS_STEP_H
#define AUDIO_PIPELINE__EXCLUSIVE_ACCESS_STEP_H

#include "IPipelineStep.h"
#include <memory>
#include <functional>

class ExclusiveAccessStep : public IPipelineStep
{
public:
    ExclusiveAccessStep(IPipelineStep* step, std::function<void()> lockFn, std::function<void()> unlockFn);
    virtual ~ExclusiveAccessStep();
    
    virtual int getInputSampleRate() const;
    virtual int getOutputSampleRate() const;
    
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples);

    virtual void dump(int indentLevel = 0) override;
    
private:
    std::shared_ptr<IPipelineStep> step_;
    std::function<void()> lockFn_;
    std::function<void()> unlockFn_;
};


#endif // AUDIO_PIPELINE__EXCLUSIVE_ACCESS_STEP_H
