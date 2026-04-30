//=========================================================================
// Name:            IRigFrequencyController.h
// Purpose:         Interface for control of frequency/mode on radios.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
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
//=========================================================================

#ifndef I_RIG_FREQUENCY_CONTROLLER_H
#define I_RIG_FREQUENCY_CONTROLLER_H

#include "IRigController.h"

class IRigFrequencyController : virtual public IRigController
{
public:
    enum Mode
    {
        UNKNOWN,
        
        USB,
        DIGU,
        LSB,
        DIGL,
        FM,
        DIGFM,
        AM,
    };
    
    virtual ~IRigFrequencyController() = default;
    
    // Event handlers that are called by implementers of this interface.
    EventHandler<IRigFrequencyController*, uint64_t, Mode> onFreqModeChange;
    
    virtual void setFrequency(uint64_t frequency) = 0;
    virtual void setMode(Mode mode) = 0;
    virtual void requestCurrentFrequencyMode() = 0;
    
protected:
    IRigFrequencyController() = default;
};

#endif // I_RIG_FREQUENCY_CONTROLLER_H