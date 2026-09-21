#pragma once

#include <Dsp/Adaa.h>
#include <Dsp/Oversampling.h>

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>
#include <vector>

// Per-voice saturation stage.
//
// Replaces the previous asymmetric tanh wrapped in juce::dsp::Oversampling
// (2x, half-band polyphase IIR) for three reasons:
//
//  1. The old curve, tanh(x + 0.12x^2) - 0.118*tanh(0.12x^2), has no elementary
//     antiderivative, so antiderivative anti-aliasing could not be applied to it
//     at all. The curve is now a tanh with a fixed input bias, whose exact
//     antiderivative is available, so eon::ADAA1 removes a large part of the
//     aliasing at its source instead of trying to filter it afterwards.
//  2. The bias keeps the asymmetric (even-harmonic) character of the old curve.
//     The bias was fitted so the default-drive harmonic profile stays put: at
//     the stock drive of 0.15 the old curve measures H2/H1 -34.1 dB and
//     H3/H1 -36.2 dB, and bias 0.10 measures -34.0 dB and -36.4 dB with the
//     fundamental within 0.1 dB. Above the default drive the two curves diverge
//     (the old one's second harmonic collapses under its own asymmetry term),
//     which is a deliberate part of this change and belongs to the listening
//     gate rather than to any automated one.
//  3. eon's half-band FIR has a far deeper stopband than the JUCE IIR
//     polyphase filter, so less of the generated high-order content folds back
//     into the audio band.
//
// Latency is integral at the stage count in use (measured for an impulse round
// trip: 35 samples at 2x, 46 at 4x, 51 at 8x), so the plug-in can report it to
// the host without a fractional-sample remainder.
//
// Header-only on purpose: it keeps the CMake dependency surface at "add the
// vendored eon include path", with no new translation unit to wire into every
// target that compiles a voice.
class SaturationStage
{
public:
    // 2x. One stage uses the strongest half-band kernel (N=71, -113 dB
    // stopband) and gives an integral round-trip latency.
    //
    // Do not drop this to 0 on the assumption that the antiderivative alone is
    // enough: measured on the folded-third-harmonic proxy with the stock probe
    // (48 kHz, 10 kHz tone), antiderivative anti-aliasing *without*
    // oversampling produces -62.7 dBc at full drive, which is 46 dB worse than
    // the JUCE IIR oversampler this stage replaced. First-order ADAA is weak
    // when the tone sits a fifth of the way up the band. With 2x it reaches the
    // render's own noise floor (-156.5 dBc). The oversampler is doing most of
    // the work; ADAA is what removes the residue the filter leaves behind.
    static constexpr int oversamplingStages = 1;

    // Input bias of the soft clip. See the note above on how it was fitted.
    static constexpr double asymmetryBias = 0.10;

    void prepare (int maximumBlockSize)
    {
        maximumBlockSizeInternal = juce::jmax (1, maximumBlockSize);
        if constexpr (oversamplingStages == 0)
        {
            // No multirate stage, so no latency to report and no filter state.
            latencySamples = 0;
            leftShaper.reset();
            rightShaper.reset();
            return;
        }
        const auto setUp = [this] (eon::Oversampler& oversampler)
        {
            oversampler.setStages (oversamplingStages);
            oversampler.prepare (maximumBlockSizeInternal);
            oversampler.reset();
        };
        setUp (leftOversampler);
        setUp (rightOversampler);
        const auto scratch = (size_t) maximumBlockSizeInternal * (size_t) leftOversampler.factor();
        leftScratch.assign (scratch, 0.0f);
        rightScratch.assign (scratch, 0.0f);
        reset();
        latencySamples = (int) std::lround (leftOversampler.latencySamples());
    }

    void reset()
    {
        leftOversampler.reset();
        rightOversampler.reset();
        leftShaper.reset();
        rightShaper.reset();
    }

    int getLatencySamples() const noexcept { return latencySamples; }

    // In place, over the first numSamples of each channel. right may be null
    // for a mono voice.
    void process (float* left, float* right, int numSamples) noexcept
    {
        if (left == nullptr || numSamples <= 0)
            return;

        if constexpr (oversamplingStages == 0)
        {
            // Antiderivative anti-aliasing alone, at the host rate. Kept as a
            // measured option: it halves the antiderivative cost and drops the
            // half-band filter entirely, at the price of folding whatever
            // content the antiderivative leaves above Nyquist.
            for (int i = 0; i < numSamples; ++i)
            {
                left[i] = shape (leftShaper, left[i]);
                if (right != nullptr)
                    right[i] = shape (rightShaper, right[i]);
            }
            return;
        }

        leftOversampler.up (left, numSamples, leftScratch.data());
        const int upSamples = numSamples * leftOversampler.factor();

        if (right != nullptr)
        {
            rightOversampler.up (right, numSamples, rightScratch.data());
            for (int i = 0; i < upSamples; ++i)
            {
                leftScratch[(size_t) i] = shape (leftShaper, leftScratch[(size_t) i]);
                rightScratch[(size_t) i] = shape (rightShaper, rightScratch[(size_t) i]);
            }
            rightOversampler.down (rightScratch.data(), right, numSamples);
        }
        else
        {
            for (int i = 0; i < upSamples; ++i)
                leftScratch[(size_t) i] = shape (leftShaper, leftScratch[(size_t) i]);
        }

        leftOversampler.down (leftScratch.data(), left, numSamples);
    }

private:
    static float shape (eon::ADAA1& adaa, float x) noexcept
    {
        constexpr double bias = asymmetryBias;
        const double offset = std::tanh (bias);
        // f(v)  = tanh(v + b) - tanh(b)          (DC-free asymmetric clip)
        // F1(v) = ln(cosh(v + b)) - tanh(b) * v  (exact, via eon::tanhF1)
        return adaa.process (x,
                             [offset] (double v) { return std::tanh (v + bias) - offset; },
                             [offset] (double v) { return eon::tanhF1 (v + bias) - offset * v; });
    }

    eon::Oversampler leftOversampler, rightOversampler;
    eon::ADAA1 leftShaper, rightShaper;
    std::vector<float> leftScratch, rightScratch;
    int maximumBlockSizeInternal = 1;
    int latencySamples = 0;
};
