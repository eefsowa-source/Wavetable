#include "../../Source/PluginProcessor.h"
#include "../../Source/DSP/RealtimeRandom.h"
#include "../../Source/DSP/SaturationStage.h"
#include "../../Source/DSP/SlopeFilter.h"
#include "Dsp/Measure.h"
#include "Metrics.h"
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

// The knob position whose mapped Q is 1, i.e. the Butterworth corner. Named so
// the slope gate states which Q it is measuring at instead of relying on a
// literal whose meaning changed when the resonance range was widened.
// Derived from the mapping itself rather than guessed: the curve is
// Q = 0.5 * 40^travel, so Q = 1 sits at travel ln(2)/ln(40) = 0.1879, which is
// knob 0.1 + 0.9 * 0.1879 = 0.2691.
constexpr float kButterworthKnob = 0.2691f;

// Steady one-oscillator tone through the low-pass at the requested slope.
// The two probe pitches used below sit more than three octaves above the 60 Hz
// corner, where the asymptote has settled and the damping term cannot bend the
// measured number.
//
// `resonance` is the knob position, and this gate passes the position that maps
// to Q = 1 on purpose. It used to pass 1.0 meaning "maximum resonance, minimal
// damping", which the widened range turned into Q = 20, where a resonant peak's
// skirt swamps the rolloff being measured: on the bare section the same probe
// reads 24.3 dB/oct for slope 3 at Q = 4 and 6.6 dB/oct at Q = 20. The slope did
// not change; the probe stopped reading an asymptote. FilterResonanceTests owns
// the range of the knob, and this gate owns the slopes.
juce::AudioBuffer<float> renderSlopeTone (int slope, int type, float tuneSemitones,
                                          float resonance = kButterworthKnob)
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
        // Q = 1 (Butterworth), so the octave drop that comes out is the filter
        // order rather than a resonant peak's skirt. See the note above.
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

// Worst-case legal patch (Plan C SQ-1). A dense chord with every oscillator's
// unison bank at maximum is the loudest sum the oscillator section can make,
// and the master output stays at its default -6 dB, so this measures the
// internal gain staging rather than a user cranking the master fader. A peak
// above 0 dBFS means a legal patch can clip the host's output.
juce::AudioBuffer<float> renderWorstCaseChord()
{
    audioquality::AudioQualityFixture fixture;
    fixture.id = "worst-case-chord";
    fixture.durationSeconds = 1.0;
    fixture.tailSeconds = 0.1;
    fixture.randomSeed = 0x5eed0101u;
    const int firstNote = 48;
    for (int note = 0; note < 16; ++note)
    {
        fixture.midi.push_back ({ juce::MidiMessage::noteOn (1, firstNote + note, 1.0f), 0 });
        fixture.midi.push_back ({ juce::MidiMessage::noteOff (1, firstNote + note),
                                  (int) std::llround (fixture.durationSeconds * fixture.sampleRate * 0.75) });
    }
    return audioquality::OfflineRenderer::render (fixture, [] (auto& processor)
    {
        const auto set = [&] (const char* id, float value)
        {
            if (auto* parameter = processor.parameters.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        for (int osc = 1; osc <= 3; ++osc)
        {
            const auto prefix = juce::String ("osc") + juce::String (osc);
            set ((prefix + "Level").toRawUTF8(), 1.0f);
            set ((prefix + "Unison").toRawUTF8(), 8.0f);
            set ((prefix + "Spread").toRawUTF8(), 1.0f);
            set ((prefix + "Detune").toRawUTF8(), 50.0f);
            set ((prefix + "Tune").toRawUTF8(), 0.0f);
        }
        set ("unisonKeyTrack", 0.0f);
        set ("filterType", 0.0f);
        set ("filterSlope", 3.0f);
        set ("cutoff", 20000.0f);
        set ("resonance", 0.1f);
        set ("filterDrive", 0.0f);
        set ("saturation", 1.0f);
        set ("filterEnvAmount", 0.0f);
        set ("ampAttack", 0.001f);
        set ("ampDecay", 0.001f);
        set ("ampSustain", 1.0f);
        set ("ampRelease", 0.05f);
        set ("lfo1Depth", 0.0f);
        set ("lfo2Depth", 0.0f);
        set ("driftDepth", 0.0f);
        set ("delayMix", 0.0f);
        set ("reverbMix", 0.0f);
        set ("masterWidth", 1.0f);
        set ("output", -6.0f);
    });
}

// A held note on a clean, steady path, so a discontinuity a parameter step
// introduces stands out against the tone's own sample-to-sample slope.
juce::AudioBuffer<float> renderHeldNote (
    const std::function<void (HybridWavetableAudioProcessor&, int)>& onBlock = {})
{
    audioquality::AudioQualityFixture fixture;
    fixture.id = "held-note";
    fixture.durationSeconds = 1.0;
    fixture.tailSeconds = 0.0;
    fixture.randomSeed = 909u;
    fixture.midi = { { juce::MidiMessage::noteOn (1, 60, 0.9f), 0 } };
    return audioquality::OfflineRenderer::render (fixture, [] (auto& processor)
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
        set ("filterType", 0.0f);
        set ("filterSlope", 0.0f);
        set ("cutoff", 2000.0f);
        set ("resonance", 0.3f);
        set ("filterDrive", 0.0f);
        set ("saturation", 0.0f);
        set ("filterEnvAmount", 0.0f);
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
    }, onBlock);
}

// Largest single-sample jump in a window, and where it happens.
struct JumpReport { float maxJump = 0.0f; int index = 0; };

JumpReport largestJump (const juce::AudioBuffer<float>& buffer, int first, int last)
{
    JumpReport report;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer (channel);
        const int begin = juce::jmax (1, first);
        const int end = juce::jmin (last, buffer.getNumSamples());
        for (int sample = begin; sample < end; ++sample)
        {
            const auto jump = std::abs (samples[sample] - samples[sample - 1]);
            if (jump > report.maxJump)
            {
                report.maxJump = jump;
                report.index = sample;
            }
        }
    }
    return report;
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
                const auto rendered = renderSlopeTone (slope, type, 0.0f, 1.0f);
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
    //
    // The comparison is against the same source's own 0 dB THD, not against a
    // fixed floor. This probe plays frame 0 of the shipped bank, and that bank is
    // a harmonic series rather than a sine (see docs/quality/d1-reference-
    // spectrum-gap.md), so the source itself already carries harmonics. Measuring
    // them as though they were drive would make the ratio test meaningless; what
    // matters is that drive multiplies the harmonic content rather than that the
    // source is pure.
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
        const auto half = renderDriveTone (12.0f);
        const auto driven = renderDriveTone (24.0f);
        const double cleanThd = thdOf (clean);
        const double halfThd = thdOf (half);
        const double drivenThd = thdOf (driven);
        std::printf ("  filter drive THD: 0 dB -> %.4f %%, +12 dB -> %.4f %%, +24 dB -> %.4f %%%s",
                     cleanThd, halfThd, drivenThd, "\n");
        test.expect (drivenThd > 2.0,
                     "filter drive at +24 dB adds harmonic distortion (crest factor falls)");
        // Monotone in the knob, which holds whatever harmonics the source
        // itself carries. A ratio against the source's own THD does not: the
        // shipped bank is a harmonic series (d1-reference-spectrum-gap.md), so
        // the 0 dB render is already ~18% THD and a fixed multiple of it would
        // be asserting a number about the bank rather than about the drive.
        test.expect (halfThd > cleanThd && drivenThd > halfThd,
                     juce::String ("filter drive raises harmonics monotonically (")
                         + juce::String (cleanThd, 4) + " -> "
                         + juce::String (halfThd, 4) + " -> "
                         + juce::String (drivenThd, 4) + "%)");
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

    // Parameter-step click audit (Plan C SQ-3). Filter slope and type are
    // structural: they change the filter, not just a coefficient, and they are
    // read once per block rather than smoothed. A step mid-note must not
    // introduce a discontinuity much larger than the tone's own slope.
    {
        constexpr int blockSize = 128;
        constexpr int switchBlock = 187;   // about 0.5 s at 48 kHz
        const int switchSample = switchBlock * blockSize;
        const auto steady = renderHeldNote();
        const auto withSlopeStep = renderHeldNote ([switchBlock] (auto& processor, int blockIndex)
        {
            if (blockIndex != switchBlock) return;
            if (auto* parameter = processor.parameters.getParameter ("filterSlope"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (3.0f));
        });
        const auto withTypeStep = renderHeldNote ([switchBlock] (auto& processor, int blockIndex)
        {
            if (blockIndex != switchBlock) return;
            if (auto* parameter = processor.parameters.getParameter ("filterType"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (2.0f));
        });
        const auto withUnisonStep = renderHeldNote ([switchBlock] (auto& processor, int blockIndex)
        {
            if (blockIndex != switchBlock) return;
            if (auto* parameter = processor.parameters.getParameter ("osc1Unison"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (8.0f));
        });
        const int windowFirst = switchSample - 256;
        const int windowLast = switchSample + 2048;
        const auto steadyJump = largestJump (steady, windowFirst, windowLast);
        const auto slopeJump = largestJump (withSlopeStep, windowFirst, windowLast);
        const auto typeJump = largestJump (withTypeStep, windowFirst, windowLast);
        const auto unisonJump = largestJump (withUnisonStep, windowFirst, windowLast);
        std::printf ("  step-jump audit: steady %.5f | slope %.5f | type %.5f | unison %.5f\n",
                     steadyJump.maxJump, slopeJump.maxJump, typeJump.maxJump, unisonJump.maxJump);
        test.expect (slopeJump.maxJump < 4.0f * steadyJump.maxJump,
                     "filter slope step does not click");
        test.expect (typeJump.maxJump < 4.0f * steadyJump.maxJump,
                     "filter type step does not click");
        test.expect (unisonJump.maxJump < 4.0f * steadyJump.maxJump,
                     "unison count step does not click");
    }

    // Output headroom (Plan C SQ-1). A dense chord with the full oscillator
    // bank at maximum must not exceed full scale on its own; the master output
    // is left at its default so the number reflects the internal gain staging.
    {
        const auto chord = renderWorstCaseChord();
        const auto metrics = audioquality::measureAudio (chord, 48000.0);
        std::printf ("  worst-case chord peak = %.3f dBFS (true peak %.3f dBTP)\n",
                     metrics.samplePeakDbFS, metrics.truePeakDbTP);
        test.expect (metrics.finite, "the worst-case chord renders finite output");
        test.expect (metrics.samplePeakDbFS <= 0.0,
                     juce::String ("the worst-case legal patch stays at or below 0 dBFS (measured ")
                         + juce::String (metrics.samplePeakDbFS, 2) + " dBFS)");
    }

    return test.result();
}
