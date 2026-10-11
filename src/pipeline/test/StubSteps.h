#ifndef PIPELINE_TEST_STUB_STEPS_H
#define PIPELINE_TEST_STUB_STEPS_H

// Simple pipeline steps for exercising steps that wrap other steps.

#include <algorithm>
#include <memory>

#include "IPipelineStep.h"

// Statistics are kept outside of the step as the step under test normally
// takes ownership of (and thus eventually deletes) the stub.
struct StubStepStats
{
    int numExecutes = 0;
    int numSamplesSeen = 0;
    int numResets = 0;
    bool destroyed = false;
};

// Outputs each input sample plus an offset. If maxOutputSamples is set, at most
// that many samples are output per call.
class OffsetStep : public IPipelineStep
{
public:
    OffsetStep(int sampleRate, short offset, StubStepStats* stats = nullptr, int maxOutputSamples = -1)
        : sampleRate_(sampleRate)
        , offset_(offset)
        , stats_(stats)
        , maxOutputSamples_(maxOutputSamples)
        , outputSamples_(std::make_unique<short[]>(sampleRate))
    {
        // empty
    }

    virtual ~OffsetStep()
    {
        if (stats_ != nullptr) stats_->destroyed = true;
    }

    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override { return sampleRate_; }
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override { return sampleRate_; }

    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override
    {
        if (stats_ != nullptr)
        {
            stats_->numExecutes++;
            stats_->numSamplesSeen += numInputSamples;
        }

        *numOutputSamples = numInputSamples;
        if (maxOutputSamples_ >= 0)
        {
            *numOutputSamples = std::min(numInputSamples, maxOutputSamples_);
        }

        for (int index = 0; index < *numOutputSamples; index++)
        {
            outputSamples_[index] = inputSamples[index] + offset_;
        }
        return outputSamples_.get();
    }

    virtual void reset() FREEDV_NONBLOCKING override
    {
        if (stats_ != nullptr) stats_->numResets++;
    }

private:
    int sampleRate_;
    short offset_;
    StubStepStats* stats_;
    int maxOutputSamples_;
    std::unique_ptr<short[]> outputSamples_;
};

#endif // PIPELINE_TEST_STUB_STEPS_H
