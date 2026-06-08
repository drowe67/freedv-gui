#include <cmath>
#include "BeepStep.h"

static const int TONE_AMPLITUDE = 20000;
// Brief pre-charge level for silence segments: high enough to prevent hardware noise-gate
// from closing (~-50 dBFS), applied only for the last 'ramp' samples of each silence so
// the inter-dit gaps are perceived as true silence (no continuous sub-tone).
static const int SILENCE_KEEPALIVE = 100;
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
                // Floor the envelope at SILENCE_KEEPALIVE so the tone starts and ends at
                // the same level as the preceding/following silence pre-charge — no step.
                pregenSamples_[idx] = static_cast<short>(
                    (SILENCE_KEEPALIVE + (TONE_AMPLITUDE - SILENCE_KEEPALIVE) * env) * std::cos(phase));
            } else {
                // True silence, except for the last 'ramp' samples which act as a brief
                // noise-gate pre-charge so the gate is open when the next tone onset arrives.
                pregenSamples_[idx] = (i >= segSamples - ramp)
                    ? static_cast<short>(SILENCE_KEEPALIVE * std::cos(phase))
                    : 0;
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
            // Beep just finished: silence the rest of this frame so that radio
            // audio passing through the bypass pipeline doesn't leak to the speaker.
            outSample = 0;
            if (!completed) {
                completed = true;
                playing_ = false;
                onCompleteFn_(*this);
            }
        } else if (completed) {
            outSample = 0;  // silence remainder of completion frame
        } else {
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
