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

#endif // FILTER_FREQUENCY_H
