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

#include "FreeDVReporter.h"

FreeDVReporter::FreeDVReporter(std::string callsign, std::string gridSquare, std::string software)
{
    // Connect and send initial info.
    // TBD - determine final location of site
    sio::message::ptr auth = sio::object_message::create();
    ((sio::object_message*)auth.get())->insert("callsign", callsign);
    ((sio::object_message*)auth.get())->insert("grid_square", gridSquare);
    ((sio::object_message*)auth.get())->insert("version", software);
    sioClient_.connect("http://127.0.0.1:8080", auth);
}

FreeDVReporter::~FreeDVReporter()
{
    // empty
}

void FreeDVReporter::freqChange(uint64_t frequency)
{
    sio::message::ptr freqData = sio::object_message::create();
    ((sio::object_message*)freqData.get())->insert("frequency", sio::int_message::create(frequency));
    sioClient_.socket()->emit("freq_change", freqData);
}

void FreeDVReporter::transmit(std::string mode, bool tx)
{
    sio::message::ptr txData = sio::object_message::create();
    ((sio::object_message*)txData.get())->insert("mode", mode);
    ((sio::object_message*)txData.get())->insert("transmitting", sio::bool_message::create(tx));
    sioClient_.socket()->emit("tx_report", txData);
}
    
void FreeDVReporter::addReceiveRecord(std::string callsign, uint64_t frequency, char snr)
{
    // TBD
}

void FreeDVReporter::send()
{
    // No implementation needed, we send RX/TX reports live.
}