#include "OfflineRenderer.h"
#include "QualityReport.h"
#include "Metrics.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <algorithm>
#include <iostream>

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
    for (const auto& fixtureValue : *fixtures)
    {
        auto fixture = fixtureFromVar (fixtureValue);
        if (fixture.id.isEmpty())
            continue;
        if (options.fixtureId.isNotEmpty() && fixture.id != options.fixtureId)
            continue;
        if (options.fixtureGroup.isNotEmpty() && ! containsGroup (fixtureValue, options.fixtureGroup))
            continue;

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
        report.parameterStateHash = fixtureStateHash (fixture);
        report.goldenPresent = false;
        report.passed = false;
        const auto reportFile = options.outputDirectory.getChildFile (fixture.id + ".json");
        if (! writeQualityReport (reportFile, report))
            failures.add (fixture.id + " (report write failed)");
        else
            failures.add (fixture.id + " (golden missing)");
        if (options.writeAudio)
            writeAudioFile (options.outputDirectory.getChildFile (fixture.id + ".wav"), rendered, fixture.sampleRate);
    }
    for (const auto& failure : failures)
        summary << "- FAIL: " << failure << "\n";
    for (const auto& pass : passes)
        summary << "- PASS: " << pass << "\n";
    if (failures.isEmpty() && passes.isEmpty())
        summary << "- FAIL: no fixture matched the selector\n";
    options.outputDirectory.getChildFile ("summary.md").replaceWithText (summary);
    for (const auto& failure : failures)
        std::cout << "FAIL " << failure << "\n";
    return failures.isEmpty() && ! passes.isEmpty() ? 0 : 1;
}
