#pragma once

#include <cstdint>

class RealtimeRandom
{
public:
    explicit RealtimeRandom (std::uint32_t seed) noexcept
        : state (seed != 0u ? seed : 0x6d2b79f5u) {}

    float nextUnitFloat() noexcept
    {
        auto x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x != 0u ? x : 0x6d2b79f5u;
        return static_cast<float> (state >> 8) * (1.0f / 16777216.0f);
    }

private:
    std::uint32_t state;
};
