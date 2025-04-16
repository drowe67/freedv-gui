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
#include "../os/os_interface.h"

FreeDVReporter::FreeDVReporter(std::string hostname, std::string callsign, std::string gridSquare, std::string software, bool rxOnly)
    : isExiting_(false)
    , isConnecting_(false)
    , hostname_(hostname)
    , callsign_(callsign)
    , gridSquare_(gridSquare)
    , software_(software)
    , lastFrequency_(0)
    , tx_(false)
    , rxOnly_(rxOnly)
    , hidden_(false)
{
    // do nothing
}

FreeDVReporter::~FreeDVReporter()
{
    // do nothing
}

void FreeDVReporter::connect()
{    
    // do nothing
}

void FreeDVReporter::requestQSY(std::string sid, uint64_t frequencyHz, std::string message)
{
    // do nothing
}

void FreeDVReporter::updateMessage(std::string message)
{
    // do nothing
}

void FreeDVReporter::inAnalogMode(bool inAnalog)
{
    // do nothing
}

void FreeDVReporter::freqChange(uint64_t frequency)
{
    // do nothing
}

void FreeDVReporter::transmit(std::string mode, bool tx)
{
    // do nothing
}

void FreeDVReporter::hideFromView()
{
    // do nothing
}

void FreeDVReporter::showOurselves()
{
    // do nothing
}
    
void FreeDVReporter::addReceiveRecord(std::string callsign, std::string mode, uint64_t frequency, char snr)
{
    // do nothing
}

void FreeDVReporter::send()
{
    // No implementation needed, we send RX/TX reports live.
}

void FreeDVReporter::setOnReporterConnectFn(ReporterConnectionFn fn)
{
    // do nothing
}

void FreeDVReporter::setOnReporterDisconnectFn(ReporterConnectionFn fn)
{
    // do nothing
}

void FreeDVReporter::setOnUserConnectFn(ConnectionDataFn fn)
{
    // do nothing
}

void FreeDVReporter::setOnUserDisconnectFn(ConnectionDataFn fn)
{
    // do nothing
}

void FreeDVReporter::setOnFrequencyChangeFn(FrequencyChangeFn fn)
{
    // do nothing
}

void FreeDVReporter::setOnTransmitUpdateFn(TxUpdateFn fn)
{
    // do nothing
}

void FreeDVReporter::setOnReceiveUpdateFn(RxUpdateFn fn)
{
    // do nothing
}

void FreeDVReporter::setOnQSYRequestFn(QsyRequestFn fn)
{
    // do nothing
}

void FreeDVReporter::setMessageUpdateFn(MessageUpdateFn fn)
{
    // do nothing
}

void FreeDVReporter::setConnectionSuccessfulFn(ConnectionSuccessfulFn fn)
{
    // do nothing
}

void FreeDVReporter::setAboutToShowSelfFn(AboutToShowSelfFn fn)
{
    // do nothing
}

bool FreeDVReporter::isValidForReporting()
{
    return false;
}
