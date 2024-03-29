#include "AudioPipeline.h"
#include "PipelineTestCommon.h"
#include "LevelAdjustStep.h"

static bool passthroughCommon(int inputSampleRate, int outputSampleRate)
{
    AudioPipeline pipeline(inputSampleRate, outputSampleRate);
    auto sineWave = std::shared_ptr<short>(generateOneSecondSineWave(2000, inputSampleRate), std::default_delete<short[]>());
    
    int outputSamples = 0;
    auto result = pipeline.execute(sineWave, inputSampleRate, &outputSamples);
    
    auto minOutputSamples = outputSampleRate * 0.9;
    auto maxOutputSamples = outputSampleRate * 1.1;
    
    if (outputSamples < minOutputSamples || outputSamples > maxOutputSamples)
    {
        std::cerr << "[outputSamples[" << outputSamples << "] != " << outputSampleRate << " +/- 10%]...";
        return false;
    }
    
    return true;
}

bool passthrough()
{
    return passthroughCommon(8000, 8000);
}

bool passthroughUpsample()
{
    return passthroughCommon(8000, 48000);
}

bool passthroughDownsample()
{
    return passthroughCommon(48000, 8000);
}

bool resampleBeforeStepCommon(int inputSampleRate, int stepSampleRate, int outputSampleRate)
{
    AudioPipeline pipeline(inputSampleRate, outputSampleRate);
    auto levelAdjustStep = new LevelAdjustStep(stepSampleRate, []() { return 1.0; });
    assert(levelAdjustStep != nullptr);
    
    pipeline.appendPipelineStep(std::shared_ptr<IPipelineStep>(levelAdjustStep));
    
    auto sineWave = std::shared_ptr<short>(generateOneSecondSineWave(2000, inputSampleRate), std::default_delete<short[]>());
    int numOutputSamples = 0;
    auto result = pipeline.execute(sineWave, inputSampleRate, &numOutputSamples);
    
    auto minOutputSamples = outputSampleRate * 0.9;
    auto maxOutputSamples = outputSampleRate * 1.1;
    
    if (numOutputSamples < minOutputSamples || numOutputSamples > maxOutputSamples)
    {
        std::cerr << "[outputSamples[" << numOutputSamples << "] != " << outputSampleRate << " +/- 10%]...";
        return false;
    }
    
    return true;
}

bool upsampleBeforeStep()
{
    return resampleBeforeStepCommon(8000, 48000, 48000);
}

bool upsampleJustForStep()
{
    return resampleBeforeStepCommon(8000, 48000, 8000);
}

bool upsampleOnlyAtEnd()
{
    return resampleBeforeStepCommon(8000, 8000, 48000);
}

bool downsampleBeforeStep()
{
    return resampleBeforeStepCommon(48000, 8000, 8000);
}

bool downsampleJustForStep()
{
    return resampleBeforeStepCommon(48000, 8000, 48000);
}

bool downsampleOnlyAtEnd()
{
    return resampleBeforeStepCommon(48000, 48000, 8000);
}

int main()
{
    TEST_CASE(passthrough);
    TEST_CASE(passthroughUpsample);
    TEST_CASE(passthroughDownsample);
    
    TEST_CASE(upsampleBeforeStep);
    TEST_CASE(upsampleJustForStep);
    TEST_CASE(upsampleOnlyAtEnd);
    
    TEST_CASE(downsampleBeforeStep);
    TEST_CASE(downsampleJustForStep);
    TEST_CASE(downsampleOnlyAtEnd);
    
    return 0;
}