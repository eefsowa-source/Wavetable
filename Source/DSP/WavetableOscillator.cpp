#include "WavetableOscillator.h"
#include "WavetableImporter.h"

namespace
{
    // Default bank content.
    //
    // The previous frames were sin(x)*(1-0.35m) + 0.25*sin(2x)*m, which holds
    // exactly two partials. Measured against Serum and Vital on the same C4
    // fixture, that made the shipped patch a near-sine: spectral centroid
    // 264 Hz, H2 at -20.5 dB, H4 at -64.6 dB and nothing above H3, where the
    // references sit at a 1418 Hz centroid with a full harmonic series out past
    // H16. Bank limiting cannot add partials the table never had, so the cause
    // was the content and not the mip selection (see
    // docs/quality/d1-reference-spectrum-gap.md).
    //
    // The replacement keeps the same 16 frames and the same morph axis but
    // fills each frame with a harmonic series whose tilt sweeps across the bank:
    // frame 0 is near-sine so the default patch still starts clean, the middle
    // is a full 1/h saw, and frame 15 is thin and bright with lifted even
    // partials. Two properties beyond the spectrum itself matter:
    //
    //   - every frame is a sum of sines starting at phase 0, so it is exactly
    //     DC-free and starts at a zero crossing (pinned by WavetableTests.cpp);
    //   - every frame is peak-normalised to the same value, so the position
    //     knob changes timbre without also changing level.
    //
    // 512 partials is the useful ceiling: the largest cap the mip bank asks for
    // in practice is 64 (level 4 covers C4), and 2048 table samples cannot
    // represent harmonics above 1023 without aliasing inside the table itself.
    constexpr int defaultBankPartialCap = 512;

    struct DefaultBankShape
    {
        double rolloff = 1.0;   // exponent on h; 1 = sawtooth
        double evenBias = 1.0;  // extra weight on even partials
    };

    DefaultBankShape defaultBankShape (float morph) noexcept
    {
        // morph runs 0 -> 1 across the 16 frames.
        //
        // The rolloff exponent is the spectral tilt. It sweeps monotonically
        // from a soft round wave at frame 0 to a bright, even-weighted one at
        // frame 15, crossing sawtooth (1.0) around the middle of the bank:
        //
        //   frame  0 : 2.6, soft and round. The default patch must not open
        //              with a full buzz.
        //   frame  7 : ~1.4, sawtooth-like. This is the shape both references
        //              measured (Serum H2..H8 roll off at roughly 1/h).
        //   frame 15 : 0.7 with even partials lifted 1.6x, bright and slightly
        //              hollow, the top of the bank.
        //
        // The 1.5 power on (1 - morph) front-loads the darkening so the low
        // half of the bank changes character quickly where the ear is most
        // sensitive to it, and leaves the bright end a finer control.
        const auto t = (double) morph;
        return { 0.70 + 1.90 * std::pow (1.0 - t, 1.5), 1.0 + 0.60 * t };
    }
}

WavetableData::WavetableData()
{
    for (int frame = 0; frame < numTables; ++frame)
    {
        const auto morph = (float) frame / (float) (numTables - 1);
        const auto shape = defaultBankShape (morph);
        float peak = 0.0f;
        for (int i = 0; i < tableSize; ++i)
        {
            const auto phase = juce::MathConstants<float>::twoPi * (float) i / (float) tableSize;
            double value = 0.0;
            for (int harmonic = 1; harmonic <= defaultBankPartialCap; ++harmonic)
            {
                const auto h = (double) harmonic;
                const auto even = (harmonic % 2 == 0) ? shape.evenBias : 1.0;
                value += even * std::sin ((double) phase * h)
                         / std::pow (h, shape.rolloff);
            }
            const auto sample = (float) value;
            frames[(size_t) frame][(size_t) i] = sample;
            peak = juce::jmax (peak, std::abs (sample));
        }
        if (peak > 0.0f)
            for (auto& sample : frames[(size_t) frame])
                sample /= peak;
    }
    regenerateMips();
}

void WavetableData::loadFromAudio (const juce::AudioBuffer<float>& source)
{
    if (source.getNumChannels() == 0 || source.getNumSamples() == 0) return;
    *this = WavetableImporter::import (source);
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
    return tableForFrameAndLevel (frameIndex, selectMipLevels (increment).detailedLevel);
}

WavetableData::MipSelection WavetableData::selectMipLevels (float increment) const noexcept
{
    if (increment <= 0.0f)
        return {};

    const float maxSafeHarmonic = 0.5f / increment;
    int detailedLevel = numMipLevels - 1;
    for (int level = 0; level < numMipLevels; ++level)
        if ((float) mipHarmonicCaps[(size_t) level] <= maxSafeHarmonic)
        {
            detailedLevel = level;
            break;
        }

    if (detailedLevel == numMipLevels - 1)
        return { detailedLevel, detailedLevel, 0.0f };

    constexpr float transitionOctaves = 0.35f;
    const auto detailedCap = (float) mipHarmonicCaps[(size_t) detailedLevel];
    const auto transitionStart = detailedCap * std::exp2 (transitionOctaves);
    const auto linearMix = juce::jlimit (0.0f, 1.0f,
                                         std::log2 (transitionStart / maxSafeHarmonic)
                                             / transitionOctaves);
    const auto smoothMix = linearMix * linearMix * (3.0f - 2.0f * linearMix);
    return { detailedLevel, detailedLevel + 1, smoothMix };
}

const std::array<float, WavetableData::tableSize>&
WavetableData::tableForFrameAndLevel (int frameIndex, int level) const noexcept
{
    const auto safeFrame = juce::jlimit (0, numTables - 1, frameIndex);
    const auto safeLevel = juce::jlimit (0, numMipLevels - 1, level);
    return mipFrames[(size_t) safeLevel][(size_t) safeFrame];
}

void WavetableOscillator::prepare (double sr) { sampleRate = sr; }
void WavetableOscillator::setFrequency (float hz) noexcept
{
    increment = hz > 0.0f ? hz / (float) sampleRate : 0.0f;
}

float WavetableOscillator::process (const WavetableData& table) noexcept
{
    if (increment >= 0.5f)
    {
        phase += increment;
        phase -= std::floor (phase);
        return 0.0f;
    }

    const float frame = position * (float) (WavetableData::numTables - 1);
    const int a = (int) frame;
    const int b = juce::jmin (a + 1, WavetableData::numTables - 1);
    const float frac = frame - (float) a;
    const float smoothFrac = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * frac));
    // Mip selection depends only on the increment, so with static pitch it is
    // effectively a per-block computation; pitch modulation simply recomputes.
    if (increment != mipSelectionIncrement)
    {
        mipSelection = table.selectMipLevels (increment);
        mipSelectionIncrement = increment;
    }
    const auto& mip = mipSelection;
    const auto& detailedA = table.tableForFrameAndLevel (a, mip.detailedLevel);
    const auto& detailedB = table.tableForFrameAndLevel (b, mip.detailedLevel);
    const auto& saferA = table.tableForFrameAndLevel (a, mip.saferLevel);
    const auto& saferB = table.tableForFrameAndLevel (b, mip.saferLevel);
    const float index = phase * (float) WavetableData::tableSize;
    const int i0 = ((int) index) & (WavetableData::tableSize - 1);
    const int i1 = (i0 + 1) & (WavetableData::tableSize - 1);
    const int i2 = (i0 + 2) & (WavetableData::tableSize - 1);
    const int i3 = (i0 + 3) & (WavetableData::tableSize - 1);
    const float t = index - std::floor (index);
    
    const auto sampleTable = [=] (const auto& samples) noexcept
    {
        return hermiteInterpolate (samples[(size_t) i0], samples[(size_t) i1],
                                   samples[(size_t) i2], samples[(size_t) i3], t);
    };
    const float detailedFrameA = sampleTable (detailedA);
    const float detailedFrameB = sampleTable (detailedB);
    const float saferFrameA = mip.saferMix > 0.0f ? sampleTable (saferA) : detailedFrameA;
    const float saferFrameB = mip.saferMix > 0.0f ? sampleTable (saferB) : detailedFrameB;
    const float va = juce::jmap (mip.saferMix, detailedFrameA, saferFrameA);
    const float vb = juce::jmap (mip.saferMix, detailedFrameB, saferFrameB);
    phase += increment;
    phase -= std::floor (phase);
    return juce::jmap (smoothFrac, va, vb);
}
