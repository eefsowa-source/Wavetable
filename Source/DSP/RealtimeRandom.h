#pragma once

#include <Dsp/Rng.h>
#include <cstdint>

class RealtimeRandom
{
public:
    explicit RealtimeRandom (std::uint32_t seed) noexcept
        : rng (expandSeed (seed)) {}

    float nextUnitFloat() noexcept
    {
        return static_cast<float> (rng.next());
    }

private:
    static std::uint64_t expandSeed (std::uint32_t seed) noexcept
    {
        const auto nonZero = seed != 0u ? seed : 0x6d2b79f5u;
        return (static_cast<std::uint64_t> (nonZero) << 32) | nonZero;
    }

    eon::Rng rng;
};
