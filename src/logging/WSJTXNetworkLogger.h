//==========================================================================
// Name:            WSJTXNetworkLogger.h
//
// Purpose:         Implements logging using the WSJT-X network protocol
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

#ifndef WSJTX_NETWORK_LOGGER_H
#define WSJTX_NETWORK_LOGGER_H

#include <cassert>
#include <chrono>

#include "ILogger.h"
#include "../util/UdpHandler.h"
#include "../util/ThreadedTimer.h"

class WSJTXNetworkLogger : protected UdpHandler, public ILogger
{
public:
    WSJTXNetworkLogger();
    virtual ~WSJTXNetworkLogger();
    
    virtual void logContact(std::string dxCall, std::string dxGrid, std::string myCall, std::string myGrid, uint64_t freqHz, int snr) override;

protected:
    virtual void onReceive_(const char*, int, char*, int) override { /* not used */ }
    
private:
    static constexpr int HEARTBEAT_INTERVAL_MS = 15000;
    static const std::string UNIQUE_ID;
    static const std::string LOG_MODE;
    static constexpr uint32_t MAX_SCHEMA_VER = 2;
    
    class PacketBuilder
    {
    public:
        PacketBuilder();
        virtual ~PacketBuilder();
        
        template<typename T>
        friend PacketBuilder& operator<<(PacketBuilder& builder, const T& obj);
        
        char* getPacket() const;
        int getPacketSize() const;
    private:
        char* packet_;
        int packetSize_;
        
        char* reallocPacket_(int addSize);
    };
    
    struct jdate_clock
    {
        using rep        = double;
        using period     = std::ratio<86400>;
        using duration   = std::chrono::duration<rep, period>;
        using time_point = std::chrono::time_point<jdate_clock>;

        static constexpr bool is_steady = false;

        static time_point now() noexcept
        {
            using namespace std::chrono;
            using namespace std::chrono_literals;
            return time_point{duration{system_clock::now().time_since_epoch()} + 58574100h};
        }
    };
    
    ThreadedTimer heartbeatTimer_;
    
    void sendHeartbeat_();
};

#endif // WSJTX_NETWORK_LOGGER_H