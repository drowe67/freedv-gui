#include <cmath>
#include <string>

#include "../../defines.h"
#include "UtilStubs.h"
#include "../../util/logging/ulog.h"
#include "../../test/UnitTestCommon.h"

// The parts of util.cpp that don't need the application's windows.

void clickTune(float freq);

namespace {

bool closeTo(const char* description, float actual, float expected)
{
    if (std::fabs(actual - expected) < 0.001f) return true;
    std::cout << "[" << description << " was " << actual << ", expected " << expected << "] ";
    return false;
}

bool offsetsAre(float txOffsetHz, float rxOffsetHz)
{
    return 
        closeTo("TX offset", g_TxFreqOffsetHz.load(), txOffsetHz) && 
        closeTo("RX offset", g_RxFreqOffsetHz.load(), rxOffsetHz);
}

// Clicking on the spectrum tells FreeDV where the signal is. Received audio 
// is shifted so that it ends up where the modem expects it, and transmitted
// audio is shifted the other way so that both stations are on the same frequency.
bool clickTuneShiftsReceiveAndTransmitInOppositeDirections()
{
    bool result = true;

    clickTune(FDMDV_FCENTRE + 300);
    result &= offsetsAre(300, -300);

    clickTune(FDMDV_FCENTRE - 450.5f);
    result &= offsetsAre(-450.5f, 450.5f);

    clickTune(FDMDV_FCENTRE);
    result &= offsetsAre(0, 0);
    return result;
}

bool clickTuneSnapsToCenterWhenClose()
{
    // It's hard to click exactly on the center to get back to no offset at all.
    bool result = true;
    for (float offsetHz : { 9.5f, -9.5f, 1.0f, -0.25f })
    {
        clickTune(FDMDV_FCENTRE + 200);
        clickTune(FDMDV_FCENTRE + offsetHz);
        result &= offsetsAre(0, 0);
    }

    for (float offsetHz : { 10.5f, -10.5f })
    {
        clickTune(FDMDV_FCENTRE + offsetHz);
        result &= offsetsAre(offsetHz, -offsetHz);
    }
    return result;
}

}

int main()
{
    ulog_set_quiet(true);

    executeTestCase("clickTuneShiftsReceiveAndTransmitInOppositeDirections", clickTuneShiftsReceiveAndTransmitInOppositeDirections);
    executeTestCase("clickTuneSnapsToCenterWhenClose", clickTuneSnapsToCenterWhenClose);
    return testResult();
}
