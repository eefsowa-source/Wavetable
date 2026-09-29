#pragma once

#include <Dsp/Stages.h>
#include <JuceHeader.h>

#include <array>
#include <atomic>

class OutputSafety
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    // True once the safety ceiling has limited a sample since the last
    // clearClipFlag(). The audio thread only ever sets it; the editor polls it
    // to light a clip indicator.
    bool clipActive() const noexcept { return clipActiveFlag.load (std::memory_order_relaxed); }
    void clearClipFlag() noexcept { clipActiveFlag.store (false, std::memory_order_relaxed); }

    // Peak magnitude of the most recently processed buffer, after the ceiling.
    // The audio thread only stores it; the editor polls it for a meter.
    float peakLevel() const noexcept { return peakValue.load (std::memory_order_relaxed); }

private:
    // Samples at or below this magnitude pass through the ceiling unchanged.
    // Above it the curve bends smoothly toward, but never reaches, full scale.
    static constexpr float ceilingThreshold = 0.9f;   // about -0.92 dBFS
    static constexpr float ceilingMaximum = 0.999f;   // asymptote, inside 0 dBFS
    static constexpr int maximumChannels = 2;
    std::array<eon::DCBlocker, maximumChannels> dcBlockers {};
    std::atomic<bool> clipActiveFlag { false };
    std::atomic<float> peakValue { 0.0f };
};
