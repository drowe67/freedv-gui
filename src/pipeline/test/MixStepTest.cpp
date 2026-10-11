#include <cmath>
#include <cstdlib>
#include <vector>

#include "../MixStep.h"
#include "StubSteps.h"
#include "../../test/UnitTestCommon.h"

namespace {

constexpr int SAMPLE_RATE = 8000;
constexpr int BLOCK_SIZE = 160;

// Independent implementation of the documented mixing algorithm: sum the two
// channels, divide by sqrt(2) and soft clip with tanh().
int expectedMix(int left, int right)
{
    double mixed = std::tanh((left / 32768.0 + right / 32768.0) / std::sqrt(2.0));
    return (int)std::trunc(mixed * 32768.0);
}

std::vector<short> run(MixStep& step, std::vector<short> input)
{
    int numOutput = 0;
    short* output = step.execute(input.data(), input.size(), &numOutput);
    return std::vector<short>(output, output + numOutput);
}

bool allNear(std::vector<short> const& actual, size_t expectedCount, int expectedValue)
{
    if (actual.size() != expectedCount)
    {
        std::cout << "[got " << actual.size() << " samples, expected " << expectedCount << "] ";
        return false;
    }

    for (auto sample : actual)
    {
        // Allow for float vs. double rounding differences.
        if (std::abs(sample - expectedValue) > 1)
        {
            std::cout << "[got " << sample << ", expected " << expectedValue << "] ";
            return false;
        }
    }
    return true;
}

// Mixes two channels that respectively add the given offsets to the input.
bool mixesTo(short input, short leftOffset, short rightOffset)
{
    MixStep step(new OffsetStep(SAMPLE_RATE, leftOffset), new OffsetStep(SAMPLE_RATE, rightOffset));
    auto output = run(step, std::vector<short>(BLOCK_SIZE, input));
    return allNear(output, BLOCK_SIZE, expectedMix(input + leftOffset, input + rightOffset));
}

bool sampleRatesMatchChannels()
{
    MixStep step(new OffsetStep(SAMPLE_RATE, 0), new OffsetStep(SAMPLE_RATE, 0));
    return step.getInputSampleRate() == SAMPLE_RATE && step.getOutputSampleRate() == SAMPLE_RATE;
}

bool mixesBothChannels()
{
    bool result = true;
    result &= mixesTo(0, 1000, 2000);
    result &= mixesTo(1000, 500, -3000);
    result &= mixesTo(-4000, 0, 0);
    return result;
}

bool silenceMixesToSilence()
{
    return mixesTo(0, 0, 0);
}

bool oppositeSignalsCancel()
{
    return mixesTo(0, 12345, -12345);
}

bool loudSignalsDoNotWrapAround()
{
    // Would overflow a short if simply added together.
    MixStep step(new OffsetStep(SAMPLE_RATE, 32767), new OffsetStep(SAMPLE_RATE, 32767));
    auto output = run(step, std::vector<short>(BLOCK_SIZE, 0));
    if (!allNear(output, BLOCK_SIZE, expectedMix(32767, 32767))) return false;

    MixStep negativeStep(new OffsetStep(SAMPLE_RATE, -32768), new OffsetStep(SAMPLE_RATE, -32768));
    output = run(negativeStep, std::vector<short>(BLOCK_SIZE, 0));
    return allNear(output, BLOCK_SIZE, expectedMix(-32768, -32768)) && output[0] < 0;
}

bool bothChannelsReceiveInput()
{
    StubStepStats leftStats;
    StubStepStats rightStats;
    MixStep step(new OffsetStep(SAMPLE_RATE, 0, &leftStats), new OffsetStep(SAMPLE_RATE, 0, &rightStats));
    run(step, std::vector<short>(BLOCK_SIZE, 0));

    return
        leftStats.numExecutes == 1 && leftStats.numSamplesSeen == BLOCK_SIZE &&
        rightStats.numExecutes == 1 && rightStats.numSamplesSeen == BLOCK_SIZE;
}

bool passesThroughSingleChannelWhenOtherHasNoOutput()
{
    // Right channel never outputs anything.
    MixStep step(new OffsetStep(SAMPLE_RATE, 1000), new OffsetStep(SAMPLE_RATE, 0, nullptr, 0));
    auto output = run(step, std::vector<short>(BLOCK_SIZE, 0));
    return allNear(output, BLOCK_SIZE, expectedMix(1000, 0));
}

bool holdsBackExcessFromLongerChannel()
{
    // Right channel only outputs half of what the left one does.
    MixStep step(new OffsetStep(SAMPLE_RATE, 1000), new OffsetStep(SAMPLE_RATE, 2000, nullptr, BLOCK_SIZE / 2));

    auto output = run(step, std::vector<short>(BLOCK_SIZE, 0));
    if (!allNear(output, BLOCK_SIZE / 2, expectedMix(1000, 2000))) return false;

    // The other half of the left channel should be mixed with the next thing
    // that comes out of the right channel rather than being dropped.
    output = run(step, std::vector<short>(BLOCK_SIZE, 0));
    return allNear(output, BLOCK_SIZE / 2, expectedMix(1000, 2000));
}

bool resetDiscardsPendingAudioAndResetsChannels()
{
    StubStepStats leftStats;
    StubStepStats rightStats;
    MixStep step(
        new OffsetStep(SAMPLE_RATE, 1000, &leftStats),
        new OffsetStep(SAMPLE_RATE, 2000, &rightStats, BLOCK_SIZE / 2));

    // Leaves half a block of the left channel pending.
    run(step, std::vector<short>(BLOCK_SIZE, 0));
    step.reset();
    if (leftStats.numResets != 1 || rightStats.numResets != 1)
    {
        std::cout << "[channels were not reset] ";
        return false;
    }

    // If the pending audio was discarded, there's nothing to mix with no new input.
    auto output = run(step, {});
    return allNear(output, 0, 0);
}

bool ownsChannels()
{
    StubStepStats leftStats;
    StubStepStats rightStats;
    {
        MixStep step(new OffsetStep(SAMPLE_RATE, 0, &leftStats), new OffsetStep(SAMPLE_RATE, 0, &rightStats));
    }
    return leftStats.destroyed && rightStats.destroyed;
}

}

int main()
{
    executeTestCase("sampleRatesMatchChannels", sampleRatesMatchChannels);
    executeTestCase("mixesBothChannels", mixesBothChannels);
    executeTestCase("silenceMixesToSilence", silenceMixesToSilence);
    executeTestCase("oppositeSignalsCancel", oppositeSignalsCancel);
    executeTestCase("loudSignalsDoNotWrapAround", loudSignalsDoNotWrapAround);
    executeTestCase("bothChannelsReceiveInput", bothChannelsReceiveInput);
    executeTestCase("passesThroughSingleChannelWhenOtherHasNoOutput", passesThroughSingleChannelWhenOtherHasNoOutput);
    executeTestCase("holdsBackExcessFromLongerChannel", holdsBackExcessFromLongerChannel);
    executeTestCase("resetDiscardsPendingAudioAndResetsChannels", resetDiscardsPendingAudioAndResetsChannels);
    executeTestCase("ownsChannels", ownsChannels);
    return testResult();
}
