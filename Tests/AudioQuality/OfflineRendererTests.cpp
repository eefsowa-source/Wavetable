#include "OfflineRenderer.h"
#include "TestHarness.h"

#include <algorithm>
#include <cmath>

using namespace audioquality;

namespace
{
void setPlainParameter (HybridWavetableAudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.parameters.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

float maxDifference (const juce::AudioBuffer<float>& lhs, const juce::AudioBuffer<float>& rhs, int samples)
{
    float difference = 0.0f;
    const auto channels = juce::jmin (lhs.getNumChannels(), rhs.getNumChannels());
    const auto count = juce::jmin (samples, juce::jmin (lhs.getNumSamples(), rhs.getNumSamples()));
    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < count; ++sample)
            difference = juce::jmax (difference, std::abs (lhs.getSample (channel, sample)
                                                           - rhs.getSample (channel, sample)));
    return difference;
}

int firstNonSilent (const juce::AudioBuffer<float>& buffer, float threshold = 1.0e-8f)
{
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            if (std::abs (buffer.getSample (channel, sample)) > threshold)
                return sample;
    return buffer.getNumSamples();
}
}

int main()
{
    TestHarness test;
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    AudioQualityFixture deterministic;
    deterministic.id = "determinism";
    deterministic.durationSeconds = 0.25;
    deterministic.tailSeconds = 0.05;
    deterministic.randomSeed = 0x13572468u;
    deterministic.midi = { { juce::MidiMessage::noteOn (1, 60, 0.8f), 0 },
                           { juce::MidiMessage::noteOff (1, 60), 6000 } };
    const auto configure = [] (HybridWavetableAudioProcessor& processor)
    {
        setPlainParameter (processor, "randomPhase", 0.0f);
        setPlainParameter (processor, "output", 0.0f);
    };
    const auto first = OfflineRenderer::render (deterministic, configure);
    const auto second = OfflineRenderer::render (deterministic, configure);
    test.expect (first.getNumSamples() == second.getNumSamples(), "deterministic render lengths match");
    test.expect (maxDifference (first, second, first.getNumSamples()) == 0.0f,
                 "same seed and state render sample-identically");

    auto randomized = deterministic;
    randomized.randomSeed += 1u;
    const auto randomizedFirst = OfflineRenderer::render (randomized, [] (auto& processor)
    {
        setPlainParameter (processor, "randomPhase", 1.0f);
        setPlainParameter (processor, "output", 0.0f);
    });
    test.expect (maxDifference (first, randomizedFirst, 1024) > 1.0e-6f,
                 "different seed changes randomized-phase onset");

    for (const auto offset : { 0, deterministic.blockSize - 1, deterministic.blockSize / 3 })
    {
        auto offsetFixture = deterministic;
        offsetFixture.midi = { { juce::MidiMessage::noteOn (1, 60, 0.8f), offset },
                               { juce::MidiMessage::noteOff (1, 60), offset + 2000 } };
        const auto rendered = OfflineRenderer::render (offsetFixture, configure);
        test.expect (firstNonSilent (rendered) >= offset,
                     "MIDI note onset is not rendered before its sample offset");
    }

    auto irregular = deterministic;
    irregular.blockSize = 127;
    irregular.durationSeconds = 0.013;
    irregular.tailSeconds = 0.001;
    irregular.midi = { { juce::MidiMessage::noteOn (1, 60, 0.8f), 126 },
                       { juce::MidiMessage::noteOff (1, 60), 400 } };
    const auto expectedSamples = (int) std::ceil ((irregular.durationSeconds + irregular.tailSeconds)
                                                  * irregular.sampleRate);
    const auto irregularRender = OfflineRenderer::render (irregular, configure);
    test.expect (irregularRender.getNumSamples() == expectedSamples,
                 "irregular final block copies only valid samples");
    bool finite = true;
    for (int channel = 0; channel < irregularRender.getNumChannels(); ++channel)
        for (int sample = 0; sample < irregularRender.getNumSamples(); ++sample)
            finite &= std::isfinite (irregularRender.getSample (channel, sample));
    test.expect (finite, "offline render remains finite");
    return test.result();
}
