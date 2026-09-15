#include "OutputSafety.h"

#include <cmath>

void OutputSafety::prepare (double sampleRate) noexcept
{
    constexpr double cutoffHz = 10.0;
    dcCoefficient = sampleRate > 0.0
                        ? (float) std::exp (-2.0 * juce::MathConstants<double>::pi
                                            * cutoffHz / sampleRate)
                        : 0.0f;
    reset();
}

void OutputSafety::reset() noexcept
{
    previousInput.fill (0.0f);
    previousOutput.fill (0.0f);
}

void OutputSafety::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto channels = juce::jmin (maximumChannels, buffer.getNumChannels());
    for (int channel = 0; channel < channels; ++channel)
    {
        auto inputState = previousInput[(size_t) channel];
        auto outputState = previousOutput[(size_t) channel];
        auto* samples = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto input = samples[sample];
            const auto output = input - inputState + dcCoefficient * outputState;
            inputState = input;
            outputState = output;
            samples[sample] = output;
        }
        previousInput[(size_t) channel] = inputState;
        previousOutput[(size_t) channel] = outputState;
    }
}
