//=========================================================================
// Name:            TapStep.cpp
// Purpose:         Describes a tap step in the audio pipeline.
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

#include "TapStep.h"

#include <assert.h>
#include <future>
#include <chrono>

#if defined(__APPLE__)
#include <pthread.h>
#endif // defined(__APPLE__)

#include "../os/os_interface.h"

using namespace std::chrono_literals;

TapStep::TapStep(int sampleRate, IPipelineStep* tapStep)
    : tapStep_(tapStep)
    , sampleRate_(sampleRate)
    , endingTapThread_(false)
    , tapThreadInput_(sampleRate)
{
    tapThread_ = std::thread([&]() {
        const int SAMPLE_RATE_AT_10MS = sampleRate_ / 100;
        short* fifoInput = new short[SAMPLE_RATE_AT_10MS];
        assert(fifoInput != nullptr);

        SetThreadName("TapStep");

#if defined(__APPLE__)
        // Downgrade thread QoS to Utility to avoid thread contention issues.        
        pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif // defined(__APPLE__)

        while (!endingTapThread_)
        {
            while (tapThreadInput_.numUsed() >= SAMPLE_RATE_AT_10MS)
            {
                int temp = 0;
                tapThreadInput_.read(fifoInput, SAMPLE_RATE_AT_10MS);
                tapStep_->execute(fifoInput, SAMPLE_RATE_AT_10MS, &temp);
            }
            sem_.wait();
        }

        delete[] fifoInput;
    });
}

TapStep::~TapStep()
{
    endingTapThread_ = true;
    sem_.signal();
    tapThread_.join();
}

int TapStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

int TapStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

short* TapStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    assert(tapStep_->getInputSampleRate() == sampleRate_);
    
    tapThreadInput_.write(inputSamples, numInputSamples);
    //if (tapThreadInput_.numUsed() > (100 * sampleRate_ / 1000))
    {
        sem_.signal();
    }
 
    *numOutputSamples = numInputSamples;
    return inputSamples;
}
