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
        // A single lane keeps the pure layout centred, while the oscillator's
        // legacy spread control still provides its intentional stereo image.
        const auto pan = juce::jlimit (-1.0f, 1.0f, active == 1 ? spread : position * spread);
        lanes[(size_t) index] = { position * detune, position * spread,
                                  1.0f / std::sqrt ((float) active),
                                  std::exp2 (position * detune * 0.01f / 12.0f),
                                  std::sqrt (0.5f * (1.0f - pan)),
                                  std::sqrt (0.5f * (1.0f + pan)) };
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
    const auto detune = juce::jmax (0.0f, detuneCents);
    const auto spread = juce::jlimit (0.0f, 1.0f, stereoSpread);
    // Detune/spread change at block rate at most; rebuild the lane layout only
    // when one of them actually moved instead of on every audio sample.
    if (count != cachedCount || detune != cachedDetuneCents || spread != cachedStereoSpread)
    {
        cachedLanes = makeUnisonLayout (active, detune, spread);
        cachedCount = count;
        cachedDetuneCents = detune;
        cachedStereoSpread = spread;
    }
    for (int index = 0; index < active; ++index)
    {
        const auto& lane = cachedLanes[(size_t) index];
        auto& oscillator = oscillators[(size_t) index];
        oscillator.setFrequency (frequency * lane.frequencyRatio);
        oscillator.setPosition (position);
        const auto value = oscillator.process (table) * lane.gain;
        left += value * lane.leftGain;
        right += value * lane.rightGain;
    }
}
