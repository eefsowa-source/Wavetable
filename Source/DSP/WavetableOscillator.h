#pragma once
#include <JuceHeader.h>

struct WavetableData
{
    static constexpr int tableSize = 2048;
    static constexpr int numTables = 16;
    // Band-limited mip levels: each level's harmonic cap keeps that many
    // partials before the inverse FFT, so high notes select a table with no
    // content above Nyquist instead of aliasing. Level 0 is the least
    // aggressive (highest cap, used by low notes); the last level is the
    // most band-limited (used by the highest notes).
    static constexpr int numMipLevels = 10;
    static constexpr std::array<int, numMipLevels> mipHarmonicCaps {
        1024, 512, 256, 128, 64, 32, 16, 8, 4, 2 };

    std::array<std::array<float, tableSize>, numTables> frames{};
    std::array<std::array<std::array<float, tableSize>, numTables>, numMipLevels> mipFrames{};

    WavetableData();
    void loadFromAudio (const juce::AudioBuffer<float>& source);

    // Rebuilds every band-limited mip level for every frame. Not realtime
    // safe (allocates via juce::dsp::FFT lookup tables and runs FFTs) - call
    // only from the message/GUI thread, never from the audio callback.
    void regenerateMips();

    // Rebuilds only one frame's mip levels; used for interactive editing
    // (waveform drawing/harmonic shifting) so a single-frame edit doesn't
    // pay for all 16 frames' FFTs on every mouse-drag event.
    void regenerateMipsForFrame (int frameIndex);

    // Selects the most detailed band-limited table that still stays under
    // Nyquist for the given oscillator increment (cycles per sample). Pure
    // lookup - safe to call from the audio thread.
    const std::array<float, tableSize>& tableForFrame (int frameIndex, float increment) const noexcept;
};

class WavetableOscillator
{
public:
    void prepare (double sampleRate);
    void setFrequency (float hz) noexcept;
    void setPosition (float p) noexcept { position = juce::jlimit (0.0f, 1.0f, p); }
    float process (const WavetableData& table) noexcept;
    void reset() noexcept { phase = 0.0f; }
    void setPhase (float p) noexcept { phase = juce::jlimit (0.0f, 1.0f, p); }
private:
    double sampleRate = 44100.0;
    float phase = 0.0f, increment = 0.0f, position = 0.0f;
};
