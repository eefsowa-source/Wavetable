#include "../../Source/PluginProcessor.h"
#include "../../Source/DSP/RealtimeRandom.h"
#include "../../Source/DSP/SaturationStage.h"
#include "../../Source/DSP/SlopeFilter.h"
#include "Dsp/Measure.h"
#include "OfflineRenderer.h"
#include "TestHarness.h"

#include <cmath>
#include <array>
#include <cstring>

namespace
{
juce::AudioBuffer<float> renderIsolatedOscillator (const char* oscillator, const char* unisonId,
                                                   int count, std::uint32_t seed)
{
    audioquality::AudioQualityFixture fixture;
    fixture.id = oscillator;
    fixture.durationSeconds = 0.25;
    fixture.tailSeconds = 0.05;
    fixture.randomSeed = seed;
    fixture.midi = { { juce::MidiMessage::noteOn (1, 60, 0.8f), 0 },
                     { juce::MidiMessage::noteOff (1, 60), 9000 } };
    return audioquality::OfflineRenderer::render (fixture, [=] (auto& processor)
    {
        const auto set = [&] (const char* id, float value)
        {
            if (auto* parameter = processor.parameters.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set ("osc1Level", std::strcmp (oscillator, "osc1") == 0 ? 1.0f : 0.0f);
        set ("osc2Level", std::strcmp (oscillator, "osc2") == 0 ? 1.0f : 0.0f);
        set ("osc3Level", std::strcmp (oscillator, "osc3") == 0 ? 1.0f : 0.0f);
        set ("osc1Unison", 1.0f);
        set (unisonId, (float) count);
        set ("output", 0.0f);
    });
}

float maximumDifference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float result = 0.0f;
    for (int channel = 0; channel < juce::jmin (a.getNumChannels(), b.getNumChannels()); ++channel)
        for (int sample = 0; sample < juce::jmin (a.getNumSamples(), b.getNumSamples()); ++sample)
            result = juce::jmax (result, std::abs (a.getSample (channel, sample) - b.getSample (channel, sample)));
    return result;
}

// Steady one-oscillator tone through the low-pass at the requested slope.
// The two probe pitches used below sit more than three octaves above the 60 Hz
// corner, where the asymptote has settled and the damping term cannot bend the
// measured number.
juce::AudioBuffer<float> renderSlopeTone (int slope, int type, float tuneSemitones,
                                          float resonance = 1.0f)
{
    audioquality::AudioQualityFixture fixture;
    fixture.id = "filter-slope-tone";
    fixture.durationSeconds = 0.5;
    fixture.tailSeconds = 0.0;
    fixture.randomSeed = 400u;
    fixture.midi = { { juce::MidiMessage::noteOn (1, 60, 0.8f), 0 } };
    return audioquality::OfflineRenderer::render (fixture, [=] (auto& processor)
    {
        const auto set = [&] (const char* id, float value)
        {
            if (auto* parameter = processor.parameters.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set ("osc1Level", 1.0f);
        set ("osc2Level", 0.0f);
        set ("osc3Level", 0.0f);
        set ("osc1Pos", 0.0f);
        set ("osc1Unison", 1.0f);
        set ("osc1Tune", tuneSemitones);
        set ("unisonKeyTrack", 0.0f);
        set ("filterType", (float) type);
        set ("filterSlope", (float) slope);
        set ("cutoff", 60.0f);
        // Maximum resonance keeps the damping term small, so the octave drop
        // that comes out is the filter order rather than its Q.
        set ("resonance", resonance);
        set ("filterDrive", 0.0f);
        set ("filterEnvAmount", 0.0f);
        set ("filterAttack", 0.001f);
        set ("filterDecay", 0.001f);
        set ("filterSustain", 1.0f);
        set ("saturation", 0.0f);
        set ("lfo1Depth", 0.0f);
        set ("lfo2Depth", 0.0f);
        set ("driftDepth", 0.0f);
        set ("ampAttack", 0.001f);
        set ("ampDecay", 0.001f);
        set ("ampSustain", 1.0f);
        set ("delayMix", 0.0f);
        set ("reverbMix", 0.0f);
        set ("masterWidth", 1.0f);
        set ("output", 0.0f);
    });
}

// RMS of the settled second half, so the attack and any envelope motion stay
// out of the measurement window.
double tailRms (const juce::AudioBuffer<float>& buffer)
{
    const int total = buffer.getNumSamples();
    const int first = total / 2;
    double sum = 0.0;
    int count = 0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = first; sample < total; ++sample)
        {
            const double value = (double) buffer.getSample (channel, sample);
            sum += value * value;
            ++count;
        }
    return count > 0 ? std::sqrt (sum / (double) count) : 0.0;
}

// One oscillator tuned so the tone lands on 512.03 Hz, which is a whole number
// of cycles in the 12000-sample window the drive test analyses (128 cycles in
// 0.25 s), so the rectangular-window DFT in eon::measure::thdPercent does not
// read window leakage as harmonics.
juce::AudioBuffer<float> renderDriveTone (float driveDecibels)
{
    audioquality::AudioQualityFixture fixture;
    fixture.id = "filter-drive-tone";
    fixture.durationSeconds = 0.5;
    fixture.tailSeconds = 0.0;
    fixture.randomSeed = 400u;
    fixture.midi = { { juce::MidiMessage::noteOn (1, 60, 0.9f), 0 } };
    return audioquality::OfflineRenderer::render (fixture, [driveDecibels] (auto& processor)
    {
        const auto set = [&] (const char* id, float value)
        {
            if (auto* parameter = processor.parameters.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set ("osc1Level", 1.0f);
        set ("osc2Level", 0.0f);
        set ("osc3Level", 0.0f);
        set ("osc1Pos", 0.0f);
        set ("osc1Unison", 1.0f);
        set ("osc1Tune", 11.62407f);
        set ("unisonKeyTrack", 0.0f);
        set ("filterType", 0.0f);
        set ("filterSlope", 1.0f);
        // Keep the harmonics the clipper makes inside the band, so the THD
        // number measures the drive rather than the filter's own rolloff.
        set ("cutoff", 20000.0f);
        set ("resonance", 0.1f);
        set ("filterDrive", driveDecibels);
        set ("filterEnvAmount", 0.0f);
        set ("saturation", 0.0f);
        set ("lfo1Depth", 0.0f);
        set ("lfo2Depth", 0.0f);
        set ("driftDepth", 0.0f);
        set ("ampAttack", 0.001f);
        set ("ampDecay", 0.001f);
        set ("ampSustain", 1.0f);
        set ("delayMix", 0.0f);
        set ("reverbMix", 0.0f);
        set ("masterWidth", 1.0f);
        set ("output", 0.0f);
    });
}

juce::AudioBuffer<float> renderWithSaturation (float saturation)
{
    audioquality::AudioQualityFixture fixture;
    fixture.id = "saturation-path";
    fixture.durationSeconds = 0.25;
    fixture.tailSeconds = 0.05;
    fixture.randomSeed = 400u;
    fixture.midi = { { juce::MidiMessage::noteOn (1, 60, 0.8f), 0 },
                     { juce::MidiMessage::noteOff (1, 60), 9000 } };
    return audioquality::OfflineRenderer::render (fixture, [=] (auto& processor)
    {
        const auto set = [&] (const char* id, float value)
        {
            if (auto* parameter = processor.parameters.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set ("osc1Level", 1.0f);
        set ("osc2Level", 0.0f);
        set ("osc3Level", 0.0f);
        set ("osc1Unison", 1.0f);
        set ("saturation", saturation);
        set ("output", 0.0f);
    });
}
}

namespace
{
void setPlainParameter (HybridWavetableAudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.parameters.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

}

int main()
{
    audioquality::TestHarness test;
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    {
        HybridWavetableAudioProcessor processor;
        test.expect (processor.getName() == "SEOUL DSP", "processor constructs as SEOUL DSP");
        test.expect (processor.parameters.getParameter ("osc2Unison") != nullptr,
                     "osc2 unison parameter is registered");
        test.expect (processor.parameters.getParameter ("osc3Unison") != nullptr,
                     "osc3 unison parameter is registered");
        for (const auto* id : { "osc1Detune", "osc2Detune", "osc3Detune", "unisonKeyTrack" })
            test.expect (processor.parameters.getParameter (id) != nullptr,
                         "unison detune/key-track parameter is registered");
        test.expect (processor.parameters.getParameter ("saturationQuality") != nullptr
                         && processor.parameters.getRawParameterValue ("saturationQuality")->load() == 1.0f,
                     "saturation quality is registered and defaults to Normal");
        processor.prepareToPlay (48000.0, 64);
        test.expect (processor.getLatencySamples() == SaturationStage::reportedLatencySamples,
                     "processor reports the fixed saturation latency");
    }

    // Every tier and the zero-drive bypass must line up at the same sample.
    // This catches a dry path that skips DSP while the processor still reports
    // the oversampler latency to its host.
    {
        const auto impulsePeak = [] (SaturationStage::Quality quality, bool bypass)
        {
            SaturationStage stage;
            stage.prepare (64);
            stage.setQuality (quality);
            std::array<float, 128> signal {};
            signal[0] = 0.5f;
            for (int offset = 0; offset < (int) signal.size(); offset += 64)
            {
                if (bypass)
                    stage.processBypass (signal.data() + offset, nullptr, 64);
                else
                    stage.process (signal.data() + offset, nullptr, 64);
            }
            return (int) std::distance (signal.begin(),
                                        std::max_element (signal.begin(), signal.end(),
                                                          [] (float a, float b)
                                                          { return std::abs (a) < std::abs (b); }));
        };
        const auto ecoPeak = impulsePeak (SaturationStage::Quality::eco, false);
        const auto normalPeak = impulsePeak (SaturationStage::Quality::normal, false);
        const auto highPeak = impulsePeak (SaturationStage::Quality::high, false);
        std::printf ("saturation impulse peaks: Eco %d, Normal %d, High %d, Bypass %d\n",
                     ecoPeak, normalPeak, highPeak,
                     impulsePeak (SaturationStage::Quality::normal, true));
        for (const auto peak : { ecoPeak, normalPeak, highPeak })
            test.expect (peak == SaturationStage::reportedLatencySamples,
                         "saturation quality tier matches the fixed reported latency");
        test.expect (impulsePeak (SaturationStage::Quality::normal, true)
                         == SaturationStage::reportedLatencySamples,
                     "zero-drive saturation bypass matches the fixed reported latency");
    }

    // Parameter resolution. JUCE's AudioParameterFloat(id, name, min, max,
    // default) constructor forwards to NormalisableRange(min, max, 0.01f), which
    // snaps every parameter to a 0.01 step *of its own unit*. Left alone that
    // silently quantises osc1Tune to 1 cent, the ADSR times to 10 ms (so a 1 ms
    // attack is unreachable) and the delay time to 10 ms. Each of these values
    // must survive the round trip through the parameter unchanged.
    {
        struct Expectation { const char* id; float value; };
        const Expectation expectations[] = {
            { "osc1Tune",   0.07453f },   // 0.75 cent at 10 kHz
            { "ampAttack",  0.002f },     // unreachable on a 0.01 s grid
            { "filterDecay", 0.0035f },
            { "delayTime",  0.1234f },    // unreachable on a 0.01 s grid
            { "osc1Pos",    0.0745f },
            { "lfo1Rate",   0.0567f },
        };
        HybridWavetableAudioProcessor resolutionProcessor;
        for (const auto& expectation : expectations)
        {
            setPlainParameter (resolutionProcessor, expectation.id, expectation.value);
            const auto actual = resolutionProcessor.parameters
                                    .getRawParameterValue (expectation.id)->load();
            test.expect (std::abs (actual - expectation.value) < 1.0e-4f,
                         juce::String (expectation.id)
                             + " keeps its requested resolution (no 0.01 snap)");
        }
    }
    // The per-voice PRNG contract is testable without touching the callback.
    bool phasesAreNormalized = true;
    bool phasesAreDistinct = false;
    RealtimeRandom firstRandom (1000u);
    const auto firstPhase = firstRandom.nextUnitFloat();
    for (std::uint32_t seed = 1001u; seed < 1017u; ++seed)
    {
        RealtimeRandom random (seed);
        const auto phase = random.nextUnitFloat();
        phasesAreNormalized &= phase >= 0.0f && phase < 1.0f;
        phasesAreDistinct |= std::abs (phase - firstPhase) > 1.0e-6f;
    }
    test.expect (phasesAreNormalized && phasesAreDistinct,
                 "phase domain has distinct normalized per-voice seeds");


    const auto positiveEnvelope = SeoulDSPQuality::filterEnvelopeCutoff (1000.0f, 1.0f, 1.0f, 48000.0f);
    const auto negativeEnvelope = SeoulDSPQuality::filterEnvelopeCutoff (1000.0f, -1.0f, 1.0f, 48000.0f);
    test.expect (std::abs (positiveEnvelope - 16000.0f) < 50.0f,
                 "filter envelope reaches +4 octaves before safe clamp");
    test.expect (std::abs (negativeEnvelope - 62.5f) < 2.0f,
                 "filter envelope reaches -4 octaves before safe clamp");

    test.expect (maximumDifference (renderIsolatedOscillator ("osc2", "osc2Unison", 1, 200u),
                                    renderIsolatedOscillator ("osc2", "osc2Unison", 4, 200u)) > 1.0e-5f,
                 "osc2 unison parameter changes the rendered signal");
    test.expect (maximumDifference (renderIsolatedOscillator ("osc3", "osc3Unison", 1, 300u),
                                    renderIsolatedOscillator ("osc3", "osc3Unison", 4, 300u)) > 1.0e-5f,
                 "osc3 unison parameter changes the rendered signal");
    test.expect (maximumDifference (renderWithSaturation (0.0f), renderWithSaturation (0.9f)) > 1.0e-4f,
                 "saturation drive changes the rendered signal (bypass path stays active)");


    const auto dry = HybridWavetableAudioProcessor::calculateDelayMixGains (0.0f);
    const auto half = HybridWavetableAudioProcessor::calculateDelayMixGains (0.5f);
    const auto wet = HybridWavetableAudioProcessor::calculateDelayMixGains (1.0f);
    test.expect (std::abs (dry.dry - 1.0f) < 1.0e-6f && std::abs (dry.wet) < 1.0e-6f,
                 "delay mix zero is dry");
    test.expect (std::abs (wet.dry) < 1.0e-6f && std::abs (wet.wet - 1.0f) < 1.0e-6f,
                 "delay mix one removes direct sample");
    test.expect (std::abs (half.dry - std::sqrt (0.5f)) < 1.0e-5f
                 && std::abs (half.wet - std::sqrt (0.5f)) < 1.0e-5f,
                 "delay mix half is equal power");

    const auto keyScale = [] (float keyTrack, int note)
    {
        return juce::jlimit (0.5f, 2.0f, std::exp2 (keyTrack * (float) (note - 60) / 48.0f));
    };
    test.expect (keyScale (0.0f, 24) == 1.0f && keyScale (0.0f, 108) == 1.0f,
                 "zero key-track preserves requested detune at every key");
    test.expect (keyScale (-1.0f, 24) > 1.0f && keyScale (-1.0f, 108) < 1.0f,
                 "negative key-track scales detune inversely");
    test.expect (keyScale (1.0f, 24) < 1.0f && keyScale (1.0f, 108) > 1.0f,
                 "positive key-track scales detune with key");
    // Real slope gate (Plan B Task 4). Every index must attenuate by the number
    // its label advertises. The measurement is the drop between two tones an
    // octave apart (523 Hz and 1046 Hz against a 60 Hz corner), so the number is
    // the filter order and not a level offset.
    {
        constexpr double kProbeTuneLow = 12.0f;   // 523.25 Hz
        constexpr double kProbeTuneHigh = 24.0f;  // 1046.50 Hz
        struct SlopeExpectation { int index; double dBPerOctave; };
        const SlopeExpectation expectations[] = { { 0, 6.0 }, { 1, 12.0 }, { 2, 18.0 }, { 3, 24.0 } };
        constexpr int slopeCount = 4;
        std::array<juce::AudioBuffer<float>, slopeCount> lowTones, highTones;
        for (int i = 0; i < slopeCount; ++i)
        {
            lowTones[(size_t) i] = renderSlopeTone (expectations[i].index, 0, (float) kProbeTuneLow);
            highTones[(size_t) i] = renderSlopeTone (expectations[i].index, 0, (float) kProbeTuneHigh);
        }
        // The selector has to change the audio before its numbers mean anything:
        // before Task 4 indices 0/1 and 2/3 rendered bit-identical samples.
        for (int i = 1; i < slopeCount; ++i)
            test.expect (maximumDifference (lowTones[(size_t) (i - 1)], lowTones[(size_t) i]) > 1.0e-4f,
                         juce::String ("filter slope ") + juce::String (i - 1)
                             + " and " + juce::String (i) + " render different audio");
        for (int i = 0; i < slopeCount; ++i)
        {
            const auto measured = -20.0 * std::log10 (tailRms (highTones[(size_t) i])
                                                       / tailRms (lowTones[(size_t) i]));
            std::printf ("  slope %d: measured %.2f dB/oct (label %.0f) [low %.6g high %.6g]\n",
                         expectations[i].index, measured, expectations[i].dBPerOctave,
                         tailRms (lowTones[(size_t) i]), tailRms (highTones[(size_t) i]));
            test.expect (std::abs (measured - expectations[i].dBPerOctave) < 1.5,
                         juce::String ("filter slope ") + juce::String (expectations[i].index)
                            + " rolls off at " + juce::String (expectations[i].dBPerOctave, 0)
                            + " dB/oct within 1.5 dB");
        }
    }

    // Resonance safety across the whole matrix. The TPT sections are
    // unconditionally stable, so this guards against a mis-set coefficient
    // rather than sweeping the knob; the resonance range itself is reported in
    // docs/quality/b4-filter-slopes.md.
    {
        bool allFinite = true;
        bool allBounded = true;
        for (int slope = 0; slope < 4; ++slope)
            for (int type = 0; type < 3; ++type)
            {
                const auto rendered = renderSlopeTone (slope, type, 0.0f);
                for (int channel = 0; channel < rendered.getNumChannels(); ++channel)
                    for (int sample = 0; sample < rendered.getNumSamples(); ++sample)
                    {
                        const auto value = rendered.getSample (channel, sample);
                        allFinite &= std::isfinite (value);
                        allBounded &= std::abs (value) < 8.0f;
                    }
            }
        test.expect (allFinite, "every slope and filter type renders finite output");
        test.expect (allBounded,
                     "every slope and filter type stays below +18 dBFS at maximum resonance");
    }

    // 0 dB drive must be exactly the linear section it wraps, even on an input
    // big enough that a "gentle" waveshaper would show. That is the property the
    // old linear pre-gain had, and SlopeFilter has to keep it.
    {
        constexpr double sr = 48000.0, fc = 3000.0, q = 0.9;
        SlopeFilter filter;
        eon::SvfTPT reference;
        filter.reset();
        reference.reset();
        double worst = 0.0;
        for (int i = 0; i < 4096; ++i)
        {
            const float x = (float) (1.5 * std::sin (2.0 * juce::MathConstants<double>::pi
                                                      * 261.0 * (double) i / sr));
            filter.setParams (1, 0, fc, q, 0.0, sr);
            reference.setParams (fc, q, sr);
            worst = juce::jmax (worst, std::abs ((double) filter.process (x)
                                                 - reference.process ((double) x).lp));
        }
        std::printf ("  0 dB drive vs eon::SvfTPT worst difference = %.3e\n", worst);
        // process() returns float, so the comparison against the double
        // reference can only hold to float precision; 1e-6 is ~2 ulp at these
        // amplitudes and well below anything audible.
        test.expect (worst < 1.0e-6, "0 dB filter drive is the linear TPT section exactly");
    }

    // Drive has to be a nonlinearity, not a level control. The clipper is off at
    // 0 dB, so the clean render is the linear filter and the harmonic content has
    // to climb steeply as the knob comes up, while the output stays bounded.
    {
        constexpr double kProbeHz = 512.0;
        constexpr int kAnalysisSamples = 12000;   // 0.25 s: 128 exact cycles
        constexpr double kSampleRateLocal = 48000.0;
        const auto thdOf = [&] (const juce::AudioBuffer<float>& buffer)
        {
            const int first = juce::jmax (0, buffer.getNumSamples() - kAnalysisSamples);
            return eon::measure::thdPercent (buffer.getReadPointer (0, first),
                                             (size_t) kAnalysisSamples, kProbeHz,
                                             kSampleRateLocal);
        };
        const auto clean = renderDriveTone (0.0f);
        const auto driven = renderDriveTone (24.0f);
        const double cleanThd = thdOf (clean);
        const double drivenThd = thdOf (driven);
        std::printf ("  filter drive THD: 0 dB -> %.4f %%, +24 dB -> %.4f %%\n",
                     cleanThd, drivenThd);
        test.expect (drivenThd > 2.0,
                     "filter drive at +24 dB adds harmonic distortion (crest factor falls)");
        test.expect (drivenThd > 10.0 * jmax (cleanThd, 1.0e-6),
                     "filter drive raises harmonics well above the 0 dB setting");
        bool drivenFinite = true;
        float drivenPeak = 0.0f;
        for (int channel = 0; channel < driven.getNumChannels(); ++channel)
            for (int sample = 0; sample < driven.getNumSamples(); ++sample)
            {
                const auto value = driven.getSample (channel, sample);
                drivenFinite &= std::isfinite (value);
                drivenPeak = juce::jmax (drivenPeak, std::abs (value));
            }
        std::printf ("  +24 dB drive peak = %.3f (finite %d)\n", drivenPeak, (int) drivenFinite);
        test.expect (drivenFinite, "the driven filter output stays finite");
        test.expect (drivenPeak < 2.0f, "the driven filter output stays bounded below +6 dBFS");
    }

    return test.result();
}
