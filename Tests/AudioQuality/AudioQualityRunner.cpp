#include "OfflineRenderer.h"
#include "QualityReport.h"
#include "Metrics.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace audioquality;

namespace
{
struct Options
{
    juce::File manifest;
    juce::File outputDirectory;
    juce::String fixtureId;
    juce::String fixtureGroup;
    juce::String matrix = "per-build";
    bool writeAudio = false;
};

juce::File resolveExistingPath (const juce::String& value)
{
    if (value.startsWithChar ('/'))
        return juce::File (value);
    auto directory = juce::File::getCurrentWorkingDirectory();
    for (int depth = 0; depth < 8; ++depth)
    {
        const auto candidate = directory.getChildFile (value);
        if (candidate.exists())
            return candidate;
        const auto parent = directory.getParentDirectory();
        if (parent == directory)
            break;
        directory = parent;
    }
    return juce::File::getCurrentWorkingDirectory().getChildFile (value);
}

bool takeValue (int& index, int argc, char** argv, juce::String& value)
{
    if (index + 1 >= argc)
        return false;
    value = argv[++index];
    return true;
}

bool parseOptions (int argc, char** argv, Options& options)
{
    juce::String manifestValue, outputValue;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String argument (argv[i]);
        juce::String value;
        if (argument == "--manifest" && takeValue (i, argc, argv, value)) manifestValue = value;
        else if (argument == "--output-dir" && takeValue (i, argc, argv, value)) outputValue = value;
        else if (argument == "--fixture" && takeValue (i, argc, argv, options.fixtureId)) {}
        else if (argument == "--fixture-group" && takeValue (i, argc, argv, options.fixtureGroup)) {}
        else if (argument == "--matrix" && takeValue (i, argc, argv, options.matrix)) {}
        else if (argument == "--write-audio") options.writeAudio = true;
        else return false;
    }
    options.manifest = resolveExistingPath (manifestValue);
    if (outputValue.startsWithChar ('/'))
        options.outputDirectory = juce::File (outputValue);
    else
        options.outputDirectory = juce::File::getCurrentWorkingDirectory().getChildFile (outputValue);
    return options.manifest.existsAsFile() && outputValue.isNotEmpty();
}

const juce::var getProperty (juce::DynamicObject* object, const char* name, const juce::var& fallback = {})
{
    return object != nullptr ? object->getProperty (name) : fallback;
}

AudioQualityFixture fixtureFromVar (const juce::var& value)
{
    AudioQualityFixture fixture;
    if (auto* object = value.getDynamicObject())
    {
        fixture.id = getProperty (object, "id", "unnamed").toString();
        fixture.sampleRate = (double) getProperty (object, "sampleRate", 48000).operator double();
        fixture.blockSize = (int) getProperty (object, "blockSize", 128);
        fixture.channels = (int) getProperty (object, "channels", 2);
        fixture.durationSeconds = (double) getProperty (object, "durationSeconds", 2.0);
        fixture.tailSeconds = (double) getProperty (object, "tailSeconds", 0.5);
        fixture.randomSeed = (std::uint32_t) (int64_t) getProperty (object, "seed", 0x53454f55);
        if (auto* midi = getProperty (object, "midi").getArray())
            for (const auto& event : *midi)
                if (auto* eventObject = event.getDynamicObject())
                {
                    const auto type = getProperty (eventObject, "type").toString();
                    const auto note = (int) getProperty (eventObject, "note", 60);
                    const auto velocity = (float) getProperty (eventObject, "velocity", 0.8);
                    const auto sample = (int) getProperty (eventObject, "sample", 0);
                    auto message = type == "noteOff" ? juce::MidiMessage::noteOff (1, note)
                                                       : juce::MidiMessage::noteOn (1, note, velocity);
                    fixture.midi.push_back ({ message, sample });
                }
    }
    return fixture;
}

juce::String fixtureStateHash (const AudioQualityFixture& fixture)
{
    juce::String text = fixture.id + ":" + juce::String (fixture.sampleRate, 3)
                      + ":" + juce::String (fixture.blockSize)
                      + ":" + juce::String (fixture.channels)
                      + ":" + juce::String (fixture.durationSeconds, 3)
                      + ":" + juce::String (fixture.tailSeconds, 3)
                      + ":" + juce::String ((int64_t) fixture.randomSeed);
    for (const auto& event : fixture.midi)
        text << ":" << event.sampleOffset << ":" << event.message.getDescription();
    return juce::SHA256 (text.toRawUTF8(), text.getNumBytesAsUTF8()).toHexString();
}

bool writeAudioFile (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate)
{
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
        return false;
    const auto options = juce::AudioFormatWriterOptions().withSampleRate (sampleRate)
                                                         .withNumChannels (audio.getNumChannels())
                                                         .withBitsPerSample (24);
    auto writer = format.createWriterFor (stream, options);
    return writer != nullptr && writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}

bool containsGroup (const juce::var& value, const juce::String& group)
{
    if (auto* groups = value.getDynamicObject() != nullptr
                    ? value.getDynamicObject()->getProperty ("groups").getArray() : nullptr)
        for (const auto& item : *groups)
            if (item.toString() == group)
                return true;
    return false;
}

struct GoldenSpec
{
    juce::File file;
    juce::String sha256;
    double sampleRate = -1.0;
    int blockSize = -1;
    int channels = -1;
    juce::String parameterStateHash;
};

juce::File resolveRelativeTo (const juce::File& base, const juce::String& value)
{
    if (value.startsWithChar ('/'))
        return juce::File (value);
    return base.getChildFile (value);
}

struct GoldenEntry
{
    juce::String id;
    GoldenSpec spec;
};

std::vector<GoldenEntry> loadGoldenEntries (const juce::File& fixtureManifest)
{
    std::vector<GoldenEntry> entries;
    const auto manifest = fixtureManifest.getParentDirectory().getChildFile ("golden/manifest.json");
    if (! manifest.existsAsFile())
        return entries;

    const auto parsed = juce::JSON::parse (manifest);
    auto* root = parsed.getDynamicObject();
    auto* values = root != nullptr ? root->getProperty ("goldens").getArray() : nullptr;
    if (root == nullptr || (int) root->getProperty ("schemaVersion") != 1 || values == nullptr)
        return entries;

    const auto base = manifest.getParentDirectory();
    for (const auto& value : *values)
    {
        auto* object = value.getDynamicObject();
        if (object == nullptr)
            continue;
        GoldenEntry entry;
        entry.id = object->getProperty ("id").toString();
        const auto fileValue = object->getProperty ("file").toString().isNotEmpty()
                              ? object->getProperty ("file").toString()
                              : object->getProperty ("wav").toString();
        entry.spec.file = resolveRelativeTo (base, fileValue.isNotEmpty() ? fileValue : entry.id + ".wav");
        entry.spec.sha256 = object->getProperty ("sha256").toString();
        if (object->hasProperty ("sampleRate")) entry.spec.sampleRate = (double) object->getProperty ("sampleRate");
        if (object->hasProperty ("blockSize")) entry.spec.blockSize = (int) object->getProperty ("blockSize");
        if (object->hasProperty ("channels")) entry.spec.channels = (int) object->getProperty ("channels");
        entry.spec.parameterStateHash = object->getProperty ("parameterStateHash").toString();
        if (entry.id.isNotEmpty())
            entries.push_back (std::move (entry));
    }
    return entries;
}

const GoldenEntry* findGolden (const std::vector<GoldenEntry>& entries, const juce::String& id)
{
    for (const auto& entry : entries)
        if (entry.id == id)
            return &entry;
    return nullptr;
}

juce::AudioBuffer<float> readAudioFile (const juce::File& file, double& sampleRate)
{
    juce::AudioBuffer<float> result;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels <= 0)
        return result;
    sampleRate = reader->sampleRate;
    result.setSize ((int) reader->numChannels, (int) reader->lengthInSamples, false, true, true);
    reader->read (&result, 0, result.getNumSamples(), 0, true, true);
    return result;
}

std::vector<AudioQualityFixture> expandFixtures (const AudioQualityFixture& source,
                                                  const juce::String& matrix)
{
    const std::vector<double> sampleRates = matrix == "full"
        ? std::vector<double> { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 }
        : std::vector<double> { 48000.0 };
    const std::vector<int> blockSizes = matrix == "full"
        ? std::vector<int> { 16, 32, 64, 128, 256, 512, 1024, 2048 }
        : std::vector<int> { 64, 128, 512 };
    const std::vector<int> channels = { 1, 2 };
    std::vector<AudioQualityFixture> result;
    std::size_t variantIndex = 0;
    for (const auto sampleRate : sampleRates)
        for (const auto blockSize : blockSizes)
            for (const auto channelCount : channels)
            {
                auto fixture = source;
                fixture.sampleRate = sampleRate;
                fixture.blockSize = blockSize;
                fixture.channels = channelCount;
                fixture.id = source.id + "__sr" + juce::String ((int) std::llround (sampleRate))
                           + "_bs" + juce::String (blockSize) + "_ch" + juce::String (channelCount);

                if (matrix == "full" && ! fixture.midi.empty())
                {
                    // Keep the event's block index (and therefore its musical
                    // timing) while cycling the remainder through the three
                    // callback-boundary positions. Across the full matrix this
                    // covers offset 0, block-1, and block/3 without tripling
                    // the matrix size.
                    const std::array<int, 3> remainders { 0, juce::jmax (0, blockSize - 1), blockSize / 3 };
                    const auto boundaryMode = (int) (variantIndex % remainders.size());
                    for (std::size_t eventIndex = 0; eventIndex < fixture.midi.size(); ++eventIndex)
                    {
                        auto& event = fixture.midi[eventIndex];
                        if (event.sampleOffset <= 0)
                            continue;
                        const auto blockIndex = event.sampleOffset / blockSize;
                        event.sampleOffset = blockIndex * blockSize
                                           + remainders[(eventIndex + (std::size_t) boundaryMode) % remainders.size()];
                    }
                    fixture.id << "_b" << boundaryMode;
                }

                const auto totalSamples = (int) std::llround ((fixture.durationSeconds + fixture.tailSeconds)
                                                               * fixture.sampleRate);
                if (matrix == "full" && totalSamples > 0 && totalSamples % fixture.blockSize == 0)
                {
                    // Force the renderer to exercise its short final block.
                    fixture.durationSeconds += 1.0 / fixture.sampleRate;
                    fixture.id << "_irr";
                }
                result.push_back (std::move (fixture));
                ++variantIndex;
            }
    return result;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    Options options;
    if (! parseOptions (argc, argv, options))
    {
        std::cerr << "usage: --manifest <path> --output-dir <path> [--fixture <id> | --fixture-group <name>] [--matrix per-build|full] [--write-audio]\n";
        return 2;
    }
    if (options.matrix != "per-build" && options.matrix != "full")
        return 2;
    if (! options.outputDirectory.createDirectory().wasOk())
        return 2;
    const auto parsed = juce::JSON::parse (options.manifest);
    auto* root = parsed.getDynamicObject();
    const auto schemaVersion = root != nullptr ? (int) root->getProperty ("schemaVersion") : 0;
    auto* fixtures = root != nullptr ? root->getProperty ("fixtures").getArray() : nullptr;
    if (schemaVersion != 1 || fixtures == nullptr)
        return 2;

    juce::StringArray failures;
    juce::StringArray passes;
    juce::String summary = "# SEOUL DSP Audio Quality Report\n\n";
    summary << "Manifest schema: " << schemaVersion << "\n\n";
    const auto executable = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    const auto executableHash = executable.existsAsFile() ? juce::SHA256 (executable).toHexString() : "unavailable";
    const auto goldenEntries = loadGoldenEntries (options.manifest);
    int fixtureCount = 0;
    int finiteFailureCount = 0;
    for (const auto& fixtureValue : *fixtures)
    {
        const auto baseFixture = fixtureFromVar (fixtureValue);
        if (baseFixture.id.isEmpty())
            continue;
        if (options.fixtureId.isNotEmpty() && baseFixture.id != options.fixtureId)
            continue;
        if (options.fixtureGroup.isNotEmpty() && ! containsGroup (fixtureValue, options.fixtureGroup))
            continue;

        for (const auto& fixture : expandFixtures (baseFixture, options.matrix))
        {
            ++fixtureCount;
            const auto rendered = OfflineRenderer::render (fixture, {});
            QualityReportData report;
            report.fixtureId = fixture.id;
            report.manifestSchemaVersion = schemaVersion;
            report.executableSha256 = executableHash;
            report.sampleRate = fixture.sampleRate;
            report.blockSize = fixture.blockSize;
            report.channels = fixture.channels;
            report.seed = fixture.randomSeed;
            report.durationSeconds = fixture.durationSeconds;
            report.tailSeconds = fixture.tailSeconds;
            report.metrics = measureAudio (rendered, fixture.sampleRate);
            if (! report.metrics.finite)
                ++finiteFailureCount;
            report.parameterStateHash = fixtureStateHash (fixture);

            const auto* golden = findGolden (goldenEntries, fixture.id);
            juce::AudioBuffer<float> goldenAudio;
            double goldenSampleRate = 0.0;
            bool metadataMatches = golden != nullptr;
            if (golden != nullptr)
            {
                metadataMatches = golden->spec.file.existsAsFile();
                if (metadataMatches && golden->spec.sha256.isNotEmpty())
                    metadataMatches = juce::SHA256 (golden->spec.file).toHexString().equalsIgnoreCase (golden->spec.sha256);
                goldenAudio = readAudioFile (golden->spec.file, goldenSampleRate);
                metadataMatches = metadataMatches && goldenAudio.getNumSamples() > 0
                               && goldenAudio.getNumChannels() == fixture.channels
                               && std::abs (goldenSampleRate - fixture.sampleRate) < 0.5;
                if (golden->spec.sampleRate > 0.0)
                    metadataMatches = metadataMatches && std::abs (golden->spec.sampleRate - fixture.sampleRate) < 0.5;
                if (golden->spec.blockSize > 0)
                    metadataMatches = metadataMatches && golden->spec.blockSize == fixture.blockSize;
                if (golden->spec.channels > 0)
                    metadataMatches = metadataMatches && golden->spec.channels == fixture.channels;
                if (golden->spec.parameterStateHash.isNotEmpty())
                    metadataMatches = metadataMatches && golden->spec.parameterStateHash == report.parameterStateHash;
            }
            report.goldenPresent = golden != nullptr && metadataMatches;
            if (report.goldenPresent)
            {
                report.comparison = compareWithGolden (rendered, goldenAudio, fixture.sampleRate);
                report.passed = report.metrics.finite
                             && report.comparison.alignedErrorDbFS <= -120.0
                             && report.comparison.loudnessDelta <= 0.25
                             && report.comparison.truePeakDeltaDb <= 0.5
                             && report.comparison.spectralMedianDeltaDb <= 0.5
                             && report.comparison.spectralP95DeltaDb <= 2.0;
                if (! report.comparison.identityMatches)
                    report.firstDivergentBlock = 0;
            }
            else
            {
                report.passed = false;
            }

            const auto reportFile = options.outputDirectory.getChildFile (fixture.id + ".json");
            if (! writeQualityReport (reportFile, report))
                failures.add (fixture.id + " (report write failed)");
            else if (! report.goldenPresent)
                failures.add (fixture.id + (golden == nullptr ? " (golden missing)" : " (golden metadata mismatch)"));
            else if (! report.passed)
                failures.add (fixture.id + " (quality threshold failed)");
            else
                passes.add (fixture.id);
            if (options.writeAudio)
                writeAudioFile (options.outputDirectory.getChildFile (fixture.id + ".wav"), rendered, fixture.sampleRate);
        }
    }
    summary << "Matrix: " << options.matrix << "\nFixtures rendered: " << fixtureCount << "\n\n";
    for (const auto& failure : failures)
        summary << "- FAIL: " << failure << "\n";
    for (const auto& pass : passes)
        summary << "- PASS: " << pass << "\n";
    if (failures.isEmpty() && passes.isEmpty())
        summary << "- FAIL: no fixture matched the selector\n";
    options.outputDirectory.getChildFile ("summary.md").replaceWithText (summary);

    auto runReport = std::make_unique<juce::DynamicObject>();
    runReport->setProperty ("manifestSchemaVersion", schemaVersion);
    runReport->setProperty ("sourceIdentity", "SEOUL DSP");
    runReport->setProperty ("matrix", options.matrix);
    runReport->setProperty ("fixtureCount", fixtureCount);
    runReport->setProperty ("finiteFailureCount", finiteFailureCount);
    runReport->setProperty ("passed", failures.isEmpty() && ! passes.isEmpty());
    juce::Array<juce::var> failureArray;
    for (const auto& failure : failures)
        failureArray.add (failure);
    runReport->setProperty ("failures", juce::var (failureArray));
    juce::Array<juce::var> passArray;
    for (const auto& pass : passes)
        passArray.add (pass);
    runReport->setProperty ("passes", juce::var (passArray));
    options.outputDirectory.getChildFile ("report.json")
        .replaceWithText (juce::JSON::toString (juce::var (runReport.release()), true));
    for (const auto& failure : failures)
        std::cout << "FAIL " << failure << "\n";
    return failures.isEmpty() && ! passes.isEmpty() ? 0 : 1;
}
