
#ifndef AUDIO_SPIN_MUTEX
#define AUDIO_SPIN_MUTEX

#include <thread>
#include <atomic>

#include "sanitizers.h"

#if defined(__aarch64__)
#include <arm_acle.h>
#define PAUSE_CPU __wfe
#elif defined(__x86_64__)
#include <emmintrin.h>
#define PAUSE_CPU _mm_pause
#else
// fall back to yield()
#define PAUSE_CPU std::this_thread::yield
#endif // defined(__aarch64__) || defined(__x86_64__)

struct audio_spin_mutex 
{
    void lock() noexcept
    {
        // approx. 5x5 ns (= 25 ns), 10x40 ns (= 400 ns), and 3000x350 ns 
        // (~ 1 ms), respectively, when measured on a 2.9 GHz Intel i9
        constexpr int iterations[] = {5, 10, 3000};

        for (int i = 0; i < iterations[0]; ++i) 
        {
            if (try_lock())
                return;
        }

        for (int i = 0; i < iterations[1]; ++i) 
        {
            if (try_lock())
                return;

            PAUSE_CPU();
        }

        while (true) 
        {
            for (int i = 0; i < iterations[2]; ++i) 
            {
                if (try_lock())
                    return;

                PAUSE_CPU();
                PAUSE_CPU();
                PAUSE_CPU();
                PAUSE_CPU();
                PAUSE_CPU();
                PAUSE_CPU();
                PAUSE_CPU();
                PAUSE_CPU();
                PAUSE_CPU();
                PAUSE_CPU();
            }

            // waiting longer than we should, let's give other threads 
            // a chance to recover
            std::this_thread::yield();
        }
    }

    bool try_lock() FREEDV_NONBLOCKING
    {
        // Test + Test and Set is better than just TAS. More info:
        // https://en.wikipedia.org/wiki/Test_and_test-and-set
        return flag.load(std::memory_order_relaxed) || !flag.test_and_set(std::memory_order_acquire);
    }

    void unlock() FREEDV_NONBLOCKING
    {
        flag.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

#endif // AUDIO_SPIN_MUTEX
