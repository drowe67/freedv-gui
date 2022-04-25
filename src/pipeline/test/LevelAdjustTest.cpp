#include "LevelAdjustStep.h"
#include "PipelineTestCommon.h"

bool levelAdjustCommon(float val)
{
    LevelAdjustStep levelAdjustStep(8000, [val]() { return val; });
    
    int outputSamples = 0;
    short* pData = new short[1];
    pData[0] = 10000;
    
    auto result = levelAdjustStep.execute(std::shared_ptr<short>(pData), 1, &outputSamples);
    if (outputSamples != 1)
    {
        std::cerr << "[outputSamples[" << outputSamples << "] != 1]...";
        return false;
    }
    
    auto expectedVal = 10000 * val;
    if (result.get()[0] != expectedVal)
    {
        std::cerr << "[result[" << result.get()[0] << "] != " << expectedVal << "]...";
        return false;
    }
    
    return true;
}

bool levelAdjustUp()
{
    return levelAdjustCommon(2.0);
}

bool levelAdjustDown()
{
    return levelAdjustCommon(0.5);
}

int main()
{
    TEST_CASE(levelAdjustUp);
    TEST_CASE(levelAdjustDown);
    return 0;
}