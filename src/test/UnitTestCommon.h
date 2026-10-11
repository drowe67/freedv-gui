#ifndef UNIT_TEST_COMMON_H
#define UNIT_TEST_COMMON_H

// Suite of common functions used by unit tests.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <new>
#include <string>
#include <utility>
#include <vector>

// M_PI is not available on some compilers, so define it here just in case.
#ifndef M_PI
    #define M_PI 3.1415926535897932384626433832795
#endif

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

// Returns the folder that tests should create their temporary files in.
inline std::string unitTestTempFolder()
{
    return std::filesystem::temp_directory_path().string();
}

// Returns a path for a temporary file whose name starts with the given
// prefix and ends with the given suffix. The path differs from those used
// by other tests running at the same time.
inline std::string unitTestTempPath(std::string const& prefix, std::string const& suffix)
{
    // Stands in for a process ID, which isn't available the same way everywhere.
    static const auto processTag = std::hash<std::string>()(
        std::to_string((unsigned long long)std::chrono::steady_clock::now().time_since_epoch().count()) +
        std::to_string((unsigned long long)(uintptr_t)&failedTestCases));

    auto path = std::filesystem::temp_directory_path() / (prefix + "-" + std::to_string(processTag) + suffix);
    return path.string();
}

// Holds an object in memory that's never used for anything else afterward.
//
// Objects that own a thread typically wait for that thread to go idle before
// they finish being destroyed. Thread sanitizer can't always tell that this
// happened, so if another object is then created at the same address (as
// happens with one local variable per test case) it reports a race between 
// the old object's thread and the new object's constructor. 
template<typename T>
class NeverReused
{
public:
    template<typename... Args>
    explicit NeverReused(Args&&... args)
        : memory_(::operator new(sizeof(T)))
        , object_(new (memory_) T(std::forward<Args>(args)...))
    {
        // empty
    }

    NeverReused(NeverReused const&) = delete;
    NeverReused& operator=(NeverReused const&) = delete;

    ~NeverReused()
    {
        object_->~T();

        // Kept until exit.
        retired_().blocks.push_back(memory_);
    }

    T& operator*() { return *object_; }
    T* operator->() { return object_; }

private:
    void* memory_;
    T* object_;

    // Frees everything on exit so that it's not reported as a leak.
    struct Retired
    {
        std::vector<void*> blocks;

        ~Retired()
        {
            for (auto block : blocks)
            {
                ::operator delete(block);
            }
        }
    };

    static Retired& retired_()
    {
        static Retired retired;
        return retired;
    }
};

#endif // UNIT_TEST_COMMON_H
