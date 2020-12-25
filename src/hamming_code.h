#ifndef HAMMING_CODE_H
#define HAMMING_CODE_H

class HammingCode
{
public:
    HammingCode(int totalBits, int dataBits, bool detectDoubleErrors)
    : totalBits_(totalBits)
    , dataBits_(dataBits)
    , detectDoubleErrors_(detectDoubleErrors)
    {
        // empty
    }
    
    virtual ~HammingCode() 
    {
         // empty
    }
    
    void encode(unsigned char* inputData, unsigned char* outputData);
    bool decode(unsigned char* inputData, unsigned char* outputData);
    
private:
    int totalBits_;
    int dataBits_;
    bool detectDoubleErrors_;
    
    void calculateParity_(unsigned char* outputData);
};

#endif // HAMMING_CODE_H