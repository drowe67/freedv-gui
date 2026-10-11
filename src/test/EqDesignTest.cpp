#include <cmath>
#include <vector>

#include "../eq_design.h"
#include "../sox_biquad.h"
#include "UnitTestCommon.h"

namespace {

constexpr double TONE_AMPLITUDE = 2000.0; // leaves headroom for +12 dB of gain
constexpr int BLOCK_SIZE = 480;           // sox_biquad_filter() allocates on the stack
constexpr double GAIN_TOLERANCE_DB = 1.0;

double rms(std::vector<short> const& samples, size_t start)
{
    double sum = 0;
    for (size_t i = start; i < samples.size(); i++)
    {
        sum += (double)samples[i] * samples[i];
    }
    return std::sqrt(sum / (samples.size() - start));
}

// Runs one second of a tone at the given frequency through the filter and
// returns the resulting steady-state gain in dB.
double measureGainDb(void* filter, double freqHz, int sampleRate)
{
    std::vector<short> input(sampleRate);
    std::vector<short> output(sampleRate);
    for (int n = 0; n < sampleRate; n++)
    {
        input[n] = (short)(TONE_AMPLITUDE * std::cos(2.0 * M_PI * freqHz * n / sampleRate));
    }

    for (int n = 0; n < sampleRate; n += BLOCK_SIZE)
    {
        int count = std::min(BLOCK_SIZE, sampleRate - n);
        sox_biquad_filter(filter, &output[n], &input[n], count);
    }

    // Skip the first half to let the filter settle.
    size_t start = sampleRate / 2;
    return 20.0 * std::log10(rms(output, start) / rms(input, start));
}

bool gainIsNear(const char* description, void* filter, double freqHz, int sampleRate, double expectedDb)
{
    if (filter == nullptr)
    {
        std::cout << "[" << description << ": no filter created] ";
        return false;
    }

    double gainDb = measureGainDb(filter, freqHz, sampleRate);
    if (std::fabs(gainDb - expectedDb) > GAIN_TOLERANCE_DB)
    {
        std::cout << "[" << description << ": gain at " << freqHz << " Hz is " << gainDb
                  << " dB, expected " << expectedDb << " dB] ";
        return false;
    }
    return true;
}

// Each band's own gain should appear at its configured frequency regardless of
// the sample rate that the filters are run at. Bands are checked one at a time
// (the others set flat) so that they can't mask each other.
bool bandFiltersFollowSampleRate(int sampleRate)
{
    const float BASS_FREQ_HZ = 100;
    const float TREBLE_FREQ_HZ = 3000;
    const float MID_FREQ_HZ = 1500;
    const float GAIN_DB = 12;

    bool result = true;
    void* bass = nullptr;
    void* treble = nullptr;
    void* mid = nullptr;

    EQBandSettings settings = {
        .bassFreqHz = BASS_FREQ_HZ,
        .bassGaindB = GAIN_DB,
        .trebleFreqHz = TREBLE_FREQ_HZ,
        .trebleGaindB = GAIN_DB,
        .midFreqHz = MID_FREQ_HZ,
        .midGaindB = GAIN_DB,
        .midQ = 1.0,
    };
    designEQBandFilters(settings, sampleRate, &bass, &treble, &mid);

    // Shelving filters provide half of their gain (in dB) at the corner
    // frequency and leave the far side of the shelf untouched.
    result &= gainIsNear("bass corner", bass, BASS_FREQ_HZ, sampleRate, GAIN_DB / 2);
    result &= gainIsNear("bass stopband", bass, 2000, sampleRate, 0);
    result &= gainIsNear("treble corner", treble, TREBLE_FREQ_HZ, sampleRate, GAIN_DB / 2);
    result &= gainIsNear("treble stopband", treble, 150, sampleRate, 0);

    // Peaking filter provides its full gain at the center frequency.
    result &= gainIsNear("mid center", mid, MID_FREQ_HZ, sampleRate, GAIN_DB);
    result &= gainIsNear("mid stopband", mid, 100, sampleRate, 0);

    if (bass != nullptr) sox_biquad_destroy(bass);
    if (treble != nullptr) sox_biquad_destroy(treble);
    if (mid != nullptr) sox_biquad_destroy(mid);

    return result;
}

bool volumeFilterAppliesGain(int sampleRate)
{
    const float GAIN_DB = 6;

    void* vol = designAnEQFilter("vol", 0, GAIN_DB, 0, sampleRate);
    bool result = gainIsNear("vol", vol, 1000, sampleRate, GAIN_DB);
    if (vol != nullptr) sox_biquad_destroy(vol);
    return result;
}

bool zeroVolumeIsNoOp()
{
    // Callers rely on a no-op filter being reported as "no filter".
    return designAnEQFilter("vol", 0, 0, 0, 48000) == nullptr;
}

}

int main()
{
    sox_biquad_start();

    for (int sampleRate : { 8000, 16000, 48000 })
    {
        std::string suffix = " @ " + std::to_string(sampleRate) + " Hz";
        executeTestCase("bandFiltersFollowSampleRate" + suffix, [sampleRate]() { return bandFiltersFollowSampleRate(sampleRate); });
        executeTestCase("volumeFilterAppliesGain" + suffix, [sampleRate]() { return volumeFilterAppliesGain(sampleRate); });
    }
    executeTestCase("zeroVolumeIsNoOp", zeroVolumeIsNoOp);

    sox_biquad_finish();
    return testResult();
}
