#include "Utils.h"
#include <random>

uint64_t Utils::GetRandomNumberUInt64()
{
    std::random_device rd; // Non-deterministic random number generator
    std::mt19937_64 gen(rd()); // Seed the Mersenne Twister with random_device
    std::uniform_int_distribution<uint64_t> dis(0, UINT64_MAX); // Uniform distribution

    return dis(gen); // Generate a random 64-bit number
}