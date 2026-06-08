#ifndef __BEEP_STEP_H__
#define __BEEP_STEP_H__

#include <initializer_list>
#include <memory>

#include "IPipelineStep.h"
#include "../util/realtime_fp.h"

struct BeepSegment { int durationMs; bool isTone; };

// Always-present pipeline step that plays a pattern when activated by isActiveFn_.
// Acts as a pure passthrough when idle — no EitherOrStep needed.
class BeepStep : public IPipelineStep {
public:
    BeepStep(int sampleRate, int frequency,
             std::initializer_list<BeepSegment> pattern,
             realtime_fp<bool()> const& isActiveFn,
             realtime_fp<void(BeepStep&)> const& onCompleteFn);
    virtual ~BeepStep();

    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    virtual void reset() FREEDV_NONBLOCKING override;

private:
    int sampleRate_;
    realtime_fp<bool()> isActiveFn_;
    realtime_fp<void(BeepStep&)> onCompleteFn_;
    int samplesToGenerate_;
    int sampleCtr_;
    bool playing_;
    std::unique_ptr<short[]> pregenSamples_;
    std::unique_ptr<short[]> execOutput_;
};

#endif // __BEEP_STEP_H__
