//==========================================================================
// Name:            callsign_decoder.h
//
// Purpose:         Encodes and decodes received callsigns.
// Created:         December 26, 2020
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

#ifndef CALLSIGN_ENCODER_H
#define CALLSIGN_ENCODER_H

#include <deque>
#include "defines.h"

class CallsignEncoder
{
public:
    CallsignEncoder();
    virtual ~CallsignEncoder();

    // TX methods
    void setCallsign(const char* callsign);
    const char* getEncodedCallsign() const { return &truncCallsign_[0]; }
    
    // RX methods
    void clearReceivedText();
    void pushReceivedByte(char byte);
    bool isInSync() const { return textInSync_; }
    bool isCallsignValid() const;
    const char* getReceivedText() const { return &receivedCallsign_[2]; }
    
private:
    std::deque<unsigned char> pendingGolayBytes_;
    bool textInSync_;
    
    char callsign_[MAX_CALLSIGN/2];
    char translatedCallsign_[MAX_CALLSIGN];
    char truncCallsign_[MAX_CALLSIGN];
    
    char receivedCallsign_[MAX_CALLSIGN];
    char* pReceivedCallsign_;
    
    void convert_callsign_to_ota_string_(const char* input, char* output) const;
    void convert_ota_string_to_callsign_(const char* input, char* output);
    
    unsigned char calculateCRC8_(char* input, int length) const;
    void convertDigitToASCII_(char* dest, unsigned char digit);
    unsigned char convertHexStringToDigit_(char* src) const;
};

#endif // CALLSIGN_ENCODER_H