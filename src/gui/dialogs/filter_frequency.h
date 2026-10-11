//==========================================================================
// Name:            filter_frequency.h
// Purpose:         Band filter frequency enumeration, shared across components
// Created:         April 2026
// Authors:         Mooneer Salem, Barry Jackson
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#ifndef FILTER_FREQUENCY_H
#define FILTER_FREQUENCY_H

#include <cstdint>

enum FilterFrequency
{
    BAND_ALL,
    BAND_160M,
    BAND_80M,
    BAND_60M,
    BAND_40M,
    BAND_30M,
    BAND_20M,
    BAND_17M,
    BAND_15M,
    BAND_12M,
    BAND_10M,
    BAND_VHF_UHF,
    BAND_OTHER,
};

// Returns the band that the given frequency (in Hz) falls into.
inline FilterFrequency GetFilterForFrequency(uint64_t freq)
{
    auto bandForFreq = FilterFrequency::BAND_OTHER;
    
    if (freq >= 1800000 && freq <= 2000000)
    {
        bandForFreq = FilterFrequency::BAND_160M;
    }
    else if (freq >= 3500000 && freq <= 4000000)
    {
        bandForFreq = FilterFrequency::BAND_80M;
    }
    else if (freq >= 5250000 && freq <= 5450000)
    {
        bandForFreq = FilterFrequency::BAND_60M;
    }
    else if (freq >= 7000000 && freq <= 7300000)
    {
        bandForFreq = FilterFrequency::BAND_40M;
    }
    else if (freq >= 10100000 && freq <= 10150000)
    {
        bandForFreq = FilterFrequency::BAND_30M;
    }
    else if (freq >= 14000000 && freq <= 14350000)
    {
        bandForFreq = FilterFrequency::BAND_20M;
    }
    else if (freq >= 18068000 && freq <= 18168000)
    {
        bandForFreq = FilterFrequency::BAND_17M;
    }
    else if (freq >= 21000000 && freq <= 21450000)
    {
        bandForFreq = FilterFrequency::BAND_15M;
    }
    else if (freq >= 24890000 && freq <= 24990000)
    {
        bandForFreq = FilterFrequency::BAND_12M;
    }
    else if (freq >= 28000000 && freq <= 29700000)
    {
        bandForFreq = FilterFrequency::BAND_10M;
    }
    else if (freq >= 50000000)
    {
        bandForFreq = FilterFrequency::BAND_VHF_UHF;
    }
    
    return bandForFreq;
}

#endif // FILTER_FREQUENCY_H
