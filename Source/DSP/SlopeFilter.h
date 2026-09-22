#pragma once

#include <JuceHeader.h>

#include "Dsp/Adaa.h"
#include "Dsp/Zdf.h"

#include <algorithm>
#include <array>
#include <cmath>

// Per-channel multimode filter with four real slopes and a nonlinear drive
// node (Plan B Task 4).
//
// Slopes. The selector used to be cosmetic: the voice only switched on a second
// 12 dB section at index >= 2, so index 0 and 1 rendered bit-identical audio and
// index 2 and 3 did as well, and the two places that name the slopes disagreed
// with each other and with the DSP (the parameter said 12/12/24/24, the editor
// said 8/12/18/24). Measured on the render path before this file existed, with a
// 3 kHz cutoff and 90 % resonance, slope 0 against slope 1 differed by exactly
// 0.0.
//
// The slopes are built from eon_dsp TPT sections, which keep the cutoff accurate
// under per-sample modulation and stay stable at any resonance:
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
//
// Drive. `filterDrive` used to be a linear pre-gain in the voice
// (Decibels::decibelsToGain) with the soft clip sitting after the whole filter,
// so it was a level control. The filter now owns it and clips the node the
// signal enters through, which is where an analog filter's input stage
// saturates.
//
// The clipper is the family tanh(k v) / k, which is exactly the identity at
// k = 0. That makes a 0 dB setting a hard bypass instead of "a very gentle
// waveshaper", so the clean path stays the filter it replaced (pinned by a test
// against eon::SvfTPT). k rises with the drive parameter, and the output carries
// a makeup that restores unity at `driveReferenceLevel`, so turning the knob up
// compresses: content below the reference is lifted and content above it is
// held. Below 0 dB the parameter stays a plain gain and the clipper is off,
// which keeps the attenuation half of the range behaving as it always did.
//
// The clip runs through eon::ADAA1 with tanh's exact first antiderivative, the
// same alias suppression the saturation stage uses, because this node is at the
// host rate.
//
// Not done here: saturating the resonant node itself. For a low-pass at a
// normal cutoff the state-variable high-pass node, which is the point the two
// integrators are fed from, carries only the fraction of the input that sits
// below the corner, so a drive knob wired there reads 0 dB of effect at 20 kHz
// cutoff and only bites when the cutoff is under the signal. That is real
// behaviour of the circuit, not a drive control, and it is why the drive node
// sits at the input for now.
class SlopeFilter
{
public:
    // Type order matches the host parameter: low-pass, high-pass, band-pass.
    enum class Type : int { lowpass = 0, highpass = 1, bandpass = 2 };

    // Poles per skirt for the band-pass split, and therefore the number of
    // one-pole sections that mode needs on each side.
    static constexpr int maximumPolesPerSide = 4;

    // Drive mapping. `driveRangeDecibels` is the parameter value that reaches
    // `maximumDriveDepth`; `driveReferenceLevel` is the amplitude the makeup
    // leaves untouched.
    static constexpr double driveRangeDecibels = 24.0;
    static constexpr double maximumDriveDepth = 7.0;
    static constexpr double driveReferenceLevel = 0.6;

    // Below this depth the clipper is treated as off rather than as a very
    // gentle one. The reason is numerical, not aesthetic: F1 for tanh is
    // ln(cosh w) = -ln2 + w^2/2 near zero, so as k - 0 the divided difference
    // cancels against the constant ln2 at double precision and the 1/k makeup
    // amplifies that rounding residue instead of signal. Measured: a drive
    // parameter that floats three float-ULPs above 0 dB (3.6e-7 dB) left the
    // clipper active and turned the clean path into broadband noise. 1e-3 of
    // depth is about -60 dB of clipping effect, so calling it off is honest.
    static constexpr double minimumActiveDriveDepth = 1.0e-3;

    void reset() noexcept
    {
        svf1.reset();
        svf2.reset();
        driveNode.reset();
        for (auto& pole : highPassPoles) pole.reset();
        for (auto& pole : lowPassPoles)  pole.reset();
        bandPassMakeup = 1.0;
        driveDepth = 0.0;
        driveMakeup = 1.0;
        preGain = 1.0;
    }

    // Called once per sample. The TPT coefficient is computed here and shared by
    // every section, so a four-pole slope still costs one tan() rather than four.
    void setParams (int slopeIndex, int typeIndex, double cutoffHz,
                    double resonance, double driveDecibels, double sampleRate) noexcept
    {
        slope = juce::jlimit (0, 3, slopeIndex);
        type  = static_cast<Type> (juce::jlimit (0, 2, typeIndex));

        const double g = std::tan (juce::MathConstants<double>::pi
                                   * std::min (cutoffHz, 0.49 * sampleRate) / sampleRate);
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

        // Negative drive stays a plain gain, so the attenuation half of the
        // range keeps the behaviour it always had; from 0 dB up the clipper
        // takes over and the depth rises with the parameter.
        preGain = driveDecibels <= 0.0 ? std::pow (10.0, driveDecibels / 20.0) : 1.0;
        driveDepth = maximumDriveDepth
                     * juce::jlimit (0.0, 1.0, driveDecibels / driveRangeDecibels);
        const double argument = driveDepth * driveReferenceLevel;
        driveMakeup = driveDepth <= 0.0 ? 1.0 : argument / std::tanh (argument);
    }

    float process (float input) noexcept
    {
        const double x = saturateDriveNode ((double) input * preGain);
        const double y = type == Type::bandpass ? processBandPass (x)
                                                : processLowOrHigh (x);
        return std::isfinite (y) ? (float) y : 0.0f;
    }

private:
    // Soft clip on the node the section cascade is fed from. k = 0 is a hard
    // bypass, so the clean path is not merely close to linear, it is the linear
    // filter.
    inline double saturateDriveNode (double v) noexcept
    {
        if (driveDepth < minimumActiveDriveDepth)
            return v;
        // The ADAA state lives in the argument domain (w = k*v), so a change in k
        // does not invalidate the antiderivative stored for the previous sample.
        const float clipped = driveNode.process ((float) (driveDepth * v),
                                                 [] (double u) { return std::tanh (u); },
                                                 [] (double u) { return eon::tanhF1 (u); });
        return (double) clipped * driveMakeup / driveDepth;
    }

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
    eon::ADAA1 driveNode;
    std::array<eon::OnePoleTPT, maximumPolesPerSide> highPassPoles, lowPassPoles;
    int slope = 1;
    Type type = Type::lowpass;
    double bandPassMakeup = 1.0;
    double driveDepth = 0.0;
    double driveMakeup = 1.0;
    double preGain = 1.0;
};
