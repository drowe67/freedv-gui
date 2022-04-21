#include "LevelAdjustStep.h"

int main()
{
    LevelAdjustStep levelAdjustStep(8000, []() { return 2.0; });
    bool testPassed = true;
    
    int outputSamples = 0;
    short* pData = new short[1];
    pData[0] = 10000;
    
    auto result = levelAdjustStep.execute(std::shared_ptr<short>(pData), 1, &outputSamples);
    testPassed &= outputSamples == 1 && result.get()[0] == 20000;
    
    LevelAdjustStep levelAdjustStep2(8000, []() { return 0.5; });
    pData = new short[1];
    pData[0] = 10000;
    result = levelAdjustStep2.execute(std::shared_ptr<short>(pData), 1, &outputSamples);
    testPassed &= outputSamples == 1 && result.get()[0] == 5000;
    
    return testPassed ? 0 : -1;
}