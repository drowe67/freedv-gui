//==========================================================================
// Name:            ILogger.h
//
// Purpose:         Interface that implements logging support
// Created:         December 12, 2025
// Authors:         Mooneer Salem
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#ifndef I_LOGGER_H
#define I_LOGGER_H

#include <string>
#include <chrono>

class ILogger
{
public:
    virtual void logContact(std::chrono::time_point<std::chrono::system_clock> logTime, std::string dxCall, std::string dxGrid, std::string myCall, std::string myGrid, uint64_t freqHz, std::string reportRx, std::string reportTx, std::string name, std::string comments) = 0;
};

#endif // I_LOGGER_H