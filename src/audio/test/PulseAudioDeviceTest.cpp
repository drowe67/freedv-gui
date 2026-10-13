#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <semaphore.h>

#include <wx/init.h>

#include "../PulseAudioDevice.h"
#include "../PulseAudioEngine.h"
#include "../../test/UnitTestCommon.h"
#include "../../util/logging/ulog.h"

// PulseAudioDevice is only created by PulseAudioEngine, and its stream
// callbacks are called by PulseAudio on a stream's thread. Both are needed
// to test either.
struct PulseAudioDeviceTestAccess
{
    static std::unique_ptr<PulseAudioDevice> create(
        wxString const& devName, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels)
    {
        return std::unique_ptr<PulseAudioDevice>(
            new PulseAudioDevice(devName, direction, sampleRate, numChannels));
    }

    static void underflow(IAudioDevice& device)
    {
        PulseAudioDevice::StreamUnderflowCallback_(nullptr, &device);
    }

    static void overflow(IAudioDevice& device)
    {
        PulseAudioDevice::StreamOverflowCallback_(nullptr, &device);
    }

    // What starting a device does, once its stream is connected, for the
    // thread that works to the stream's timing to wait on.
    static void prepareToPaceWork(PulseAudioDevice& device)
    {
        sem_init(&device.sem_, 0, 0);
    }

    static void finishedPacingWork(PulseAudioDevice& device)
    {
        sem_destroy(&device.sem_);
    }
};

namespace {

// A PulseAudio server address that nothing could be listening at. Connecting
// to one fails immediately, as it does when there's no sound server at all,
// and without PulseAudio trying to start one in the background.
const char* const NO_SOUND_SERVER = "unix:/nonexistent/freedv-test-pulse-socket";

const char* const MISSING_DEVICE = "FreeDVTestMissingDevice";

// Where this was told to find the sound server when it was run, if anywhere.
const char* const originalSoundServer = getenv("PULSE_SERVER") != nullptr ? strdup(getenv("PULSE_SERVER")) : nullptr;

void useNoSoundServer()
{
    setenv("PULSE_SERVER", NO_SOUND_SERVER, 1);
}

// The one that FreeDV itself would use, if there is one.
void useUsualSoundServer()
{
    if (originalSoundServer != nullptr)
    {
        setenv("PULSE_SERVER", originalSoundServer, 1);
    }
    else
    {
        unsetenv("PULSE_SERVER");
    }
}

// What PulseAudioEngine::start() and PulseAudioDevice::start() report when
// there's no sound server to connect to.
bool saysThereIsNoSoundServer(std::string const& error)
{
    return error.find("Could not connect PulseAudio context") != std::string::npos;
}

void printErrors(std::vector<std::string> const& errors)
{
    for (auto& error : errors)
    {
        std::cout << "[\"" << error << "\"] ";
    }
}

// What the engine or a device reported to the application. The state is
// where it's recorded, as it is for FreeDV's own handlers.
void recordEngineError(IAudioEngine&, std::string const& error, void* state)
{
    static_cast<std::vector<std::string>*>(state)->push_back(error);
}

void recordDeviceError(IAudioDevice&, std::string const& error, void* state)
{
    static_cast<std::vector<std::string>*>(state)->push_back(error);
}

// The engine says that it couldn't connect, rather than leaving the
// application to find out when nothing it asks for comes back.
bool engineStartSaysThereIsNoSoundServer()
{
    useNoSoundServer();

    PulseAudioEngine engine;
    std::vector<std::string> reported;
    engine.setOnEngineError(&recordEngineError, &reported);

    engine.start();
    engine.stop();

    if (reported.size() != 1 || !saysThereIsNoSoundServer(reported[0]))
    {
        std::cout << "[start() reported " << reported.size() << " error(s) ";
        printErrors(reported);
        std::cout << "] ";
        return false;
    }
    return true;
}

// With no sound server to ask, there's nothing to list, no default device,
// and nothing to open. FreeDV's settings and startup rely on all three being
// empty rather than on there being something there.
bool engineWithoutASoundServerHasNoDevices()
{
    useNoSoundServer();

    PulseAudioEngine engine;
    engine.start();

    std::vector<std::string> problems;
    for (auto direction : {IAudioEngine::AUDIO_ENGINE_IN, IAudioEngine::AUDIO_ENGINE_OUT})
    {
        auto devices = engine.getAudioDeviceList(direction);
        if (!devices.empty())
        {
            problems.push_back("listed " + std::to_string(devices.size()) + " device(s)");
        }
        if (engine.getDefaultAudioDevice(direction).isValid())
        {
            problems.push_back("returned a default device");
        }
        if (engine.getAudioDevice(MISSING_DEVICE, direction, 48000, 2) != nullptr)
        {
            problems.push_back("returned a device that isn't there");
        }
        if (!engine.getSupportedSampleRates(MISSING_DEVICE, direction).empty())
        {
            problems.push_back("returned sample rates for a device that isn't there");
        }
    }

    engine.stop();

    if (!problems.empty())
    {
        std::cout << "[";
        for (size_t index = 0; index < problems.size(); index++)
        {
            std::cout << (index ? ", " : "") << problems[index];
        }
        std::cout << "] ";
        return false;
    }
    return true;
}

// A device that couldn't connect reports the failure along with the name of
// the device it was for, which is what FreeDV shows the user.
bool deviceStartReportsTheFailureAndWhichDeviceItWas()
{
    useNoSoundServer();

    auto device = PulseAudioDeviceTestAccess::create(MISSING_DEVICE, IAudioEngine::AUDIO_ENGINE_OUT, 48000, 2);

    std::vector<std::string> reported;
    device->setOnAudioError(&recordDeviceError, &reported);

    device->start();
    device->stop();

    bool namedTheDevice = reported.size() == 1 && saysThereIsNoSoundServer(reported[0]) &&
        reported[0].find(std::string("(") + MISSING_DEVICE + ")") != std::string::npos;
    if (!namedTheDevice)
    {
        std::cout << "[start() reported " << reported.size() << " error(s) ";
        printErrors(reported);
        std::cout << "] ";
    }
    return namedTheDevice;
}

// What a device reports when it's started with a sound server running (the
// one that FreeDV would use), or nothing if there isn't one to try it with.
bool startAgainstSoundServer(const char* deviceName, IAudioEngine::AudioDirection direction, int sampleRate,
                             int numChannels, std::vector<std::string>& reported)
{
    useUsualSoundServer();

    auto device = PulseAudioDeviceTestAccess::create(deviceName, direction, sampleRate, numChannels);
    device->setOnAudioError(&recordDeviceError, &reported);
    device->start();
    device->stop();

    if (reported.size() == 1 && saysThereIsNoSoundServer(reported[0]))
    {
        std::cout << "[no sound server to try this with] ";
        reported.clear();
        useNoSoundServer();
        return false;
    }

    // It copes with having nobody to report to as well, and is left with
    // nothing running either way.
    auto unheard = PulseAudioDeviceTestAccess::create(deviceName, direction, sampleRate, numChannels);
    unheard->start();
    unheard->stop();

    useNoSoundServer();
    if (device->isRunning() || unheard->isRunning())
    {
        reported.push_back("still running after stop()");
    }
    return true;
}

bool reportedOnly(std::vector<std::string> const& reported, std::string const& expected)
{
    bool matches = reported.size() == 1 && reported[0] == expected;
    if (!matches)
    {
        std::cout << "[start() reported " << reported.size() << " error(s) ";
        printErrors(reported);
        std::cout << "] ";
    }
    return matches;
}

// The sound server won't make a stream of something that has no channels.
// That's reported rather than left for whatever uses the stream to find out.
bool streamThatCannotBeCreatedIsReported()
{
    std::vector<std::string> reported;
    if (!startAgainstSoundServer("FreeDVTestRadio", IAudioEngine::AUDIO_ENGINE_OUT, 48000, 0, reported))
    {
        return reported.empty();
    }
    return reportedOnly(reported, "Could not create PulseAudio stream for FreeDVTestRadio");
}

// A device whose start() failed is left with nothing to stop: FreeDV stops
// and frees every device it created when one of them fails, including the
// ones that never started. No error callback is registered here either, as
// the device must cope with that too.
bool failedDeviceStartLeavesNothingToStop()
{
    useNoSoundServer();

    auto device = PulseAudioDeviceTestAccess::create(MISSING_DEVICE, IAudioEngine::AUDIO_ENGINE_IN, 8000, 1);

    device->start();
    bool wasRunning = device->isRunning();
    device->stop();
    bool runningAfterStop = device->isRunning();

    // And again, as FreeDV's cleanup and the destructor both do.
    device->stop();

    if (wasRunning || runningAfterStop)
    {
        std::cout << "[running after start(): " << wasRunning << ", after stop(): " << runningAfterStop << "] ";
        return false;
    }
    return true;
}

// What a stream of one's own reported to the application.
struct XrunReport
{
    IAudioDevice* device = nullptr;
    void* state = nullptr;
    int numTimes = 0;
};

// Records what a device reported, in the state registered with its handler:
// how FreeDV's own handlers get to the counters they're for.
void recordXrun(IAudioDevice& device, void* state)
{
    XrunReport* report = static_cast<XrunReport*>(state);
    report->device = &device;
    report->state = state;
    report->numTimes++;
}

// An underflow or overflow is reported to the handler registered for that
// device, with the state registered with it, and not to any other device's:
// FreeDV counts these separately for what it sends to the radio and what it
// plays, so a callback reaching the wrong one counts against the wrong side.
bool xrunsAreReportedToTheDeviceTheyBelongTo()
{
    auto mic = PulseAudioDeviceTestAccess::create("FreeDVTestMic", IAudioEngine::AUDIO_ENGINE_IN, 48000, 1);
    auto radio = PulseAudioDeviceTestAccess::create("FreeDVTestRadio", IAudioEngine::AUDIO_ENGINE_OUT, 48000, 2);

    XrunReport micUnderflow, micOverflow, radioUnderflow, radioOverflow;

    mic->setOnAudioUnderflow(&recordXrun, &micUnderflow);
    mic->setOnAudioOverflow(&recordXrun, &micOverflow);
    radio->setOnAudioUnderflow(&recordXrun, &radioUnderflow);
    radio->setOnAudioOverflow(&recordXrun, &radioOverflow);

    PulseAudioDeviceTestAccess::underflow(*mic);
    PulseAudioDeviceTestAccess::overflow(*mic);

    auto expected = [](XrunReport const& report, IAudioDevice* device, void* state)
    {
        return report.numTimes == 1 && report.device == device && report.state == state;
    };

    bool reported =
        expected(micUnderflow, mic.get(), &micUnderflow) &&
        expected(micOverflow, mic.get(), &micOverflow) &&
        radioUnderflow.numTimes == 0 &&
        radioOverflow.numTimes == 0;

    if (!reported)
    {
        std::cout << "[mic underflow: " << micUnderflow.numTimes << ", mic overflow: " << micOverflow.numTimes
                  << ", radio underflow: " << radioUnderflow.numTimes << ", radio overflow: " << radioOverflow.numTimes
                  << "] ";
    }
    return reported;
}

// A device that nobody's asked to hear about underflows and overflows from
// (the microphone and speaker tests in Settings, for one) has nothing to call
// when they happen.
bool xrunsWithNobodyListeningAreIgnored()
{
    auto device = PulseAudioDeviceTestAccess::create("FreeDVTestSpeaker", IAudioEngine::AUDIO_ENGINE_OUT, 48000, 2);
    PulseAudioDeviceTestAccess::underflow(*device);
    PulseAudioDeviceTestAccess::overflow(*device);

    // The same once a handler has been taken away again.
    XrunReport report;
    device->setOnAudioUnderflow(&recordXrun, &report);
    device->setOnAudioUnderflow(nullptr, nullptr);
    PulseAudioDeviceTestAccess::underflow(*device);
    return report.numTimes == 0;
}

// How long one round of work takes, from starting it to having waited for
// the next: the work itself (if any), then stopRealTimeWork().
double millisecondsForRoundOfWork(PulseAudioDevice& device, int millisecondsWorking, bool fastMode = false)
{
    auto begin = std::chrono::steady_clock::now();
    device.startRealTimeWork();
    if (millisecondsWorking > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(millisecondsWorking));
    }
    device.stopRealTimeWork(fastMode);
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
}

// The thread that feeds a device does a round of work every 20 ms, the
// latency FreeDV asks PulseAudio for (10 ms when it's catching up): however
// long the work took, the wait afterwards makes the round up to that.
bool workIsPacedToTheStreamsLatency()
{
    auto device = PulseAudioDeviceTestAccess::create("FreeDVTestRadio", IAudioEngine::AUDIO_ENGINE_OUT, 48000, 2);
    PulseAudioDeviceTestAccess::prepareToPaceWork(*device);

    double idle = millisecondsForRoundOfWork(*device, 0);
    double busy = millisecondsForRoundOfWork(*device, 5);
    double fast = millisecondsForRoundOfWork(*device, 0, true);

    PulseAudioDeviceTestAccess::finishedPacingWork(*device);

    // Generous at the top end: the machine may be busy with other things.
    // At the bottom end it's the rounds so far that are checked rather than
    // each by itself, as a wait that ends late is made up for by shortening
    // the one after.
    bool paced = idle >= 19 && idle < 200 && idle + busy >= 39 && busy < 200 && idle + busy + fast >= 49 && fast < 200;
    if (!paced)
    {
        std::cout << "[idle: " << idle << " ms, busy: " << busy << " ms, fast: " << fast << " ms] ";
    }
    return paced;
}

// Work that overruns isn't waited after, and what it overran by comes out
// of the next wait too, so that the thread catches up rather than falling
// further and further behind what the device needs.
bool overrunningWorkIsNotWaitedAfter()
{
    auto device = PulseAudioDeviceTestAccess::create("FreeDVTestRadio", IAudioEngine::AUDIO_ENGINE_OUT, 48000, 2);
    PulseAudioDeviceTestAccess::prepareToPaceWork(*device);

    double overrun = millisecondsForRoundOfWork(*device, 60);   // 40 ms over
    double catchingUp = millisecondsForRoundOfWork(*device, 0); // no wait at all
    double caughtUp = millisecondsForRoundOfWork(*device, 0);   // back to normal

    PulseAudioDeviceTestAccess::finishedPacingWork(*device);

    bool caught = overrun >= 60 && overrun < 75 + 100 && catchingUp < 10 && caughtUp >= 19 && caughtUp < 200;
    if (!caught)
    {
        std::cout << "[overrun: " << overrun << " ms, catching up: " << catchingUp << " ms, caught up: " << caughtUp
                  << " ms] ";
    }
    return caught;
}

// The same goes for work that's held up for seconds, as when the machine
// is too busy to run it: the next round isn't made to wait even longer.
bool workHeldUpForSecondsIsNotWaitedAfter()
{
    auto device = PulseAudioDeviceTestAccess::create("FreeDVTestRadio", IAudioEngine::AUDIO_ENGINE_OUT, 48000, 2);
    PulseAudioDeviceTestAccess::prepareToPaceWork(*device);

    millisecondsForRoundOfWork(*device, 2300);
    double catchingUp = millisecondsForRoundOfWork(*device, 0);
    double caughtUp = millisecondsForRoundOfWork(*device, 0);

    PulseAudioDeviceTestAccess::finishedPacingWork(*device);

    bool caught = catchingUp < 10 && caughtUp >= 19 && caughtUp < 200;
    if (!caught)
    {
        std::cout << "[catching up: " << catchingUp << " ms, caught up: " << caughtUp << " ms] ";
    }
    return caught;
}

} // namespace

int main()
{
    wxInitializer initializer;
    if (!initializer.IsOk())
    {
        std::cout << "Could not initialize wxWidgets" << std::endl;
        return -1;
    }

    ulog_set_quiet(true);

    executeTestCase("engineStartSaysThereIsNoSoundServer", engineStartSaysThereIsNoSoundServer);
    executeTestCase("engineWithoutASoundServerHasNoDevices", engineWithoutASoundServerHasNoDevices);
    executeTestCase("deviceStartReportsTheFailureAndWhichDeviceItWas", deviceStartReportsTheFailureAndWhichDeviceItWas);
    executeTestCase("failedDeviceStartLeavesNothingToStop", failedDeviceStartLeavesNothingToStop);
    executeTestCase("streamThatCannotBeCreatedIsReported", streamThatCannotBeCreatedIsReported);
    executeTestCase("xrunsAreReportedToTheDeviceTheyBelongTo", xrunsAreReportedToTheDeviceTheyBelongTo);
    executeTestCase("xrunsWithNobodyListeningAreIgnored", xrunsWithNobodyListeningAreIgnored);
    executeTestCase("workIsPacedToTheStreamsLatency", workIsPacedToTheStreamsLatency);
    executeTestCase("overrunningWorkIsNotWaitedAfter", overrunningWorkIsNotWaitedAfter);
    executeTestCase("workHeldUpForSecondsIsNotWaitedAfter", workHeldUpForSecondsIsNotWaitedAfter);

    return testResult();
}
