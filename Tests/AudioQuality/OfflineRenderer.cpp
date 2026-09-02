#include "OfflineRenderer.h"

#include <algorithm>

namespace audioquality
{
juce::AudioBuffer<float> OfflineRenderer::render (const AudioQualityFixture& fixture,
                                                   const std::function<void (HybridWavetableAudioProcessor&)>& configure)
{
    const auto channels = juce::jlimit (1, 2, fixture.channels);
    const auto sampleRate = fixture.sampleRate > 0.0 ? fixture.sampleRate : 48000.0;
    const auto blockSize = juce::jmax (1, fixture.blockSize);
    const auto renderSeconds = juce::jmax (0.0, fixture.durationSeconds + fixture.tailSeconds);
    const auto totalSamples = juce::jmax (0, (int) std::ceil (renderSeconds * sampleRate));
    juce::AudioBuffer<float> result (channels, totalSamples);
    result.clear();

    HybridWavetableAudioProcessor processor (fixture.randomSeed);
    const auto busSet = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    processor.setBusesLayout ({ {}, busSet });
    processor.prepareToPlay (sampleRate, blockSize);
    if (configure)
        configure (processor);

    std::vector<MidiEventSpec> events = fixture.midi;
    std::stable_sort (events.begin(), events.end(), [] (const auto& lhs, const auto& rhs)
    {
        return lhs.sampleOffset < rhs.sampleOffset;
    });

    size_t nextEvent = 0;
    for (int absoluteStart = 0; absoluteStart < totalSamples; absoluteStart += blockSize)
    {
        const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - absoluteStart);
        juce::AudioBuffer<float> block (channels, samplesThisBlock);
        block.clear();
        juce::MidiBuffer midi;
        while (nextEvent < events.size() && events[nextEvent].sampleOffset < absoluteStart)
            ++nextEvent;
        auto eventCursor = nextEvent;
        while (eventCursor < events.size() && events[eventCursor].sampleOffset < absoluteStart + samplesThisBlock)
        {
            const auto localOffset = juce::jmax (0, events[eventCursor].sampleOffset - absoluteStart);
            midi.addEvent (events[eventCursor].message, localOffset);
            ++eventCursor;
        }
        nextEvent = eventCursor;
        processor.processBlock (block, midi);
        for (int channel = 0; channel < channels; ++channel)
            result.copyFrom (channel, absoluteStart, block, channel, 0, samplesThisBlock);
    }
    processor.releaseResources();
    return result;
}
}
