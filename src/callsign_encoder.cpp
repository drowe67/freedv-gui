//==========================================================================
// Name:            callsign_decoder.cpp
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

#include <cstdlib>
#include <cstring>
#include "callsign_encoder.h"

extern "C" {
    extern int  golay23_encode(int data);
    extern int  golay23_decode(int received_codeword);
}

CallsignEncoder::CallsignEncoder()
{
    memset(&translatedCallsign_, 0, MAX_CALLSIGN);
    memset(&truncCallsign_, 0, MAX_CALLSIGN);
    memset(&callsign_, 0, MAX_CALLSIGN/2);
    clearReceivedText();
}

CallsignEncoder::~CallsignEncoder()
{
    // empty
}

void CallsignEncoder::setCallsign(const char* callsign)
{
    memset(&translatedCallsign_, 0, MAX_CALLSIGN);
    memset(&truncCallsign_, 0, MAX_CALLSIGN);
    memset(&callsign_, 0, MAX_CALLSIGN/2);
    
    memcpy(&callsign_, callsign, strlen(callsign) + 1);
    convert_callsign_to_ota_string_(callsign_, &translatedCallsign_[2]);
    
    unsigned char crc = calculateCRC8_((char*)&translatedCallsign_[2], strlen(&translatedCallsign_[2]));
    unsigned char crcDigit1 = crc >> 4;
    unsigned char crcDigit2 = crc & 0xF;
    convertDigitToASCII_(&translatedCallsign_[0], crcDigit1);
    convertDigitToASCII_(&translatedCallsign_[1], crcDigit2);
    
    int truncIndex = 0;
    for(int index = 0; index < strlen(translatedCallsign_); index += 2, truncIndex += 4)
    {
        // Encode the character as four bytes with parity bits.
        int inputRaw = ((translatedCallsign_[index] & 0x3F) << 6) | (translatedCallsign_[index+1] & 0x3F);
        unsigned int outputEncoding = golay23_encode(inputRaw) & 0x7FFFFF;
        truncCallsign_[truncIndex] = (unsigned int)(outputEncoding >> 18) & 0x3F;
        truncCallsign_[truncIndex + 1] = (unsigned int)(outputEncoding >> 12) & 0x3F;
        truncCallsign_[truncIndex + 2] = (unsigned int)(outputEncoding >> 6) & 0x3F;
        truncCallsign_[truncIndex + 3] = (unsigned int)outputEncoding & 0x3F;
    }
}

void CallsignEncoder::clearReceivedText()
{
    memset(&receivedCallsign_, 0, MAX_CALLSIGN);
    pReceivedCallsign_ = &receivedCallsign_[0];
    textInSync_ = false;
    pendingGolayBytes_.clear();
}

void CallsignEncoder::pushReceivedByte(char incomingChar)
{
    // If we're not in sync, we should look for a space to establish sync.
    pendingGolayBytes_.push_back(incomingChar);
    if (!textInSync_)
    {
        if (pendingGolayBytes_.size() >= 4)
        {
            // Minimum number of characters received to begin attempting sync.
            unsigned int encodedInput =
                (pendingGolayBytes_[pendingGolayBytes_.size() - 4] << 18) |
                (pendingGolayBytes_[pendingGolayBytes_.size() - 3] << 12) |
                (pendingGolayBytes_[pendingGolayBytes_.size() - 2] << 6) |
                pendingGolayBytes_[pendingGolayBytes_.size() - 1];
            
            encodedInput &= 0x7FFFFF;                
            int rawOutput = golay23_decode(encodedInput) >> 11;
            char rawStr[3];
            char decodedStr[3];
            
            rawStr[0] = ((unsigned int)rawOutput) >> 6;
            rawStr[1] = ((unsigned int)rawOutput) & 0x3F;
            rawStr[2] = 0;
            
            convert_ota_string_to_callsign_(rawStr, decodedStr);
            if (decodedStr[0] == 0x7F && decodedStr[1] == 0x7F)
            {
                // We're now in sync. Pop off the non-aligned bytes we received at the beginning
                // (if any) and give us the chance to decode the remaining ones.
                textInSync_ = true;
                fprintf(stderr, "text now in sync\n");
                while ((pendingGolayBytes_.size() % 4) > 0)
                {
                    pendingGolayBytes_.pop_front();
                }
            }
        }
    }
    else
    {
        while (pendingGolayBytes_.size() >= 4)
        {
            // Minimum number of characters received.
            int encodedInput =
                (pendingGolayBytes_[0] << 18) |
                (pendingGolayBytes_[1] << 12) |
                (pendingGolayBytes_[2] << 6) |
                pendingGolayBytes_[3];
            encodedInput &= 0x7FFFFF;
                            
            int rawOutput = golay23_decode(encodedInput) >> 11;
            pendingGolayBytes_.pop_front();
            pendingGolayBytes_.pop_front();
            pendingGolayBytes_.pop_front();
            pendingGolayBytes_.pop_front();
            char rawStr[3];
            char decodedStr[3];
            
            rawStr[0] = ((unsigned int)rawOutput) >> 6;
            rawStr[1] = ((unsigned int)rawOutput) & 0x3F;
            rawStr[2] = 0;
                        
            convert_ota_string_to_callsign_(rawStr, decodedStr);

            if (decodedStr[0] == '\r' || ((pReceivedCallsign_ - &receivedCallsign_[0]) > MAX_CALLSIGN-1))
            {                        
                // CR completes line
                *pReceivedCallsign_ = 0;
                pReceivedCallsign_ = &receivedCallsign_[0];
            }
            else if (decodedStr[0] != '\0' && decodedStr[0] != 0x7F)
            {
                // Ignore nulls and sync characters.
                *pReceivedCallsign_++ = decodedStr[0];
            }
            
            if (decodedStr[1] == '\r' || ((pReceivedCallsign_ - &receivedCallsign_[0]) > MAX_CALLSIGN-1))
            {
                // CR completes line
                *pReceivedCallsign_ = 0;
                pReceivedCallsign_ = &receivedCallsign_[0];
            }
            else if (decodedStr[1] != '\0' && decodedStr[1] != 0x7F)
            {
                // Ignore nulls and sync characters.
                *pReceivedCallsign_++ = decodedStr[1];
            }
        }
    }
}

bool CallsignEncoder::isCallsignValid() const
{
    if (strlen(receivedCallsign_) <= 2)
    {
        return false;
    }
    
    // Retrieve received CRC and calculate the CRC from the other received text.
    unsigned char receivedCRC = convertHexStringToDigit_((char*)&receivedCallsign_[0]);
    
    char buf[MAX_CALLSIGN];
    memset(&buf, 0, MAX_CALLSIGN);
    convert_callsign_to_ota_string_(&receivedCallsign_[2], &buf[0]);
    unsigned char calcCRC = calculateCRC8_((char*)&buf, strlen(&buf[0]));
    
    // Return true if both are equal.
    return receivedCRC == calcCRC;
}

// 6 bit character set for text field use:
// 0: ASCII null
// 1-9: ASCII 38-47
// 10-19: ASCII '0'-'9'
// 20-46: ASCII 'A'-'Z'
// 47: ASCII ' '
// 48: ASCII '\r'
// 48-62: TBD/for future use.
// 63: sync (2x in a 2 byte block indicates sync)
void CallsignEncoder::convert_callsign_to_ota_string_(const char* input, char* output) const
{
    int outidx = 0;
    for (int index = 0; index < strlen(input); index++)
    {
        bool addSync = false;
        if (input[index] >= 38 && input[index] <= 47)
        {
            output[outidx++] = input[index] - 37;
        }
        else if (input[index] >= '0' && input[index] <= '9')
        {
            output[outidx++] = input[index] - '0' + 10;
        }
        else if (input[index] >= 'A' && input[index] <= 'Z')
        {
            output[outidx++] = input[index] - 'A' + 20;
        }
        else if (input[index] >= 'a' && input[index] <= 'z')
        {
            output[outidx++] = toupper(input[index]) - 'A' + 20;
        }
        else if (input[index] == '\r')
        {
            output[outidx++] = 48;
            addSync = true;
        }
        else
        {
            // Invalid characters become spaces. We also add up to three sync
            // characters (63) to the end depending on the current length.
            output[outidx++] = 47;
            addSync = true;
        }
        
        if (addSync)
        {
            if (outidx % 2)
            {
                output[outidx++] = 63;
            }
            output[outidx++] = 63;
            output[outidx++] = 63;
        }
    }
    output[outidx] = 0;
}

void CallsignEncoder::convert_ota_string_to_callsign_(const char* input, char* output)
{
    int outidx = 0;
    for (int index = 0; index < strlen(input); index++)
    {
        if (input[index] >= 1 && input[index] <= 9)
        {
            output[outidx++] = input[index] + 37;
        }
        else if (input[index] >= 10 && input[index] <= 19)
        {
            output[outidx++] = input[index] - 10 + '0';
        }
        else if (input[index] >= 20 && input[index] <= 46)
        {
            output[outidx++] = input[index] - 20 + 'A';
        }
        else if (input[index] == 48)
        {
            output[outidx++] = '\r';
        }
        else if (input[index] == 63)
        {
            // Use ASCII 0x7F to signify sync. The caller will need to strip this out.
            output[outidx++] = 0x7F;
        }
        else
        {
            // Invalid characters become spaces. 
            output[outidx++] = ' ';
        }
    }
    output[outidx] = 0;
}

unsigned char CallsignEncoder::calculateCRC8_(char* input, int length) const
{
    unsigned char generator = 0x1D;
    unsigned char crc = 0; /* start with 0 so first byte can be 'xored' in */

    while (length > 0)
    {
        unsigned char ch = *input++;
        length--;

        // Ignore 6-bit carriage return and sync characters.
        if (ch == 63 || ch == 48) continue;
        
        crc ^= ch; /* XOR-in the next input byte */
        
        for (int i = 0; i < 8; i++)
        {
            if ((crc & 0x80) != 0)
            {
                crc = (unsigned char)((crc << 1) ^ generator);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

void CallsignEncoder::convertDigitToASCII_(char* dest, unsigned char digit)
{
    if (digit >= 0 && digit <= 9)
    {
        *dest = digit + 10; // using 6 bit character set defined above.
    }
    else if (digit >= 0xA && digit <= 0xF)
    {
        *dest = (digit - 0xA) + 20; // using 6 bit character set defined above.
    }
    else
    {
        // Should not reach here.
        *dest = 10;
    }
}

unsigned char CallsignEncoder::convertHexStringToDigit_(char* src) const
{
    unsigned char ret = 0;
    for (int i = 0; i < 2; i++)
    {
        ret <<= 4;
        unsigned char temp = 0;
        if (*src >= '0' && *src <= '9')
        {
            temp = *src - '0';
        }
        else if (*src >= 'A' && *src <= 'F')
        {
            temp = *src - 'A' + 0xA;
        }
        ret |= temp;
        src++;
    }
    
    return ret;
}