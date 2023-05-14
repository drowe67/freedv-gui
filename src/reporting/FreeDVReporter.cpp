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
    : currFreq_(0)
{
    // Connect and send initial info.
    // TBD - determine final location of site
    sio::message::ptr authPtr = sio::object_message::create();
    auto auth = (sio::object_message*)authPtr.get();
    auth->insert("callsign", callsign);
    auth->insert("grid_square", gridSquare);
    auth->insert("version", software);
    auth->insert("role", "report");
    sioClient_.connect("https://freedv-reporter.k6aq.net/", authPtr);
}

FreeDVReporter::~FreeDVReporter()
{
    // empty
}

void FreeDVReporter::freqChange(uint64_t frequency)
{
    if (currFreq_ != frequency)
    {
        sio::message::ptr freqDataPtr = sio::object_message::create();
        auto freqData = (sio::object_message*)freqDataPtr.get();
        freqData->insert("freq", sio::int_message::create(frequency));
        sioClient_.socket()->emit("freq_change", freqDataPtr);
    }
    currFreq_ = frequency;
}

void FreeDVReporter::transmit(std::string mode, bool tx)
{
    sio::message::ptr txDataPtr = sio::object_message::create();
    auto txData = (sio::object_message*)txDataPtr.get();
    txData->insert("mode", mode);
    txData->insert("transmitting", sio::bool_message::create(tx));
    sioClient_.socket()->emit("tx_report", txDataPtr);
}
    
void FreeDVReporter::addReceiveRecord(std::string callsign, std::string mode, uint64_t frequency, char snr)
{
    sio::message::ptr rxDataPtr = sio::object_message::create();
    auto rxData = (sio::object_message*)rxDataPtr.get();
    
    rxData->insert("callsign", callsign);
    rxData->insert("mode", mode);
    rxData->insert("snr", sio::int_message::create(snr));
    
    sioClient_.socket()->emit("rx_report", rxDataPtr);
}

void FreeDVReporter::send()
{
    // No implementation needed, we send RX/TX reports live.
}
