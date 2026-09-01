#include "WavetableOscillator.h"

WavetableData::WavetableData()
{
    for (int frame = 0; frame < numTables; ++frame)
        for (int i = 0; i < tableSize; ++i)
        {
            const auto phase = juce::MathConstants<float>::twoPi * (float) i / (float) tableSize;
            const float morph = (float) frame / (float) (numTables - 1);
            frames[(size_t) frame][(size_t) i] = std::sin (phase)
                * (1.0f - morph * 0.35f) + std::sin (phase * 2.0f) * morph * 0.25f;
        }
    regenerateMips();
}

void WavetableData::loadFromAudio (const juce::AudioBuffer<float>& source)
{
    if (source.getNumChannels() == 0 || source.getNumSamples() == 0) return;
    for (int frame = 0; frame < numTables; ++frame)
        for (int i = 0; i < tableSize; ++i)
        {
            const bool hasMultipleFrames = source.getNumSamples() >= tableSize * numTables;
            const auto frameStart = hasMultipleFrames ? (double) frame * (double) source.getNumSamples() / (double) numTables : 0.0;
            const auto frameLength = hasMultipleFrames ? (double) source.getNumSamples() / (double) numTables : (double) source.getNumSamples();
            const auto sample = (int) juce::jlimit (0, source.getNumSamples() - 1,
                                                     (int) std::floor (frameStart + (double) i / (double) tableSize * frameLength));
            frames[(size_t) frame][(size_t) i] = source.getSample (0, sample);
        }
    regenerateMips();
}

namespace
{
    constexpr int wavetableFftOrder = 11;

    void bandLimitFrame (std::array<float, WavetableData::tableSize>& out,
                          const std::array<float, WavetableData::tableSize>& in,
                          int harmonicCap,
                          juce::dsp::FFT& fft)
    {
        constexpr int size = WavetableData::tableSize;
        std::array<float, size * 2> buffer{};
        std::copy (in.begin(), in.end(), buffer.begin());
        fft.performRealOnlyForwardTransform (buffer.data());
        const int nyquistBin = size / 2;
        for (int bin = harmonicCap + 1; bin <= nyquistBin; ++bin)
        {
            buffer[(size_t) (2 * bin)] = 0.0f;
            buffer[(size_t) (2 * bin + 1)] = 0.0f;
        }
        fft.performRealOnlyInverseTransform (buffer.data());
        std::copy (buffer.begin(), buffer.begin() + size, out.begin());
    }
    
    // Hermite cubic interpolation (4-point)
    // y0, y1, y2, y3 are sample values at indices 0, 1, 2, 3
    // t is fractional position in [0, 1] between y1 and y2
    static inline float hermiteInterpolate (float y0, float y1, float y2, float y3, float t)
    {
        const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float a2 = -0.5f * y0 + 0.5f * y2;
        const float a3 = y1;
        const float t2 = t * t;
        const float t3 = t2 * t;
        return a0 * t3 + a1 * t2 + a2 * t + a3;
    }
}

void WavetableData::regenerateMips()
{
    for (int frame = 0; frame < numTables; ++frame)
        regenerateMipsForFrame (frame);
}

void WavetableData::regenerateMipsForFrame (int frameIndex)
{
    if (frameIndex < 0 || frameIndex >= numTables) return;
    juce::dsp::FFT fft (wavetableFftOrder);
    for (int level = 0; level < numMipLevels; ++level)
        bandLimitFrame (mipFrames[(size_t) level][(size_t) frameIndex],
                         frames[(size_t) frameIndex],
                         mipHarmonicCaps[(size_t) level],
                         fft);
}

const std::array<float, WavetableData::tableSize>& WavetableData::tableForFrame (int frameIndex, float increment) const noexcept
{
    const float maxSafeHarmonic = increment > 0.0f ? 0.5f / increment : (float) tableSize;
    for (int level = 0; level < numMipLevels; ++level)
        if ((float) mipHarmonicCaps[(size_t) level] <= maxSafeHarmonic)
            return mipFrames[(size_t) level][(size_t) frameIndex];
    return mipFrames[(size_t) (numMipLevels - 1)][(size_t) frameIndex];
}

void WavetableOscillator::prepare (double sr) { sampleRate = sr; }
void WavetableOscillator::setFrequency (float hz) noexcept
{
    increment = hz > 0.0f ? hz / (float) sampleRate : 0.0f;
}

float WavetableOscillator::process (const WavetableData& table) noexcept
{
    const float frame = position * (float) (WavetableData::numTables - 1);
    const int a = (int) frame;
    const int b = juce::jmin (a + 1, WavetableData::numTables - 1);
    const float frac = frame - (float) a;
    const auto& tableA = table.tableForFrame (a, increment);
    const auto& tableB = table.tableForFrame (b, increment);
    const float index = phase * (float) WavetableData::tableSize;
    const int i0 = ((int) index) & (WavetableData::tableSize - 1);
    const int i1 = (i0 + 1) & (WavetableData::tableSize - 1);
    const int i2 = (i0 + 2) & (WavetableData::tableSize - 1);
    const int i3 = (i0 + 3) & (WavetableData::tableSize - 1);
    const float t = index - std::floor (index);
    
    // Use Hermite 4-point interpolation instead of linear
    const float va = hermiteInterpolate (tableA[(size_t) i0], tableA[(size_t) i1],
                                         tableA[(size_t) i2], tableA[(size_t) i3], t);
    const float vb = hermiteInterpolate (tableB[(size_t) i0], tableB[(size_t) i1],
                                         tableB[(size_t) i2], tableB[(size_t) i3], t);
    phase += increment;
    phase -= std::floor (phase);
    return juce::jmap (frac, va, vb);
}

