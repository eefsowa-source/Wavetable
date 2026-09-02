#pragma once

#include "RealtimeRandom.h"
#include "WavetableOscillator.h"
#include <array>
#include <cmath>

struct UnisonLane
{
    float cents = 0.0f;
    float pan = 0.0f;
    float gain = 1.0f;
};

std::array<UnisonLane, 8> makeUnisonLayout (int count,
                                            float detuneCents,
                                            float stereoSpread) noexcept;

class OscillatorUnisonBank
{
public:
    void prepare (double sampleRate) noexcept;
    void resetPhases() noexcept;
    void setRandomPhases (RealtimeRandom& random, float amount) noexcept;
    void processStereo (const WavetableData& table, int count, float frequency,
                        float position, float detuneCents, float stereoSpread,
                        float& left, float& right) noexcept;

private:
    std::array<WavetableOscillator, 8> oscillators;
};
