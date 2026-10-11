#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <numeric>
#include <set>
#include <thread>
#include <vector>

#include "../ParallelStep.h"
#include "StubSteps.h"
#include "../../test/UnitTestCommon.h"

using namespace std::chrono_literals;

namespace {

constexpr int SAMPLE_RATE = 8000;
constexpr int FRAME_SIZE = 160; // 20 ms; ParallelStep processes input one frame at a time
constexpr int NUM_STEPS = 3;
constexpr int ROUTE_TO_ALL = -1;

// Offsets added by each of the parallel steps, to tell their output apart.
constexpr short OFFSETS[NUM_STEPS] = { 100, 200, 300 };

// realtime_fp only accepts plain function pointers, so routing decisions are
// passed in via the step's callback state.
struct Routing
{
    int input = ROUTE_TO_ALL;
    int output = 0;
};

struct Fixture
{
    Routing routing;
    StubStepStats stats[NUM_STEPS];
    std::shared_ptr<int> state = std::make_shared<int>(42);
    std::unique_ptr<ParallelStep> step;

    explicit Fixture(bool multiThreaded)
    {
        // Note: ParallelStep takes ownership of the steps.
        std::vector<IPipelineStep*> steps;
        for (int index = 0; index < NUM_STEPS; index++)
        {
            steps.push_back(new OffsetStep(SAMPLE_RATE, OFFSETS[index], &stats[index]));
        }

        step = std::make_unique<ParallelStep>(
            SAMPLE_RATE, SAMPLE_RATE, multiThreaded,
            +[](ParallelStep* s) FREEDV_NONBLOCKING { return ((Routing*)s->getCallbackState())->input; },
            +[](ParallelStep* s) FREEDV_NONBLOCKING { return ((Routing*)s->getCallbackState())->output; },
            steps, state, &routing, nullptr);
    }

    std::vector<short> run(std::vector<short> input)
    {
        // Always pass a valid buffer, even if there's nothing in it.
        short empty = 0;
        int numOutput = 0;
        short* output = step->execute(input.empty() ? &empty : input.data(), input.size(), &numOutput);
        return std::vector<short>(output, output + numOutput);
    }
};

std::vector<short> ramp(int numSamples, short start = 1)
{
    std::vector<short> result(numSamples);
    std::iota(result.begin(), result.end(), start);
    return result;
}

std::vector<short> withOffset(std::vector<short> samples, short offset)
{
    for (auto& sample : samples) sample += offset;
    return samples;
}

bool accessorsReturnConstructorArguments()
{
    Fixture f(false);
    return
        f.step->getInputSampleRate() == SAMPLE_RATE &&
        f.step->getOutputSampleRate() == SAMPLE_RATE &&
        f.step->getState() == f.state.get() &&
        f.step->getCallbackState() == &f.routing &&
        f.step->getParallelSteps().size() == NUM_STEPS;
}

bool routesInputToAllSteps()
{
    Fixture f(false);
    f.routing.input = ROUTE_TO_ALL;
    f.run(ramp(FRAME_SIZE));

    for (int index = 0; index < NUM_STEPS; index++)
    {
        if (f.stats[index].numSamplesSeen != FRAME_SIZE)
        {
            std::cout << "[step " << index << " saw " << f.stats[index].numSamplesSeen << " samples] ";
            return false;
        }
    }
    return true;
}

bool routesInputToSingleStep()
{
    for (int target = 0; target < NUM_STEPS; target++)
    {
        Fixture f(false);
        f.routing.input = target;
        f.routing.output = target;
        f.run(ramp(FRAME_SIZE));

        for (int index = 0; index < NUM_STEPS; index++)
        {
            int expected = (index == target) ? FRAME_SIZE : 0;
            if (f.stats[index].numSamplesSeen != expected)
            {
                std::cout << "[routed to " << target << " but step " << index << " saw "
                          << f.stats[index].numSamplesSeen << " samples] ";
                return false;
            }
        }
    }
    return true;
}

bool outputsSelectedStep()
{
    for (int target = 0; target < NUM_STEPS; target++)
    {
        Fixture f(false);
        f.routing.output = target;

        auto input = ramp(FRAME_SIZE);
        if (f.run(input) != withOffset(input, OFFSETS[target]))
        {
            std::cout << "[wrong output for step " << target << "] ";
            return false;
        }
    }
    return true;
}

bool processesInputLargerThanOneFrame()
{
    Fixture f(false);

    // Two and a half frames.
    auto input = ramp(FRAME_SIZE * 5 / 2);
    return f.run(input) == withOffset(input, OFFSETS[0]) && f.stats[0].numSamplesSeen == (int)input.size();
}

bool outputIsLimitedToInputSize()
{
    Fixture f(false);

    // Step 1's output isn't being consumed, so it builds up...
    auto first = ramp(FRAME_SIZE, 1);
    auto second = ramp(FRAME_SIZE, 1000);
    f.run(first);
    f.run(second);

    // ...and is then released in order, no more than a block at a time.
    f.routing.input = 0;
    f.routing.output = 1;
    auto third = ramp(FRAME_SIZE, 2000);
    return
        f.run(third) == withOffset(first, OFFSETS[1]) &&
        f.run(third) == withOffset(second, OFFSETS[1]);
}

bool noOutputWhenSelectedStepHasNone()
{
    Fixture f(false);
    f.routing.input = 0;
    f.routing.output = 1;

    int numOutput = -1;
    auto input = ramp(FRAME_SIZE);
    short* output = f.step->execute(input.data(), input.size(), &numOutput);
    if (numOutput != 0 || output == nullptr)
    {
        std::cout << "[got " << numOutput << " samples] ";
        return false;
    }

    // Callers may still read a block's worth, so it needs to be silence.
    for (int index = 0; index < FRAME_SIZE; index++)
    {
        if (output[index] != 0) return false;
    }
    return true;
}

bool resetDiscardsPendingAudio()
{
    Fixture f(false);

    // Leaves a frame pending on steps 1 and 2.
    f.run(ramp(FRAME_SIZE));
    f.step->reset();

    f.routing.input = 0;
    f.routing.output = 1;
    return f.run(ramp(FRAME_SIZE)).size() == 0;
}

bool ownsSteps()
{
    StubStepStats* stats;
    std::unique_ptr<Fixture> f = std::make_unique<Fixture>(false);
    stats = f->stats;
    f->step = nullptr;
    return stats[0].destroyed && stats[1].destroyed && stats[2].destroyed;
}

bool multiThreadedProducesSameOutput()
{
    Fixture f(true);
    f.routing.output = 2;

    // Processing happens on other threads, so output shows up some time later.
    std::vector<short> input;
    std::vector<short> output;
    for (int block = 0; block < 10; block++)
    {
        auto blockInput = ramp(FRAME_SIZE, block * 1000);
        input.insert(input.end(), blockInput.begin(), blockInput.end());

        auto blockOutput = f.run(blockInput);
        output.insert(output.end(), blockOutput.begin(), blockOutput.end());
        std::this_thread::sleep_for(5ms);
    }

    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (output.size() < input.size() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(5ms);

        // Not providing any input returns everything that's pending.
        auto blockOutput = f.run({});
        output.insert(output.end(), blockOutput.begin(), blockOutput.end());
    }

    if (output != withOffset(input, OFFSETS[2]))
    {
        std::cout << "[got " << output.size() << " samples, expected " << input.size() << "] ";
        return false;
    }
    return true;
}

// Converts between the given sample rates around a step that runs at stepRate.
bool convertsSampleRates(int inputRate, int stepRate, int outputRate)
{
    const double TONE_FREQ_HZ = 400;
    const int BLOCK_MS = 20;

    Routing routing;
    routing.input = 0;
    routing.output = 0;
    StubStepStats stats;
    std::vector<IPipelineStep*> steps { new OffsetStep(stepRate, 0, &stats) };
    ParallelStep step(
        inputRate, outputRate, false,
        +[](ParallelStep* s) FREEDV_NONBLOCKING { return ((Routing*)s->getCallbackState())->input; },
        +[](ParallelStep* s) FREEDV_NONBLOCKING { return ((Routing*)s->getCallbackState())->output; },
        steps, nullptr, &routing, nullptr);

    if (step.getInputSampleRate() != inputRate || step.getOutputSampleRate() != outputRate) return false;

    // A second of audio, a block at a time. The caller asks for as much as
    // would be produced from each block.
    int inputBlock = inputRate * BLOCK_MS / 1000;
    std::vector<short> output;
    int sampleNumber = 0;
    for (int block = 0; block < 1000 / BLOCK_MS; block++)
    {
        std::vector<short> input(inputBlock);
        for (auto& sample : input)
        {
            sample = (short)(8000 * std::sin(2 * M_PI * TONE_FREQ_HZ * sampleNumber++ / inputRate));
        }

        int numOutput = 0;
        short* blockOutput = step.execute(input.data(), input.size(), &numOutput);
        output.insert(output.end(), blockOutput, blockOutput + numOutput);
    }

    // Collect anything that's left over.
    short empty = 0;
    int numOutput = 0;
    short* remainder = step.execute(&empty, 0, &numOutput);
    output.insert(output.end(), remainder, remainder + numOutput);

    // The step in the middle should have seen about a second at its own rate...
    if (std::abs(stats.numSamplesSeen - stepRate) > stepRate / 10)
    {
        std::cout << "[step saw " << stats.numSamplesSeen << " samples, expected about " << stepRate << "] ";
        return false;
    }

    // ...and the same goes for what comes out, at the same pitch as what went in.
    int crossings = 0;
    for (size_t i = 1; i < output.size(); i++)
    {
        if ((output[i - 1] < 0) != (output[i] < 0)) crossings++;
    }
    double freqHz = output.empty() ? 0 : crossings / 2.0 / ((double)output.size() / outputRate);
    if (std::abs((int)output.size() - outputRate) > outputRate / 10 || std::fabs(freqHz - TONE_FREQ_HZ) > 10)
    {
        std::cout << "[" << inputRate << " -> " << stepRate << " -> " << outputRate << ": got " << output.size()
                  << " samples of a " << freqHz << " Hz tone, expected about " << outputRate << " of "
                  << TONE_FREQ_HZ << " Hz] ";
        return false;
    }
    return true;
}

bool convertsSampleRatesAroundSteps()
{
    bool result = true;
    result &= convertsSampleRates(16000, 8000, 8000);
    result &= convertsSampleRates(8000, 8000, 16000);
    result &= convertsSampleRates(48000, 8000, 48000);
    result &= convertsSampleRates(8000, 16000, 8000);
    result &= convertsSampleRates(48000, 16000, 8000);
    return result;
}

// Keeps track of which threads were made (and unmade) real-time.
class RecordingRealtimeHelper : public IRealtimeHelper
{
public:
    virtual void setHelperRealTime() override
    {
        std::unique_lock<std::mutex> lk(mtx);
        realTimeThreads.insert(std::this_thread::get_id());
        numSet++;
    }

    virtual void clearHelperRealTime() override
    {
        std::unique_lock<std::mutex> lk(mtx);
        if (realTimeThreads.erase(std::this_thread::get_id()) == 0) numClearedOnWrongThread++;
        numCleared++;
    }

    virtual void startRealTimeWork() override { }
    virtual void stopRealTimeWork(bool) override { }
    virtual bool mustStopWork() FREEDV_NONBLOCKING override { return false; }

    std::mutex mtx;
    std::set<std::thread::id> realTimeThreads;
    int numSet = 0;
    int numCleared = 0;
    int numClearedOnWrongThread = 0;
};

std::unique_ptr<ParallelStep> createStepWithHelper(
    bool multiThreaded, std::shared_ptr<IRealtimeHelper> helper, Routing* routing)
{
    std::vector<IPipelineStep*> steps;
    for (int index = 0; index < NUM_STEPS; index++)
    {
        steps.push_back(new OffsetStep(SAMPLE_RATE, OFFSETS[index]));
    }

    return std::make_unique<ParallelStep>(
        SAMPLE_RATE, SAMPLE_RATE, multiThreaded,
        +[](ParallelStep* s) FREEDV_NONBLOCKING { return ((Routing*)s->getCallbackState())->input; },
        +[](ParallelStep* s) FREEDV_NONBLOCKING { return ((Routing*)s->getCallbackState())->output; },
        steps, nullptr, routing, std::move(helper));
}

bool workerThreadsAreMadeRealTime()
{
    auto helper = std::make_shared<RecordingRealtimeHelper>();
    Routing routing;
    auto step = createStepWithHelper(true, helper, &routing);

    // Each step's thread sets itself up before doing any work.
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        {
            std::unique_lock<std::mutex> lk(helper->mtx);
            if (helper->numSet == NUM_STEPS) break;
        }
        std::this_thread::sleep_for(1ms);
    }

    {
        std::unique_lock<std::mutex> lk(helper->mtx);
        if (helper->numSet != NUM_STEPS || (int)helper->realTimeThreads.size() != NUM_STEPS ||
            helper->realTimeThreads.count(std::this_thread::get_id()) != 0 || helper->numCleared != 0)
        {
            std::cout << "[" << helper->numSet << " thread(s) made real-time, " << helper->numCleared << " reverted] ";
            return false;
        }
    }

    auto input = ramp(FRAME_SIZE);
    int numOutput = 0;
    step->execute(input.data(), input.size(), &numOutput);

    // ...and undoes it, on the same thread, before going away.
    step = nullptr;
    std::unique_lock<std::mutex> lk(helper->mtx);
    if (helper->numCleared != NUM_STEPS || helper->numClearedOnWrongThread != 0 || helper->numSet != NUM_STEPS)
    {
        std::cout << "[" << helper->numCleared << " thread(s) reverted, " << helper->numClearedOnWrongThread
                  << " on the wrong thread] ";
        return false;
    }
    return true;
}

bool callerThreadIsLeftAloneWhenSingleThreaded()
{
    // The caller's already real-time (or not) as it sees fit.
    auto helper = std::make_shared<RecordingRealtimeHelper>();
    Routing routing;
    auto step = createStepWithHelper(false, helper, &routing);

    auto input = ramp(FRAME_SIZE);
    int numOutput = 0;
    step->execute(input.data(), input.size(), &numOutput);
    step = nullptr;

    return helper->numSet == 0 && helper->numCleared == 0;
}

bool multiThreadedShutsDownCleanly()
{
    // Destruction needs to wake up and join each of the threads, whether or
    // not they've ever been given anything to do.
    for (int iteration = 0; iteration < 20; iteration++)
    {
        Fixture f(true);
        if (iteration % 2 == 0)
        {
            f.run(ramp(FRAME_SIZE));
        }
    }
    return true;
}

}

int main()
{
    executeTestCase("accessorsReturnConstructorArguments", accessorsReturnConstructorArguments);
    executeTestCase("routesInputToAllSteps", routesInputToAllSteps);
    executeTestCase("routesInputToSingleStep", routesInputToSingleStep);
    executeTestCase("outputsSelectedStep", outputsSelectedStep);
    executeTestCase("processesInputLargerThanOneFrame", processesInputLargerThanOneFrame);
    executeTestCase("outputIsLimitedToInputSize", outputIsLimitedToInputSize);
    executeTestCase("noOutputWhenSelectedStepHasNone", noOutputWhenSelectedStepHasNone);
    executeTestCase("resetDiscardsPendingAudio", resetDiscardsPendingAudio);
    executeTestCase("ownsSteps", ownsSteps);
    executeTestCase("multiThreadedProducesSameOutput", multiThreadedProducesSameOutput);
    executeTestCase("convertsSampleRatesAroundSteps", convertsSampleRatesAroundSteps);
    executeTestCase("workerThreadsAreMadeRealTime", workerThreadsAreMadeRealTime);
    executeTestCase("callerThreadIsLeftAloneWhenSingleThreaded", callerThreadIsLeftAloneWhenSingleThreaded);
    executeTestCase("multiThreadedShutsDownCleanly", multiThreadedShutsDownCleanly);
    return testResult();
}
