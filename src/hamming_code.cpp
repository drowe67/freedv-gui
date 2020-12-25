#include <string>
#include <cstdlib>
#include "hamming_code.h"

#define GET_BIT(data, bit) ((data[bit >> 3] >> (bit & 7)) & 1)
#define SET_BIT(data, bit) (data[bit >> 3] |= (1 << (bit & 7)))
#define CLEAR_BIT(data, bit) (data[bit >> 3] &= ~(1 << (bit & 7)))

void HammingCode::encode(unsigned char* inputData, unsigned char* outputData)
{
    int numParityBits = totalBits_ - dataBits_;
    int currParityBit = 0;
    int currDataBit = 0;
    
    // Stage 1: Copy data bits into output
    for (int bit = 0; bit < totalBits_; bit++)
    {
        // Bits that are at power of two positions are parity.
        if (bit == (1 << currParityBit) - 1)
        {
            // Clear the parity bit to start. We'll fill it in with the actual value later.
            CLEAR_BIT(outputData, bit);
            
            // Move the current parity to the next power of two.
            currParityBit++;
            
            // Continue with the next bit.
            continue;
        }
        
        // Set data bit to 1 or 0 depending on input.
        if (GET_BIT(inputData, currDataBit)) SET_BIT(outputData, bit);
        else CLEAR_BIT(outputData, bit);
        currDataBit++;
    }
    
    // Clear final overall parity bit.
    CLEAR_BIT(outputData, totalBits_);
        
    // Stage 2: calculate parity.
    calculateParity_(outputData);
}

bool HammingCode::decode(unsigned char* inputData, unsigned char* outputData)
{
    // Copy inputData to temporary location.
    unsigned char tempData[(totalBits_ >> 3) + 1];
    memcpy(tempData, inputData, (totalBits_ >> 3) + 1);
    
    // Verify end parity bit. If the parity bit is correct, either the entire block
    // is correct or we've had a double bit error.
    bool parityBitCorrect = true;
    if (detectDoubleErrors_)
    {
        char overallBitTotal = 0;
        for (int bit = 0; bit < totalBits_; bit++)
        {
            overallBitTotal += GET_BIT(tempData, bit);            
        }

        unsigned char calculatedBit = (overallBitTotal & 1) == 1;
        parityBitCorrect = GET_BIT(tempData, totalBits_) == calculatedBit;
    }
    
    // Recalculate parity on temp data and compare with input.
    calculateParity_(tempData);
    if (memcmp(tempData, inputData, (totalBits_ >> 3) + 1) != 0)
    {
        // If the parity bit is correct, we've had a double bit error.
        if (detectDoubleErrors_ && parityBitCorrect)
        {
            return false;
        }

        // Otherwise, the number indicated by the parity bits is the location
        // of the error. Flip the bit indicated by this location.
        int errorLoc = 0;
        int numParityBits = totalBits_ - dataBits_;
        for (int parityBit = numParityBits; parityBit >= 1; parityBit--)
        {
            int parityBitToGet = (1 << (parityBit - 1)) - 1;
            errorLoc += GET_BIT(tempData, parityBitToGet) << (parityBit - 1);
        }
    
        errorLoc--;
    
        if (GET_BIT(tempData, errorLoc))
        {
            CLEAR_BIT(tempData, errorLoc);
        }
        else
        {
            SET_BIT(tempData, errorLoc);
        }
    }
    
    // Data is good; extract data bits.
    int currParityBit = 0;
    int currDataBit = 0;
    
    // Stage 1: Copy data bits into output
    for (int bit = 0; bit < totalBits_; bit++)
    {
        // Bits that are at power of two positions are parity and should be ignored.
        if (bit == (1 << currParityBit) - 1)
        {
            currParityBit++;
            continue;
        }
        
        // Set data bit to 1 or 0 depending on input.
        if (GET_BIT(tempData, bit)) SET_BIT(outputData, currDataBit);
        else CLEAR_BIT(outputData, currDataBit);
        currDataBit++;
    }
    
    return true;
}

void HammingCode::calculateParity_(unsigned char* outputData)
{
    int numParityBits = totalBits_ - dataBits_;
    for (int parityBit = 1; parityBit <= numParityBits; parityBit++)
    {
        char bitTotal = 0;
        for (int bit = (1 << (parityBit - 1)); bit <= totalBits_; bit++)
        {
            // If the binary position has a 1 in the parityBit place, the
            // bit is part of this parity calculation.
            if (bit & (1 << (parityBit-1)))
            {
                bitTotal += GET_BIT(outputData, (bit - 1));
            }
        }
        
        // If bitTotal is odd (1 in the LSB), we should also set the parity bit to 1
        // (even parity).
        int parityBitToSet = (1 << (parityBit - 1)) - 1;
        if ((bitTotal & 1) == 1)
        {
            SET_BIT(outputData, parityBitToSet);
        }
        else
        {
            CLEAR_BIT(outputData, parityBitToSet);
        }
    }
    
    // Set final parity bit.
    CLEAR_BIT(outputData, totalBits_);
    if (detectDoubleErrors_)
    {
        char overallBitTotal = 0;
        for (int bit = 0; bit < totalBits_; bit++)
        {
            overallBitTotal += GET_BIT(outputData, bit);            
        }

        if ((overallBitTotal & 1) == 1)
        {
            SET_BIT(outputData, totalBits_);
        }
    }
}

#ifdef TEST_HAMMING_CLASS
#include <iostream>
#include <iomanip>

int main()
{
    unsigned char inputData = 0b1010;
    unsigned char outputData = 0;
    HammingCode encoder(7, 4, true);
    
    encoder.encode(&inputData, &outputData);
    
    std::cout << "Input data: " << std::hex << (unsigned int)inputData << std::endl;
    std::cout << "Output data: " << std::hex << (unsigned int)outputData << std::endl;
    
    std::cout << "Single bit error test:" << std::endl;
    for (int i = 0; i < 7; i++)
    {
        outputData ^= (1 << i);
        inputData = 0;
        std::cout << "Error data: " << std::hex << (unsigned int)outputData << std::endl;
        bool hasDecoded = encoder.decode(&outputData, &inputData);
        std::cout << "Can decode: " << (int)hasDecoded << std::endl;
        std::cout << "Decoded data: " << std::hex << (unsigned int)inputData << std::endl;
        outputData ^= (1 << i);
    }
    
    std::cout << "Double bit error test:" << std::endl;
    for (int i = 0; i < 7; i++)
    {
        outputData ^= (1 << (i+1)) | (1 << i);
        inputData = 0;
        std::cout << "Error data: " << std::hex << (unsigned int)outputData << std::endl;
        bool hasDecoded = encoder.decode(&outputData, &inputData);
        std::cout << "Can decode: " << (int)hasDecoded << std::endl;
        std::cout << "Decoded data: " << std::hex << (unsigned int)inputData << std::endl;
        outputData ^= (1 << (i+1)) | (1 << i);
    }
    
    return 0;
}
#endif // TEST_HAMMING_CLASS