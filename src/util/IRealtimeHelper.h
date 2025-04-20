//=========================================================================
// Name:            IRealtimeHelper.h
// Purpose:         Defines the interface to a helper for real-time operation.
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


#ifndef I_REALTIME_HELPER_H
#define I_REALTIME_HELPER_H

class IRealtimeHelper
{
public:
    // Configures current thread for real-time priority. This should be
    // called from the thread that will be operating on received audio.
    virtual void setHelperRealTime() = 0;
    
    // Lets audio system know that we're beginning to do work with the
    // received audio.
    virtual void startRealTimeWork() = 0;
    
    // Lets audio system know that we're done with the work on the received
    // audio.
    virtual void stopRealTimeWork() = 0;
    
    // Reverts real-time priority for current thread.
    virtual void clearHelperRealTime() = 0;
};

#endif