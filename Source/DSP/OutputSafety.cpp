#include "OutputSafety.h"

#include <cmath>

void OutputSafety::prepare (double sampleRate) noexcept
{
    constexpr double cutoffHz = 18.0;
    for (auto& blocker : dcBlockers)
        blocker.prepare (sampleRate, cutoffHz);
    reset();
}

void OutputSafety::reset() noexcept
{
    for (auto& blocker : dcBlockers)
        blocker.reset();
    clipActiveFlag.store (false, std::memory_order_relaxed);
}

void OutputSafety::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto channels = juce::jmin (maximumChannels, buffer.getNumChannels());
    constexpr float knee = ceilingMaximum - ceilingThreshold;
    bool limited = false;
    for (int channel = 0; channel < channels; ++channel)
    {
        auto* samples = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            auto value = dcBlockers[(size_t) channel].process (samples[sample]);
            const auto magnitude = std::abs (value);
            if (magnitude > ceilingThreshold)
            {
                // Linear up to the threshold, then a tanh knee that asymptotes
                // just inside full scale, so no legal patch can exceed it.
                const auto shaped = ceilingThreshold
                                    + knee * std::tanh ((magnitude - ceilingThreshold) / knee);
                value = std::copysign (shaped, value);
                limited = true;
            }
            samples[sample] = value;
        }
    }
    if (limited)
        clipActiveFlag.store (true, std::memory_order_relaxed);
}
