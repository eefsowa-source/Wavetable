// Vendored-copy contract tests for third_party/eon_dsp.
//
// Purpose: prove that the copy checked into this repository (see
// third_party/eon_dsp/PROVENANCE.md, rev c8e71f3) actually compiles in this
// project's toolchain and still satisfies the contracts the SEOUL DSP upgrade
// plans depend on. This is a *vendored copy* guard, not a re-validation of the
// eon_dsp project itself: the upstream suite at eon_dsp/Tests covers far more.
//
// Deliberately JUCE-free. eon_dsp must stay framework-independent so the same
// headers can serve plugins, iPlug2 projects, and headless measurement tools;
// this test fails to build if someone couples the vendored copy to JUCE.
//
// Consumer mapping (Plan B):
//   ADAA1/ADAA2 + Oversampler -> Task 3 (saturation stage)
//   Solvers + Zdf             -> Task 4 (nonlinear filter drive, real slopes)
//   Triode/Transformer/Stages  -> Task 5 (analog color, conditional)
//   DCBlocker/RtGuard/Rng      -> Task 6 (output safety, RT hygiene, determinism)
//   Measure                    -> Task 2 (alias proxy measurement)

#include "Dsp/Adaa.h"
#include "Dsp/Measure.h"
#include "Dsp/Oversampling.h"
#include "Dsp/Rng.h"
#include "Dsp/RtGuard.h"
#include "Dsp/Solvers.h"
#include "Dsp/Stages.h"
#include "Dsp/Transformer.h"
#include "Dsp/Triode.h"
#include "Dsp/Wdf.h"
#include "Dsp/Zdf.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const char* message)
{
    std::printf ("%s  %s\n", condition ? "ok  " : "FAIL", message);
    if (! condition)
        ++failures;
}

bool near (double actual, double expected, double tolerance)
{
    return std::isfinite (actual) && std::abs (actual - expected) <= tolerance;
}

bool finite (double value) { return std::isfinite (value); }

// Deterministic, non-bin-aligned probe tone so no measurement can accidentally
// land on an exact FFT bin or an exact period.
float probeTone (int i, double hz, double sr)
{
    return (float) (0.7 * std::sin (2.0 * std::numbers::pi * hz * (double) i / sr));
}

// ---------------------------------------------------------------------------
// ADAA
// ---------------------------------------------------------------------------

void testAdaaFiniteness()
{
    // Repeated-node branch: x0 == x2 must collapse to the analytic limit
    // f(x0) instead of dividing by ~0.
    {
        eon::SoftClipSat sat;
        sat.reset();
        sat.process (0.5f);
        sat.process (0.5f);
        const float y = sat.process (0.5f);
        const double expected = eon::SoftClip::f (0.5);
        check (near (y, expected, 1.0e-6),
               "ADAA2 repeated-node branch returns the analytic limit f(x0)");
    }

    // Near-duplicate samples (the numerically hostile case) must stay finite
    // and bounded for the whole antiderivative arithmetic.
    {
        eon::SoftClipSat sat;
        sat.reset();
        bool ok = true;
        double worst = 0.0;
        float x = 0.3f;
        for (int i = 0; i < 512; ++i)
        {
            x = std::nextafter (x, 0.31f);
            const double y = sat.process (x);
            ok = ok && finite (y) && std::abs (y) < 1.0;
            worst = std::max (worst, std::abs (y));
        }
        check (ok, "ADAA2 stays finite and bounded across nextafter-close samples");
        check (worst <= eon::SoftClip::f (1.0) + 1.0e-9,
               "ADAA2 output never exceeds the soft-clip ceiling");
    }

    // tanh ADAA1: the antiderivative must stay usable at large |v|, which is
    // exactly where the naive log(cosh(v)) form overflows.
    {
        check (near (eon::tanhF1 (100.0), 100.0 - std::log (2.0), 1.0e-9),
               "tanhF1 is stable at large positive arguments");
        check (near (eon::tanhF1 (-100.0), eon::tanhF1 (100.0), 1.0e-12),
               "tanhF1 is even, matching ln(cosh v)");
        check (near (eon::tanhF1 (0.0), 0.0, 1.0e-12),
               "tanhF1(0) is zero");
    }

    {
        eon::TanhSat sat;
        sat.reset();
        bool ok = true;
        for (int i = 0; i < 4096; ++i)
        {
            const double y = sat.process (probeTone (i, 997.0, 48000.0));
            ok = ok && finite (y) && std::abs (y) <= 1.0;
        }
        check (ok, "ADAA1 tanh saturation stays finite and within tanh bounds");
    }
}

// ---------------------------------------------------------------------------
// Oversampling
// ---------------------------------------------------------------------------

template <typename Taps>
void testTapSet (const char* label, int expectedP)
{
    constexpr int L = (int) (sizeof (Taps::hE) / sizeof (double));
    bool symmetric = true;
    double sum = 0.0;
    for (int i = 0; i < L; ++i)
    {
        sum += Taps::hE[i];
        if (std::abs (Taps::hE[i] - Taps::hE[L - 1 - i]) > 1.0e-15)
            symmetric = false;
    }
    char message[160];

    std::snprintf (message, sizeof (message),
                   "%s even taps are symmetric", label);
    check (symmetric, message);

    // Half-band: the even-tap branch doubles, so the taps must sum to 0.5 for
    // unit reconstruction gain.
    std::snprintf (message, sizeof (message),
                   "%s odd-branch gain is unity (2 * sum(hE) == 1)", label);
    check (near (2.0 * sum, 1.0, 1.0e-4), message);

    std::snprintf (message, sizeof (message),
                   "%s port delay p == %d and fits the even-branch cap", label, expectedP);
    check (Taps::p == expectedP && Taps::p < eon::HalfBand2x<Taps>::eCap, message);
}

std::vector<float> renderRoundTrip (int blockSize, int stages)
{
    constexpr int total = 8192;
    eon::Oversampler oversampler;
    oversampler.setStages (stages);
    oversampler.prepare (blockSize);

    std::vector<float> input ((size_t) blockSize);
    std::vector<float> up ((size_t) blockSize * (size_t) oversampler.factor());
    std::vector<float> output ((size_t) total);

    for (int offset = 0; offset < total; offset += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
            input[(size_t) i] = probeTone (offset + i, 1000.0, 48000.0);
        oversampler.up (input.data(), blockSize, up.data());
        oversampler.down (up.data(), output.data() + offset, blockSize);
    }
    return output;
}

void testOversampler()
{
    testTapSet<eon::HbTaps71> ("HbTaps71", 17);
    testTapSet<eon::HbTaps47> ("HbTaps47", 11);
    testTapSet<eon::HbTaps35> ("HbTaps35", 8);

    for (const int stages : { 1, 2, 3 })
    {
        eon::Oversampler oversampler;
        oversampler.setStages (stages);
        oversampler.prepare (64);

        std::vector<float> input (64, 1.0f);
        std::vector<float> up ((size_t) 64 * (size_t) oversampler.factor());
        std::vector<float> output (64);
        double last = 0.0;
        for (int block = 0; block < 200; ++block)
        {
            oversampler.up (input.data(), 64, up.data());
            oversampler.down (up.data(), output.data(), 64);
            last = output[63];
        }

        char message[160];
        std::snprintf (message, sizeof (message),
                       "oversampler round trip has unity DC gain at %d stage(s)", stages);
        check (near (last, 1.0, 1.0e-4), message);
    }

    // The host may partition a stream arbitrarily. A multirate stage that
    // changes output when the block size changes is unusable in a plug-in.
    for (const int stages : { 1, 2, 3 })
    {
        const auto small = renderRoundTrip (16, stages);
        const auto large = renderRoundTrip (256, stages);
        double peak = 0.0;
        for (size_t i = 256; i < small.size(); ++i)
            peak = std::max (peak, std::abs ((double) small[i] - (double) large[i]));

        char message[160];
        std::snprintf (message, sizeof (message),
                       "oversampler output at %d stage(s) is independent of block partition", stages);
        check (peak < 1.0e-6, message);
    }
}

// ---------------------------------------------------------------------------
// ZDF filters
// ---------------------------------------------------------------------------

double measureSineAmplitude (auto&& process, double hz, double sr, int samples)
{
    double peak = 0.0;
    const int warmup = samples / 2;
    for (int i = 0; i < samples; ++i)
    {
        const double y = process (probeTone (i, hz, sr) / 0.7);
        if (i > warmup)
            peak = std::max (peak, std::abs (y));
    }
    return peak;
}

void testZdfFilters()
{
    // Ladder4 without saturation is the linear reference the drive path is
    // calibrated against, so its DC gain must be exactly one.
    {
        eon::Ladder4 ladder;
        ladder.setCutoff (1000.0, 48000.0);
        ladder.setResonance (0.0);
        ladder.saturate = false;
        ladder.reset();
        double y = 0.0;
        for (int i = 0; i < 40000; ++i)
            y = ladder.process (1.0);
        check (near (y, 1.0, 1.0e-6), "Ladder4 unsaturated DC gain is unity");
    }

    // With saturation enabled the input itself is shaped too, so a DC input of
    // 1.0 settles at tanh(1.0). Pin that so Task 4 cannot silently change it.
    {
        eon::Ladder4 ladder;
        ladder.setCutoff (1000.0, 48000.0);
        ladder.setResonance (0.0);
        ladder.saturate = true;
        ladder.reset();
        double y = 0.0;
        for (int i = 0; i < 40000; ++i)
            y = ladder.process (1.0);
        check (near (y, std::tanh (1.0), 1.0e-6),
               "Ladder4 saturating DC gain matches tanh(x)/x shaping");
    }

    // Self-oscillation region must stay bounded and finite: this is the
    // failure mode that makes a resonant filter unusable at high resonance.
    {
        eon::Ladder4 ladder;
        ladder.setCutoff (1000.0, 48000.0);
        ladder.setResonance (3.5);
        ladder.saturate = true;
        ladder.reset();
        bool ok = true;
        double worst = 0.0;
        for (int i = 0; i < 100000; ++i)
        {
            const double y = ladder.process (probeTone (i, 220.0, 48000.0));
            ok = ok && finite (y);
            worst = std::max (worst, std::abs (y));
        }
        check (ok && worst <= 1.0 + 1.0e-9,
               "Ladder4 at high resonance stays finite and bounded");
    }

    {
        eon::Ladder4 ladder;
        ladder.setCutoff (1000.0, 48000.0);
        ladder.setResonance (0.0);
        ladder.saturate = false;
        ladder.reset();
        const double amplitude = measureSineAmplitude (
            [&ladder] (float x) { return ladder.process (x); }, 10000.0, 48000.0, 48000);
        check (amplitude < 0.1,
               "4-pole ladder attenuates a tone one decade above cutoff by at least 20 dB");
    }

    // State-variable outputs: lowpass passes DC, highpass/bandpass reject it.
    {
        eon::SvfTPT svf;
        svf.setParams (1000.0, 0.7071067811865476, 48000.0);
        svf.reset();
        eon::SvfTPT::Out out {};
        for (int i = 0; i < 40000; ++i)
            out = svf.process (1.0);
        check (near (out.lp, 1.0, 1.0e-6), "SvfTPT lowpass DC gain is unity");
        check (near (out.hp, 0.0, 1.0e-6), "SvfTPT highpass rejects DC");
        check (near (out.bp, 0.0, 1.0e-6), "SvfTPT bandpass rejects DC");
    }

    // Cutoff is modulated per sample, so track a moving cutoff and confirm the
    // filter keeps producing finite output rather than blowing up mid-sweep.
    {
        eon::SvfTPT svf;
        svf.reset();
        bool ok = true;
        for (int i = 0; i < 48000; ++i)
        {
            const double fc = 60.0 * std::exp2 (7.0 * std::sin (2.0 * std::numbers::pi * (double) i / 48000.0));
            svf.setParams (fc, 0.9, 48000.0);
            ok = ok && finite (svf.process (probeTone (i, 440.0, 48000.0)).lp);
        }
        check (ok, "SvfTPT stays finite under per-sample cutoff modulation");
    }
}

// ---------------------------------------------------------------------------
// Output safety, RT hygiene, determinism
// ---------------------------------------------------------------------------

void testOutputAndRtHelpers()
{
    // DC removal: a constant input is rejected, and the transient decays with
    // the configured time constant (~8.85 ms at 18 Hz / 48 kHz).
    {
        eon::DCBlocker blocker;
        blocker.prepare (48000.0, 18.0);
        blocker.reset();
        double y = 0.0;
        for (int i = 0; i < 20000; ++i)
            y = blocker.process (1.0f);
        check (near (y, 0.0, 1.0e-6), "DCBlocker removes a constant (DC) input");
    }

    // ... while leaving audio-band content essentially untouched.
    {
        eon::DCBlocker blocker;
        blocker.prepare (48000.0, 18.0);
        blocker.reset();
        const double peak = measureSineAmplitude (
            [&blocker] (float x) { return blocker.process (x); }, 1000.0, 48000.0, 48000);
        check (near (peak, 1.0, 2.0e-3),
               "DCBlocker passes a 1 kHz tone within 0.2 percent");
    }

    // Invalid setup must fail closed rather than emit NaN into the output.
    {
        eon::DCBlocker blocker;
        blocker.prepare (0.0, 18.0);
        blocker.reset();
        bool ok = true;
        for (int i = 0; i < 64; ++i)
            ok = ok && finite (blocker.process (probeTone (i, 440.0, 48000.0)));
        check (ok, "DCBlocker fails closed on an invalid sample rate");
    }

    {
        check (eon::ftz (1.0e-30f) == 0.0f, "ftz flushes denormal-scale values to zero");
        check (eon::ftz (0.5f) == 0.5f, "ftz leaves normal values untouched");

        const eon::ScopedDenormalsOff guard;
        eon::SvfTPT svf;
        svf.setParams (1000.0, 0.7071067811865476, 48000.0);
        svf.reset();
        bool ok = true;
        for (int i = 0; i < 256; ++i)
            ok = ok && finite (svf.process (probeTone (i, 440.0, 48000.0)).lp);
        check (ok, "ScopedDenormalsOff is usable in a scope on this architecture");
    }

    // Determinism matters because SEOUL DSP renders Goldens offline.
    {
        eon::Rng a (12345u), b (12345u), c (54321u);
        bool same = true, different = false, inRange = true;
        for (int i = 0; i < 256; ++i)
        {
            const double x = a.next(), y = b.next(), z = c.next();
            same = same && (x == y);
            different = different || (x != z);
            inRange = inRange && x >= 0.0 && x < 1.0;
        }
        check (same, "Rng reproduces the same sequence for the same seed");
        check (different, "Rng produces a different sequence for a different seed");
        check (inRange, "Rng stays in the half-open unit interval");
    }
}

// ---------------------------------------------------------------------------
// Solvers and circuit primitives (consumed by Plan B Task 4/5)
// ---------------------------------------------------------------------------

void testSolvers()
{
    bool ok = true;
    for (const double x : { 1.0e-6, 1.0e-3, 0.1, 1.0, 3.0, 1.0e3, 1.0e6 })
    {
        const double w = eon::lambertW0 (x);
        ok = ok && finite (w) && near (w * std::exp (w), x, 1.0e-9 * std::max (1.0, x));
    }
    check (ok, "lambertW0 satisfies w * exp(w) == x on the principal branch");

    const double root = eon::newtonScalar ([] (double x) { return x * x - 2.0; },
                                           [] (double x) { return 2.0 * x; },
                                           1.0, 1.0, 2.0);
    check (near (root, std::sqrt (2.0), 1.0e-12), "newtonScalar converges on sqrt(2)");

    double A[3][3] = { { 2.0, 1.0, -1.0 }, { -3.0, -1.0, 2.0 }, { -2.0, 1.0, 2.0 } };
    double b[3] = { 8.0, -11.0, -3.0 };
    double x[3] = { 0.0, 0.0, 0.0 };
    const bool solved = eon::solveDense (A, b, x);
    check (solved && near (x[0], 2.0, 1.0e-12) && near (x[1], 3.0, 1.0e-12)
                   && near (x[2], -1.0, 1.0e-12),
           "solveDense solves a 3x3 system with the expected roots");

    bool singularRejected = false;
    {
        double S[3][3] = { { 1.0, 2.0, 3.0 }, { 2.0, 4.0, 6.0 }, { 1.0, 1.0, 1.0 } };
        double sb[3] = { 1.0, 2.0, 3.0 };
        double sx[3] = { 0.0, 0.0, 0.0 };
        singularRejected = ! eon::solveDense (S, sb, sx);
    }
    check (singularRejected, "solveDense reports a singular system instead of returning garbage");
}

void testCircuitPrimitives()
{
    // These stages are bought into production in Plan B Task 5 only if they
    // stay finite and bounded first. Smoke level is intentional here; their
    // tonal behaviour belongs to the listening gate, not to this test.
    auto sweepIsFinite = [] (auto&& process, int samples, double bound)
    {
        bool ok = true;
        for (int i = 0; i < samples; ++i)
        {
            const double y = process (probeTone (i, 220.0, 48000.0));
            ok = ok && finite (y) && std::abs (y) <= bound;
        }
        return ok;
    };

    eon::InductorResonator inductor;
    inductor.reset();
    check (sweepIsFinite ([&inductor] (float x) { return inductor.process (x, 200.0f, 0.7f, 1.5f); },
                          8192, 10.0),
           "InductorResonator stays finite and bounded");

    eon::AnalogAir air;
    air.reset();
    check (sweepIsFinite ([&air] (float) { return air.process(); }, 8192, 1.0),
           "AnalogAir noise stays finite and well below full scale");

    eon::ClassAStage classA;
    classA.reset();
    check (sweepIsFinite ([&classA] (float x) { return classA.process (x); }, 8192, 4.0),
           "ClassAStage stays finite and bounded");

    eon::TriodeStage triode;
    triode.reset();
    check (sweepIsFinite ([&triode] (float x) { return triode.process (x); }, 4096, 4.0),
           "TriodeStage (Koren + Newton solve) stays finite and bounded");

    eon::JilesAtherton transformer;
    transformer.reset();
    check (sweepIsFinite ([&transformer] (float x) { return transformer.process (x); }, 8192, 4.0),
           "JilesAtherton transformer stays finite and bounded");

    // Wave-digital diode roots: the Lambert-W path must not overflow, which is
    // the documented failure mode of the naive exp() formulation.
    {
        eon::WdfDiode diode;
        diode.R = 1000.0;
        bool ok = true;
        for (double a = -10.0; a <= 10.0; a += 0.25)
        {
            diode.incident (a);
            ok = ok && finite (diode.emitted ());
        }
        check (ok, "WdfDiode root stays finite across a wide wave range");
    }
    {
        eon::WdfDiodePair pair;
        pair.R = 1000.0;
        bool ok = true;
        for (double a = -10.0; a <= 10.0; a += 0.25)
        {
            pair.incident (a);
            ok = ok && finite (pair.emitted ());
        }
        check (ok, "WdfDiodePair root stays finite across a wide wave range");
    }
}

void testMeasurePrimitives()
{
    // 1000 Hz over exactly one second at 48 kHz is 1000 whole cycles, so the
    // direct DFT has no rectangular-window leakage. Without that, a "pure"
    // sine leaks into the harmonic bins and the THD floor becomes a property
    // of the window rather than of the signal.
    constexpr int N = 48000;
    constexpr double sr = 48000.0;
    std::vector<float> tone ((size_t) N);
    for (int i = 0; i < N; ++i)
        tone[(size_t) i] = probeTone (i, 1000.0, sr);

    const double mag = eon::measure::binMag (tone.data(), (size_t) N, 1000.0, sr);
    check (near (mag, 0.7, 1.0e-3), "measure::binMag recovers the tone amplitude");

    // A pure sine has no harmonics, so THD must collapse to the numerical
    // floor and, critically, stay finite.
    const double thd = eon::measure::thdPercent (tone.data(), (size_t) N, 1000.0, sr);
    check (finite (thd) && thd < 1.0e-6, "measure::thdPercent reports zero THD for a pure sine");

    for (int i = 0; i < N; ++i)
        tone[(size_t) i] = (float) std::tanh (2.0 * (double) tone[(size_t) i]);
    const double driven = eon::measure::thdPercent (tone.data(), (size_t) N, 1000.0, sr);
    check (finite (driven) && driven > 1.0, "measure::thdPercent detects a driven nonlinearity");
}

} // namespace

int main()
{
    std::printf ("eon_dsp vendored-copy contract tests\n");
    testAdaaFiniteness();
    testOversampler();
    testZdfFilters();
    testOutputAndRtHelpers();
    testSolvers();
    testCircuitPrimitives();
    testMeasurePrimitives();
    std::printf ("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
