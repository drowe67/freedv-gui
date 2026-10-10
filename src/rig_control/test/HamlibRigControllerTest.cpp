#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <string>
#include <vector>

#include "../HamlibRigController.h"
#include "util/logging/ulog.h"
#include "../../test/UnitTestCommon.h"

using namespace std::chrono_literals;

// Hamlib provides a simulated radio that needs no hardware, which lets us
// exercise the controller all the way through the real library.

// Stands in for the operator: changes things on the radio itself, behind the
// controller's back. This runs on the controller's thread as Hamlib isn't
// meant to be used from more than one thread at a time.
struct HamlibRigControllerTestAccess
{
    static void onRadio(HamlibRigController& controller, std::function<void(RIG*)> const& fn)
    {
        std::promise<void> done;
        controller.enqueue_([&]() {
            fn(controller.rig_.load());
            done.set_value();
        });
        done.get_future().wait();
    }
};

namespace {

const std::string DUMMY_RIG_NAME = "Hamlib Dummy";
constexpr auto TIMEOUT = 5s;

// Records the events raised by a controller. These are raised from the
// controller's own thread, so tests have to wait for them to show up.
struct Fixture
{
    std::mutex mtx;
    std::condition_variable cv;
    int numConnects = 0;
    int numDisconnects = 0;
    std::vector<std::string> errors;
    std::vector<bool> pttChanges;
    std::vector<uint64_t> frequencies;
    std::vector<IRigFrequencyController::Mode> modes;

    // Declared last so that it's destroyed (and its thread stopped) first.
    HamlibRigController controller;

    explicit Fixture(
        std::string const& rigName = DUMMY_RIG_NAME, 
        HamlibRigController::PttType pttType = HamlibRigController::PTT_VIA_CAT,
        bool restoreOnDisconnect = false, bool freqOnly = false, std::string const& serialPort = "")
        : controller(rigName, serialPort, 0, 0, pttType, "", restoreOnDisconnect, freqOnly, false, false)
    {
        controller.onRigConnected += [this](IRigController*) {
            record_([this]() { numConnects++; });
        };
        controller.onRigDisconnected += [this](IRigController*) {
            record_([this]() { numDisconnects++; });
        };
        controller.onRigError += [this](IRigController*, std::string const& err) {
            record_([&]() { errors.push_back(err); });
        };
        controller.onPttChange += [this](IRigPttController*, bool state) {
            record_([&]() { pttChanges.push_back(state); });
        };
        controller.onFreqModeChange += [this](IRigFrequencyController*, uint64_t freq, IRigFrequencyController::Mode mode) {
            record_([&]() { 
                frequencies.push_back(freq); 
                modes.push_back(mode);
            });
        };
    }

    // Waits until the given condition (evaluated with the lock held) is true.
    bool waitFor(std::function<bool()> const& condition)
    {
        std::unique_lock<std::mutex> lk(mtx);
        return cv.wait_for(lk, TIMEOUT, condition);
    }

    bool connect()
    {
        controller.connect();
        return waitFor([this]() { return numConnects == 1 || !errors.empty(); }) && errors.empty();
    }

    // Asks the radio for its frequency/mode until it reports the expected ones.
    bool radioReports(uint64_t freq, IRigFrequencyController::Mode mode)
    {
        controller.requestCurrentFrequencyMode();
        bool result = waitFor([&]() { 
            return !frequencies.empty() && frequencies.back() == freq && modes.back() == mode; 
        });
        if (!result)
        {
            std::unique_lock<std::mutex> lk(mtx);
            std::cout << "[radio last reported ";
            if (frequencies.empty()) std::cout << "nothing";
            else std::cout << frequencies.back() << " Hz, mode " << modes.back();
            std::cout << "; expected " << freq << " Hz, mode " << mode << "] ";
        }
        return result;
    }

    // Waits for everything asked of the controller so far to be dealt with.
    void waitUntilIdle()
    {
        HamlibRigControllerTestAccess::onRadio(controller, [](RIG*) { });
    }

    void onRadio(std::function<void(RIG*)> const& fn)
    {
        HamlibRigControllerTestAccess::onRadio(controller, fn);
    }

    size_t numReports()
    {
        std::unique_lock<std::mutex> lk(mtx);
        return frequencies.size();
    }

private:
    void record_(std::function<void()> const& fn)
    {
        {
            std::unique_lock<std::mutex> lk(mtx);
            fn();
        }
        cv.notify_all();
    }
};

bool rigListIsPopulated()
{
    return 
        HamlibRigController::GetNumberSupportedRadios() > 0 &&
        HamlibRigController::RigNameToIndex(DUMMY_RIG_NAME) >= 0;
}

bool rigNamesMapBackToThemselves()
{
    int numRadios = HamlibRigController::GetNumberSupportedRadios();
    for (int index = 0; index < numRadios; index++)
    {
        // Note: names aren't guaranteed to be unique, so the index found may
        // be that of an earlier radio with the same name.
        auto name = HamlibRigController::RigIndexToName(index);
        int foundIndex = HamlibRigController::RigNameToIndex(name);
        if (name.empty() || foundIndex < 0 || foundIndex > index || 
            HamlibRigController::RigIndexToName(foundIndex) != name)
        {
            std::cout << "[index " << index << " (\"" << name << "\") maps to index " << foundIndex << "] ";
            return false;
        }
    }
    return true;
}

bool rigNameLookupIgnoresTrailingWhitespace()
{
    // Older configuration files may have these.
    int expected = HamlibRigController::RigNameToIndex(DUMMY_RIG_NAME);
    return 
        HamlibRigController::RigNameToIndex(DUMMY_RIG_NAME + " ") == expected &&
        HamlibRigController::RigNameToIndex(DUMMY_RIG_NAME + " \t\r\n") == expected;
}

bool unknownRigsAreReported()
{
    int numRadios = HamlibRigController::GetNumberSupportedRadios();
    return 
        HamlibRigController::RigNameToIndex("Not A Real Radio") == -1 &&
        HamlibRigController::RigNameToIndex("") == -1 &&
        HamlibRigController::RigIndexToName(numRadios) == "" &&
        HamlibRigController::RigIndexToName(numRadios + 1000) == "";
}

bool baudRateRangesAreSane()
{
    int numRadios = HamlibRigController::GetNumberSupportedRadios();
    for (int index = 0; index < numRadios; index++)
    {
        int minRate = HamlibRigController::GetMinimumSerialBaudRate(index);
        int maxRate = HamlibRigController::GetMaximumSerialBaudRate(index);
        if (minRate < 0 || maxRate < minRate)
        {
            std::cout << "[" << HamlibRigController::RigIndexToName(index) << ": " << minRate << " to " << maxRate << "] ";
            return false;
        }
    }
    return true;
}

bool canBeConstructedFromRigIndex()
{
    int index = HamlibRigController::RigNameToIndex(DUMMY_RIG_NAME);
    HamlibRigController controller(index, "", 0, 0, HamlibRigController::PTT_VIA_CAT, "", false, false, false, false);

    std::mutex mtx;
    std::condition_variable cv;
    bool connected = false;
    controller.onRigConnected += [&](IRigController*) {
        std::unique_lock<std::mutex> lk(mtx);
        connected = true;
        cv.notify_all();
    };
    controller.connect();

    std::unique_lock<std::mutex> lk(mtx);
    return cv.wait_for(lk, TIMEOUT, [&]() { return connected; });
}

bool connectsAndReportsCurrentFrequency()
{
    Fixture f;
    if (f.controller.isConnected()) return false;

    return 
        f.connect() && 
        f.controller.isConnected() &&
        f.waitFor([&]() { return !f.frequencies.empty() && f.frequencies.back() > 0; });
}

bool disconnects()
{
    Fixture f;
    if (!f.connect()) return false;

    f.controller.disconnect();
    return f.waitFor([&]() { return f.numDisconnects == 1; }) && !f.controller.isConnected();
}

bool canReconnectAfterDisconnecting()
{
    Fixture f;
    if (!f.connect()) return false;

    f.controller.disconnect();
    f.controller.connect();
    return 
        f.waitFor([&]() { return f.numConnects == 2; }) && 
        f.numDisconnects == 1 && f.controller.isConnected() && f.errors.empty();
}

bool reportsErrorForUnknownRig()
{
    Fixture f("Not A Real Radio");
    f.controller.connect();
    return 
        f.waitFor([&]() { return !f.errors.empty(); }) && 
        f.errors[0].find("Not A Real Radio") != std::string::npos &&
        !f.controller.isConnected() && f.numConnects == 0;
}

bool reportsErrorWhenAlreadyConnected()
{
    Fixture f;
    if (!f.connect()) return false;

    f.controller.connect();
    return 
        f.waitFor([&]() { return !f.errors.empty(); }) && 
        f.controller.isConnected() && f.numConnects == 1;
}

bool setsFrequencyAndMode()
{
    Fixture f;
    if (!f.connect()) return false;

    f.controller.setFrequency(14236000);
    f.controller.setMode(IRigFrequencyController::DIGU);
    if (!f.radioReports(14236000, IRigFrequencyController::DIGU)) return false;

    f.controller.setFrequency(7177000);
    f.controller.setMode(IRigFrequencyController::LSB);
    return f.radioReports(7177000, IRigFrequencyController::LSB) && f.errors.empty();
}

bool setsEachSupportedMode()
{
    Fixture f;
    if (!f.connect()) return false;
    f.controller.setFrequency(14236000);

    for (auto mode : { 
        IRigFrequencyController::USB, IRigFrequencyController::DIGU, 
        IRigFrequencyController::LSB, IRigFrequencyController::DIGL, 
        IRigFrequencyController::FM, IRigFrequencyController::DIGFM,
        IRigFrequencyController::AM })
    {
        f.controller.setMode(mode);
        if (!f.radioReports(14236000, mode)) return false;
    }
    return f.errors.empty();
}

// A frequency and/or mode requested before the radio's connected is meant to 
// be applied once it is.
bool appliesModeSetBeforeConnecting()
{
    Fixture f;
    f.controller.setMode(IRigFrequencyController::DIGL);
    if (!f.connect()) return false;

    f.controller.requestCurrentFrequencyMode();
    return f.waitFor([&]() { return !f.modes.empty() && f.modes.back() == IRigFrequencyController::DIGL; });
}

bool appliesFrequencySetBeforeConnecting()
{
    Fixture f;
    f.controller.setFrequency(14236000);
    if (!f.connect()) return false;

    f.controller.requestCurrentFrequencyMode();
    bool result = f.waitFor([&]() { return !f.frequencies.empty() && f.frequencies.back() == 14236000; });
    if (!result)
    {
        std::unique_lock<std::mutex> lk(f.mtx);
        std::cout << "[radio is on " << (f.frequencies.empty() ? 0 : f.frequencies.back()) << " Hz] ";
    }
    return result;
}

bool reportsPttChanges(HamlibRigController::PttType pttType)
{
    Fixture f(DUMMY_RIG_NAME, pttType);
    if (!f.connect()) return false;

    f.controller.ptt(true);
    if (!f.waitFor([&]() { return f.pttChanges.size() == 1; }) || !f.pttChanges[0]) return false;

    // Asking for the same state again isn't a change.
    f.controller.ptt(true);
    f.controller.ptt(false);
    return 
        f.waitFor([&]() { return f.pttChanges.size() >= 2; }) && 
        f.pttChanges == std::vector<bool> { true, false } && 
        f.controller.getRigResponseTimeMicroseconds() >= 0 &&
        f.errors.empty();
}

bool doesNotQueryRadioWhileTransmitting()
{
    // Some radios glitch their transmit audio (or don't respond at all) if
    // asked for their frequency during TX.
    Fixture f;
    if (!f.connect()) return false;
    f.controller.setFrequency(14236000);
    f.controller.setMode(IRigFrequencyController::USB);
    if (!f.radioReports(14236000, IRigFrequencyController::USB)) return false;

    f.controller.ptt(true);
    if (!f.waitFor([&]() { return f.pttChanges.size() == 1; })) return false;

    size_t numReports;
    {
        std::unique_lock<std::mutex> lk(f.mtx);
        numReports = f.frequencies.size();
    }
    f.controller.requestCurrentFrequencyMode();
    f.controller.requestCurrentFrequencyMode();

    // Requests are handled in order, so once PTT is seen to drop we know
    // that the above requests have been dealt with. Dropping PTT itself
    // triggers one query.
    f.controller.ptt(false);
    if (!f.waitFor([&]() { return f.pttChanges.size() == 2 && f.frequencies.size() > numReports; })) return false;

    std::unique_lock<std::mutex> lk(f.mtx);
    return f.frequencies.size() == numReports + 1;
}

bool changesFrequencyWhileTransmitting()
{
    Fixture f;
    if (!f.connect()) return false;
    f.controller.setFrequency(14236000);
    f.controller.setMode(IRigFrequencyController::USB);
    if (!f.radioReports(14236000, IRigFrequencyController::USB)) return false;

    f.controller.ptt(true);
    f.controller.setFrequency(14240000);
    f.controller.ptt(false);

    // PTT should be seen to change exactly twice (i.e. the controller briefly
    // unkeying the radio to change frequency isn't reported).
    return 
        f.radioReports(14240000, IRigFrequencyController::USB) &&
        f.pttChanges == std::vector<bool> { true, false } &&
        f.errors.empty();
}

// Checks what happens to the radio when FreeDV is done with it. The radio's
// last report before the connection closes tells us how it was left.
bool restoreOnDisconnect(bool restore, bool freqOnly)
{
    Fixture f(DUMMY_RIG_NAME, HamlibRigController::PTT_VIA_CAT, restore, freqOnly);
    if (!f.connect() || !f.waitFor([&]() { return !f.frequencies.empty(); })) return false;

    uint64_t origFreq;
    IRigFrequencyController::Mode origMode;
    {
        std::unique_lock<std::mutex> lk(f.mtx);
        origFreq = f.frequencies[0];
        origMode = f.modes[0];
    }

    // Something other than what the radio starts out on.
    const uint64_t NEW_FREQ = 14236000;
    auto newMode = origMode == IRigFrequencyController::DIGU ? IRigFrequencyController::LSB : IRigFrequencyController::DIGU;
    if (origFreq == NEW_FREQ || origMode == IRigFrequencyController::UNKNOWN)
    {
        std::cout << "[simulated radio starts on " << origFreq << " Hz, mode " << origMode << "] ";
        return false;
    }

    f.controller.setFrequency(NEW_FREQ);
    f.controller.setMode(newMode);
    if (!f.radioReports(NEW_FREQ, newMode)) return false;

    f.controller.disconnect();
    if (!f.waitFor([&]() { return f.numDisconnects == 1; })) return false;

    uint64_t expectedFreq = restore ? origFreq : NEW_FREQ;
    auto expectedMode = (restore && !freqOnly) ? origMode : newMode;

    std::unique_lock<std::mutex> lk(f.mtx);
    if (f.frequencies.back() != expectedFreq || f.modes.back() != expectedMode)
    {
        std::cout << "[radio left on " << f.frequencies.back() << " Hz, mode " << f.modes.back()
                  << "; expected " << expectedFreq << " Hz, mode " << expectedMode << "] ";
        return false;
    }
    return f.errors.empty();
}

bool restoresOriginalSettingsEachTimeItConnects()
{
    Fixture f(DUMMY_RIG_NAME, HamlibRigController::PTT_VIA_CAT, true, false);
    if (!f.connect() || !f.waitFor([&]() { return !f.frequencies.empty(); })) return false;
    uint64_t origFreq = f.frequencies[0];

    for (uint64_t freq : { (uint64_t)14236000, (uint64_t)7177000 })
    {
        f.controller.setFrequency(freq);
        f.controller.requestCurrentFrequencyMode();
        if (!f.waitFor([&]() { return f.frequencies.back() == freq; })) return false;

        int disconnectsSoFar = f.numDisconnects;
        f.controller.disconnect();
        if (!f.waitFor([&]() { return f.numDisconnects == disconnectsSoFar + 1; })) return false;
        {
            std::unique_lock<std::mutex> lk(f.mtx);
            if (f.frequencies.back() != origFreq)
            {
                std::cout << "[radio left on " << f.frequencies.back() << " Hz, expected " << origFreq << "] ";
                return false;
            }
        }

        f.controller.connect();
        if (!f.waitFor([&]() { return f.numConnects == disconnectsSoFar + 2; })) return false;
        f.waitUntilIdle();
    }

    // Reconnecting doesn't reapply anything from the previous connection.
    std::unique_lock<std::mutex> lk(f.mtx);
    if (!f.errors.empty())
    {
        std::cout << "[error: " << f.errors[0] << "] ";
        return false;
    }
    return f.frequencies.back() == origFreq;
}

bool reportsErrorWhenRadioCannotBeOpened()
{
    // Any radio that's controlled over a serial port will do.
    std::string rigName;
    int numRadios = HamlibRigController::GetNumberSupportedRadios();
    for (int index = 0; index < numRadios && rigName.empty(); index++)
    {
        auto name = HamlibRigController::RigIndexToName(index);
        if (name.find("FT-817") != std::string::npos) rigName = name;
    }
    if (rigName.empty())
    {
        std::cout << "[no suitable radio in Hamlib's list] ";
        return false;
    }

    Fixture f(rigName, HamlibRigController::PTT_VIA_CAT, false, false, "/nonexistent/serial/port");
    f.controller.connect();
    if (!f.waitFor([&]() { return !f.errors.empty(); }))
    {
        std::cout << "[no error reported] ";
        return false;
    }

    f.waitUntilIdle();
    if (f.controller.isConnected() || f.numConnects != 0)
    {
        std::cout << "[reported as connected] ";
        return false;
    }

    // Nothing's left behind that would stop the user from trying again
    // (e.g. after plugging the radio in), and requests in the meantime are harmless.
    f.controller.ptt(true);
    f.controller.setFrequency(14236000);
    f.controller.requestCurrentFrequencyMode();
    f.controller.connect();
    if (!f.waitFor([&]() { return f.errors.size() >= 2; })) return false;
    f.waitUntilIdle();

    std::unique_lock<std::mutex> lk(f.mtx);
    for (auto& err : f.errors)
    {
        if (err.find("Already connected") != std::string::npos)
        {
            std::cout << "[failed connection was left open] ";
            return false;
        }
    }
    return !f.controller.isConnected() && f.numConnects == 0 && f.numDisconnects == 0 && f.pttChanges.empty();
}

bool reportsUnknownForOtherModes()
{
    Fixture f;
    if (!f.connect()) return false;
    f.controller.setFrequency(14236000);
    f.controller.setMode(IRigFrequencyController::USB);
    if (!f.radioReports(14236000, IRigFrequencyController::USB)) return false;

    // The operator switches to something FreeDV has no use for.
    f.onRadio([](RIG* rig) { rig_set_mode(rig, RIG_VFO_CURR, RIG_MODE_CW, RIG_PASSBAND_NOCHANGE); });
    if (!f.radioReports(14236000, IRigFrequencyController::UNKNOWN)) return false;

    // FreeDV can still put it back.
    f.controller.setMode(IRigFrequencyController::USB);
    return f.radioReports(14236000, IRigFrequencyController::USB) && f.errors.empty();
}

bool ignoresRequestForCurrentFrequency()
{
    Fixture f;
    if (!f.connect()) return false;
    f.controller.setFrequency(14236000);
    f.controller.setMode(IRigFrequencyController::USB);
    if (!f.radioReports(14236000, IRigFrequencyController::USB)) return false;
    f.waitUntilIdle();

    // Setting a frequency normally results in the radio being asked to confirm it.
    size_t numReports = f.numReports();
    f.controller.setFrequency(14236000);
    f.controller.setMode(IRigFrequencyController::USB);
    f.waitUntilIdle();
    if (f.numReports() != numReports)
    {
        std::cout << "[radio was sent a frequency/mode it was already on] ";
        return false;
    }

    f.controller.setFrequency(14240000);
    f.waitUntilIdle();
    return f.numReports() > numReports && f.radioReports(14240000, IRigFrequencyController::USB);
}

bool followsFrequencyChangedOnRadio()
{
    Fixture f;
    if (!f.connect()) return false;
    f.controller.setFrequency(14236000);
    f.controller.setMode(IRigFrequencyController::USB);
    if (!f.radioReports(14236000, IRigFrequencyController::USB)) return false;

    // The operator turns the dial...
    f.onRadio([](RIG* rig) { rig_set_freq(rig, RIG_VFO_CURR, 14240000); });
    if (!f.radioReports(14240000, IRigFrequencyController::USB)) return false;

    // ...so going back to the earlier frequency is a real change again.
    f.controller.setFrequency(14236000);
    return f.radioReports(14236000, IRigFrequencyController::USB) && f.errors.empty();
}

bool usesWhicheverVfoIsSelected()
{
    Fixture f;
    if (!f.connect()) return false;
    f.controller.setFrequency(14236000);
    f.controller.setMode(IRigFrequencyController::USB);
    if (!f.radioReports(14236000, IRigFrequencyController::USB)) return false;

    int result = RIG_OK;
    f.onRadio([&](RIG* rig) { result = rig_set_vfo(rig, RIG_VFO_B); });
    if (result != RIG_OK)
    {
        std::cout << "[simulated radio would not select VFO B] ";
        return false;
    }

    f.controller.setFrequency(7177000);
    f.controller.setMode(IRigFrequencyController::LSB);
    if (!f.radioReports(7177000, IRigFrequencyController::LSB)) return false;

    // VFO A should be as it was.
    freq_t freqA = 0;
    freq_t freqB = 0;
    rmode_t modeA = RIG_MODE_NONE;
    vfo_t vfo = RIG_VFO_NONE;
    f.onRadio([&](RIG* rig) {
        pbwidth_t width;
        rig_get_vfo(rig, &vfo);
        rig_get_freq(rig, RIG_VFO_A, &freqA);
        rig_get_freq(rig, RIG_VFO_B, &freqB);
        rig_get_mode(rig, RIG_VFO_A, &modeA, &width);
    });
    if (vfo != RIG_VFO_B || freqA != 14236000 || freqB != 7177000 || modeA != RIG_MODE_USB)
    {
        std::cout << "[VFO " << rig_strvfo(vfo) << " selected, A = " << (uint64_t)freqA << " Hz ("
                  << rig_strrmode(modeA) << "), B = " << (uint64_t)freqB << " Hz] ";
        return false;
    }
    return f.errors.empty();
}

bool leavesMemoryChannelToChangeFrequency()
{
    Fixture f;
    if (!f.connect()) return false;

    // Note: Hamlib reports an error here as it can't read back the memory
    // channel's frequency, but the simulated radio does switch over.
    vfo_t vfoBefore = RIG_VFO_NONE;
    f.onRadio([&](RIG* rig) {
        rig_set_vfo(rig, RIG_VFO_MEM);
        rig_get_vfo(rig, &vfoBefore);
    });
    if (vfoBefore != RIG_VFO_MEM)
    {
        std::cout << "[simulated radio would not select a memory channel] ";
        return false;
    }

    // A memory channel's frequency can't be changed, so the radio is switched to VFO A first.
    f.controller.setFrequency(14236000);
    f.controller.setMode(IRigFrequencyController::DIGU);
    if (!f.radioReports(14236000, IRigFrequencyController::DIGU)) return false;

    freq_t freqA = 0;
    vfo_t vfo = RIG_VFO_NONE;
    f.onRadio([&](RIG* rig) {
        rig_get_vfo(rig, &vfo);
        rig_get_freq(rig, RIG_VFO_A, &freqA);
    });
    if (vfo != RIG_VFO_A || freqA != 14236000)
    {
        std::cout << "[VFO " << rig_strvfo(vfo) << " selected, A = " << (uint64_t)freqA << " Hz] ";
        return false;
    }
    return f.errors.empty();
}

bool ignoresRequestsWhenNotConnected()
{
    Fixture f;
    f.controller.ptt(true);
    f.controller.requestCurrentFrequencyMode();

    // Requests are handled in order, so by the time the connect attempt is
    // done, we know that the above have been dealt with.
    return f.connect() && f.pttChanges.empty();
}

bool destroysCleanlyWhileConnected()
{
    for (int i = 0; i < 2; i++)
    {
        Fixture f;
        if (!f.connect()) return false;
        if (i % 2 == 0) f.controller.ptt(true);
    }
    return true;
}

}

int main()
{
    // Hamlib's own debug output is routed through the logger and is very verbose.
    ulog_set_quiet(true);

    executeTestCase("rigListIsPopulated", rigListIsPopulated);
    executeTestCase("rigNamesMapBackToThemselves", rigNamesMapBackToThemselves);
    executeTestCase("rigNameLookupIgnoresTrailingWhitespace", rigNameLookupIgnoresTrailingWhitespace);
    executeTestCase("unknownRigsAreReported", unknownRigsAreReported);
    executeTestCase("baudRateRangesAreSane", baudRateRangesAreSane);
    executeTestCase("canBeConstructedFromRigIndex", canBeConstructedFromRigIndex);
    executeTestCase("connectsAndReportsCurrentFrequency", connectsAndReportsCurrentFrequency);
    executeTestCase("disconnects", disconnects);
    executeTestCase("canReconnectAfterDisconnecting", canReconnectAfterDisconnecting);
    executeTestCase("reportsErrorForUnknownRig", reportsErrorForUnknownRig);
    executeTestCase("reportsErrorWhenAlreadyConnected", reportsErrorWhenAlreadyConnected);
    executeTestCase("setsFrequencyAndMode", setsFrequencyAndMode);
    executeTestCase("setsEachSupportedMode", setsEachSupportedMode);
    executeTestCase("appliesModeSetBeforeConnecting", appliesModeSetBeforeConnecting);
    executeTestCase("appliesFrequencySetBeforeConnecting", appliesFrequencySetBeforeConnecting);
    executeTestCase("reportsPttChanges (CAT)", []() { return reportsPttChanges(HamlibRigController::PTT_VIA_CAT); });
    executeTestCase("reportsPttChanges (CAT data)", []() { return reportsPttChanges(HamlibRigController::PTT_VIA_CAT_DATA); });
    executeTestCase("reportsPttChanges (none)", []() { return reportsPttChanges(HamlibRigController::PTT_VIA_NONE); });
    executeTestCase("doesNotQueryRadioWhileTransmitting", doesNotQueryRadioWhileTransmitting);
    executeTestCase("changesFrequencyWhileTransmitting", changesFrequencyWhileTransmitting);
    executeTestCase("leavesRadioAsIsOnDisconnect", []() { return restoreOnDisconnect(false, false); });
    executeTestCase("restoresFrequencyAndModeOnDisconnect", []() { return restoreOnDisconnect(true, false); });
    executeTestCase("restoresOnlyFrequencyOnDisconnect", []() { return restoreOnDisconnect(true, true); });
    executeTestCase("restoresOriginalSettingsEachTimeItConnects", restoresOriginalSettingsEachTimeItConnects);
    executeTestCase("reportsErrorWhenRadioCannotBeOpened", reportsErrorWhenRadioCannotBeOpened);
    executeTestCase("reportsUnknownForOtherModes", reportsUnknownForOtherModes);
    executeTestCase("ignoresRequestForCurrentFrequency", ignoresRequestForCurrentFrequency);
    executeTestCase("followsFrequencyChangedOnRadio", followsFrequencyChangedOnRadio);
    executeTestCase("usesWhicheverVfoIsSelected", usesWhicheverVfoIsSelected);
    executeTestCase("leavesMemoryChannelToChangeFrequency", leavesMemoryChannelToChangeFrequency);
    executeTestCase("ignoresRequestsWhenNotConnected", ignoresRequestsWhenNotConnected);
    executeTestCase("destroysCleanlyWhileConnected", destroysCleanlyWhileConnected);
    return testResult();
}
