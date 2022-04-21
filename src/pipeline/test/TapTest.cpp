#include "TapStep.h"

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

int main()
{
    PassThroughStep* step = new PassThroughStep;
    TapStep tapStep(8000, step);
    bool testPassed = true;
    
    int outputSamples = 0;
    short* pData = new short[1];
    pData[0] = 10000;
    
    std::shared_ptr<short> input(pData);
    auto result = tapStep.execute(input, 1, &outputSamples);
    testPassed &= outputSamples == 1 && result == input && result == step->lastInputSamples;
    
    return testPassed ? 0 : -1;
}