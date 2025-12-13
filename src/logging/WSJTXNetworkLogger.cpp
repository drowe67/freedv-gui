//==========================================================================
// Name:            WSJTXNetworkLogger.cpp
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

#include "WSJTXNetworkLogger.h"

#include <cstring>
#include <sstream>

// For htonl etc.
#if defined(WIN32) || defined(__MINGW32__)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif // !_WIN32_WINNT

#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2def.h>
#else
#include <arpa/inet.h>
#endif // defined(WIN32) || defined(__MINGW32__)

const std::string WSJTXNetworkLogger::UNIQUE_ID("FreeDV");
const std::string WSJTXNetworkLogger::LOG_MODE("DIGITALVOICE");

template<>
WSJTXNetworkLogger::PacketBuilder& WSJTXNetworkLogger::PacketBuilder::serialize_<uint32_t>(const uint32_t& obj)
{
    char* ptr = reallocPacket_(sizeof(uint32_t));
    assert(ptr != nullptr);
    
    auto tmp = htonl(obj);
    memcpy(ptr, &tmp, sizeof(uint32_t));

    return *this;
}

template<>
WSJTXNetworkLogger::PacketBuilder& WSJTXNetworkLogger::PacketBuilder::serialize_<signed char>(const signed char& obj)
{
    char* ptr = reallocPacket_(sizeof(signed char));
    assert(ptr != nullptr);
    
    *((signed char*)ptr) = obj;
    return *this;
}

template<>
WSJTXNetworkLogger::PacketBuilder& WSJTXNetworkLogger::PacketBuilder::serialize_<unsigned char>(const unsigned char& obj)
{
    char* ptr = reallocPacket_(sizeof(unsigned char));
    assert(ptr != nullptr);
    
    *((unsigned char*)ptr) = obj;
    return *this;
}

template<>
WSJTXNetworkLogger::PacketBuilder& WSJTXNetworkLogger::PacketBuilder::serialize_<uint16_t>(const uint16_t& obj)
{
    char* ptr = reallocPacket_(sizeof(uint16_t));
    assert(ptr != nullptr);
    
    auto tmp = htons(obj);
    memcpy(ptr, &tmp, sizeof(uint16_t));

    return *this;
}

template<>
WSJTXNetworkLogger::PacketBuilder& WSJTXNetworkLogger::PacketBuilder::serialize_<uint64_t>(const uint64_t& obj)
{
    char* ptr = reallocPacket_(sizeof(uint64_t));
    assert(ptr != nullptr);
    
    auto tmp = __builtin_bswap64(obj);
    memcpy(ptr, &tmp, sizeof(uint64_t));
    
    return *this;
}

template<>
WSJTXNetworkLogger::PacketBuilder& WSJTXNetworkLogger::PacketBuilder::serialize_<int32_t>(const int32_t& obj)
{
    char* ptr = reallocPacket_(sizeof(int32_t));
    assert(ptr != nullptr);
    
    auto tmp = htonl((uint32_t)obj);
    memcpy(ptr, &tmp, sizeof(int32_t));

    return *this;
}

template<>
WSJTXNetworkLogger::PacketBuilder& WSJTXNetworkLogger::PacketBuilder::serialize_<int16_t>(const int16_t& obj)
{
    char* ptr = reallocPacket_(sizeof(int16_t));
    assert(ptr != nullptr);
    
    auto tmp = htons((uint16_t)obj);
    memcpy(ptr, &tmp, sizeof(int16_t));
    
    return *this;
}

template<>
WSJTXNetworkLogger::PacketBuilder& WSJTXNetworkLogger::PacketBuilder::serialize_<int64_t>(const int64_t& obj)
{
    char* ptr = reallocPacket_(sizeof(int64_t));
    assert(ptr != nullptr);
    
    auto tmp = __builtin_bswap64((uint64_t)obj);
    memcpy(ptr, &tmp, sizeof(int64_t));

    return *this;
}

template<>
WSJTXNetworkLogger::PacketBuilder& WSJTXNetworkLogger::PacketBuilder::serialize_<std::string>(const std::string& obj)
{
    char* ptr = reallocPacket_(obj.size() + sizeof(uint32_t));
    assert(ptr != nullptr);
    
    auto tmp = htonl(obj.size());
    memcpy(ptr, &tmp, sizeof(uint32_t));
    memcpy(ptr + sizeof(uint32_t), obj.c_str(), obj.size());
    
    return *this;
}

template<>
WSJTXNetworkLogger::PacketBuilder& WSJTXNetworkLogger::PacketBuilder::serialize_<WSJTXNetworkLogger::jdate_clock::time_point>(const WSJTXNetworkLogger::jdate_clock::time_point& obj)
{
    auto currentTimeAsJulian = obj.time_since_epoch().count();
    auto fracDay = currentTimeAsJulian - ((int64_t)currentTimeAsJulian);
    auto msSinceMidnight = fracDay * (1000*60*60*24);
    
    *this   << (int64_t)currentTimeAsJulian
            << (uint32_t)msSinceMidnight
            << (unsigned char)0 // local time
                ;
    return *this;
}

WSJTXNetworkLogger::WSJTXNetworkLogger()
    : heartbeatTimer_(HEARTBEAT_INTERVAL_MS, [&](ThreadedTimer&) {
        sendHeartbeat_();
    }, true)
{
    // TBD: allow configuration of the below
    open("127.0.0.1", 0);
    heartbeatTimer_.start();
    sendHeartbeat_();
}

WSJTXNetworkLogger::~WSJTXNetworkLogger()
{
    heartbeatTimer_.stop();
    close();
}

void WSJTXNetworkLogger::logContact(std::string dxCall, std::string dxGrid, std::string myCall, std::string myGrid, uint64_t freqHz, int snr)
{
    auto currentTimeAsJulian = jdate_clock::now();

    std::stringstream reportSent;
    reportSent << snr;
    
    PacketBuilder builder;
    builder << (uint32_t)5
            << UNIQUE_ID
            << currentTimeAsJulian // time off
            << dxCall
            << dxGrid
            << freqHz
            << LOG_MODE
            << reportSent.str()
            << std::string("") // report RX
            << std::string("") // TX power
            << std::string("") // comments
            << std::string("") // name
            << currentTimeAsJulian // time on
            << myCall
            << myGrid
            << std::string("") // exchange sent
            << std::string("") // exchange RX
            << std::string("") // ADIF propagation mode
                ;
    
    send("127.0.0.1", 2237, builder.getPacket(), builder.getPacketSize()).wait();
}

void WSJTXNetworkLogger::sendHeartbeat_()
{
    PacketBuilder builder;
    builder << (uint32_t)0;
    builder << UNIQUE_ID;
    builder << MAX_SCHEMA_VER;
    builder << std::string("1.2.3"); // TBD
    builder << std::string(""); // TBD
    
    send("127.0.0.1", 2237, builder.getPacket(), builder.getPacketSize()).wait();
}

WSJTXNetworkLogger::PacketBuilder::PacketBuilder()
    : packet_(nullptr)
    , packetSize_(0)
{
    // Initialize with header
    *this << (uint32_t)0xadbccbda;     // magic number
    *this << MAX_SCHEMA_VER;           // schema version
}

WSJTXNetworkLogger::PacketBuilder::~PacketBuilder()
{
    delete[] packet_;
}

char* WSJTXNetworkLogger::PacketBuilder::getPacket() const
{
    return packet_;
}

int WSJTXNetworkLogger::PacketBuilder::getPacketSize() const
{
    return packetSize_;
}

char* WSJTXNetworkLogger::PacketBuilder::reallocPacket_(int addSize)
{
    char* newPacket = new char[packetSize_ + addSize];
    assert(newPacket != nullptr);
    
    // Copy existing data to new packet.
    memcpy(newPacket, packet_, packetSize_);
    
    char *returnVal = newPacket + packetSize_;
    if (packet_ != nullptr)
    {
        delete[] packet_;
    }
    packetSize_ += addSize;
    packet_ = newPacket;
    return returnVal;
}
