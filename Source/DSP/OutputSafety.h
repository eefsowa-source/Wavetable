#pragma once

#include <JuceHeader.h>

#include <array>

class OutputSafety
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int maximumChannels = 2;
    std::array<float, maximumChannels> previousInput {};
    std::array<float, maximumChannels> previousOutput {};
    float dcCoefficient = 0.0f;
};
