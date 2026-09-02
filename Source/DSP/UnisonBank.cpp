#include "UnisonBank.h"

std::array<UnisonLane, 8> makeUnisonLayout (int count,
                                            float detuneCents,
                                            float stereoSpread) noexcept
{
    std::array<UnisonLane, 8> lanes {};
    const auto active = juce::jlimit (1, 8, count);
    const auto detune = juce::jmax (0.0f, detuneCents);
    const auto spread = juce::jlimit (0.0f, 1.0f, stereoSpread);
    for (int index = 0; index < active; ++index)
    {
        const auto position = active == 1 ? 0.0f : 2.0f * (float) index / (float) (active - 1) - 1.0f;
        lanes[(size_t) index] = { position * detune, position * spread,
                                  1.0f / std::sqrt ((float) active) };
    }
    return lanes;
}

void OscillatorUnisonBank::prepare (double sampleRate) noexcept
{
    for (auto& oscillator : oscillators)
        oscillator.prepare (sampleRate);
}

void OscillatorUnisonBank::resetPhases() noexcept
{
    for (auto& oscillator : oscillators)
        oscillator.reset();
}

void OscillatorUnisonBank::setRandomPhases (RealtimeRandom& random, float amount) noexcept
{
    const auto normalizedAmount = juce::jlimit (0.0f, 1.0f, amount);
    for (auto& oscillator : oscillators)
        oscillator.setPhase (random.nextUnitFloat() * normalizedAmount);
}

void OscillatorUnisonBank::processStereo (const WavetableData& table, int count, float frequency,
                                          float position, float detuneCents, float stereoSpread,
                                          float& left, float& right) noexcept
{
    left = 0.0f;
    right = 0.0f;
    const auto active = juce::jlimit (1, 8, count);
    const auto lanes = makeUnisonLayout (active, detuneCents, stereoSpread);
    for (int index = 0; index < active; ++index)
    {
        const auto& lane = lanes[(size_t) index];
        auto& oscillator = oscillators[(size_t) index];
        oscillator.setFrequency (frequency * std::exp2 (lane.cents * 0.01f / 12.0f));
        oscillator.setPosition (position);
        const auto value = oscillator.process (table) * lane.gain;
        // A single lane keeps the pure layout centred, while the oscillator's
        // legacy spread control still provides its intentional stereo image.
        const auto pan = juce::jlimit (-1.0f, 1.0f, active == 1 ? stereoSpread : lane.pan);
        const auto leftGain = std::sqrt (0.5f * (1.0f - pan));
        const auto rightGain = std::sqrt (0.5f * (1.0f + pan));
        left += value * leftGain;
        right += value * rightGain;
    }
}
