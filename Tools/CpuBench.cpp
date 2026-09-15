// CPU benchmark harness for the SEOUL DSP audio path.
//
// Measures wall time to render a fixed amount of audio through the real
// HybridWavetableAudioProcessor::processBlock with the documented worst-case
// patch (16 held voices, unison 8/8/8, saturation/drive max, LFO + drift on)
// and a solo-voice scenario for scaling comparison. Output is one JSON array
// on stdout so runs can be archived as evidence next to quality gates.

#include "../Source/PluginProcessor.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
struct BenchResult
{
    std::string scenario;
    int voices = 0;
    double audioSeconds = 0.0;
    std::vector<double> runMilliseconds;
};

void setPlainParameter (HybridWavetableAudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.parameters.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void configureWorstCasePatch (HybridWavetableAudioProcessor& processor)
{
    setPlainParameter (processor, "osc1Level", 1.0f);
    setPlainParameter (processor, "osc2Level", 1.0f);
    setPlainParameter (processor, "osc3Level", 1.0f);
    setPlainParameter (processor, "osc1Unison", 8.0f);
    setPlainParameter (processor, "osc2Unison", 8.0f);
    setPlainParameter (processor, "osc3Unison", 8.0f);
    setPlainParameter (processor, "osc1Detune", 24.0f);
    setPlainParameter (processor, "osc2Detune", 24.0f);
    setPlainParameter (processor, "osc3Detune", 24.0f);
    setPlainParameter (processor, "osc1Spread", 0.8f);
    setPlainParameter (processor, "osc2Spread", 0.8f);
    setPlainParameter (processor, "osc3Spread", 0.8f);
    setPlainParameter (processor, "cutoff", 12000.0f);
    setPlainParameter (processor, "saturation", 1.0f);
    setPlainParameter (processor, "filterDrive", 12.0f);
    setPlainParameter (processor, "lfo1Rate", 5.0f);
    setPlainParameter (processor, "lfo1Depth", 0.5f);
    setPlainParameter (processor, "lfo2Rate", 3.0f);
    setPlainParameter (processor, "lfo2Depth", 0.5f);
    setPlainParameter (processor, "driftRate", 0.8f);
    setPlainParameter (processor, "driftDepth", 1.0f);
    setPlainParameter (processor, "ampAttack", 0.001f);
    setPlainParameter (processor, "ampSustain", 0.9f);
    setPlainParameter (processor, "output", 0.0f);
}

BenchResult runScenario (const std::string& scenario, int voiceCount,
                         double audioSeconds, std::uint32_t seed)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 64;
    HybridWavetableAudioProcessor processor (seed);
    processor.setBusesLayout ({ {}, juce::AudioChannelSet::stereo() });
    processor.prepareToPlay (sampleRate, blockSize);
    configureWorstCasePatch (processor);

    const int totalSamples = (int) (audioSeconds * sampleRate);
    std::vector<int> notes ((size_t) voiceCount);
    for (int i = 0; i < voiceCount; ++i)
        notes[(size_t) i] = 36 + i * 5; // spread from C2 upward

    auto renderOnce = [&] (bool measure)
    {
        const auto started = std::chrono::steady_clock::now();
        for (int absoluteStart = 0; absoluteStart < totalSamples; absoluteStart += blockSize)
        {
            const auto samplesThisBlock = std::min (blockSize, totalSamples - absoluteStart);
            juce::AudioBuffer<float> block (2, samplesThisBlock);
            block.clear();
            juce::MidiBuffer midi;
            if (absoluteStart == 0)
                for (int i = 0; i < voiceCount; ++i)
                    midi.addEvent (juce::MidiMessage::noteOn (1, notes[(size_t) i], 0.9f), 0);
            processor.processBlock (block, midi);
        }
        const auto finished = std::chrono::steady_clock::now();
        if (! measure)
            return 0.0;
        return std::chrono::duration<double, std::milli> (finished - started).count();
    };

    renderOnce (false); // warm-up, not measured
    BenchResult result;
    result.scenario = scenario;
    result.voices = voiceCount;
    result.audioSeconds = audioSeconds;
    for (int run = 0; run < 3; ++run)
        result.runMilliseconds.push_back (renderOnce (true));
    processor.releaseResources();
    return result;
}

double medianOf (std::vector<double> values)
{
    std::sort (values.begin(), values.end());
    return values.empty() ? 0.0 : values[values.size() / 2];
}

juce::var resultToVar (const BenchResult& result)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("scenario", juce::String (result.scenario));
    object->setProperty ("voices", result.voices);
    object->setProperty ("audioSeconds", result.audioSeconds);
    juce::Array<juce::var> runs;
    for (const auto milliseconds : result.runMilliseconds)
        runs.add (milliseconds);
    object->setProperty ("runsMs", runs);
    const double median = medianOf (result.runMilliseconds);
    object->setProperty ("medianMs", median);
    object->setProperty ("realtimeFactor", result.audioSeconds * 1000.0 / median);
    return juce::var (object);
}
}

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto solo = runScenario ("solo-unison8", 1, 2.0, 0x53454f55u);
    const auto dense = runScenario ("dense16-unison8", 16, 2.0, 0x53454f55u);
    juce::Array<juce::var> results;
    results.add (resultToVar (solo));
    results.add (resultToVar (dense));
    std::cout << juce::JSON::toString (juce::var (results), true) << std::endl;
    return 0;
}
