#ifndef CALLSIGN_ENCODER_H
#define CALLSIGN_ENCODER_H

#include <deque>
#include "fdmdv2_defines.h"
#include "golay23.h"

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
    const char* getReceivedText() const { return &receivedCallsign_[0]; }
    
private:
    std::deque<unsigned char> pendingGolayBytes_;
    bool textInSync_;
    
    char callsign_[MAX_CALLSIGN/2];
    char translatedCallsign_[MAX_CALLSIGN];
    char truncCallsign_[MAX_CALLSIGN];
    
    char receivedCallsign_[MAX_CALLSIGN];
    char* pReceivedCallsign_;
    
    void convert_callsign_to_ota_string_(const char* input, char* output);
    void convert_ota_string_to_callsign_(const char* input, char* output);
};

#endif // CALLSIGN_ENCODER_H