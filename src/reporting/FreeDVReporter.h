#ifndef FREEDV_REPORTER_H
#define FREEDV_REPORTER_H

//=========================================================================
// Name:            FreeDVReporter.h
// Purpose:         Implementation of interface to freedv-reporter
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
#include "sio_client.h"
#include "IReporter.h"

class FreeDVReporter : public IReporter
{
public:
    FreeDVReporter(std::string callsign, std::string gridSquare, std::string software);
    virtual ~FreeDVReporter();

    virtual void freqChange(uint64_t frequency) override;
    virtual void transmit(std::string mode, bool tx) override;
        
    virtual void addReceiveRecord(std::string callsign, uint64_t frequency, char snr) override;
    virtual void send() override;
    
private:
    sio::client sioClient_;
};

#endif // FREEDV_REPORTER_H