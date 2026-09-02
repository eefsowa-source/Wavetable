#pragma once

#include "AudioQualityTypes.h"

namespace audioquality
{
struct GoldenComparison
{
    bool identityMatches = false;
    double alignedErrorDbFS = 0.0;
    double loudnessDelta = 0.0;
    double truePeakDeltaDb = 0.0;
    double spectralMedianDeltaDb = 0.0;
    double spectralP95DeltaDb = 0.0;
    int alignmentSamples = 0;
};

GoldenComparison compareWithGolden (const juce::AudioBuffer<float>& candidate,
                                    const juce::AudioBuffer<float>& golden,
                                    double sampleRate,
                                    int maximumAlignmentSamples = 32);
}
