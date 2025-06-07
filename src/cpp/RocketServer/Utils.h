#pragma once
#include <cstdint>
#include <chrono>
#include <emmintrin.h>

class Utils
{
public:
	static uint64_t GetRandomNumberUInt64();

    template <typename Duration>
    static void BusySleepFor(const Duration& duration)
    {
        // Convert the duration to the steady clock's duration type.
        auto target = std::chrono::duration_cast<std::chrono::steady_clock::duration>(duration);
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < target)
        {
            _mm_pause();
        }
    }
};

