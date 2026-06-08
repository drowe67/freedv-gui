#include <cmath>
#include "BeepStep.h"

static const int TONE_AMPLITUDE = 20000;
static const double M_2PI = 2.0 * M_PI;

BeepStep::BeepStep(int sampleRate, int frequency,
                   std::initializer_list<BeepSegment> pattern,
                   realtime_fp<bool()> const& isActiveFn,
                   realtime_fp<void(BeepStep&)> const& onCompleteFn)
    : sampleRate_(sampleRate)
    , isActiveFn_(isActiveFn)
    , onCompleteFn_(onCompleteFn)
    , samplesToGenerate_(0)
    , sampleCtr_(0)
    , playing_(false)
{
    // Count total samples
    for (auto& seg : pattern)
        samplesToGenerate_ += (sampleRate * seg.durationMs) / 1000;

    pregenSamples_ = std::make_unique<short[]>(samplesToGenerate_);
    execOutput_    = std::make_unique<short[]>(samplesToGenerate_);

    int ramp = (sampleRate * 5) / 1000;
    double phase = 0.0;
    double phaseStep = M_2PI * frequency / sampleRate;
    int idx = 0;

    for (auto& seg : pattern) {
        int segSamples = (sampleRate * seg.durationMs) / 1000;
        for (int i = 0; i < segSamples; i++) {
            double env = 0.0;
            if (seg.isTone) {
                if (i < ramp)
                    env = 0.5 * (1.0 - std::cos(M_PI * i / ramp));
                else if (i >= segSamples - ramp)
                    env = 0.5 * (1.0 + std::cos(M_PI * (i - (segSamples - ramp)) / ramp));
                else
                    env = 1.0;
                pregenSamples_[idx] = static_cast<short>(TONE_AMPLITUDE * env * std::cos(phase));
            } else {
                pregenSamples_[idx] = 0;
            }
            phase += phaseStep;
            if (phase >= M_2PI) phase -= M_2PI;
            idx++;
        }
    }
}

BeepStep::~BeepStep() = default;

int BeepStep::getInputSampleRate() const FREEDV_NONBLOCKING { return sampleRate_; }
int BeepStep::getOutputSampleRate() const FREEDV_NONBLOCKING { return sampleRate_; }

short* BeepStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    // Check for activation once per buffer (never mid-buffer)
    if (!playing_ && isActiveFn_()) {
        sampleCtr_ = 0;
        playing_ = true;
    }

    *numOutputSamples = numInputSamples;
    bool completed = false;

    for (int index = 0; index < numInputSamples; index++) {
        short outSample;

        if (playing_ && sampleCtr_ < samplesToGenerate_) {
            outSample = pregenSamples_[sampleCtr_++];
        } else if (playing_) {
            // Pattern complete — passthrough + fire callback once
            outSample = inputSamples[index];
            if (!completed) {
                completed = true;
                playing_ = false;
                onCompleteFn_(*this);
            }
        } else {
            // Idle passthrough
            outSample = inputSamples[index];
        }

        execOutput_[index] = outSample;
    }

    return execOutput_.get();
}

void BeepStep::reset() FREEDV_NONBLOCKING
{
    playing_ = false;
    sampleCtr_ = 0;
}
