//=========================================================================
// Name:            IRigController.h
// Purpose:         Interface for control of radios.
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

#ifndef I_RIG_CONTROLLER_H
#define I_RIG_CONTROLLER_H

#include <string>
#include "util/EventHandler.h"

class IRigController
{
public:
    virtual ~IRigController() = default;
    
    // Event handlers common across all rig control objects.
    EventHandler<IRigController*, std::string const&> onRigError;
    EventHandler<IRigController*> onRigConnected;
    EventHandler<IRigController*> onRigDisconnected;
    
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;
    
protected:
    IRigController() = default;
};

#endif // I_RIG_CONTROLLER_H
