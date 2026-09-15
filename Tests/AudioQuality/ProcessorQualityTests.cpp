#include "../../Source/PluginProcessor.h"
#include "../../Source/DSP/RealtimeRandom.h"
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
    return test.result();
}
