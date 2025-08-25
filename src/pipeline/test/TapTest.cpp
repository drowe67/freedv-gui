#include "TapStep.h"
#include "PipelineTestCommon.h"

class PassThroughStep : public IPipelineStep
{
public:
    virtual int getInputSampleRate() const { return 8000; }
    virtual int getOutputSampleRate() const { return 8000; }
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples)
    {
        lastInputSamples = inputSamples;
        *numOutputSamples = numInputSamples;
        return inputSamples;
    }
    
    short* lastInputSamples;
};

bool tapDataEqual()
{
    PassThroughStep* step = new PassThroughStep;
    TapStep tapStep(8000, step);
    
    int outputSamples = 0;
    std::unique_ptr<short[]> pData = std::make_unique<short[]>(1);
    pData[0] = 10000;
    
    auto result = tapStep.execute(pData.get(), 1, &outputSamples);
    if (outputSamples != 1)
    {
        std::cerr << "[outputSamples[" << outputSamples << "] != 1]...";
        return false;
    } 
    
    if (result != pData.get())
    {
        std::cerr << "[result != input]...";
        return false;
    }
    
    if (result != step->lastInputSamples)
    {
        std::cerr << "[result != step->lastInputSamples]...";
    }
    
    return true;
}

int main()
{
    TEST_CASE(tapDataEqual);
    return 0;
}
