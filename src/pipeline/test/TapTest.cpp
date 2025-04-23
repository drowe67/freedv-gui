#include "TapStep.h"
#include "PipelineTestCommon.h"

class PassThroughStep : public IPipelineStep
{
public:
    virtual int getInputSampleRate() const { return 8000; }
    virtual int getOutputSampleRate() const { return 8000; }
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
    {
        lastInputSamples = inputSamples;
        *numOutputSamples = numInputSamples;
        return inputSamples;
    }
    
    std::shared_ptr<short> lastInputSamples;
};

bool tapDataEqual()
{
    PassThroughStep* step = new PassThroughStep;
    TapStep tapStep(8000, step, false);
    
    int outputSamples = 0;
    short* pData = new short[1];
    pData[0] = 10000;
    
    std::shared_ptr<short> input(pData, std::default_delete<short[]>());
    auto result = tapStep.execute(input, 1, &outputSamples);
    if (outputSamples != 1)
    {
        std::cerr << "[outputSamples[" << outputSamples << "] != 1]...";
        return false;
    } 
    
    if (result != input)
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
