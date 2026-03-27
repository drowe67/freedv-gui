//=========================================================================
// Name:            TapStep.cpp
// Purpose:         Describes a tap step in the audio pipeline.
//
// Authors:         Mooneer Salem
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
    if (tapThreadInput_.numUsed() > (100 * sampleRate_ / 1000))
    {
        sem_.signal();
    }
 
    *numOutputSamples = numInputSamples;
    return inputSamples;
}
