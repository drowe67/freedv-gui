#include <cmath>
#include <cstdlib>
#include <vector>

#include "freedv_api.h"

#include "../FreeDVReceiveStep.h"
#include "UtilStubs.h"
#include "../../test/UnitTestCommon.h"

namespace {

// Prints a bracketed mismatch on failure, like the config tests' helper.
template<typename T>
bool check(const char* description, T actual, T expected)
{
    if (actual == expected) return true;
    std::cout << "[" << description << " was " << actual << ", expected " << expected << "] ";
    return false;
}

// Modulates a tone with a fresh 1600 instance and returns the real modem
// samples, i.e. exactly what FreeDVTransmitStep would produce for it.
std::vector<short> modulateTone(int numFrames, double freqHz = 1000.0)
{
    struct freedv* dv = freedv_open(FREEDV_MODE_1600);
    int nSpeech = freedv_get_n_speech_samples(dv);
    int nModem = freedv_get_n_nom_modem_samples(dv);

    std::vector<short> speech(nSpeech);
    std::vector<COMP> fdm(nModem);
    std::vector<short> modemSamples;

    for (int frame = 0; frame < numFrames; frame++)
    {
        for (int i = 0; i < nSpeech; i++)
        {
            int sample = frame * nSpeech + i;
            speech[i] = (short)(3000.0 * std::sin(2.0 * M_PI * freqHz * sample / 8000.0));
        }

        freedv_comptx(dv, fdm.data(), speech.data());
        for (int i = 0; i < nModem; i++)
        {
            modemSamples.push_back((short)fdm[i].real);
        }
    }

    freedv_close(dv);
    return modemSamples;
}

// Feeds samples to the step in chunks, as the audio callback would.
std::vector<short> feed(FreeDVReceiveStep& step, std::vector<short> const& samples, int chunkSize)
{
    std::vector<short> output;

    for (size_t offset = 0; offset < samples.size(); offset += chunkSize)
    {
        size_t length = std::min((size_t)chunkSize, samples.size() - offset);
        std::vector<short> chunk(samples.begin() + offset, samples.begin() + offset + length);

        int numOutput = 0;
        short* produced = step.execute(chunk.data(), length, &numOutput);
        output.insert(output.end(), produced, produced + numOutput);
    }

    return output;
}

bool sampleRatesMatchMode()
{
    struct freedv* dv = freedv_open(FREEDV_MODE_1600);
    FreeDVReceiveStep step(dv);

    bool result = true;
    result &= check("input rate", step.getInputSampleRate(), freedv_get_modem_sample_rate(dv));
    result &= check("output rate", step.getOutputSampleRate(), freedv_get_speech_sample_rate(dv));

    freedv_close(dv);
    return result;
}

bool demodulatesTransmittedSignalAndSyncs()
{
    // fdmdv's sync state machine needs roughly 35 160-sample frames of a
    // clean signal before it locks; 40 modem frames (80 fdmdv frames) is
    // comfortable headroom.
    auto modemSamples = modulateTone(40);

    struct freedv* dv = freedv_open(FREEDV_MODE_1600);
    FreeDVReceiveStep step(dv);
    auto speech = feed(step, modemSamples, 160);

    bool result = true;
    if (speech.empty())
    {
        std::cout << "No speech samples produced for a valid transmitted signal" << std::endl;
        result = false;
    }
    result &= check("sync state", step.getSync(), 1);

    freedv_close(dv);
    return result;
}

bool squelchedSilenceProducesNoOutput()
{
    // 1600 opens with squelch enabled; out of sync, freedv_comprx returns no
    // samples, so the step must pass through an empty frame.
    struct freedv* dv = freedv_open(FREEDV_MODE_1600);
    int nModem = freedv_get_n_nom_modem_samples(dv);
    FreeDVReceiveStep step(dv);

    auto speech = feed(step, std::vector<short>(10 * nModem, 0), nModem);

    bool result = true;
    if (!speech.empty())
    {
        std::cout << "Silence produced " << speech.size()
              << " samples; squelch expected to suppress them" << std::endl;
        result = false;
    }
    result &= check("sync state", step.getSync(), 0);

    freedv_close(dv);
    return result;
}

bool resetDiscardsPendingSamples()
{
    struct freedv* dv = freedv_open(FREEDV_MODE_1600);
    FreeDVReceiveStep step(dv);

    int nin = freedv_nin(dv);
    auto partial = std::vector<short>(nin - 1, 0);

    auto output = feed(step, partial, nin - 1);
    if (!output.empty())
    {
        std::cout << "Partial frame produced " << output.size() << " samples" << std::endl;
        freedv_close(dv);
        return false;
    }

    step.reset();

    // Without reset(), the two partial frames would combine into a full one.
    output = feed(step, partial, nin - 1);
    if (!output.empty())
    {
        std::cout << "reset() kept " << output.size() << " samples across the boundary" << std::endl;
        freedv_close(dv);
        return false;
    }

    freedv_close(dv);
    return true;
}

bool largeFreqOffsetBreaksSync()
{
    // The same 40 frames that sync without an offset must not stay demodulated
    // when the step shifts them 500 Hz away from the carrier plan. Trial sync
    // may still emit a little garbage audio, so the comparison is against the
    // clean baseline's output volume.
    auto modemSamples = modulateTone(40);

    struct freedv* dv = freedv_open(FREEDV_MODE_1600);
    FreeDVReceiveStep step(dv);
    auto baseline = feed(step, modemSamples, 160);
    int baselineSync = step.getSync();
    freedv_close(dv);

    dv = freedv_open(FREEDV_MODE_1600);
    FreeDVReceiveStep shiftedStep(dv);
    shiftedStep.setFreqOffset(500.0f);
    auto shifted = feed(shiftedStep, modemSamples, 160);
    int shiftedSync = shiftedStep.getSync();
    freedv_close(dv);

    bool result = true;
    result &= check("baseline sync", baselineSync, 1);
    result &= check("sync with 500 Hz offset", shiftedSync, 0);
    if (shifted.size() >= baseline.size())
    {
        std::cout << "500 Hz offset produced " << shifted.size()
              << " samples, baseline produced " << baseline.size() << std::endl;
        result = false;
    }

    return result;
}

bool channelNoiseChangesOutput()
{
    // Enabling noise during sync acquisition trips an rx_est_timing assertion
    // inside fdmdv for some random draws, so noise is enabled after sync —
    // the way the app's "Simulate Channel Noise" option is used in practice.
    auto modemSamples = modulateTone(50);

    struct freedv* dv = freedv_open(FREEDV_MODE_1600);
    int nModem = freedv_get_n_nom_modem_samples(dv);
    size_t split = (size_t)40 * nModem;
    std::vector<short> head(modemSamples.begin(), modemSamples.begin() + split);
    std::vector<short> tail(modemSamples.begin() + split, modemSamples.end());

    FreeDVReceiveStep step(dv);
    feed(step, head, 160);
    auto baseline = feed(step, tail, 160);
    freedv_close(dv);

    dv = freedv_open(FREEDV_MODE_1600);
    FreeDVReceiveStep noisyStep(dv);
    feed(noisyStep, head, 160);
    srand(7); // fdmdv_simulate_channel draws from rand()
    noisyStep.setChannelNoiseEnable(true, 10);
    auto noisy = feed(noisyStep, tail, 160);
    freedv_close(dv);

    if (noisy.empty())
    {
        std::cout << "Noise at 10 dB SNR produced no output; demodulation should still sync" << std::endl;
        return false;
    }
    if (baseline == noisy)
    {
        std::cout << "Output identical with and without channel noise enabled" << std::endl;
        return false;
    }

    return true;
}

} // namespace

int main()
{
    executeTestCase("sampleRatesMatchMode", sampleRatesMatchMode);
    executeTestCase("demodulatesTransmittedSignalAndSyncs", demodulatesTransmittedSignalAndSyncs);
    executeTestCase("squelchedSilenceProducesNoOutput", squelchedSilenceProducesNoOutput);
    executeTestCase("resetDiscardsPendingSamples", resetDiscardsPendingSamples);
    executeTestCase("largeFreqOffsetBreaksSync", largeFreqOffsetBreaksSync);
    executeTestCase("channelNoiseChangesOutput", channelNoiseChangesOutput);

    return testResult();
}
