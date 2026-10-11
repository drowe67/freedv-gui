#include <atomic>
#include <cstdlib>
#include <vector>

#include "../BeepStep.h"
#include "../../test/UnitTestCommon.h"

namespace {

constexpr int SAMPLE_RATE = 8000;
constexpr int BEEP_FREQ_HZ = 750;
constexpr int BEEP_DURATION_MS = 80;
constexpr int NUM_REPEATS = 2;

constexpr int BEEP_SAMPLES = SAMPLE_RATE * BEEP_DURATION_MS / 1000;
constexpr int SILENCE_SAMPLES = SAMPLE_RATE * 80 / 1000; // 80 ms between repeats
constexpr int CYCLE_SAMPLES = BEEP_SAMPLES + SILENCE_SAMPLES;
constexpr int TOTAL_SAMPLES = NUM_REPEATS * CYCLE_SAMPLES;
constexpr int MAX_AMPLITUDE = 8192;

// realtime_fp only accepts plain function pointers, so state is kept here.
std::atomic<bool> g_active;
int g_completeCount;

BeepStep* createStep(bool resetOnComplete)
{
    g_active = true;
    g_completeCount = 0;

    if (resetOnComplete)
    {
        // Same as how the step is used inside FreeDV.
        return new BeepStep(
            SAMPLE_RATE, BEEP_FREQ_HZ, BEEP_DURATION_MS, NUM_REPEATS,
            +[]() FREEDV_NONBLOCKING { return g_active.load(); },
            +[](BeepStep& step) FREEDV_NONBLOCKING {
                g_completeCount++;
                g_active = false;
                step.reset();
            });
    }

    return new BeepStep(
        SAMPLE_RATE, BEEP_FREQ_HZ, BEEP_DURATION_MS, NUM_REPEATS,
        +[]() FREEDV_NONBLOCKING { return g_active.load(); },
        +[](BeepStep&) FREEDV_NONBLOCKING { g_completeCount++; });
}

std::vector<short> run(BeepStep& step, int numSamples, short* input = nullptr)
{
    int numOutput = 0;
    short* output = step.execute(input, numSamples, &numOutput);
    return std::vector<short>(output, output + numOutput);
}

int peak(std::vector<short> const& samples, int start, int length)
{
    int result = 0;
    for (int i = start; i < start + length; i++)
    {
        result = std::max(result, std::abs((int)samples[i]));
    }
    return result;
}

bool sampleRatesMatchConstructor()
{
    std::unique_ptr<BeepStep> step(createStep(false));
    return step->getInputSampleRate() == SAMPLE_RATE && step->getOutputSampleRate() == SAMPLE_RATE;
}

bool generatesBeepsSeparatedBySilence()
{
    std::unique_ptr<BeepStep> step(createStep(false));
    auto output = run(*step, SAMPLE_RATE);
    if ((int)output.size() != SAMPLE_RATE)
    {
        std::cout << "[got " << output.size() << " samples] ";
        return false;
    }

    bool result = true;
    for (int repeat = 0; repeat < NUM_REPEATS; repeat++)
    {
        int start = repeat * CYCLE_SAMPLES;

        // The middle of each beep should be at (close to) full amplitude but never above.
        int beepPeak = peak(output, start, BEEP_SAMPLES);
        if (beepPeak > MAX_AMPLITUDE || beepPeak < MAX_AMPLITUDE * 0.95)
        {
            std::cout << "[beep " << repeat << " has peak " << beepPeak << "] ";
            result = false;
        }

        int silencePeak = peak(output, start + BEEP_SAMPLES, SILENCE_SAMPLES);
        if (silencePeak != 0)
        {
            std::cout << "[silence after beep " << repeat << " has peak " << silencePeak << "] ";
            result = false;
        }
    }

    // Nothing should be generated once all of the repeats are done.
    int trailingPeak = peak(output, TOTAL_SAMPLES, SAMPLE_RATE - TOTAL_SAMPLES);
    if (trailingPeak != 0)
    {
        std::cout << "[audio after final beep has peak " << trailingPeak << "] ";
        result = false;
    }

    return result;
}

bool beepIsRampedUpAndDown()
{
    // Avoids clicks at the beginning and end of each beep.
    const int RAMP_SAMPLES = SAMPLE_RATE * 5 / 1000;

    std::unique_ptr<BeepStep> step(createStep(false));
    auto output = run(*step, BEEP_SAMPLES);

    int startPeak = peak(output, 0, RAMP_SAMPLES / 8);
    int endPeak = peak(output, BEEP_SAMPLES - RAMP_SAMPLES / 8, RAMP_SAMPLES / 8);
    if (startPeak > MAX_AMPLITUDE / 10 || endPeak > MAX_AMPLITUDE / 10)
    {
        std::cout << "[start peak " << startPeak << ", end peak " << endPeak << "] ";
        return false;
    }
    return true;
}

bool beepHasRequestedFrequency()
{
    std::unique_ptr<BeepStep> step(createStep(false));
    auto output = run(*step, BEEP_SAMPLES);

    int crossings = 0;
    for (int i = 1; i < BEEP_SAMPLES; i++)
    {
        if ((output[i - 1] < 0) != (output[i] < 0)) crossings++;
    }

    // Two zero crossings per cycle.
    int expected = 2 * BEEP_FREQ_HZ * BEEP_DURATION_MS / 1000;
    if (std::abs(crossings - expected) > 2)
    {
        std::cout << "[got " << crossings << " zero crossings, expected " << expected << "] ";
        return false;
    }
    return true;
}

bool inputIsIgnored()
{
    std::unique_ptr<BeepStep> silentStep(createStep(false));
    auto expected = run(*silentStep, TOTAL_SAMPLES);

    std::vector<short> input(TOTAL_SAMPLES, 10000);
    std::unique_ptr<BeepStep> step(createStep(false));
    return run(*step, TOTAL_SAMPLES, input.data()) == expected;
}

bool outputIsContinuousAcrossCalls()
{
    std::unique_ptr<BeepStep> singleCallStep(createStep(false));
    auto expected = run(*singleCallStep, TOTAL_SAMPLES);

    std::unique_ptr<BeepStep> step(createStep(false));
    std::vector<short> actual;
    for (int i = 0; i < TOTAL_SAMPLES; i += 100)
    {
        auto block = run(*step, std::min(100, TOTAL_SAMPLES - i));
        actual.insert(actual.end(), block.begin(), block.end());
    }
    return actual == expected;
}

bool silentWhenInactive()
{
    std::unique_ptr<BeepStep> step(createStep(false));
    g_active = false;

    auto output = run(*step, SAMPLE_RATE);
    return (int)output.size() == SAMPLE_RATE && peak(output, 0, SAMPLE_RATE) == 0 && g_completeCount == 0;
}

bool pausesWhileInactiveAndResumesWhereItLeftOff()
{
    std::unique_ptr<BeepStep> uninterrupted(createStep(false));
    auto expected = run(*uninterrupted, TOTAL_SAMPLES);

    // Stop partway through the first beep, the silence after it and the second beep.
    std::unique_ptr<BeepStep> step(createStep(false));
    std::vector<short> actual;
    int position = 0;
    for (int pauseAt : { BEEP_SAMPLES / 3, BEEP_SAMPLES + SILENCE_SAMPLES / 2, CYCLE_SAMPLES + BEEP_SAMPLES / 2 })
    {
        auto block = run(*step, pauseAt - position);
        actual.insert(actual.end(), block.begin(), block.end());
        position = pauseAt;

        g_active = false;
        auto paused = run(*step, 500);
        if (paused.size() != 500 || peak(paused, 0, 500) != 0)
        {
            std::cout << "[not silent while paused at sample " << pauseAt << "] ";
            return false;
        }
        g_active = true;
    }

    auto remainder = run(*step, TOTAL_SAMPLES - position);
    actual.insert(actual.end(), remainder.begin(), remainder.end());
    if (actual != expected)
    {
        std::cout << "[output differs from an uninterrupted beep] ";
        return false;
    }

    // Time spent paused doesn't count towards completion.
    return g_completeCount == 0;
}

bool veryShortBeepStaysWithinBounds()
{
    // Shorter than the fade in and fade out put together (5 ms each).
    for (int durationMs : { 1, 4, 9, 10, 11 })
    {
        g_active = true;
        g_completeCount = 0;
        BeepStep step(
            SAMPLE_RATE, BEEP_FREQ_HZ, durationMs, 1,
            +[]() FREEDV_NONBLOCKING { return g_active.load(); },
            +[](BeepStep&) FREEDV_NONBLOCKING { g_completeCount++; });

        int beepSamples = SAMPLE_RATE * durationMs / 1000;
        auto output = run(step, beepSamples + SILENCE_SAMPLES + 100);

        if (peak(output, 0, beepSamples) == 0 || peak(output, 0, beepSamples) > MAX_AMPLITUDE)
        {
            std::cout << "[" << durationMs << " ms beep peaked at " << peak(output, 0, beepSamples) << "] ";
            return false;
        }
        if (peak(output, beepSamples, SILENCE_SAMPLES + 100) != 0)
        {
            std::cout << "[" << durationMs << " ms beep ran on for too long] ";
            return false;
        }
        if (g_completeCount == 0)
        {
            std::cout << "[" << durationMs << " ms beep never completed] ";
            return false;
        }
    }
    return true;
}

bool veryShortBeepFadesOut()
{
    // An abrupt end is heard as a click.
    for (int durationMs : { 4, 9 })
    {
        g_active = true;
        BeepStep step(
            SAMPLE_RATE, BEEP_FREQ_HZ, durationMs, 1,
            +[]() FREEDV_NONBLOCKING { return g_active.load(); },
            +[](BeepStep&) FREEDV_NONBLOCKING { });

        int beepSamples = SAMPLE_RATE * durationMs / 1000;
        auto output = run(step, beepSamples);
        if (std::abs((int)output[beepSamples - 1]) > MAX_AMPLITUDE / 10)
        {
            std::cout << "[" << durationMs << " ms beep ends at " << output[beepSamples - 1] << "] ";
            return false;
        }
    }
    return true;
}

bool noRepeatsMeansNoBeep()
{
    g_active = true;
    g_completeCount = 0;
    BeepStep step(
        SAMPLE_RATE, BEEP_FREQ_HZ, BEEP_DURATION_MS, 0,
        +[]() FREEDV_NONBLOCKING { return g_active.load(); },
        +[](BeepStep&) FREEDV_NONBLOCKING { g_completeCount++; });

    auto output = run(step, 1000);
    return output.size() == 1000 && peak(output, 0, 1000) == 0 && g_completeCount > 0;
}

bool completionNotCalledUntilRepeatsAreDone()
{
    std::unique_ptr<BeepStep> step(createStep(false));

    run(*step, TOTAL_SAMPLES);
    if (g_completeCount != 0)
    {
        std::cout << "[called " << g_completeCount << " time(s) before repeats were done] ";
        return false;
    }

    run(*step, 1);
    return g_completeCount > 0;
}

bool completionCalledOnceWhenStepIsReset()
{
    std::unique_ptr<BeepStep> step(createStep(true));

    auto output = run(*step, SAMPLE_RATE);
    if (g_completeCount != 1)
    {
        std::cout << "[called " << g_completeCount << " time(s)] ";
        return false;
    }

    // Reactivating should produce the same thing all over again.
    g_active = true;
    auto secondOutput = run(*step, SAMPLE_RATE);
    return secondOutput == output && g_completeCount == 2;
}

bool resetRestartsBeep()
{
    std::unique_ptr<BeepStep> step(createStep(false));
    auto expected = run(*step, TOTAL_SAMPLES);

    // Reset somewhere inside the second beep.
    step->reset();
    run(*step, CYCLE_SAMPLES + BEEP_SAMPLES / 2);
    step->reset();

    return run(*step, TOTAL_SAMPLES) == expected;
}

}

int main()
{
    executeTestCase("sampleRatesMatchConstructor", sampleRatesMatchConstructor);
    executeTestCase("generatesBeepsSeparatedBySilence", generatesBeepsSeparatedBySilence);
    executeTestCase("beepIsRampedUpAndDown", beepIsRampedUpAndDown);
    executeTestCase("beepHasRequestedFrequency", beepHasRequestedFrequency);
    executeTestCase("inputIsIgnored", inputIsIgnored);
    executeTestCase("outputIsContinuousAcrossCalls", outputIsContinuousAcrossCalls);
    executeTestCase("silentWhenInactive", silentWhenInactive);
    executeTestCase("pausesWhileInactiveAndResumesWhereItLeftOff", pausesWhileInactiveAndResumesWhereItLeftOff);
    executeTestCase("veryShortBeepStaysWithinBounds", veryShortBeepStaysWithinBounds);
    executeTestCase("veryShortBeepFadesOut", veryShortBeepFadesOut);
    executeTestCase("noRepeatsMeansNoBeep", noRepeatsMeansNoBeep);
    executeTestCase("completionNotCalledUntilRepeatsAreDone", completionNotCalledUntilRepeatsAreDone);
    executeTestCase("completionCalledOnceWhenStepIsReset", completionCalledOnceWhenStepIsReset);
    executeTestCase("resetRestartsBeep", resetRestartsBeep);
    return testResult();
}
