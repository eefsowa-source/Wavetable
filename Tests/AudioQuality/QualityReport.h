#pragma once

#include "GoldenComparator.h"

namespace audioquality
{
struct QualityReportData
{
    juce::String fixtureId;
    int manifestSchemaVersion = 1;
    juce::String sourceIdentity = "SEOUL DSP";
    juce::String executableSha256;
    double sampleRate = 48000.0;
    int blockSize = 128;
    int channels = 2;
    std::uint32_t seed = 0;
    juce::String parameterStateHash;
    double durationSeconds = 0.0;
    double tailSeconds = 0.0;
    AudioMetrics metrics;
    GoldenComparison comparison;
    bool goldenPresent = false;
    bool passed = false;
    int firstDivergentBlock = -1;
};

juce::var makeQualityReport (const QualityReportData& data);
bool writeQualityReport (const juce::File& file, const QualityReportData& data);
}
