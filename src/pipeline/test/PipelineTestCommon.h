#ifndef PIPELINE_TEST_COMMON_H
#define PIPELINE_TEST_COMMON_H

// Suite of common functions used by unit tests.

#include <iostream>
#include <functional>
#include <string>
#include <cstdlib>
#include <cmath>
#include <assert.h>

inline short* generateOneSecondSineWave(float frequency, int sampleRate)
{
    short* result = new short[sampleRate];
    assert(result != nullptr);
    
    for (auto n = 0; n < sampleRate; n++)
    {
        // f(n) = A * cos(2*pi*f*t) = A * cos(2*pi*f*n/sampleRate)
        result[n] = frequency * cos(6.2832*(n)*400.0/sampleRate);
    }
    
    return result;
}

inline void executeTestCase(std::string const& testName, std::function<bool()> const& fn)
{
    std::cout << "Executing " << testName << "...";
    if (fn())
    {
        std::cout << "passed" << std::endl;
    }
    else
    {
        std::cout << "FAILED" << std::endl;
        exit(-1);
    }
}

#define TEST_CASE(name) executeTestCase( #name, name)

#endif // PIPELINE_TEST_COMMON_H
