#pragma once

#include <Dsp/Stages.h>
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
    std::array<eon::DCBlocker, maximumChannels> dcBlockers {};
};
