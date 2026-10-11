#include "../../dialogs/filter_frequency.h"
#include "../../../test/UnitTestCommon.h"

// FreeDV Reporter can show only the stations on the band the user is on,
// which depends on working out the band from a frequency.

namespace {

bool bandIs(uint64_t frequency, FilterFrequency expected)
{
    auto actual = GetFilterForFrequency(frequency);
    if (actual == expected) return true;
    std::cout << "[" << frequency << " Hz gave band " << actual << ", expected " << expected << "] ";
    return false;
}

// Both edges belong to the band; the hertz either side of them doesn't.
bool bandCovers(FilterFrequency band, uint64_t lowest, uint64_t highest)
{
    bool result = true;
    result &= bandIs(lowest - 1, BAND_OTHER);
    result &= bandIs(lowest, band);
    result &= bandIs((lowest + highest) / 2, band);
    result &= bandIs(highest, band);
    result &= bandIs(highest + 1, BAND_OTHER);
    return result;
}

bool hfBandsHaveExpectedEdges()
{
    bool result = true;
    result &= bandCovers(BAND_160M, 1800000, 2000000);
    result &= bandCovers(BAND_80M, 3500000, 4000000);
    result &= bandCovers(BAND_60M, 5250000, 5450000);
    result &= bandCovers(BAND_40M, 7000000, 7300000);
    result &= bandCovers(BAND_30M, 10100000, 10150000);
    result &= bandCovers(BAND_20M, 14000000, 14350000);
    result &= bandCovers(BAND_17M, 18068000, 18168000);
    result &= bandCovers(BAND_15M, 21000000, 21450000);
    result &= bandCovers(BAND_12M, 24890000, 24990000);
    result &= bandCovers(BAND_10M, 28000000, 29700000);
    return result;
}

bool everythingFromSixMetersUpIsVhfUhf()
{
    bool result = true;
    result &= bandIs(49999999, BAND_OTHER);
    result &= bandIs(50000000, BAND_VHF_UHF);
    result &= bandIs(144200000, BAND_VHF_UHF);
    result &= bandIs(432100000, BAND_VHF_UHF);
    result &= bandIs(10368000000ULL, BAND_VHF_UHF);
    return result;
}

bool frequenciesOutsideAmateurBandsAreOther()
{
    bool result = true;
    result &= bandIs(0, BAND_OTHER);
    result &= bandIs(1000000, BAND_OTHER);  // medium wave broadcast
    result &= bandIs(9500000, BAND_OTHER);  // short wave broadcast
    result &= bandIs(27185000, BAND_OTHER); // CB
    return result;
}

bool noFrequencyMapsToAllBands()
{
    // "All" is a choice of filter, not a band a station can be on.
    bool result = true;
    for (uint64_t frequency = 0; frequency <= 60000000; frequency += 12500)
    {
        if (GetFilterForFrequency(frequency) == BAND_ALL)
        {
            std::cout << "[" << frequency << " Hz gave the \"all bands\" filter] ";
            result = false;
            break;
        }
    }
    return result;
}

} // namespace

int main()
{
    executeTestCase("hfBandsHaveExpectedEdges", hfBandsHaveExpectedEdges);
    executeTestCase("everythingFromSixMetersUpIsVhfUhf", everythingFromSixMetersUpIsVhfUhf);
    executeTestCase("frequenciesOutsideAmateurBandsAreOther", frequenciesOutsideAmateurBandsAreOther);
    executeTestCase("noFrequencyMapsToAllBands", noFrequencyMapsToAllBands);

    return testResult();
}
