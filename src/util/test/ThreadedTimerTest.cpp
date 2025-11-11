#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

#include "../ThreadedTimer.h"

using namespace std::chrono_literals;

int main(int argc, char** argv)
{
    // Single-shot time ordering test
    std::cout << "Test 1 (Single-shot time ordering): ";
    std::atomic<int> count = 0;
    bool result = true;
    ThreadedTimer timer1(250, [&](ThreadedTimer&) { std::cout << "1 "; if (count == 0) count++; }, false);
    ThreadedTimer timer2(500, [&](ThreadedTimer&) { std::cout << "2 "; if (count == 1) count++; }, false);
    ThreadedTimer timer3(750, [&](ThreadedTimer&) { std::cout << "3 "; if (count == 2) count++; }, false);

    timer1.start();
    timer2.start();
    timer3.start();

    std::this_thread::sleep_for(1s);
    bool testResult = (count == 3);
    std::cout << "(count = " << count << ") ";
    std::cout << (testResult ? "PASS" : "FAIL") << "\n";
    result &= testResult;

    // Repeated trigger test
    std::cout << "Test 2 (Repeated triggering): ";
    count = 0;
    ThreadedTimer timer4(250, [&](ThreadedTimer&) { std::cout << "4 "; count++; }, true);
    timer4.start();

    std::this_thread::sleep_for(800ms);
    testResult = (count == 3);
    std::cout << "(count = " << count << ") ";
    std::cout << (testResult ? "PASS" : "FAIL") << "\n";
    result &= testResult;

    // Timer stop test
    std::cout << "Test 3 (Stop repeated timer): ";
    timer4.stop();

    std::this_thread::sleep_for(800ms);
    testResult = (count == 3);
    std::cout << "(count = " << count << ") ";
    std::cout << (testResult ? "PASS" : "FAIL") << "\n";
    result &= testResult;

    return result ? 0 : -1;
}
