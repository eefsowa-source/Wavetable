#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdint>
#include <limits>
#include <vector>

namespace audioquality
{
struct MidiEventSpec
{
    juce::MidiMessage message;
    int sampleOffset = 0;
};

struct AudioQualityFixture
{
    juce::String id;
    double sampleRate = 48000.0;
    int blockSize = 128;
    int channels = 2;
    double durationSeconds = 2.0;
    double tailSeconds = 0.5;
    std::uint32_t randomSeed = 0x53454f55u;
    std::vector<MidiEventSpec> midi;
};

struct AudioMetrics
{
    bool finite = true;
    double samplePeakDbFS = -std::numeric_limits<double>::infinity();
    double truePeakDbTP = -std::numeric_limits<double>::infinity();
    double rmsDbFS = -std::numeric_limits<double>::infinity();
    double dcDbFS = -std::numeric_limits<double>::infinity();
    double integratedLufs = -std::numeric_limits<double>::infinity();
};
}
