#include <cstdint>
#include <cstring>
#include <iomanip>
#include <vector>

#include "../WSJTXNetworkLogger.h"
#include "../../test/UnitTestCommon.h"

// WSJT-X network messages are serialized using Qt's QDataStream, which is
// big-endian for all multi-byte values (including floating point). See
// NetworkMessage.hpp in the WSJT-X source code for the message format.

namespace {

using Bytes = std::vector<unsigned char>;

// Every packet begins with the magic number followed by the schema version.
const Bytes HEADER = { 0xad, 0xbc, 0xcb, 0xda, 0x00, 0x00, 0x00, 0x02 };

void printBytes(Bytes const& bytes)
{
    for (auto b : bytes)
    {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    std::cout << std::dec;
}

// Verifies that the packet consists of the header followed by the given payload.
bool packetIs(WSJTXNetworkLogger::PacketBuilder const& builder, Bytes const& expectedPayload)
{
    Bytes expected = HEADER;
    expected.insert(expected.end(), expectedPayload.begin(), expectedPayload.end());

    Bytes actual;
    if (builder.getPacketSize() > 0)
    {
        auto packet = (const unsigned char*)builder.getPacket();
        actual.assign(packet, packet + builder.getPacketSize());
    }

    if (actual != expected)
    {
        std::cout << "[got ";
        printBytes(actual);
        std::cout << ", expected ";
        printBytes(expected);
        std::cout << "] ";
        return false;
    }
    return true;
}

template<typename T>
bool serializesAs(T const& value, Bytes const& expectedPayload)
{
    WSJTXNetworkLogger::PacketBuilder builder;
    builder << value;
    return packetIs(builder, expectedPayload);
}

bool emptyPacketHasHeader()
{
    WSJTXNetworkLogger::PacketBuilder builder;
    return packetIs(builder, {});
}

bool serializeUnsignedIntegers()
{
    bool result = true;
    result &= serializesAs((unsigned char)0xab, { 0xab });
    result &= serializesAs((uint16_t)0x0102, { 0x01, 0x02 });
    result &= serializesAs((uint32_t)0x01020304, { 0x01, 0x02, 0x03, 0x04 });
    result &= serializesAs((uint64_t)14236000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0xd9, 0x39, 0x60 });
    result &= serializesAs((uint64_t)0x0102030405060708ULL, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 });
    return result;
}

bool serializeSignedIntegers()
{
    bool result = true;
    result &= serializesAs((signed char)-2, { 0xfe });
    result &= serializesAs((int16_t)-2, { 0xff, 0xfe });
    result &= serializesAs((int32_t)-2, { 0xff, 0xff, 0xff, 0xfe });
    result &= serializesAs((int32_t)10, { 0x00, 0x00, 0x00, 0x0a });
    result &= serializesAs((int64_t)-2, { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe });
    result &= serializesAs((int64_t)2460959, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x25, 0x8d, 0x1f });
    return result;
}

bool serializeBool()
{
    bool result = true;
    result &= serializesAs(true, { 0x01 });
    result &= serializesAs(false, { 0x00 });
    return result;
}

bool serializeDouble()
{
    bool result = true;
    result &= serializesAs((double)0.0, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });

    // IEEE 754: 1.0 = 0x3ff0000000000000, -2.5 = 0xc004000000000000
    result &= serializesAs((double)1.0, { 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
    result &= serializesAs((double)-2.5, { 0xc0, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
    return result;
}

bool serializeString()
{
    bool result = true;

    // Strings are a 32 bit length followed by the string itself (no null terminator).
    result &= serializesAs(std::string("K6AQ"), { 0x00, 0x00, 0x00, 0x04, 'K', '6', 'A', 'Q' });
    result &= serializesAs(std::string(""), { 0x00, 0x00, 0x00, 0x00 });
    return result;
}

bool serializeMultipleFieldsInOrder()
{
    // Equivalent to the beginning of a heartbeat message.
    WSJTXNetworkLogger::PacketBuilder builder;
    builder << (uint32_t)0 << std::string("FreeDV") << (uint32_t)2 << true;

    return packetIs(builder, {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x06, 'F', 'r', 'e', 'e', 'D', 'V',
        0x00, 0x00, 0x00, 0x02,
        0x01
    });
}

bool packetGrowsWithoutCorruptingEarlierFields()
{
    WSJTXNetworkLogger::PacketBuilder builder;
    Bytes expected;
    for (uint32_t i = 0; i < 1000; i++)
    {
        builder << i;
        expected.push_back((i >> 24) & 0xff);
        expected.push_back((i >> 16) & 0xff);
        expected.push_back((i >> 8) & 0xff);
        expected.push_back(i & 0xff);
    }

    // Avoid dumping 4K of hex on failure.
    return
        builder.getPacketSize() == (int)(HEADER.size() + expected.size()) &&
        memcmp(builder.getPacket() + HEADER.size(), expected.data(), expected.size()) == 0;
}

}

int main()
{
    executeTestCase("emptyPacketHasHeader", emptyPacketHasHeader);
    executeTestCase("serializeUnsignedIntegers", serializeUnsignedIntegers);
    executeTestCase("serializeSignedIntegers", serializeSignedIntegers);
    executeTestCase("serializeBool", serializeBool);
    executeTestCase("serializeDouble", serializeDouble);
    executeTestCase("serializeString", serializeString);
    executeTestCase("serializeMultipleFieldsInOrder", serializeMultipleFieldsInOrder);
    executeTestCase("packetGrowsWithoutCorruptingEarlierFields", packetGrowsWithoutCorruptingEarlierFields);
    return testResult();
}
