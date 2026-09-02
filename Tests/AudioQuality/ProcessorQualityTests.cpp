#include "../../Source/PluginProcessor.h"
#include "../../Source/DSP/RealtimeRandom.h"
#include "TestHarness.h"

#include <cmath>
#include <array>

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
    return test.result();
}
