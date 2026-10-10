#ifndef UNIT_TEST_COMMON_H
#define UNIT_TEST_COMMON_H

// Suite of common functions used by unit tests.

#include <functional>
#include <iostream>
#include <string>
#include <vector>

// Names of the test cases that have failed so far. Failures are reported via
// testResult() rather than exiting immediately so that every test case gets
// to run before CTest sees the failure.
inline std::vector<std::string>& failedTestCases()
{
    static std::vector<std::string> failed;
    return failed;
}

inline void executeTestCase(std::string const& testName, std::function<bool()> const& fn)
{
    std::cout << "Executing " << testName << "..." << std::flush;
    if (fn())
    {
        std::cout << "passed" << std::endl;
    }
    else
    {
        std::cout << "FAILED" << std::endl;
        failedTestCases().push_back(testName);
    }
}

// Prints a summary (useful if the code under test logs a lot) and returns
// the value that main() should return.
inline int testResult()
{
    if (failedTestCases().empty())
    {
        std::cout << "All test cases passed" << std::endl;
        return 0;
    }

    std::cout << failedTestCases().size() << " test case(s) FAILED:" << std::endl;
    for (auto& testName : failedTestCases())
    {
        std::cout << "    " << testName << std::endl;
    }
    return -1;
}

#endif // UNIT_TEST_COMMON_H
