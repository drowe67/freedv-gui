#include <cstdlib>
#include <cstring>
#include "callsign_encoder.h"

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
    
    memcpy(callsign_, callsign, strlen(callsign) + 1);
    convert_callsign_to_ota_string_(callsign_, translatedCallsign_);
    
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
            //fprintf(stderr, "pending bytes: 0 = %x, 1 = %x, 2 = %x, 3 = %x\n", pendingGolayBytes[0], pendingGolayBytes[1], pendingGolayBytes[2], pendingGolayBytes[3]);
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

// 6 bit character set for text field use:
// 0: ASCII null
// 1-9: ASCII 38-47
// 10-19: ASCII '0'-'9'
// 20-46: ASCII 'A'-'Z'
// 47: ASCII ' '
// 48: ASCII '\r'
// 48-62: TBD/for future use.
// 63: sync (2x in a 2 byte block indicates sync)
void CallsignEncoder::convert_callsign_to_ota_string_(const char* input, char* output)
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