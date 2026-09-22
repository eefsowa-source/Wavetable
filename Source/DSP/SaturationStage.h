#pragma once

#include <Dsp/Adaa.h>
#include <Dsp/Oversampling.h>

#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Per-voice saturation stage with three quality tiers.
//
// The stage shapes a slowly saturating asymmetric curve; the tiers trade alias
// suppression against CPU:
//
//   Eco    : half-band FIR 2x + the legacy curve (tanh(x + 0.12x^2)
//            - 0.118*tanh(0.12x^2)). Faster than Normal (no antiderivative
//            transcendentals), while retaining the pre-Task-3 curve character.
//   Normal : half-band FIR 2x + antiderivative anti-aliasing on a biased tanh.
//            The shipping quality since Task 3; reaches the render's own noise
//            floor on the folded-third-harmonic proxy.
//   High   : half-band FIR 4x + the same antiderivative stage.
//
// Measured folded-third-harmonic alias at full drive, stock probe
// (48 kHz / 10 kHz tone): all three tiers land at the render's float32 floor
// (Eco -153.1, Normal -156.6, High -156.7 dBc). ADAA alone (no oversampling at all)
// is not a viable tier: it measures -62.7 dBc, 40 dB *worse* than Eco, because
// first-order ADAA is weak when the tone sits a fifth of the way up the band.
//
// Latency: 2x round trip peaks at 35 samples and 4x at 47 (46.5 theoretical).
// The stage reports 47 for every tier and pads the lower two by 12 samples so a
// quality change never moves the reported latency, and therefore never needs
// the host to re-buffer. The oversampler objects are reused across tiers, only
// their stage count changes, so a voice carries one set of filter state.
class SaturationStage
{
public:
    enum class Quality : int { eco = 0, normal = 1, high = 2 };

    // Input bias of the ADAA soft clip. Fitted so the default-drive harmonic
    // profile of the legacy curve is preserved (H2/H1 -34.1 db, H3/H1 -36.2 dB
    // on the legacy curve; bias 0.10 measures -33.9 dB and -36.6 dB).
    static constexpr double asymmetryBias = 0.10;

    // The latency every tier reports. Eco/Normal have 35 samples of filter
    // latency plus 12 samples of compensation; High's 46.5-sample filter
    // latency has its impulse peak at sample 47 and needs no compensation.
    static constexpr int reportedLatencySamples = 47;

    void prepare (int maximumBlockSize)
    {
        maximumBlockSizeInternal = juce::jmax (1, maximumBlockSize);
        // Size the filter scratch and delay lines for the deepest resampling
        // we can use (4x), then let the active tier pick its stage count.
        leftOversampler.setStages (2);
        rightOversampler.setStages (2);
        leftOversampler.prepare (maximumBlockSizeInternal);
        rightOversampler.prepare (maximumBlockSizeInternal);
        const auto scratch = (size_t) maximumBlockSizeInternal * (size_t) leftOversampler.factor();
        leftScratch.assign (scratch, 0.0f);
        rightScratch.assign (scratch, 0.0f);
        reset();
    }

    void reset()
    {
        resetNonlinearState();
        padDelay.fill ({ 0.0f, 0.0f });
        padHead = 0;
    }

    int getLatencySamples() const noexcept { return reportedLatencySamples; }

    void setQuality (Quality newQuality) noexcept
    {
        if (newQuality == quality)
            return;
        quality = newQuality;
        // Filter and antiderivative state depend on the active tier. Keep the
        // fixed-latency history alive so changing quality does not inject an
        // avoidable 47-sample hole into the voice.
        resetNonlinearState();
    }

    Quality getQuality() const noexcept { return quality; }

    // The dry path still has to honour the latency reported to the host. It
    // also keeps the delay history moving, so a drive ramp can enter or leave
    // the nonlinear path without reading stale compensation samples.
    void processBypass (float* left, float* right, int numSamples) noexcept
    {
        if (left == nullptr || numSamples <= 0)
            return;
        if (! bypassActive)
        {
            resetNonlinearState();
            bypassActive = true;
        }
        delayOutput (left, right, numSamples, reportedLatencySamples);
    }

    // In place, over the first numSamples of each channel. right may be null.
    void process (float* left, float* right, int numSamples) noexcept
    {
        if (left == nullptr || numSamples <= 0)
            return;
        if (bypassActive)
        {
            resetNonlinearState();
            bypassActive = false;
        }

        const int stages = (quality == Quality::high) ? 2 : 1;
        leftOversampler.setStages (stages);
        const bool applyAda = (quality != Quality::eco);

        leftOversampler.up (left, numSamples, leftScratch.data());
        const int upSamples = numSamples * (int) leftOversampler.factor();

        if (right != nullptr)
        {
            rightOversampler.setStages (stages);
            rightOversampler.up (right, numSamples, rightScratch.data());
            if (applyAda)
                for (int i = 0; i < upSamples; ++i)
                {
                    leftScratch[(size_t) i] = shapeAda (leftShaper, leftScratch[(size_t) i]);
                    rightScratch[(size_t) i] = shapeAda (rightShaper, rightScratch[(size_t) i]);
                }
            else
                for (int i = 0; i < upSamples; ++i)
                {
                    leftScratch[(size_t) i] = shapeLegacy (leftScratch[(size_t) i]);
                    rightScratch[(size_t) i] = shapeLegacy (rightScratch[(size_t) i]);
                }
            rightOversampler.down (rightScratch.data(), right, numSamples);
        }
        else
        {
            if (applyAda)
                for (int i = 0; i < upSamples; ++i)
                    leftScratch[(size_t) i] = shapeAda (leftShaper, leftScratch[(size_t) i]);
            else
                for (int i = 0; i < upSamples; ++i)
                    leftScratch[(size_t) i] = shapeLegacy (leftScratch[(size_t) i]);
        }

        leftOversampler.down (leftScratch.data(), left, numSamples);

        const int tierLatency = (stages == 2) ? 47 : 35;
        delayOutput (left, right, numSamples, reportedLatencySamples - tierLatency);
    }

private:
    void resetNonlinearState() noexcept
    {
        leftOversampler.reset();
        rightOversampler.reset();
        leftShaper.reset();
        rightShaper.reset();
        bypassActive = false;
    }
    static float shapeLegacy (float x) noexcept
    {
        const float xOffset = x + 0.12f * x * x;
        return std::tanh (xOffset) - 0.118f * std::tanh (0.12f * x * x);
    }

    static float shapeAda (eon::ADAA1& adaa, float x) noexcept
    {
        constexpr double bias = asymmetryBias;
        const double offset = std::tanh (bias);
        // f(v)  = tanh(v + b) - tanh(b)          (DC-free asymmetric clip)
        // F1(v) = ln(cosh(v + b)) - tanh(b) * v  (exact, via eon::tanhF1)
        return adaa.process (x,
                             [offset] (double v) { return std::tanh (v + bias) - offset; },
                             [offset] (double v) { return eon::tanhF1 (v + bias) - offset * v; });
    }

    void delayOutput (float* left, float* right, int numSamples, int delaySamples) noexcept
    {
        if (numSamples <= 0)
            return;
        for (int i = 0; i < numSamples; ++i)
        {
            const int read = (padHead + reportedLatencySamples - delaySamples) % reportedLatencySamples;
            const float inL = left[i];
            const float inR = right != nullptr ? right[i] : 0.0f;
            if (delaySamples > 0)
            {
                left[i] = padDelay[(size_t) read].first;
                if (right != nullptr)
                    right[i] = padDelay[(size_t) read].second;
            }
            padDelay[(size_t) padHead] = { inL, inR };
            padHead = (padHead + 1) % reportedLatencySamples;
        }
    }

    eon::Oversampler leftOversampler, rightOversampler;
    eon::ADAA1 leftShaper, rightShaper;
    std::vector<float> leftScratch, rightScratch;
    std::array<std::pair<float, float>, reportedLatencySamples> padDelay;
    int padHead = 0;
    int maximumBlockSizeInternal = 1;
    Quality quality = Quality::normal;
    bool bypassActive = false;
};
