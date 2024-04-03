#include "ExclusiveAccessStep.h"
#include "PipelineTestCommon.h"

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

bool exclusiveAccessMethodsCalled()
{
    PassThroughStep* step = new PassThroughStep;
    ExclusiveAccessStep exclusiveAccessStep(step, []() { lockCalled = true; }, []() { unlockCalled = true; });
    short* pVal = new short[1];
    pVal[0] = 10000;
    
    int outputSamples = 0;
    std::shared_ptr<short> input(pVal, std::default_delete<short[]>());
    auto result = exclusiveAccessStep.execute(input, 1, &outputSamples);
    
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
    
    if (!lockCalled)
    {
        std::cerr << "[lock not called]...";
        return false;
    }
     
    if (!stepCalled)
    {
        std::cerr << "[step not called]...";
        return false;
    }
    
    if (!unlockCalled)
    {
        std::cerr << "[unlock not called]...";
        return false;
    }
    
    return true;
}

int main()
{
    TEST_CASE(exclusiveAccessMethodsCalled);
    return 0;
}
