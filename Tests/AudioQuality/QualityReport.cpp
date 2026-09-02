#include "QualityReport.h"
#include <cmath>

namespace audioquality
{
namespace
{
juce::var metricValue (double value, juce::DynamicObject& state)
{
    if (! std::isfinite (value))
    {
        state.setProperty ("metricState", "silence");
        return juce::var ("-inf");
    }
    return juce::var (value);
}
}

juce::var makeQualityReport (const QualityReportData& data)
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("fixtureId", data.fixtureId);
    root->setProperty ("manifestSchemaVersion", data.manifestSchemaVersion);
    root->setProperty ("sourceIdentity", data.sourceIdentity);
    root->setProperty ("executableSha256", data.executableSha256);
    root->setProperty ("sampleRate", data.sampleRate);
    root->setProperty ("blockSize", data.blockSize);
    root->setProperty ("channels", data.channels);
    root->setProperty ("seed", (int64_t) data.seed);
    root->setProperty ("parameterStateHash", data.parameterStateHash);
    root->setProperty ("durationSeconds", data.durationSeconds);
    root->setProperty ("tailSeconds", data.tailSeconds);
    auto metrics = std::make_unique<juce::DynamicObject>();
    metrics->setProperty ("samplePeakDbFS", metricValue (data.metrics.samplePeakDbFS, *metrics));
    metrics->setProperty ("truePeakDbTP", metricValue (data.metrics.truePeakDbTP, *metrics));
    metrics->setProperty ("rmsDbFS", metricValue (data.metrics.rmsDbFS, *metrics));
    metrics->setProperty ("dcDbFS", metricValue (data.metrics.dcDbFS, *metrics));
    metrics->setProperty ("integratedLufs", metricValue (data.metrics.integratedLufs, *metrics));
    metrics->setProperty ("finite", data.metrics.finite);
    root->setProperty ("metrics", juce::var (metrics.release()));
    auto comparison = std::make_unique<juce::DynamicObject>();
    comparison->setProperty ("identityMatches", data.comparison.identityMatches);
    comparison->setProperty ("alignedErrorDbFS", metricValue (data.comparison.alignedErrorDbFS, *comparison));
    comparison->setProperty ("loudnessDelta", data.comparison.loudnessDelta);
    comparison->setProperty ("truePeakDeltaDb", data.comparison.truePeakDeltaDb);
    comparison->setProperty ("spectralMedianDeltaDb", data.comparison.spectralMedianDeltaDb);
    comparison->setProperty ("spectralP95DeltaDb", data.comparison.spectralP95DeltaDb);
    comparison->setProperty ("alignmentSamples", data.comparison.alignmentSamples);
    root->setProperty ("comparison", juce::var (comparison.release()));
    auto thresholds = std::make_unique<juce::DynamicObject>();
    thresholds->setProperty ("alignedErrorDbFS", -120.0);
    thresholds->setProperty ("shortFixtureRmsDeltaDb", 0.25);
    thresholds->setProperty ("loudnessDeltaLu", 0.25);
    thresholds->setProperty ("truePeakDeltaDb", 0.5);
    thresholds->setProperty ("spectralMedianDeltaDb", 0.5);
    thresholds->setProperty ("spectralP95DeltaDb", 2.0);
    root->setProperty ("thresholds", juce::var (thresholds.release()));
    root->setProperty ("goldenPresent", data.goldenPresent);
    root->setProperty ("pass", data.passed);
    root->setProperty ("firstDivergentBlock", data.firstDivergentBlock);
    return juce::var (root.release());
}

bool writeQualityReport (const juce::File& file, const QualityReportData& data)
{
    if (file.getParentDirectory().createDirectory().failed())
        return false;
    return file.replaceWithText (juce::JSON::toString (makeQualityReport (data), true));
}
}
