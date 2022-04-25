#include "EitherOrStep.h"
#include "PipelineTestCommon.h"

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

bool eitherOrCommon(bool val)
{
    EitherOrStep eitherOrStep([&]() {
        return val;
    }, std::shared_ptr<IPipelineStep>(new TrueStep()),
    std::shared_ptr<IPipelineStep>(new FalseStep()));
    
    int outputSamples = 0;
    auto result = eitherOrStep.execute(std::shared_ptr<short>(nullptr), 0, &outputSamples);
    if (outputSamples != 1)
    {
        std::cerr << "[outputSamples[" << outputSamples << "] != 1]...";
        return false;
    }
    
    if (result.get()[0] != (val ? 1 : 0))
    {
        std::cerr << "[result != " << (val ? 1 : 0) << "]...";
        return false;
    }
    
    return true;
}

bool trueStep()
{
    return eitherOrCommon(true);
}

bool falseStep()
{
    return eitherOrCommon(false);
}

int main()
{
    TEST_CASE(trueStep);
    TEST_CASE(falseStep);
    return 0;
}