//==========================================================================
// Name:            FrequencyOps.h
// Purpose:         Common code to handle frequency/mode related operations.
// Created:         May 13, 2026
// Authors:         Mooneer Salem
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

#ifndef FREQUENCY_OPS_H
#define FREQUENCY_OPS_H

#include "../../rig_control/HamlibRigController.h"

inline HamlibRigController::Mode GetModeForFrequency(uint64_t frequency, bool useAnalogModes, bool inAnalog)
{
    if (!inAnalog)
    {
        // Always DIGU or USB if Analog decode isn't active.
        return (useAnalogModes) ? HamlibRigController::USB : HamlibRigController::DIGU;
    }

    // Widest 60 meter allocation is 5.250-5.450 MHz per https://en.wikipedia.org/wiki/60-meter_band.
    bool is60MeterBand = 
        frequency >= 5250000 && 
        frequency <= 5450000;
    
    HamlibRigController::Mode lsbMode = HamlibRigController::LSB;
    HamlibRigController::Mode usbMode = HamlibRigController::USB;
    
    HamlibRigController::Mode newMode;
    if (frequency < 10000000 &&
        !is60MeterBand)
    {
        newMode = lsbMode;
    }
    else
    {
        newMode = usbMode;
    }

    return newMode;
}

#endif // FREQUENCY_OPS_H