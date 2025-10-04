#include "EitherOrStep.h"
#include "PipelineTestCommon.h"

class FalseStep : public IPipelineStep
{
public:
    FalseStep()
    {
        result_ = std::make_unique<short[]>(1);
        result_[0] = 0;
    }

    virtual int getInputSampleRate() const FREEDV_NONBLOCKING { return 8000; }
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING { return 8000; }
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
    {
        *numOutputSamples = 1;
        return result_.get();
    }

private:
    std::unique_ptr<short[]> result_;
};

class TrueStep : public IPipelineStep
{
public:
    TrueStep()
    {
        result_ = std::make_unique<short[]>(1);
        result_[0] = 1;
    }

    virtual int getInputSampleRate() const FREEDV_NONBLOCKING { return 8000; }
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING { return 8000; }
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
    {
        *numOutputSamples = 1;
        return result_.get();
    }

private:
    std::unique_ptr<short[]> result_;
};

static bool EitherOrCommonVal_;

bool eitherOrCommon(bool val)
{
    EitherOrCommonVal_ = val;
    EitherOrStep eitherOrStep(+[]() FREEDV_NONBLOCKING {
        return EitherOrCommonVal_;
    }, new TrueStep(), new FalseStep());
    
    int outputSamples = 0;
    auto result = eitherOrStep.execute(nullptr, 0, &outputSamples);
    if (outputSamples != 1)
    {
        std::cerr << "[outputSamples[" << outputSamples << "] != 1]...";
        return false;
    }
    
    if (result[0] != (val ? 1 : 0))
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
