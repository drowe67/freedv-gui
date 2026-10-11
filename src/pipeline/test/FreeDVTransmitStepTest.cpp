#include <cmath>
#include <vector>

#include "freedv_api.h"

#include "../FreeDVTransmitStep.h"
#include "UtilStubs.h"
#include "../../test/UnitTestCommon.h"

namespace {

// The frequency offset FreeDVTransmitStep is asked to apply. realtime_fp only
// accepts plain function pointers, so the value is kept in a global.
float g_txFreqOffsetHz = 0.0f;

// Prints a bracketed mismatch on failure, like the config tests' helper.
template<typename T>
bool check(const char* description, T actual, T expected)
{
    if (actual == expected) return true;
    std::cout << "[" << description << " was " << actual << ", expected " << expected << "] ";
    return false;
}

// Tracks a freedv instance and the frame sizes codec2 reports for it.
struct Modem
{
    struct freedv* dv;
    int nSpeech;
    int nModem;

    explicit Modem(int mode = FREEDV_MODE_1600)
        : dv(freedv_open(mode))
        , nSpeech(freedv_get_n_speech_samples(dv))
        , nModem(freedv_get_n_nom_modem_samples(dv))
    {
    }

    ~Modem()
    {
        if (dv != nullptr)
        {
            freedv_close(dv);
        }
    }
};

std::vector<short> tone(int numSamples, double freqHz = 1000.0, double amplitude = 3000.0)
{
    std::vector<short> samples(numSamples);
    for (int i = 0; i < numSamples; i++)
    {
        samples[i] = (short)(amplitude * std::sin(2.0 * M_PI * freqHz * i / 8000.0));
    }
    return samples;
}

std::vector<short> run(FreeDVTransmitStep& step, std::vector<short>& input)
{
    int numOutput = 0;
    short* output = step.execute(input.data(), input.size(), &numOutput);
    return std::vector<short>(output, output + numOutput);
}

bool sampleRatesMatchMode()
{
    Modem modem;
    FreeDVTransmitStep step(modem.dv, +[]() FREEDV_NONBLOCKING { return 0.0f; });

    bool result = true;
    result &= check("input rate", step.getInputSampleRate(), freedv_get_speech_sample_rate(modem.dv));
    result &= check("output rate", step.getOutputSampleRate(), freedv_get_modem_sample_rate(modem.dv));
    return result;
}

bool emitsOneFramePerInputFrame()
{
    Modem modem;
    FreeDVTransmitStep step(modem.dv, +[]() FREEDV_NONBLOCKING { return 0.0f; });

    auto input = tone(modem.nSpeech);
    auto output = run(step, input);

    return check("samples emitted", (int)output.size(), modem.nModem);
}

bool buffersPartialFrames()
{
    Modem modem;
    FreeDVTransmitStep step(modem.dv, +[]() FREEDV_NONBLOCKING { return 0.0f; });

    auto partial = tone(modem.nSpeech - 1);
    auto output = run(step, partial);
    if (!output.empty())
    {
        std::cout << "Partial frame produced " << output.size() << " samples" << std::endl;
        return false;
    }

    auto rest = tone(1);
    output = run(step, rest);
    return check("samples emitted after completing frame", (int)output.size(), modem.nModem);
}

bool emitsMultipleFramesFromOneRun()
{
    Modem modem;
    FreeDVTransmitStep step(modem.dv, +[]() FREEDV_NONBLOCKING { return 0.0f; });

    auto input = tone(2 * modem.nSpeech);
    auto output = run(step, input);

    return check("samples emitted", (int)output.size(), 2 * modem.nModem);
}

bool zeroOffsetOutputMatchesFreedvComptx()
{
    Modem reference;
    auto input = tone(reference.nSpeech);

    std::vector<COMP> expected(reference.nModem);
    freedv_comptx(reference.dv, expected.data(), input.data());

    // A second, freshly opened instance must produce the identical frame.
    Modem modem;
    FreeDVTransmitStep step(modem.dv, +[]() FREEDV_NONBLOCKING { return 0.0f; });
    auto output = run(step, input);

    if ((int)output.size() != reference.nModem)
    {
        std::cout << "Expected " << reference.nModem << " samples, got " << output.size() << std::endl;
        return false;
    }

    for (int i = 0; i < reference.nModem; i++)
    {
        short expectedSample = (short)expected[i].real;
        if (output[i] != expectedSample)
        {
            std::cout << "Sample " << i << ": expected " << expectedSample
                  << ", got " << output[i] << std::endl;
            return false;
        }
    }

    return true;
}

bool appliesTxFrequencyOffset()
{
    Modem modem;
    auto input = tone(modem.nSpeech);

    g_txFreqOffsetHz = 0.0f;
    FreeDVTransmitStep unshifted(modem.dv, +[]() FREEDV_NONBLOCKING { return g_txFreqOffsetHz; });
    auto baseline = run(unshifted, input);

    // A second instance is needed so the comparison starts from the same
    // codec2 state; the only difference is the applied offset.
    Modem shiftedModem;
    g_txFreqOffsetHz = 50.0f;
    FreeDVTransmitStep shifted(shiftedModem.dv, +[]() FREEDV_NONBLOCKING { return g_txFreqOffsetHz; });
    auto shiftedOutput = run(shifted, input);
    g_txFreqOffsetHz = 0.0f;

    if (baseline.size() != shiftedOutput.size() || baseline.empty())
    {
        std::cout << "Unexpected output sizes: " << baseline.size() << " vs "
              << shiftedOutput.size() << std::endl;
        return false;
    }

    for (size_t i = 0; i < baseline.size(); i++)
    {
        if (baseline[i] != shiftedOutput[i])
        {
            return true;
        }
    }

    std::cout << "Output identical with and without 50 Hz offset" << std::endl;
    return false;
}

bool resetDiscardsPendingSamples()
{
    Modem modem;
    FreeDVTransmitStep step(modem.dv, +[]() FREEDV_NONBLOCKING { return 0.0f; });

    auto partial = tone(modem.nSpeech - 1);
    auto output = run(step, partial);
    if (!output.empty())
    {
        std::cout << "Partial frame produced " << output.size() << " samples" << std::endl;
        return false;
    }

    step.reset();

    // Without reset(), the two partial frames would combine into a full one.
    output = run(step, partial);
    if (!output.empty())
    {
        std::cout << "reset() kept " << output.size() << " samples across the boundary" << std::endl;
        return false;
    }

    return true;
}

// Direct coverage of the shared freq_shift_coh() helper from util.cpp.

bool freqShiftCohZeroOffsetIsIdentity()
{
    constexpr int NUM_SAMPLES = 100;
    COMP input[NUM_SAMPLES];
    COMP output[NUM_SAMPLES];

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        input[i].real = 1000.0f * std::cos(2.0 * M_PI * 440.0 * i / 8000.0);
        input[i].imag = 1000.0f * std::sin(2.0 * M_PI * 440.0 * i / 8000.0);
    }

    COMP phaseRect = { 1.0f, 0.0f };
    freq_shift_coh(output, input, 0.0f, 8000.0f, &phaseRect, NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        if (output[i].real != input[i].real || output[i].imag != input[i].imag)
        {
            std::cout << "Sample " << i << " changed despite zero offset" << std::endl;
            return false;
        }
    }

    if (phaseRect.real != 1.0f || phaseRect.imag != 0.0f)
    {
        std::cout << "Phase oscillator moved despite zero offset" << std::endl;
        return false;
    }

    return true;
}

bool freqShiftCohAppliesProgressivePhase()
{
    constexpr int NUM_SAMPLES = 50;
    constexpr float Fs = 8000.0f;
    constexpr float FREQ_OFFSET = 100.0f;

    COMP input[NUM_SAMPLES];
    COMP output[NUM_SAMPLES];
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        input[i].real = 1000.0f;
        input[i].imag = 0.0f;
    }

    COMP phaseRect = { 1.0f, 0.0f };
    freq_shift_coh(output, input, FREQ_OFFSET, Fs, &phaseRect, NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        double phase = 2.0 * M_PI * FREQ_OFFSET * (i + 1) / Fs;
        double expectedReal = 1000.0 * std::cos(phase);
        double expectedImag = 1000.0 * std::sin(phase);

        if (std::fabs(output[i].real - expectedReal) > 1e-2 ||
            std::fabs(output[i].imag - expectedImag) > 1e-2)
        {
            std::cout << "Sample " << i << ": expected (" << expectedReal << ", " << expectedImag
                  << "), got (" << output[i].real << ", " << output[i].imag << ")" << std::endl;
            return false;
        }
    }

    return true;
}

bool freqShiftCohKeepsOscillatorNormalized()
{
    constexpr int NUM_SAMPLES = 5000;
    std::vector<COMP> input(NUM_SAMPLES);
    std::vector<COMP> output(NUM_SAMPLES);
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        input[i].real = 1000.0f;
        input[i].imag = 0.0f;
    }

    COMP phaseRect = { 1.0f, 0.0f };
    freq_shift_coh(output.data(), input.data(), 123.4f, 8000.0f, &phaseRect, NUM_SAMPLES);

    double magnitude = std::sqrt(
        (double)phaseRect.real * phaseRect.real + (double)phaseRect.imag * phaseRect.imag);
    if (std::fabs(magnitude - 1.0) > 1e-3)
    {
        std::cout << "Phase oscillator magnitude drifted to " << magnitude << std::endl;
        return false;
    }

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        double sampleMagnitude = std::sqrt(
            (double)output[i].real * output[i].real + (double)output[i].imag * output[i].imag);
        if (std::fabs(sampleMagnitude - 1000.0) > 1.0)
        {
            std::cout << "Sample " << i << " magnitude drifted to " << sampleMagnitude << std::endl;
            return false;
        }
    }

    return true;
}

} // namespace

int main()
{
    executeTestCase("sampleRatesMatchMode", sampleRatesMatchMode);
    executeTestCase("emitsOneFramePerInputFrame", emitsOneFramePerInputFrame);
    executeTestCase("buffersPartialFrames", buffersPartialFrames);
    executeTestCase("emitsMultipleFramesFromOneRun", emitsMultipleFramesFromOneRun);
    executeTestCase("zeroOffsetOutputMatchesFreedvComptx", zeroOffsetOutputMatchesFreedvComptx);
    executeTestCase("appliesTxFrequencyOffset", appliesTxFrequencyOffset);
    executeTestCase("resetDiscardsPendingSamples", resetDiscardsPendingSamples);
    executeTestCase("freqShiftCohZeroOffsetIsIdentity", freqShiftCohZeroOffsetIsIdentity);
    executeTestCase("freqShiftCohAppliesProgressivePhase", freqShiftCohAppliesProgressivePhase);
    executeTestCase("freqShiftCohKeepsOscillatorNormalized", freqShiftCohKeepsOscillatorNormalized);

    return testResult();
}
