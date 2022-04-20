#include "EitherOrStep.h"

class FalseStep : public IPipelineStep
{
public:
    virtual int getInputSampleRate() const { return 8000; }
    virtual int getOutputSampleRate() const { return 8000; }
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
    {
        *numOutputSamples = 1;
        short* result = new short[1];
        result[0] = 0;
        
        return std::shared_ptr<short>(result, std::default_delete<short[]>());
    }
};

class TrueStep : public IPipelineStep
{
public:
    virtual int getInputSampleRate() const { return 8000; }
    virtual int getOutputSampleRate() const { return 8000; }
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
    {
        *numOutputSamples = 1;
        short* result = new short[1];
        result[0] = 1;
        
        return std::shared_ptr<short>(result, std::default_delete<short[]>());
    }
};

int main()
{
    bool testPassed = true;
    
    bool isStepTrue = false;
    EitherOrStep eitherOrStep([&]() {
        return isStepTrue;
    }, std::shared_ptr<IPipelineStep>(new TrueStep()),
    std::shared_ptr<IPipelineStep>(new FalseStep()));
    
    int outputSamples = 0;
    auto result = eitherOrStep.execute(std::shared_ptr<short>(nullptr), 0, &outputSamples);
    testPassed &= outputSamples == 1 && result.get()[0] == 0;
    
    isStepTrue = true;
    result = eitherOrStep.execute(std::shared_ptr<short>(nullptr), 0, &outputSamples);
    testPassed &= outputSamples == 1 && result.get()[0] == 1;
    
    return testPassed ? 0 : -1;
}