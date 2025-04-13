//=========================================================================
// Name:            IRigPttController.h
// Purpose:         Interface for control of PTT on radios.
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

#ifndef I_RIG_PTT_CONTROLLER_H
#define I_RIG_PTT_CONTROLLER_H

#include "IRigController.h"

class IRigPttController : virtual public IRigController
{
public:
    virtual ~IRigPttController() = default;
    
    // Event handlers that are called by implementers of this interface.
    EventHandler<IRigPttController*, bool> onPttChange;
    
    virtual void ptt(bool state) = 0;
    
    virtual int getRigResponseTimeMicroseconds() = 0;
    
protected:
    IRigPttController() = default;
};

#endif // I_RIG_PTT_CONTROLLER_H