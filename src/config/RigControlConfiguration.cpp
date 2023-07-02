//==========================================================================
// Name:            RigControlConfiguration.cpp
// Purpose:         Implements the rig control configuration for FreeDV
// Created:         July 2, 2023
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

#include "RigControlConfiguration.h"

RigControlConfiguration::RigControlConfiguration()
    : hamlibUseForPTT("/Hamlib/UseForPTT", false)
    , hamlibEnableFreqModeChanges("/Hamlib/EnableFreqModeChanges", true)
    , hamlibUseAnalogModes("/Hamlib/UseAnalogModes", false)
    , hamlibIcomCIVAddress("/Hamlib/IcomCIVHex", 0)
    , hamlibRigName("/Hamlib/RigNameStr", _(""))
    , hamlibPTTType("/Hamlib/PttType", 0)
    , hamlibSerialRate("/Hamlib/SerialRate", 0)
    , hamlibSerialPort("/Hamlib/SerialPort", _(""))
        
    , useSerialPTT("/Rig/UseSerialPTT", false)
    , serialPTTPort("/Rig/Port", _(""))
    , serialPTTUseRTS("/Rig/UseRTS", true)
    , serialPTTPolarityRTS("/Rig/RTSPolarity", true)
    , serialPTTUseDTR("/Rig/UseDTR", false)
    , serialPTTPolarityDTR("/Rig/DTRPolarity", false)
        
    , useSerialPTTInput("/Rig/UseSerialPTTInput", false)
    , serialPTTInputPort("/Rig/PttInPort", _(""))
    , serialPTTInputPolarityCTS("/Rig/CTSPolarity", false)
{
    // empty
}

void RigControlConfiguration::load(wxConfigBase* config)
{
    load_(config, hamlibUseForPTT);
    load_(config, hamlibEnableFreqModeChanges);
    load_(config, hamlibUseAnalogModes);
    load_(config, hamlibIcomCIVAddress);
    load_(config, hamlibRigName);
    load_(config, hamlibPTTType);
    load_(config, hamlibSerialRate);
    load_(config, hamlibSerialPort);
    
    load_(config, useSerialPTT);
    load_(config, serialPTTPort);
    load_(config, serialPTTUseRTS);
    load_(config, serialPTTPolarityRTS);
    load_(config, serialPTTUseDTR);
    load_(config, serialPTTPolarityDTR);
    
    load_(config, useSerialPTTInput);
    load_(config, serialPTTInputPort);
    load_(config, serialPTTInputPolarityCTS);
}

void RigControlConfiguration::save(wxConfigBase* config)
{
    save_(config, hamlibUseForPTT);
    save_(config, hamlibEnableFreqModeChanges);
    save_(config, hamlibUseAnalogModes);
    save_(config, hamlibIcomCIVAddress);
    save_(config, hamlibRigName);
    save_(config, hamlibPTTType);
    save_(config, hamlibSerialRate);
    save_(config, hamlibSerialPort);
    
    save_(config, useSerialPTT);
    save_(config, serialPTTPort);
    save_(config, serialPTTUseRTS);
    save_(config, serialPTTPolarityRTS);
    save_(config, serialPTTUseDTR);
    save_(config, serialPTTPolarityDTR);
    
    save_(config, useSerialPTTInput);
    save_(config, serialPTTInputPort);
    save_(config, serialPTTInputPolarityCTS);
}