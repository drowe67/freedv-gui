#include "../FrequencyOps.h"
#include "../../../test/UnitTestCommon.h"

// By convention, voice on the amateur bands uses lower sideband below 10 MHz
// and upper sideband above, with 60 meters being the one exception.

namespace {

using Mode = HamlibRigController::Mode;

bool modeIs(uint64_t frequency, bool useAnalog, Mode expected)
{
    Mode actual = GetModeForFrequency(frequency, useAnalog);
    if (actual != expected)
    {
        std::cout << "[" << frequency << " Hz (" << (useAnalog ? "analog" : "digital") << "): got mode " 
                  << actual << ", expected " << expected << "] ";
        return false;
    }
    return true;
}

// Checks both the digital and analog variants.
bool sidebandIs(uint64_t frequency, bool upper)
{
    bool result = true;
    result &= modeIs(frequency, false, upper ? HamlibRigController::DIGU : HamlibRigController::DIGL);
    result &= modeIs(frequency, true, upper ? HamlibRigController::USB : HamlibRigController::LSB);
    return result;
}

bool lowerSidebandBelow10MHz()
{
    bool result = true;
    result &= sidebandIs(1997000, false); // 160m
    result &= sidebandIs(3625000, false); // 80m
    result &= sidebandIs(7177000, false); // 40m
    result &= sidebandIs(9999999, false);
    return result;
}

bool upperSidebandFrom10MHz()
{
    bool result = true;
    result &= sidebandIs(10000000, true);
    result &= sidebandIs(10136000, true);  // 30m
    result &= sidebandIs(14236000, true);  // 20m
    result &= sidebandIs(28330000, true);  // 10m
    result &= sidebandIs(50313000, true);  // 6m
    result &= sidebandIs(144200000, true); // 2m
    return result;
}

bool upperSidebandOn60Meters()
{
    bool result = true;
    result &= sidebandIs(5250000, true); // bottom edge
    result &= sidebandIs(5403500, true);
    result &= sidebandIs(5450000, true); // top edge
    return result;
}

bool lowerSidebandJustOutside60Meters()
{
    bool result = true;
    result &= sidebandIs(5249999, false);
    result &= sidebandIs(5450001, false);
    return result;
}

}

int main()
{
    executeTestCase("lowerSidebandBelow10MHz", lowerSidebandBelow10MHz);
    executeTestCase("upperSidebandFrom10MHz", upperSidebandFrom10MHz);
    executeTestCase("upperSidebandOn60Meters", upperSidebandOn60Meters);
    executeTestCase("lowerSidebandJustOutside60Meters", lowerSidebandJustOutside60Meters);
    return testResult();
}
