#include <atomic>
#include <cmath>
#include <vector>

#include "../EqualizerStep.h"
#include "../../eq_design.h"
#include "../../sox_biquad.h"
#include "../../test/UnitTestCommon.h"

namespace {

constexpr int SAMPLE_RATE = 48000;
constexpr int BLOCK_SIZE = 960;
constexpr int NUM_BLOCKS = 25;

const EQBandSettings BAND_SETTINGS = {
    .bassFreqHz = 100,
    .bassGaindB = 6,
    .trebleFreqHz = 3000,
    .trebleGaindB = -6,
    .midFreqHz = 1500,
    .midGaindB = 3,
    .midQ = 1.0,
};

// Mirrors the way FreeDV holds on to the filters: the step only has pointers
// to them so that they can be redesigned while the pipeline is running.
struct Fixture
{
    std::atomic<bool> enable { false };
    void* bass = nullptr;
    void* mid = nullptr;
    void* treble = nullptr;
    void* vol = nullptr;
    audio_spin_mutex lock;
    EqualizerStep step { SAMPLE_RATE, &enable, &bass, &mid, &treble, &vol, lock };

    ~Fixture()
    {
        for (auto filter : { bass, mid, treble, vol })
        {
            if (filter != nullptr) sox_biquad_destroy(filter);
        }
    }

    void designBandFilters()
    {
        designEQBandFilters(BAND_SETTINGS, SAMPLE_RATE, &bass, &treble, &mid);
    }

    void designVolumeFilter(float gaindB)
    {
        vol = designAnEQFilter("vol", 0, gaindB, 0, SAMPLE_RATE);
    }
};

// Something with content across the whole audio band.
std::vector<short> testSignal(int block)
{
    std::vector<short> result(BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        double t = (double)(block * BLOCK_SIZE + i) / SAMPLE_RATE;
        result[i] = (short)(
            1000 * std::sin(2 * M_PI * 60 * t) +
            1000 * std::sin(2 * M_PI * 1500 * t) +
            1000 * std::sin(2 * M_PI * 6000 * t));
    }
    return result;
}

double rms(std::vector<short> const& samples)
{
    double sum = 0;
    for (auto sample : samples) sum += (double)sample * sample;
    return std::sqrt(sum / samples.size());
}

bool sampleRatesMatchConstructor()
{
    Fixture f;
    return f.step.getInputSampleRate() == SAMPLE_RATE && f.step.getOutputSampleRate() == SAMPLE_RATE;
}

bool passesThroughInputWhenNothingEnabled()
{
    Fixture f;
    f.designBandFilters(); // designed, but not enabled

    auto input = testSignal(0);
    int numOutput = 0;
    short* output = f.step.execute(input.data(), input.size(), &numOutput);
    return output == input.data() && numOutput == BLOCK_SIZE;
}

bool enabledWithoutFiltersLeavesAudioUnchanged()
{
    Fixture f;
    f.enable = true;

    auto input = testSignal(0);
    int numOutput = 0;
    short* output = f.step.execute(input.data(), input.size(), &numOutput);
    return numOutput == BLOCK_SIZE && std::vector<short>(output, output + numOutput) == input;
}

bool appliesBandFiltersWhenEnabled()
{
    Fixture f;
    f.enable = true;
    f.designBandFilters();

    // Separate copies of the same filters, as the filters have state.
    void* bass = nullptr;
    void* treble = nullptr;
    void* mid = nullptr;
    designEQBandFilters(BAND_SETTINGS, SAMPLE_RATE, &bass, &treble, &mid);

    bool result = true;
    bool changedAudio = false;
    for (int block = 0; block < NUM_BLOCKS && result; block++)
    {
        auto input = testSignal(block);
        auto expected = input;
        sox_biquad_filter(bass, expected.data(), expected.data(), BLOCK_SIZE);
        sox_biquad_filter(treble, expected.data(), expected.data(), BLOCK_SIZE);
        sox_biquad_filter(mid, expected.data(), expected.data(), BLOCK_SIZE);

        int numOutput = 0;
        short* output = f.step.execute(input.data(), input.size(), &numOutput);
        result = numOutput == BLOCK_SIZE && std::vector<short>(output, output + numOutput) == expected;
        changedAudio |= expected != input;
    }

    sox_biquad_destroy(bass);
    sox_biquad_destroy(treble);
    sox_biquad_destroy(mid);

    return result && changedAudio;
}

bool appliesVolumeEvenWhenEqualizerDisabled()
{
    const float GAIN_DB = 6;

    Fixture f;
    f.designVolumeFilter(GAIN_DB);

    double gaindB = 0;
    for (int block = 0; block < NUM_BLOCKS; block++)
    {
        auto input = testSignal(block);
        int numOutput = 0;
        short* output = f.step.execute(input.data(), input.size(), &numOutput);
        if (numOutput != BLOCK_SIZE || output == input.data())
        {
            std::cout << "[volume not applied] ";
            return false;
        }
        gaindB = 20 * std::log10(rms(std::vector<short>(output, output + numOutput)) / rms(input));
    }

    if (std::fabs(gaindB - GAIN_DB) > 0.5)
    {
        std::cout << "[gain was " << gaindB << " dB, expected " << GAIN_DB << " dB] ";
        return false;
    }
    return true;
}

bool appliesVolumeBeforeBandFilters()
{
    const float GAIN_DB = -6;

    Fixture f;
    f.enable = true;
    f.designBandFilters();
    f.designVolumeFilter(GAIN_DB);

    void* bass = nullptr;
    void* treble = nullptr;
    void* mid = nullptr;
    designEQBandFilters(BAND_SETTINGS, SAMPLE_RATE, &bass, &treble, &mid);
    void* vol = designAnEQFilter("vol", 0, GAIN_DB, 0, SAMPLE_RATE);

    bool result = true;
    for (int block = 0; block < NUM_BLOCKS && result; block++)
    {
        auto input = testSignal(block);
        auto expected = input;
        sox_biquad_filter(vol, expected.data(), expected.data(), BLOCK_SIZE);
        sox_biquad_filter(bass, expected.data(), expected.data(), BLOCK_SIZE);
        sox_biquad_filter(treble, expected.data(), expected.data(), BLOCK_SIZE);
        sox_biquad_filter(mid, expected.data(), expected.data(), BLOCK_SIZE);

        int numOutput = 0;
        short* output = f.step.execute(input.data(), input.size(), &numOutput);
        result = numOutput == BLOCK_SIZE && std::vector<short>(output, output + numOutput) == expected;
    }

    sox_biquad_destroy(bass);
    sox_biquad_destroy(treble);
    sox_biquad_destroy(mid);
    sox_biquad_destroy(vol);

    return result;
}

bool passesThroughInputWhileFiltersAreBeingUpdated()
{
    Fixture f;
    f.enable = true;
    f.designBandFilters();
    f.designVolumeFilter(6);

    // The UI holds the lock while it replaces the filters.
    f.lock.lock();
    auto input = testSignal(0);
    int numOutput = 0;
    short* output = f.step.execute(input.data(), input.size(), &numOutput);
    f.lock.unlock();

    return output == input.data() && numOutput == BLOCK_SIZE;
}

bool releasesLockAfterExecuting()
{
    // One fixture per code path through execute().
    for (int path = 0; path < 3; path++)
    {
        Fixture f;
        if (path >= 1) f.designVolumeFilter(6);
        if (path >= 2)
        {
            f.enable = true;
            f.designBandFilters();
        }

        auto input = testSignal(0);
        int numOutput = 0;
        f.step.execute(input.data(), input.size(), &numOutput);

        if (!f.lock.try_lock())
        {
            std::cout << "[lock still held for path " << path << "] ";
            return false;
        }
        f.lock.unlock();
    }
    return true;
}

bool handlesEmptyInput()
{
    Fixture f;
    f.enable = true;
    f.designBandFilters();

    short dummy = 0;
    int numOutput = -1;
    f.step.execute(&dummy, 0, &numOutput);
    return numOutput == 0 && f.lock.try_lock();
}

}

int main()
{
    sox_biquad_start();

    executeTestCase("sampleRatesMatchConstructor", sampleRatesMatchConstructor);
    executeTestCase("passesThroughInputWhenNothingEnabled", passesThroughInputWhenNothingEnabled);
    executeTestCase("enabledWithoutFiltersLeavesAudioUnchanged", enabledWithoutFiltersLeavesAudioUnchanged);
    executeTestCase("appliesBandFiltersWhenEnabled", appliesBandFiltersWhenEnabled);
    executeTestCase("appliesVolumeEvenWhenEqualizerDisabled", appliesVolumeEvenWhenEqualizerDisabled);
    executeTestCase("appliesVolumeBeforeBandFilters", appliesVolumeBeforeBandFilters);
    executeTestCase("passesThroughInputWhileFiltersAreBeingUpdated", passesThroughInputWhileFiltersAreBeingUpdated);
    executeTestCase("releasesLockAfterExecuting", releasesLockAfterExecuting);
    executeTestCase("handlesEmptyInput", handlesEmptyInput);

    sox_biquad_finish();
    return testResult();
}
