#ifndef I_REPORTER_H
#define I_REPORTER_H

//=========================================================================
// Name:            IReporter.h
// Purpose:         Public interface for reporting classes.
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

#include <string>

class IReporter
{
public:
    virtual ~IReporter() = default;

    virtual void freqChange(uint64_t frequency) = 0;
    virtual void transmit(std::string mode, bool tx) = 0;
    
    virtual void inAnalogMode(bool inAnalog) = 0;
    
    virtual void addReceiveRecord(std::string callsign, std::string mode, uint64_t frequency, signed char snr) = 0;
    virtual void send() = 0;
};

#endif // I_REPORTER_H