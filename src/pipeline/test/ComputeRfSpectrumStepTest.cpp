#include <cmath>
#include <vector>

#include "../ComputeRfSpectrumStep.h"
#include "../../test/UnitTestCommon.h"

namespace {

constexpr int SAMPLE_RATE = 8000;
constexpr int BLOCK_SIZE = 160;
constexpr int NUM_BLOCKS = 20; // enough to fill the FFT's input buffer

// realtime_fp only accepts plain function pointers, so state is kept here.
struct MODEM_STATS g_stats;
GenericFIFO<float>* g_avMagFifo;

struct Fixture
{
    GenericFIFO<float> fifo { MODEM_STATS_NSPEC * NUM_BLOCKS };
    ComputeRfSpectrumStep step {
        +[]() FREEDV_NONBLOCKING { return &g_stats; },
        +[]() FREEDV_NONBLOCKING { return g_avMagFifo; }
    };

    Fixture()
    {
        modem_stats_open(&g_stats);
        g_avMagFifo = &fifo;
    }

    ~Fixture()
    {
        modem_stats_close(&g_stats);
    }

    // Feeds a tone through the step and returns the last spectrum it produced.
    std::vector<float> spectrumForTone(double freqHz, double amplitude)
    {
        for (int block = 0; block < NUM_BLOCKS; block++)
        {
            short input[BLOCK_SIZE];
            for (int i = 0; i < BLOCK_SIZE; i++)
            {
                input[i] = (short)(amplitude * std::cos(2 * M_PI * freqHz * (block * BLOCK_SIZE + i) / SAMPLE_RATE));
            }

            int numOutput = -1;
            step.execute(input, BLOCK_SIZE, &numOutput);
        }

        std::vector<float> spectrum(MODEM_STATS_NSPEC);
        while (fifo.numUsed() >= MODEM_STATS_NSPEC)
        {
            fifo.read(spectrum.data(), MODEM_STATS_NSPEC);
        }
        return spectrum;
    }
};

int peakBin(std::vector<float> const& spectrum)
{
    int result = 0;
    for (int bin = 1; bin < (int)spectrum.size(); bin++)
    {
        if (spectrum[bin] > spectrum[result]) result = bin;
    }
    return result;
}

bool sampleRatesAreModemRate()
{
    Fixture f;
    return f.step.getInputSampleRate() == SAMPLE_RATE && f.step.getOutputSampleRate() == SAMPLE_RATE;
}

bool isTapWithNoOutput()
{
    Fixture f;
    short input[BLOCK_SIZE] = {};
    int numOutput = -1;
    return f.step.execute(input, BLOCK_SIZE, &numOutput) == nullptr && numOutput == 0;
}

bool writesOneSpectrumPerCall()
{
    Fixture f;
    short input[BLOCK_SIZE] = {};
    int numOutput = 0;

    for (int call = 1; call <= 3; call++)
    {
        f.step.execute(input, BLOCK_SIZE, &numOutput);
        if (f.fifo.numUsed() != MODEM_STATS_NSPEC * call)
        {
            std::cout << "[" << f.fifo.numUsed() << " values after " << call << " call(s)] ";
            return false;
        }
    }
    return true;
}

bool spectrumPeaksAtToneFrequency()
{
    // Spectrum covers 0 to MODEM_STATS_MAX_F_HZ.
    const double HZ_PER_BIN = (double)MODEM_STATS_MAX_F_HZ / MODEM_STATS_NSPEC;

    for (double freqHz : { 500.0, 1500.0, 2800.0 })
    {
        Fixture f;
        auto spectrum = f.spectrumForTone(freqHz, 10000);

        double peakHz = peakBin(spectrum) * HZ_PER_BIN;
        if (std::fabs(peakHz - freqHz) > 2 * HZ_PER_BIN)
        {
            std::cout << "[" << freqHz << " Hz tone has peak at " << peakHz << " Hz] ";
            return false;
        }
    }
    return true;
}

bool spectrumLevelFollowsSignalLevel()
{
    // Only one fixture can exist at a time as they share the modem stats object.
    auto peakForAmplitude = [](double amplitude) {
        Fixture f;
        auto spectrum = f.spectrumForTone(1500, amplitude);
        return spectrum[peakBin(spectrum)];
    };
    float loudPeak = peakForAmplitude(10000);
    float quietPeak = peakForAmplitude(1000);

    // 10x amplitude = 20 dB.
    if (std::fabs((loudPeak - quietPeak) - 20) > 0.5)
    {
        std::cout << "[loud peak " << loudPeak << " dB, quiet peak " << quietPeak << " dB] ";
        return false;
    }
    return true;
}

}

int main()
{
    executeTestCase("sampleRatesAreModemRate", sampleRatesAreModemRate);
    executeTestCase("isTapWithNoOutput", isTapWithNoOutput);
    executeTestCase("writesOneSpectrumPerCall", writesOneSpectrumPerCall);
    executeTestCase("spectrumPeaksAtToneFrequency", spectrumPeaksAtToneFrequency);
    executeTestCase("spectrumLevelFollowsSignalLevel", spectrumLevelFollowsSignalLevel);
    return testResult();
}
