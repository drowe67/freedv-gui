#include <algorithm>
#include <vector>

#include "../ResamplePlotStep.h"
#include "../../defines.h"
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

constexpr int PLOT_FIFO_SIZE = 1024;

// Reads everything the step queued for the plot.
std::vector<short> drain(GenericFIFO<short>& fifo)
{
    std::vector<short> result;
    short buffer[256];

    while (fifo.numUsed() > 0)
    {
        int length = std::min(fifo.numUsed(), (int)(sizeof(buffer) / sizeof(buffer[0])));
        if (fifo.read(buffer, length) != 0)
        {
            break;
        }
        result.insert(result.end(), buffer, buffer + length);
    }

    return result;
}

bool sampleRatesAreFixedToFs()
{
    GenericFIFO<short> fifo(PLOT_FIFO_SIZE);
    ResampleForPlotStep step(&fifo);

    bool result = true;
    result &= check("input rate", step.getInputSampleRate(), FS);
    result &= check("output rate", step.getOutputSampleRate(), FS);
    return result;
}

bool executeReturnsNoPipelineSamples()
{
    GenericFIFO<short> fifo(PLOT_FIFO_SIZE);
    ResampleForPlotStep step(&fifo);

    std::vector<short> input(80, 100);
    int numOutput = -1;
    short* result = step.execute(input.data(), input.size(), &numOutput);

    bool ok = (result == nullptr) && (numOutput == 0);
    if (!ok)
    {
        std::cout << "execute() returned " << numOutput << " pipeline samples; "
              << "plot data must go to the FIFO only" << std::endl;
    }
    return ok;
}

bool outputsEnvelopeMaxMinPairs()
{
    GenericFIFO<short> fifo(PLOT_FIFO_SIZE);
    ResampleForPlotStep step(&fifo);

    // Decimation is FS/WAVEFORM_PLOT_FS = 20, and each emitted max/min pair
    // covers two decimation periods (40 input samples). max/min start at 0,
    // so the envelope is clamped to the zero line: an all-positive block
    // reports min 0, an all-negative block reports max 0.
    std::vector<short> input(80);
    for (int i = 0; i < 40; i++)
    {
        input[i] = 10 + i;             // 10..49
    }
    for (int i = 40; i < 80; i++)
    {
        input[i] = -30 - (i - 40);     // -30..-69
    }

    int numOutput = 0;
    step.execute(input.data(), input.size(), &numOutput);

    std::vector<short> plotted = drain(fifo);
    std::vector<short> expected = { 49, 0, 0, -69 };

    if (plotted != expected)
    {
        std::cout << "Expected " << expected.size() << " envelope samples, got "
              << plotted.size() << std::endl;
        for (size_t i = 0; i < std::max(plotted.size(), expected.size()); i++)
        {
            std::cout << "  [" << i << "] expected "
                  << (i < expected.size() ? expected[i] : 0) << ", got "
                  << (i < plotted.size() ? plotted[i] : 0) << std::endl;
        }
        return false;
    }

    return true;
}

bool handlesPartialFinalBlock()
{
    GenericFIFO<short> fifo(PLOT_FIFO_SIZE);
    ResampleForPlotStep step(&fifo);

    // 60 samples = 3 decimation periods; the count is rounded up to 4 so the
    // last pair covers only samples 40..59.
    std::vector<short> input(60);
    for (int i = 0; i < 40; i++)
    {
        input[i] = 10 + i;             // 10..49
    }
    for (int i = 40; i < 60; i++)
    {
        input[i] = (i % 2) ? -3 : 7;   // max 7, min -3
    }

    int numOutput = 0;
    step.execute(input.data(), input.size(), &numOutput);

    std::vector<short> plotted = drain(fifo);
    std::vector<short> expected = { 49, 0, 7, -3 };

    if (plotted != expected)
    {
        std::cout << "Expected " << expected.size() << " envelope samples, got "
              << plotted.size() << std::endl;
        return false;
    }

    return true;
}

} // namespace

int main()
{
    executeTestCase("sampleRatesAreFixedToFs", sampleRatesAreFixedToFs);
    executeTestCase("executeReturnsNoPipelineSamples", executeReturnsNoPipelineSamples);
    executeTestCase("outputsEnvelopeMaxMinPairs", outputsEnvelopeMaxMinPairs);
    executeTestCase("handlesPartialFinalBlock", handlesPartialFinalBlock);

    return testResult();
}
