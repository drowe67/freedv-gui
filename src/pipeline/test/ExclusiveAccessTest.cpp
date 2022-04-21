#include "ExclusiveAccessStep.h"

static bool lockCalled = false;
static bool stepCalled = false;
static bool unlockCalled = false;

class PassThroughStep : public IPipelineStep
{
public:
    virtual int getInputSampleRate() const { return 8000; }
    virtual int getOutputSampleRate() const { return 8000; }
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
    {
        stepCalled = true;
        *numOutputSamples = numInputSamples;
        return inputSamples;
    }
};

int main()
{
    bool testPassed = true;
    
    PassThroughStep* step = new PassThroughStep;
    ExclusiveAccessStep exclusiveAccessStep(step, []() { lockCalled = true; }, []() { unlockCalled = true; });
    short* pVal = new short[1];
    pVal[0] = 10000;
    
    int outputSamples = 0;
    std::shared_ptr<short> input(pVal);
    auto result = exclusiveAccessStep.execute(input, 1, &outputSamples);
    testPassed &= outputSamples == 1 && result == input && lockCalled && stepCalled && unlockCalled;
    
    return testPassed ? 0 : -1;
}