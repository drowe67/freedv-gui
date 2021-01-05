#if !defined(PSK_REPORTER_H)
#define PSK_REPORTER_H

struct SenderRecord
{
    std::string callsign;
    unsigned int frequency;
    char snr;
    std::string mode;
    char infoSource;
    int flowTimeSeconds;
    
    SenderRecord(std::string callsign, unsigned int frequency, char snr);
    
    int recordSize();    
    void encode(char* buf);
};

class PskReporter
{
public:
    PskReporter(std::string callsign, std::string gridSquare, std::string software);
    virtual ~PskReporter();
    
    void addReceiveRecord(std::string callsign, unsigned int frequency, char snr);
    bool send();

private:
    unsigned int currentSequenceNumber_;
    unsigned int randomIdentifier_;
    
    std::string receiverCallsign_;
    std::string receiverGridSquare_;
    std::string decodingSoftware_;
    std::vector<SenderRecord> recordList_;
    
    int getRxDataSize_();    
    int getTxDataSize_();    
    void encodeReceiverRecord_(char* buf);    
    void encodeSenderRecords_(char* buf);
};


#endif // PSK_REPORTER_H