#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#if defined(WIN32) || defined(MINGW)
#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2def.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif // defined(WIN32) || defined(MINGW)

#include "pskreporter.h"

//#define PSK_REPORTER_TEST
#if defined(PSK_REPORTER_TEST)
// Test server
#define PSK_REPORTER_HOSTNAME "report.pskreporter.info"
#define PSK_REPORTER_PORT "14739"
#else
// Live server
#define PSK_REPORTER_HOSTNAME "report.pskreporter.info"
#define PSK_REPORTER_PORT "4739"
#endif // PSK_REPORTER_TEST

extern int g_verbose;

// RX record:
/* For receiverCallsign, receiverLocator, decodingSoftware use */

static const unsigned char rxFormatHeader[] = {
    0x00, 0x03, 0x00, 0x24, 0x99, 0x92, 0x00, 0x03, 0x00, 0x00, 
    0x80, 0x02, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x04, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F, 
    0x80, 0x08, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F, 
    0x00, 0x00 
};

// TX record:
/* For senderCallsign, frequency, sNR (1 byte), mode, informationSource (1 byte), flowStartSeconds use */

static const unsigned char txFormatHeader[] = {
    0x00, 0x02, 0x00, 0x34, 0x99, 0x93, 0x00, 0x06,
    0x80, 0x01, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x05, 0x00, 0x04, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x0A, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x0B, 0x00, 0x01, 0x00, 0x00, 0x76, 0x8F,
    0x00, 0x96, 0x00, 0x04
};

SenderRecord::SenderRecord(std::string callsign, unsigned int frequency, char snr)
    : callsign(callsign)
    , frequency(frequency)
    , snr(snr)
{
    mode = "FREEDV";
    infoSource = 1;
    flowTimeSeconds = time(0);
} 

int SenderRecord::recordSize()
{
    return (1 + callsign.size()) + 4 + 1 + (1 + mode.size()) + 1 + 4;
}

void SenderRecord::encode(char* buf)
{    
    // Encode callsign
    char* fieldPtr = &buf[0];
    *fieldPtr = (char)callsign.size();
    memcpy(fieldPtr + 1, callsign.c_str(), callsign.size());
    
    // Encode frequency
    fieldPtr += 1 + callsign.size();
    *((unsigned int*)fieldPtr) = htonl(frequency);
    
    // Encode SNR
    fieldPtr += sizeof(unsigned int);
    *fieldPtr = snr;
    
    // Encode mode
    fieldPtr += 1;
    *fieldPtr = (char)mode.size();
    memcpy(fieldPtr + 1, mode.c_str(), mode.size());
    
    // Encode infoSource
    fieldPtr += 1 + mode.size();
    *fieldPtr = infoSource;
    
    // Encode flow start time
    fieldPtr += 1;
    *((unsigned int*)fieldPtr) = htonl(flowTimeSeconds);
}

PskReporter::PskReporter(std::string callsign, std::string gridSquare, std::string software)
    : currentSequenceNumber_(0)
    , receiverCallsign_(callsign)
    , receiverGridSquare_(gridSquare)
    , decodingSoftware_(software)
{
    srand(time(0));
    randomIdentifier_ = rand();
}

PskReporter::~PskReporter()
{
    if (recordList_.size() > 0)
    {
        reportCommon_();
    }
}

void PskReporter::addReceiveRecord(std::string callsign, unsigned int frequency, char snr)
{
    recordList_.push_back(SenderRecord(callsign, frequency, snr));
}

void PskReporter::send()
{
    auto task = std::thread([&]() { reportCommon_(); });
    
    // Allow the reporting to run without needing to wait for it.
    task.detach();
}

int PskReporter::getRxDataSize_()
{
    int size = 4 + (1 + receiverCallsign_.size()) + (1 + receiverGridSquare_.size()) + (1 + decodingSoftware_.size());
    if ((size % 4) > 0)
    {
        // Pad to aligned boundary.
        size += (4 - (size % 4));
    }
    return size;
}

int PskReporter::getTxDataSize_()
{
    if (recordList_.size() == 0)
    {
        return 0;
    }
    
    int size = 4;
    for (auto& item : recordList_)
    {
        size += item.recordSize();
    }
    if ((size % 4) > 0)
    {
        // Pad to aligned boundary.
        size += (4 - (size % 4));
    }
    return size;
}

void PskReporter::encodeReceiverRecord_(char* buf)
{
    // Encode RX record header.
    buf[0] = 0x99;
    buf[1] = 0x92;

    // Encode record size.
    char* fieldLoc = &buf[2];
    *((unsigned short*)fieldLoc) = htons(getRxDataSize_());

    // Encode RX callsign.
    fieldLoc += sizeof(unsigned short);
    *fieldLoc = (char)receiverCallsign_.size();
    memcpy(fieldLoc + 1, receiverCallsign_.c_str(), receiverCallsign_.size());

    // Encode RX locator.
    fieldLoc += 1 + receiverCallsign_.size();
    *fieldLoc = (char)receiverGridSquare_.size();
    memcpy(fieldLoc + 1, receiverGridSquare_.c_str(), receiverGridSquare_.size());

    // Encode RX decoding software.
    fieldLoc += 1 + receiverGridSquare_.size();
    *fieldLoc = (char)decodingSoftware_.size();
    memcpy(fieldLoc + 1, decodingSoftware_.c_str(), decodingSoftware_.size());
}

void PskReporter::encodeSenderRecords_(char* buf)
{
    if (recordList_.size() == 0) return;
    
    // Encode TX record header.
    buf[0] = 0x99;
    buf[1] = 0x93;

    // Encode record size.
    char* fieldLoc = &buf[2];
    *((unsigned short*)fieldLoc) = htons(getTxDataSize_());

    // Encode individual records.
    fieldLoc += sizeof(unsigned short);
    for(auto& rec : recordList_)
    {
        rec.encode(fieldLoc);
        fieldLoc += rec.recordSize();
    }
}

bool PskReporter::reportCommon_()
{
    // Header (2) + length (2) + time (4) + sequence # (4) + random identifier (4) +
    // RX format block + TX format block + RX data + TX data
    int dgSize = 16 + sizeof(rxFormatHeader) + sizeof(txFormatHeader) + getRxDataSize_() + getTxDataSize_();
    if (getTxDataSize_() == 0) dgSize -= sizeof(txFormatHeader);

    char* packet = new char[dgSize];
    memset(packet, 0, dgSize);

    // Encode packet header.
    packet[0] = 0x00;
    packet[1] = 0x0A;

    // Encode datagram size.
    char* fieldLoc = &packet[2];
    *((unsigned short*)fieldLoc) = htons(dgSize);

    // Encode send time.
    fieldLoc += sizeof(unsigned short);
    *((unsigned int*)fieldLoc) = htonl(time(0));

    // Encode sequence number.
    fieldLoc += sizeof(unsigned int);
    *((unsigned int*)fieldLoc) = htonl(currentSequenceNumber_++);

    // Encode random identifier.
    fieldLoc += sizeof(unsigned int);
    *((unsigned int*)fieldLoc) = htonl(randomIdentifier_);

    // Copy RX and TX format headers.
    fieldLoc += sizeof(unsigned int);
    memcpy(fieldLoc, rxFormatHeader, sizeof(rxFormatHeader));
    fieldLoc += sizeof(rxFormatHeader);

    if (getTxDataSize_() > 0)
    {
        memcpy(fieldLoc, txFormatHeader, sizeof(txFormatHeader));
        fieldLoc += sizeof(txFormatHeader);
    }

    // Encode receiver and sender records.
    encodeReceiverRecord_(fieldLoc);
    fieldLoc += getRxDataSize_();
    encodeSenderRecords_(fieldLoc);

    recordList_.clear();

    // Send to PSKReporter.    
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
#ifdef WIN32
    hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;
#else
    hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED | AI_NUMERICSERV;
#endif // WIN32
    struct addrinfo* res = NULL;
    int err = getaddrinfo(PSK_REPORTER_HOSTNAME, PSK_REPORTER_PORT, &hints, &res);
    if (err != 0) {
        if (g_verbose) fprintf(stderr, "cannot resolve %s (err=%d)", PSK_REPORTER_HOSTNAME, err);
        return false;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(fd < 0){
        if (g_verbose) fprintf(stderr, "cannot open PSK Reporter socket (err=%d)\n", errno);
        return false;
    }

    if (sendto(fd, packet, dgSize, 0, res->ai_addr, res->ai_addrlen) < 0){
        delete[] packet;
        if (g_verbose) fprintf(stderr, "cannot send message to PSK Reporter (err=%d)\n", errno);
        close(fd);
        return false;
    }
    delete[] packet;
    close(fd);

    freeaddrinfo(res);
    return true;    
}

#if 0
int main()
{
    PskReporter reporter("K6AQ", "DM12kw", "FreeDV 1.3");
    reporter.addReceiveRecord("N1DQ", 14236000, 5);
    reporter.send();
    return 0;
}
#endif