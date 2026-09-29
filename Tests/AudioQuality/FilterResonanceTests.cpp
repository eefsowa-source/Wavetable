// Renderer-backed filter resonance gate.
//
// Why the real renderer: the resonance knob's range is a property of the
// plug-in wrapper and its parameter plumbing, not of the TPT section. A
// DSP-core unit test would read R = 1/(2*q) and call the range fine. The
// renderer is what a host automates and what a listener hears, so the knob's
// reach is measured through HybridWavetableAudioProcessor::processBlock.
//
// Why this matters for the reference set (Serum, Vital, Massive, Surge XT):
// all four make the resonance knob reach a pronounced resonant peak, and the
// top of the travel is where their character lives. docs/quality/b4-filter-
// slopes.md section 4 recorded that this plug-in's knob only trims damping
// inside a narrow band and deferred widening it, because it changes existing
// presets' tone. Left deferred, the knob is measurably inert, which is a sound
// quality gap against every reference instrument.
//
// The measurement is the filter's gain at its own corner, passband-normalised:
// the same filter renders one sine at the cutoff and another three decades
// below it, and the ratio is the peaking the knob produced. Dividing by the
// passband rather than by an "open filter" render is what makes the number
// mean Q; an open filter is not flat at the probe's frequency, so the two
// references are not interchangeable.
//
// The file calibrates itself against theory first. For the TPT state-variable
// low-pass used here |H(j*w0)| = Q, so corner gain must equal 20*log10(Q).
// The gate fails if the instrument cannot reproduce that, which is the same
// injected-line discipline QualityOrderTests uses to prove its alias meter is
// not passing vacuously. Only then does the knob sweep mean anything.
//
// It does not claim a subjective tone verdict, and it does not claim IMD.

#include "../../Source/PluginProcessor.h"
#include "OfflineRenderer.h"
#include "TestHarness.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include <vector>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kRenderSamples = 48000;
constexpr std::uint32_t kSeed = 0x5245534fu;   // "RESO"

// The knob's host range, mirrored from PluginProcessor.cpp so the sweep spans
// the full travel whatever the mapping between the parameter and Q becomes.
constexpr float kResonanceMinimum = 0.1f;
constexpr float kResonanceMaximum = 1.0f;

float resonanceParameterFor (double travel)
{
    return kResonanceMinimum + (float) travel * (kResonanceMaximum - kResonanceMinimum);
}

double semitonesFor (double frequencyHz)
{
    return 12.0 * std::log2 (frequencyHz / 440.0);
}

// RMS of a sine after the filter has settled. The cutoff used throughout is
// around 1 kHz, where a 48 kHz filter settles in well under 20 samples, so the
// second half of the render is steady state with room to spare.
double sineRmsThroughFilter (int slope, int type, double cutoffHz, double q,
                              double toneHz)
{
    SlopeFilter filter;
    filter.reset();
    double sumSquares = 0.0;
    int taken = 0;
    for (int i = 0; i < kRenderSamples; ++i)
    {
        filter.setParams (slope, type, cutoffHz, q, 0.0, kSampleRate);
        const auto input = (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                             * toneHz * (double) i / kSampleRate);
        const auto output = (double) filter.process (input);
        if (i >= kRenderSamples / 2)
        {
            sumSquares += output * output;
            ++taken;
        }
    }
    return taken > 0 ? std::sqrt (sumSquares / (double) taken) : 0.0;
}

// Corner gain of a bare section, normalised against a render at a known Q.
//
// Dividing by a "passband" tone instead would read the filter's own rolloff: at
// a quarter of the corner a Q = 0.25 section is already 9.3 dB down rather than
// the 12 dB its Q implies, which is how this file's first revision ended up
// reporting a 2.7 dB calibration error while measuring something real.
double sectionCornerGainDb (int slope, int type, double cutoffHz, double q)
{
    const auto reference = sineRmsThroughFilter (slope, type, cutoffHz, 1.0, cutoffHz);
    const auto corner = sineRmsThroughFilter (slope, type, cutoffHz, q, cutoffHz);
    return 20.0 * std::log10 (corner / juce::jmax (reference, 1.0e-30));
}

// Instrument calibration: the section must report the gain its own theory
// predicts. If this fails, every knob number below is meaningless and the gate
// must not be trusted to pass or fail anything.
void runInstrumentCalibration (audioquality::TestHarness& test)
{
    std::printf ("\nResonance instrument calibration (12 dB/oct low-pass, 1 kHz corner)\n");
    const double qs[] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
    for (const auto q : qs)
    {
        const auto measured = sectionCornerGainDb (1, 0, 1000.0, q);
        const auto theory = 20.0 * std::log10 (q);
        std::printf ("  Q %6.2f -> measured %+7.3f dB, theory %+7.3f dB, error %+6.3f dB\n",
                     q, measured, theory, measured - theory);
        test.expect (std::abs (measured - theory) < 1.0,
                     juce::String ("instrument reproduces 20*log10(Q) at Q = ")
                         + juce::String (q, 2));
    }
}

// A steady tone through the real renderer. Frame 0 of the default table is a
// sine, one oscillator at level 1 and the other two at zero, so what the probe
// measures past the oscillator is the filter and the output stage.
juce::AudioBuffer<float> renderTone (float resonance, int slope, int type,
                                     double toneHz, double cutoffHz)
{
    audioquality::AudioQualityFixture fixture;
    fixture.id = "resonance-corner-probe";
    fixture.sampleRate = kSampleRate;
    fixture.blockSize = 128;
    fixture.channels = 2;
    fixture.durationSeconds = 1.0;
    fixture.tailSeconds = 0.0;
    fixture.randomSeed = kSeed;
    fixture.midi = { { juce::MidiMessage::noteOn (1, 69, 0.8f), 0 } };

    return audioquality::OfflineRenderer::render (fixture, [=] (auto& processor)
    {
        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.parameters.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        // Level is deliberately low. A resonant corner peaks by up to Q, which
        // at the top of this knob's travel is +26 dB; a unity probe would drive
        // that straight into OutputSafety's 0.9 ceiling and have the limiter
        // compress the very quantity being measured. Starting 30 dB down leaves
        // the whole travel below the knee, so the number is the filter's.
        set ("osc1Level", 0.0316f);   // -30 dB
        set ("osc2Level", 0.0f);
        set ("osc3Level", 0.0f);
        set ("osc1Pos", 0.0f);
        set ("osc1Unison", 1.0f);
        set ("unisonKeyTrack", 0.0f);
        set ("osc1Tune", (float) semitonesFor (toneHz));
        set ("cutoff", (float) cutoffHz);
        set ("resonance", resonance);
        set ("filterDrive", 0.0f);
        set ("saturation", 0.0f);
        set ("filterType", (float) type);
        set ("filterSlope", (float) slope);
        set ("filterEnvAmount", 0.0f);
        set ("ampAttack", 0.001f);
        set ("ampDecay", 0.001f);
        set ("ampSustain", 1.0f);
        set ("ampRelease", 0.001f);
        set ("filterAttack", 0.001f);
        set ("filterDecay", 0.001f);
        set ("filterSustain", 1.0f);
        set ("filterRelease", 0.001f);
        set ("lfo1Depth", 0.0f);
        set ("lfo2Depth", 0.0f);
        set ("driftDepth", 0.0f);
        set ("randomPhase", 0.0f);
        set ("arpEnabled", 0.0f);
        set ("delayMix", 0.0f);
        set ("reverbMix", 0.0f);
        set ("masterWidth", 1.0f);
        set ("output", 0.0f);
    });
}

// Confirms the probe never reached the output ceiling during a sweep, so the
// corner-gain numbers are the filter's and not the limiter's. Without this the
// instrument could quietly saturate at the top of the travel and report a knob
// as less resonant than it is.
bool probeStayedBelowCeiling (double cutoffHz, int slope, int type)
{
    for (int step = 0; step <= 10; ++step)
    {
        const auto parameter = resonanceParameterFor ((double) step / 10.0);
        const auto rendered = renderTone (parameter, slope, type, cutoffHz, cutoffHz);
        for (int channel = 0; channel < rendered.getNumChannels(); ++channel)
        {
            const auto* samples = rendered.getReadPointer (channel);
            for (int i = 0; i < rendered.getNumSamples(); ++i)
                if (std::abs (samples[i]) > 0.89f)
                    return false;
            }
    }
    return true;
}

double tailRms (const juce::AudioBuffer<float>& buffer)
{
    const auto count = buffer.getNumSamples() / 2;
    const auto first = buffer.getNumSamples() - count;
    double sum = 0.0;
    int taken = 0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer (channel);
        for (int i = 0; i < count; ++i)
        {
            const auto value = (double) samples[first + i];
            sum += value * value;
            ++taken;
        }
    }
    return taken > 0 ? std::sqrt (sum / (double) taken) : 0.0;
}

struct SweepPoint
{
    double travel = 0.0;
    float parameter = 0.0f;
    double measuredQ = 0.0;
    double cornerGainDb = 0.0;
};

// Recovers the Q the knob actually reaches, measured through the renderer.
//
// The section identity is |H(j*w0)| = Q, so two renders of the same tone at the
// same cutoff that differ only in the knob give a ratio of the two Q values.
// Rendering at one frequency is deliberate: any reference tone at a different
// frequency would be read through the filter's own rolloff and through the
// processor's 100 Hz mono-bass crossover, neither of which belongs in a
// measurement of resonance. That is what made an earlier revision of this file
// report the high-pass as 24 dB louder than the low-pass when the section itself
// is symmetric to the sample.
std::vector<SweepPoint> sweepMeasuredQ (int slope, int type, double cutoffHz, int steps)
{
    // Reference at the knob's own minimum. Its Q comes from the mapping itself
    // rather than a literal: hardcoding a floor here scales every reported Q by
    // the same factor, which leaves the sweep self-consistent while the
    // absolute numbers come out wrong by that factor.
    const auto referenceQ = (double) SeoulDSPQuality::filterResonanceQ (kResonanceMinimum);
    const auto referenceRms = tailRms (renderTone (kResonanceMinimum, slope, type,
                                                  cutoffHz, cutoffHz));

    std::vector<SweepPoint> points;
    points.reserve ((size_t) steps);
    for (int step = 0; step < steps; ++step)
    {
        const auto travel = (double) step / (double) (steps - 1);
        const auto parameter = resonanceParameterFor (travel);
        const auto rms = tailRms (renderTone (parameter, slope, type, cutoffHz, cutoffHz));
        const auto ratio = rms / juce::jmax (referenceRms, 1.0e-30);
        points.push_back ({ travel, parameter, referenceQ * ratio,
                            20.0 * std::log10 (juce::jmax (ratio, 1.0e-30)) });
    }
    return points;
}

void printSweep (const char* label, const std::vector<SweepPoint>& points)
{
    std::printf ("  %s\n", label);
    for (const auto& point : points)
        std::printf ("      travel %3.0f%%  parameter %.3f  Q %7.2f  corner %+6.2f dB\n",
                     point.travel * 100.0, point.parameter, point.measuredQ,
                     point.cornerGainDb);
}

// Ringing, measured as oscillation cycles rather than decay time.
//
// Counting "samples above a floor" is the wrong instrument here: a heavily
// overdamped section has a slow pole and takes a long time to fall through any
// floor while never oscillating at all, so that metric ranks an inert knob above
// a resonant one. Sign changes do not have that failure mode.
int ringingCyclesForQ (double q)
{
    if (q <= 0.0)
        return 0;

    SlopeFilter filter;
    filter.reset();
    std::vector<float> response;
    response.reserve ((size_t) kRenderSamples);
    for (int i = 0; i < kRenderSamples; ++i)
    {
        filter.setParams (1, 0, 1000.0, q, 0.0, kSampleRate);
        response.push_back (filter.process (i == 0 ? 1.0f : 0.0f));
    }
    const auto peak = *std::max_element (response.begin(), response.end(),
                                       [] (float a, float b)
                                       { return std::abs (a) < std::abs (b); });
    const auto floor = std::abs (peak) * 1.0e-3f;   // 60 dB down

    int crossings = 0;
    float previous = 0.0f;
    for (const auto value : response)
    {
        if (std::abs (value) > floor)
        {
            if (previous != 0.0f && ((value > 0.0f) != (previous > 0.0f)))
                ++crossings;
            previous = value;
        }
    }
    return crossings / 2;
}

void runCornerGainSweep (audioquality::TestHarness& test)
{
    std::printf ("\nResonance knob reach, measured through the renderer (1 kHz corner)\n");
    constexpr double cutoffHz = 1000.0;

    const auto lowPass = sweepMeasuredQ (1, 0, cutoffHz, 11);
    printSweep ("low-pass, 12 dB/oct", lowPass);

    test.expect (probeStayedBelowCeiling (cutoffHz, 1, 0),
                 "the probe stays below the output ceiling across the whole knob travel");

    const auto loudest = lowPass.back();
    const auto loudestQ = *std::max_element (lowPass.begin(), lowPass.end(),
                                           [] (const SweepPoint& a, const SweepPoint& b)
                                           { return a.measuredQ < b.measuredQ; });
    std::printf ("  loudest setting: Q %.2f (%+.2f dB) at %.0f%% of travel\n",
                 loudestQ.measuredQ, loudestQ.cornerGainDb, loudestQ.travel * 100.0);

    // The gap this gate exists to close. Mapped straight onto Q over 0.1..1.0,
    // the knob tops out at Q = 1, which is 0 dB: no peak anywhere on the travel,
    // so it trims damping instead of adding resonance and cannot ring.
    // Serum, Vital, Massive and Surge XT all put a pronounced peak at the top of
    // their resonance controls, and that is where their character lives.
    test.expect (loudestQ.measuredQ > 15.0,
                 "the resonance knob reaches a pronounced peak (Q above 15)");

    // The bottom of the knob still has to be a damped filter. Q below 0.707 is
    // past flat into controlled damping, which is what a closed filter is.
    test.expect (lowPass.front().measuredQ < 0.71,
                 "minimum resonance is a damped filter (Q below 0.707)");

    // Reach has to grow with the knob rather than wobble. 5% slack covers float
    // and measurement noise without allowing a real dip through the middle.
    bool monotonic = true;
    for (size_t i = 1; i < lowPass.size(); ++i)
        monotonic &= lowPass[i].measuredQ >= lowPass[i - 1].measuredQ * 0.95;
    test.expect (monotonic, "resonance grows monotonically with the knob");
    test.expect (loudest.measuredQ >= loudestQ.measuredQ * 0.99,
                 "maximum resonance sits at the top of the knob's travel");

    // A high-Q section sustains a pitched tail; that sustain is the audible
    // difference between a damping control and a resonance control, so the
    // travel has to end somewhere a note can be held on.
    test.expect (ringingCyclesForQ (lowPass.back().measuredQ) > 12,
                 "the top of the travel rings long enough to sustain a note");
}

// Ringing, measured as oscillation cycles rather than decay time. Counting
// "samples above a floor" is the wrong instrument here: a heavily overdamped
// section has a slow pole and takes a long time to fall through the floor
// while never oscillating at all, so that metric ranks an inert knob above a
// resonant one. Sign changes do not have that failure mode.
void runRingingSweep (audioquality::TestHarness& test)
{
    std::printf ("\nRinging versus Q (impulse into a 12 dB/oct low-pass, 1 kHz corner)\n");

    // Q = 0.5 is overdamped and does not ring. Past that the cycle count grows
    // with Q, which is the sustain a resonant sweep gives the reference
    // instruments and what the knob sweep above depends on.
    int previousCycles = -1;
    bool growsWithQ = true;
    for (const auto q : { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 20.0 })
    {
        const auto cycles = ringingCyclesForQ (q);
        std::printf ("  Q %6.2f -> %4d cycles within 60 dB of the peak\n", q, cycles);
        if (previousCycles >= 0)
            growsWithQ &= cycles >= previousCycles;
        previousCycles = cycles;
    }

    test.expect (ringingCyclesForQ (0.5) < 3, "a damped section does not ring");
    test.expect (growsWithQ, "ringing grows monotonically with Q");
    test.expect (ringingCyclesForQ (20.0) > 12,
                 "a Q of 20 sustains a pitched tail within 60 dB");
}

// The section's own gain has to stay finite and bounded across the matrix
// before the signal ever reaches OutputSafety's ceiling.
void runSectionStabilityMatrix (audioquality::TestHarness& test)
{
    std::printf ("\nResonance section stability matrix\n");
    bool allFinite = true;
    float worstRatio = 0.0f;
    for (int slope = 0; slope < 4; ++slope)
        for (int type = 0; type < 3; ++type)
            for (const auto q : { 0.5, 1.0, 4.0, 10.0, 20.0 })
            {
                // Only slope 1 is a bare resonant section, so only it has a
                // corner gain that must track Q. Slope 0 is a one-pole where the
                // damping coefficient does nothing, and band-pass splits into
                // one-pole skirts that deliberately ignore resonance, so
                // scoring either against Q would be measuring the wrong thing.
                if (slope == 1 && type != 2)
                {
                    const auto measured = sectionCornerGainDb (slope, type, 1000.0, q);
                    worstRatio = juce::jmax (worstRatio,
                                             std::abs ((float) (measured
                                                                - 20.0 * std::log10 (q))));
                }
                for (int i = 0; i < 2048; ++i)
                {
                    SlopeFilter filter;
                    filter.reset();
                    filter.setParams (slope, type, 1000.0, q, 12.0, kSampleRate);
                    const auto input = (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                         * 1000.0 * (double) i / kSampleRate);
                    const auto value = filter.process (input);
                    allFinite &= std::isfinite (value);
                    worstRatio = std::isfinite (value)
                        ? worstRatio : std::numeric_limits<float>::max();
                }
            }
    std::printf ("  worst |corner gain - 20*log10(Q)| over the matrix: %.3f dB\n", worstRatio);
    test.expect (allFinite,
                 "every slope, type and resonance setting stays finite, including at drive");
    test.expect (worstRatio < 1.5,
                 "the resonant section's corner gain tracks Q across the matrix");
}
}

int main()
{
    audioquality::TestHarness test;
    runInstrumentCalibration (test);
    runCornerGainSweep (test);
    runRingingSweep (test);
    runSectionStabilityMatrix (test);
    std::printf ("\n%d failure(s)\n", test.failureCount());
    return test.result();
}
