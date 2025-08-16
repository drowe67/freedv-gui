#include "ResampleStep.h"
#include "PipelineTestCommon.h"

bool resampleTestCaseCommon(int inputSampleRate, int outputSampleRate)
{
    ResampleStep resampleStep(inputSampleRate, outputSampleRate);
    auto inputSineWave = std::unique_ptr<short[]>(generateOneSecondSineWave(2000, inputSampleRate), std::default_delete<short[]>());
    auto outputSineWave = std::unique_ptr<short[]>(generateOneSecondSineWave(2000, outputSampleRate), std::default_delete<short[]>());
    
    int numOutputSamples = 0;
    resampleStep.execute(inputSineWave.get(), inputSampleRate, &numOutputSamples);
    
    // Allowed output samples are +/- 10% of the theoretical max.
    int minOutputSamples = outputSampleRate * 0.9;
    int maxOutputSamples = outputSampleRate * 1.1;    
    if (numOutputSamples < minOutputSamples || numOutputSamples > maxOutputSamples)
    {
        std::cerr << "[numOutputSamples(" << numOutputSamples << ") != " << outputSampleRate << "]...";
        return false;
    }
    
    return true;
}

bool resampleEqual()
{
    return resampleTestCaseCommon(8000, 8000);
}

bool resampleToHigher()
{
    return resampleTestCaseCommon(8000, 48000);
}

bool resampleToLower()
{
    return resampleTestCaseCommon(48000, 8000);
}

int main()
{
    TEST_CASE(resampleEqual);
    TEST_CASE(resampleToHigher);
    TEST_CASE(resampleToLower);
    return 0;
}
