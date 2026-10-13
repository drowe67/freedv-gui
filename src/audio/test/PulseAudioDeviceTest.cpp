#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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
};

namespace {

// A PulseAudio server address that nothing could be listening at. Connecting
// to one fails immediately, as it does when there's no sound server at all,
// and without PulseAudio trying to start one in the background.
const char* const NO_SOUND_SERVER = "unix:/nonexistent/freedv-test-pulse-socket";

const char* const MISSING_DEVICE = "FreeDVTestMissingDevice";

void useNoSoundServer()
{
    setenv("PULSE_SERVER", NO_SOUND_SERVER, 1);
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
    executeTestCase("xrunsAreReportedToTheDeviceTheyBelongTo", xrunsAreReportedToTheDeviceTheyBelongTo);

    return testResult();
}
