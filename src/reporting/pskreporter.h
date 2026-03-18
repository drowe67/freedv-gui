#if !defined(PSK_REPORTER_H)
#define PSK_REPORTER_H

//=========================================================================
// Name:            pskreporter.h
// Purpose:         Implementation of PSK Reporter support.
//
// Authors:         Mooneer Salem
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================

#include <mutex>
#include "IReporter.h"

struct SenderRecord
{
    std::string callsign;
    uint64_t frequency;
    signed char snr;
    std::string mode;
    char infoSource;
    int flowTimeSeconds;
    
    SenderRecord(std::string callsign, uint64_t frequency, signed char snr);
    
    int recordSize();    
    void encode(char* buf);
};

class PskReporter : public IReporter
{
public:
    PskReporter(std::string callsign, std::string gridSquare, std::string software);
    virtual ~PskReporter() override;
    
    virtual void addReceiveRecord(std::string callsign, std::string mode, uint64_t frequency, signed char snr) override;
    virtual void send() override;
    
    // The below aren't implemented for PSK Reporter.
    virtual void freqChange(uint64_t) override { };
    virtual void transmit(std::string, bool) override { }
    virtual void inAnalogMode(bool) override { }

private:
    unsigned int currentSequenceNumber_;
    unsigned int randomIdentifier_;
    
    std::string receiverCallsign_;
    std::string receiverGridSquare_;
    std::string decodingSoftware_;
    std::vector<SenderRecord> recordList_;
    std::mutex recordListMutex_;
    
    int getRxDataSize_();    
    int getTxDataSize_();    
    void encodeReceiverRecord_(char* buf);    
    void encodeSenderRecords_(char* buf);
    
    bool reportCommon_();
};


#endif // PSK_REPORTER_H
