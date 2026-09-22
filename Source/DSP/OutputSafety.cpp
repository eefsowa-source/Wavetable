#include "OutputSafety.h"

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
}

void OutputSafety::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto channels = juce::jmin (maximumChannels, buffer.getNumChannels());
    for (int channel = 0; channel < channels; ++channel)
    {
        auto* samples = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            samples[sample] = dcBlockers[(size_t) channel].process (samples[sample]);
    }
}
