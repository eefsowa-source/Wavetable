#pragma once

#include <JuceHeader.h>

#include "Dsp/Zdf.h"

#include <algorithm>
#include <array>
#include <cmath>

// Per-channel multimode filter with four real slopes (Plan B Task 4).
//
// The slope selector used to be cosmetic. The voice only switched on a second
// 12 dB section at index >= 2, so index 0 and 1 rendered bit-identical audio and
// index 2 and 3 did as well, and the two places that name the slopes disagreed
// with each other and with the DSP (the parameter said 12/12/24/24, the editor
// said 8/12/18/24). Measured on the render path before this file existed, with a
// 3 kHz cutoff and 90 % resonance, slope 0 against slope 1 differed by exactly
// 0.0.
//
// The slopes are now built from eon_dsp TPT sections, which keep the cutoff
// accurate under per-sample modulation and stay stable at any resonance:
//
//   index 0 (6 dB/oct) : one TPT pole
//   index 1 (12 dB/oct): one eon::SvfTPT section
//   index 2 (18 dB/oct): eon::SvfTPT section -> one TPT pole
//   index 3 (24 dB/oct): eon::SvfTPT section -> eon::SvfTPT at Q = 1/sqrt(2)
//
// The resonance parameter damps the first section only, so a steeper slope does
// not multiply the peaking. At a single pole there is nothing to resonate, which
// the 6 dB/oct label admits instead of promising a filter that cannot ring.
//
// The band-pass mode keeps the same pole count as the label but splits it into
// two equal skirts: a cascade of one-pole high-pass sections into the same
// number of one-pole low-pass sections. Splitting avoids the asymmetric skirt a
// "band-pass tap then low-pass" cascade produces, and it is why the band-pass
// mode ignores resonance: there is no first section to damp.
class SlopeFilter
{
public:
    // Type order matches the host parameter: low-pass, high-pass, band-pass.
    enum class Type : int { lowpass = 0, highpass = 1, bandpass = 2 };

    // Poles per skirt for the band-pass split, and therefore the number of
    // one-pole sections that mode needs on each side.
    static constexpr int maximumPolesPerSide = 4;

    void reset() noexcept
    {
        svf1.reset();
        svf2.reset();
        for (auto& pole : highPassPoles) pole.reset();
        for (auto& pole : lowPassPoles)  pole.reset();
        bandPassMakeup = 1.0;
    }

    // Called once per sample. The TPT coefficient is computed here and shared by
    // every section, so a four-pole slope still costs one tan() rather than four.
    void setParams (int slopeIndex, int typeIndex, double cutoffHz,
                    double resonance, double sampleRate) noexcept
    {
        slope = juce::jlimit (0, 3, slopeIndex);
        type  = static_cast<Type> (juce::jlimit (0, 2, typeIndex));

        const double nyquistLimit = 0.49 * sampleRate;
        const double g = std::tan (juce::MathConstants<double>::pi
                                   * std::min (cutoffHz, nyquistLimit) / sampleRate);
        const double q = std::max (0.05, resonance);

        svf1.g = g;
        svf1.R = 1.0 / (2.0 * q);
        // The second section is Butterworth so a steeper slope adds attenuation
        // instead of a second resonant peak.
        svf2.g = g;
        svf2.R = 1.0 / (2.0 * juce::MathConstants<double>::sqrt2);

        for (auto& pole : highPassPoles) pole.g = g;
        for (auto& pole : lowPassPoles)  pole.g = g;

        // Each high-pass/low-pass pole pair loses 6 dB at the corner, so the
        // split band-pass is normalised back to roughly unity at its centre.
        bandPassMakeup = std::exp2 ((double) (slope + 1));
    }

    float process (float input) noexcept
    {
        const double x = (double) input;
        const double y = type == Type::bandpass ? processBandPass (x)
                                                : processLowOrHigh (x);
        return std::isfinite (y) ? (float) y : 0.0f;
    }

private:
    double processLowOrHigh (double x) noexcept
    {
        switch (slope)
        {
            case 0:
            {
                const double low = lowPassPoles[0].process (x);
                return type == Type::highpass ? x - low : low;
            }
            case 1:
            {
                const auto out = svf1.process (x);
                return type == Type::highpass ? out.hp : out.lp;
            }
            case 2:
            {
                const auto out = svf1.process (x);
                const double section = type == Type::highpass ? out.hp : out.lp;
                const double low = lowPassPoles[0].process (section);
                return type == Type::highpass ? section - low : low;
            }
            default:
            {
                const auto first = svf1.process (x);
                const auto second = svf2.process (type == Type::highpass ? first.hp : first.lp);
                return type == Type::highpass ? second.hp : second.lp;
            }
        }
    }

    double processBandPass (double x) noexcept
    {
        const int polesPerSide = juce::jlimit (1, maximumPolesPerSide, slope + 1);
        double y = x;
        for (int i = 0; i < polesPerSide; ++i)
            y -= highPassPoles[(size_t) i].process (y);
        for (int i = 0; i < polesPerSide; ++i)
            y = lowPassPoles[(size_t) i].process (y);
        return y * bandPassMakeup;
    }

    eon::SvfTPT svf1, svf2;
    std::array<eon::OnePoleTPT, maximumPolesPerSide> highPassPoles, lowPassPoles;
    int slope = 1;
    Type type = Type::lowpass;
    double bandPassMakeup = 1.0;
};
