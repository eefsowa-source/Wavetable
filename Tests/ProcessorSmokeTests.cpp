#include "../Source/PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <limits>

static bool check (bool condition, const char* message)
{
    if (! condition) std::cerr << "FAIL: " << message << "\n";
    return condition;
}

static void setPlainParameter (HybridWavetableAudioProcessor& processor,
                               const char* id, float value)
{
    if (auto* parameter = processor.parameters.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

static bool writeTestAudio (const juce::File& file, juce::AudioFormat& format)
{
    juce::AudioBuffer<float> source (1, WavetableData::tableSize);
    for (int i = 0; i < source.getNumSamples(); ++i)
        source.setSample (0, i, std::sin (juce::MathConstants<float>::twoPi * i
                                         / (float) source.getNumSamples()));

    std::unique_ptr<juce::OutputStream> output = file.createOutputStream();
    if (output == nullptr)
        return false;
    const auto options = juce::AudioFormatWriterOptions().withSampleRate (48000.0)
                                                         .withNumChannels (1)
                                                         .withBitsPerSample (24);
    auto writer = format.createWriterFor (output, options);
    if (writer == nullptr)
        return false;
    return writer->writeFromAudioSampleBuffer (source, 0, source.getNumSamples());
}

static juce::File locateStateFixture()
{
    auto directory = juce::File::getCurrentWorkingDirectory();
    for (int depth = 0; depth < 8; ++depth)
    {
        const auto candidate = directory.getChildFile ("Tests/AudioQuality/state-v1-fixture.b64");
        if (candidate.existsAsFile())
            return candidate;
        const auto parent = directory.getParentDirectory();
        if (parent == directory)
            break;
        directory = parent;
    }
    return {};
}

static float maximumAudioDifference (const juce::AudioBuffer<float>& lhs,
                                     const juce::AudioBuffer<float>& rhs)
{
    if (lhs.getNumChannels() != rhs.getNumChannels() || lhs.getNumSamples() != rhs.getNumSamples())
        return std::numeric_limits<float>::infinity();
    float maximum = 0.0f;
    for (int channel = 0; channel < lhs.getNumChannels(); ++channel)
        for (int sample = 0; sample < lhs.getNumSamples(); ++sample)
            maximum = juce::jmax (maximum,
                                  std::abs (lhs.getSample (channel, sample)
                                            - rhs.getSample (channel, sample)));
    return maximum;
}

static void removeParameterFromXml (juce::XmlElement& xml, const char* id)
{
    for (int index = xml.getNumChildElements(); --index >= 0;)
        if (auto* child = xml.getChildElement (index))
            if (child->hasTagName ("PARAM") && child->getStringAttribute ("id") == id)
                xml.removeChildElement (child, true);
}

int main()
{
    bool ok = true;
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    HybridWavetableAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    ok &= check (processor.getName() == "SEOUL DSP", "plugin identity is SEOUL DSP");
    const char* foundationIDs[] = { "osc1Level", "osc1Tune", "osc1Unison", "osc1Spread",
                                    "osc2Level", "osc2Tune", "osc2Unison", "osc2Spread",
                                    "osc3Level", "osc3Tune", "osc3Unison", "osc3Spread",
                                    "osc1Detune", "osc2Detune", "osc3Detune", "unisonKeyTrack",
                                    "filterEnvAmount", "masterWidth" };
    for (const auto* id : foundationIDs)
        ok &= check (processor.parameters.getParameter (id) != nullptr, "expanded synthesis parameter is registered");

    // The tabbed workstation exposes these through MOD, FX and ARP. Keep the
    // IDs stable: stored sessions and host automation depend on them.
    const char* workstationIDs[] = { "lfo1Rate", "lfo1Depth", "lfo1Destination",
                                     "lfo2Rate", "lfo2Depth", "lfo2Destination",
                                     "delayTime", "delayFeedback", "delayMix",
                                     "reverbSize", "reverbDamping", "reverbMix",
                                     "arpEnabled", "arpRate", "arpGate", "arpPattern" };
    for (const auto* id : workstationIDs)
        ok &= check (processor.parameters.getParameter (id) != nullptr,
                     "workstation parameter is registered");

    ok &= check (processor.parameters.getParameter ("osc3Pos") != nullptr,
                 "third oscillator parameter is registered");
    ok &= check (HybridWavetableAudioProcessor::getFactoryPresetNames().size() == 10,
                 "ten factory presets are available");
    processor.applyFactoryPreset (6);
    ok &= check (processor.parameters.getRawParameterValue ("filterDrive")->load() >= -6.0f,
                 "factory preset applies filter drive");
    ok &= check (processor.parameters.getRawParameterValue ("osc3Pos")->load() >= 0.0f
                 && processor.parameters.getRawParameterValue ("osc3Pos")->load() <= 1.0f,
                 "factory preset applies third oscillator position");
    for (int presetIndex = 0; presetIndex < 10; ++presetIndex)
    {
        processor.applyFactoryPreset (presetIndex);
        ok &= check (std::isfinite (processor.parameters.getRawParameterValue ("cutoff")->load()),
                     "every factory preset has a finite cutoff");
    }
    ok &= check (processor.parameters.getRawParameterValue ("filterType")->load() >= 0.0f
                 && processor.parameters.getRawParameterValue ("filterType")->load() <= 2.0f,
                 "factory preset selects a valid filter type");
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    processor.processBlock (buffer, midi);
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        { const auto s = buffer.getSample (ch, i); peak = juce::jmax (peak, std::abs (s)); ok &= check (std::isfinite (s), "render output is finite"); }
    ok &= check (peak > 0.0f, "MIDI note produces audio");

    // Spread must create a real stereo source before master width is applied.
    // Keep only Osc 1 audible so its left pan is easy to observe.
    HybridWavetableAudioProcessor stereoProcessor;
    stereoProcessor.prepareToPlay (48000.0, 512);
    setPlainParameter (stereoProcessor, "osc1Level", 1.0f);
    setPlainParameter (stereoProcessor, "osc2Level", 0.0f);
    setPlainParameter (stereoProcessor, "osc3Level", 0.0f);
    setPlainParameter (stereoProcessor, "osc1Spread", 1.0f);
    setPlainParameter (stereoProcessor, "masterWidth", 1.0f);
    juce::AudioBuffer<float> stereoBuffer (2, 512);
    juce::MidiBuffer stereoMidi;
    stereoMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    stereoProcessor.processBlock (stereoBuffer, stereoMidi);
    float stereoDifference = 0.0f;
    for (int sample = 0; sample < stereoBuffer.getNumSamples(); ++sample)
        stereoDifference = juce::jmax (stereoDifference,
                                       std::abs (stereoBuffer.getSample (0, sample)
                                                 - stereoBuffer.getSample (1, sample)));
    ok &= check (stereoDifference > 1.0e-4f,
                 "oscillator spread produces a stereo voice image");

    HybridWavetableAudioProcessor monoProcessor;
    monoProcessor.prepareToPlay (48000.0, 512);
    setPlainParameter (monoProcessor, "osc1Level", 1.0f);
    setPlainParameter (monoProcessor, "osc2Level", 0.0f);
    setPlainParameter (monoProcessor, "osc3Level", 0.0f);
    setPlainParameter (monoProcessor, "osc1Spread", 1.0f);
    setPlainParameter (monoProcessor, "masterWidth", 0.0f);
    juce::AudioBuffer<float> monoBuffer (2, 512);
    juce::MidiBuffer monoMidi;
    monoMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    monoProcessor.processBlock (monoBuffer, monoMidi);
    float monoDifference = 0.0f;
    for (int sample = 0; sample < monoBuffer.getNumSamples(); ++sample)
        monoDifference = juce::jmax (monoDifference,
                                     std::abs (monoBuffer.getSample (0, sample)
                                               - monoBuffer.getSample (1, sample)));
    ok &= check (monoDifference < 1.0e-5f,
                 "master width zero collapses the voice to mono");

    HybridWavetableAudioProcessor gatedArpProcessor;
    gatedArpProcessor.prepareToPlay (48000.0, 2048);
    setPlainParameter (gatedArpProcessor, "arpEnabled", 1.0f);
    setPlainParameter (gatedArpProcessor, "arpRate", 24.0f);
    setPlainParameter (gatedArpProcessor, "arpGate", 0.05f);
    juce::AudioBuffer<float> arpGateBuffer (2, 2048);
    juce::MidiBuffer arpGateMidi;
    arpGateMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    gatedArpProcessor.processBlock (arpGateBuffer, arpGateMidi);
    bool sawGatedNoteOff = false;
    for (const auto metadata : arpGateMidi)
    {
        const auto message = metadata.getMessage();
        sawGatedNoteOff |= message.isNoteOff() && message.getNoteNumber() == 60
                           && metadata.samplePosition > 0 && metadata.samplePosition < 2000;
    }
    ok &= check (sawGatedNoteOff,
                 "arpeggiator gate releases a note before its next step");

    HybridWavetableAudioProcessor disableArpProcessor;
    disableArpProcessor.prepareToPlay (48000.0, 64);
    setPlainParameter (disableArpProcessor, "arpEnabled", 1.0f);
    setPlainParameter (disableArpProcessor, "arpRate", 0.5f);
    setPlainParameter (disableArpProcessor, "arpGate", 1.0f);
    juce::AudioBuffer<float> arpDisableBuffer (2, 64);
    juce::MidiBuffer arpEnableMidi;
    arpEnableMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    disableArpProcessor.processBlock (arpDisableBuffer, arpEnableMidi);
    setPlainParameter (disableArpProcessor, "arpEnabled", 0.0f);
    juce::MidiBuffer arpDisableMidi;
    disableArpProcessor.processBlock (arpDisableBuffer, arpDisableMidi);
    bool sawDisableNoteOff = false;
    for (const auto metadata : arpDisableMidi)
    {
        const auto message = metadata.getMessage();
        sawDisableNoteOff |= message.isNoteOff() && message.getNoteNumber() == 60
                             && metadata.samplePosition == 0;
    }
    ok &= check (sawDisableNoteOff,
                 "disabling arpeggiator releases its active generated note");

    processor.parameters.getParameter ("arpEnabled")->setValueNotifyingHost (1.0f);
    juce::MidiBuffer arpInput;
    arpInput.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
    arpInput.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 100), 0);
    buffer.clear();
    processor.processBlock (buffer, arpInput);
    float arpPeak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            arpPeak = juce::jmax (arpPeak, std::abs (buffer.getSample (ch, i)));
    ok &= check (arpPeak > 0.0f, "arpeggiator emits an audible held note");
    processor.parameters.getParameter ("arpEnabled")->setValueNotifyingHost (0.0f);

    // Publishing while rendering must stay finite. The implementation must not
    // reclaim an old wavetable from the realtime thread.
    for (int generation = 0; generation < 16; ++generation)
    {
        processor.wavetable.frames[0][0] = -1.0f + 2.0f * (float) generation / 15.0f;
        processor.publishWavetable();
        processor.processBlock (buffer, midi);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                ok &= check (std::isfinite (buffer.getSample (ch, i)),
                             "rapid wavetable publishing remains finite");
    }

    // Exercise the platform decoder path for common interchange formats.
    const auto tempRoot = juce::File::getSpecialLocation (juce::File::tempDirectory);
    const auto wavFile = tempRoot.getNonexistentChildFile ("hybrid-wavetable-smoke", ".wav");
    const auto aiffFile = tempRoot.getNonexistentChildFile ("hybrid-wavetable-smoke", ".aiff");
    juce::WavAudioFormat wavFormat;
    juce::AiffAudioFormat aiffFormat;
    ok &= check (writeTestAudio (wavFile, wavFormat), "test WAV is writable");
    processor.loadAudioFile (wavFile);
    ok &= check (std::abs (processor.wavetable.frames[0][512] - 1.0f) < 0.02f,
                 "WAV import reaches the wavetable");
    ok &= check (writeTestAudio (aiffFile, aiffFormat), "test AIFF is writable");
    processor.loadAudioFile (aiffFile);
    ok &= check (std::abs (processor.wavetable.frames[0][512] - 1.0f) < 0.02f,
                 "AIFF import reaches the wavetable");
    wavFile.deleteFile();
    aiffFile.deleteFile();

    processor.beginMidiLearn (3); // Cutoff (Osc 3 position is target 2)
    juce::MidiBuffer learn;
    learn.addEvent (juce::MidiMessage::controllerEvent (1, 74, 127), 0);
    processor.processBlock (buffer, learn);
    juce::MidiBuffer mapped;
    mapped.addEvent (juce::MidiMessage::controllerEvent (1, 74, 64), 0);
    processor.processBlock (buffer, mapped);
    ok &= check (processor.parameters.getRawParameterValue ("cutoff")->load() > 9000.0f,
                 "MIDI learn maps a CC to cutoff");

    processor.wavetable.frames[0][0] = 0.73f;
    processor.publishWavetable();
    processor.parameters.getParameter ("osc3Pos")->setValueNotifyingHost (0.42f);
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    std::unique_ptr<juce::XmlElement> savedXml (HybridWavetableAudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()));
    ok &= check (savedXml != nullptr && savedXml->getIntAttribute ("stateSchemaVersion", 0) == 2,
                 "state carries schema version 2");
    processor.wavetable.frames[0][0] = -0.42f;
    processor.publishWavetable();
    processor.parameters.getParameter ("cutoff")->setValueNotifyingHost (0.9f);
    processor.setStateInformation (state.getData(), (int) state.getSize());
    const auto restored = processor.parameters.getRawParameterValue ("cutoff")->load();
    ok &= check (restored > 9000.0f, "state restore returns saved cutoff");
    ok &= check (std::abs (processor.wavetable.frames[0][0] - 0.73f) < 0.001f,
                 "state restore returns edited wavetable data");
    ok &= check (std::abs (processor.parameters.getRawParameterValue ("osc3Pos")->load() - 0.42f) < 0.001f,
                 "state restore returns third oscillator position");
    ok &= check (std::abs (processor.parameters.getRawParameterValue ("osc1Detune")->load() - 12.0f) < 0.001f,
                 "state restore supplies default detune for legacy-compatible state");
    if (savedXml != nullptr)
    {
        savedXml->removeAttribute ("stateSchemaVersion");
        removeParameterFromXml (*savedXml, "osc1Detune");
        removeParameterFromXml (*savedXml, "osc2Detune");
        removeParameterFromXml (*savedXml, "osc3Detune");
        removeParameterFromXml (*savedXml, "unisonKeyTrack");
        juce::MemoryBlock legacyState;
        HybridWavetableAudioProcessor::copyXmlToBinary (*savedXml, legacyState);
        processor.setStateInformation (legacyState.getData(), (int) legacyState.getSize());
        ok &= check (std::abs (processor.parameters.getRawParameterValue ("osc1Detune")->load() - 12.0f) < 0.001f,
                     "version-1 state migration fills osc1 detune");
        ok &= check (std::abs (processor.parameters.getRawParameterValue ("unisonKeyTrack")->load()) < 0.001f,
                     "version-1 state migration fills key-track");
    }

    // Load the checked-in v1 fixture itself, upgrade it to schema 2, and
    // verify that a same-seed render survives the state round trip exactly.
    const auto stateFixture = locateStateFixture();
    juce::MemoryBlock fixtureState;
    const auto fixtureText = stateFixture.existsAsFile() ? stateFixture.loadFileAsString().trim() : juce::String();
    juce::MemoryOutputStream fixtureDecoded;
    const auto fixtureBytesDecoded = stateFixture.existsAsFile()
                                   && juce::Base64::convertFromBase64 (fixtureDecoded, fixtureText);
    const auto fixtureXmlText = fixtureBytesDecoded
                              ? juce::String::fromUTF8 (static_cast<const char*> (fixtureDecoded.getData()),
                                                        (int) fixtureDecoded.getDataSize())
                              : juce::String();
    auto fixtureXml = fixtureBytesDecoded ? juce::XmlDocument::parse (fixtureXmlText) : nullptr;
    const auto fixtureLoaded = fixtureXml != nullptr;
    if (fixtureLoaded)
        HybridWavetableAudioProcessor::copyXmlToBinary (*fixtureXml, fixtureState);
    ok &= check (fixtureLoaded, "version-1 state fixture is readable");
    if (fixtureLoaded)
    {
        HybridWavetableAudioProcessor legacyProcessor (0x51a7e001u);
        legacyProcessor.prepareToPlay (48000.0, 256);
        legacyProcessor.setStateInformation (fixtureState.getData(), (int) fixtureState.getSize());
        ok &= check (std::abs (legacyProcessor.parameters.getRawParameterValue ("osc1Pos")->load() - 0.5f) < 0.001f,
                     "fixture preserves legacy oscillator position");
        ok &= check (std::abs (legacyProcessor.parameters.getRawParameterValue ("osc2Pos")->load() - 0.22f) < 0.001f,
                     "fixture preserves second legacy oscillator position");
        ok &= check (std::abs (legacyProcessor.parameters.getRawParameterValue ("osc3Pos")->load() - 0.73f) < 0.001f,
                     "fixture preserves third legacy oscillator position");

        juce::MemoryBlock upgradedState;
        legacyProcessor.getStateInformation (upgradedState);
        std::unique_ptr<juce::XmlElement> upgradedXml (HybridWavetableAudioProcessor::getXmlFromBinary (upgradedState.getData(),
                                                                                                           (int) upgradedState.getSize()));
        ok &= check (upgradedXml != nullptr && upgradedXml->getIntAttribute ("stateSchemaVersion", 0) == 2,
                     "legacy fixture saves as schema version 2");

        HybridWavetableAudioProcessor restoredProcessor (0x51a7e001u);
        restoredProcessor.prepareToPlay (48000.0, 256);
        restoredProcessor.setStateInformation (upgradedState.getData(), (int) upgradedState.getSize());
        juce::AudioBuffer<float> legacyRender (2, 256), restoredRender (2, 256);
        juce::MidiBuffer legacyMidi, restoredMidi;
        legacyMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        restoredMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        legacyProcessor.processBlock (legacyRender, legacyMidi);
        restoredProcessor.processBlock (restoredRender, restoredMidi);
        ok &= check (maximumAudioDifference (legacyRender, restoredRender) == 0.0f,
                     "version-1 to version-2 render is sample-identical with the same seed");
    }
    juce::MidiBuffer restoredMapping;
    restoredMapping.addEvent (juce::MidiMessage::controllerEvent (1, 74, 32), 0);
    processor.processBlock (buffer, restoredMapping);
    ok &= check (processor.parameters.getRawParameterValue ("cutoff")->load() < 7000.0f,
                 "state restore returns MIDI CC mapping");

    // Exercise the full 128-voice pool in one block. This catches accidental
    // per-voice allocation, denormals, and buffer overruns under a dense chord.
    processor.prepareToPlay (48000.0, 1024);
    juce::AudioBuffer<float> denseBuffer (2, 1024);
    juce::MidiBuffer denseChord;
    for (int note = 0; note < 128; ++note)
        denseChord.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 96), 0);
    processor.processBlock (denseBuffer, denseChord);
    float densePeak = 0.0f;
    for (int ch = 0; ch < denseBuffer.getNumChannels(); ++ch)
        for (int i = 0; i < denseBuffer.getNumSamples(); ++i)
        {
            const auto sample = denseBuffer.getSample (ch, i);
            densePeak = juce::jmax (densePeak, std::abs (sample));
            ok &= check (std::isfinite (sample), "128-voice output remains finite");
        }
    ok &= check (densePeak > 0.0f, "128-voice chord produces audio");

    juce::MidiBuffer release;
    for (int note = 0; note < 128; ++note)
        release.addEvent (juce::MidiMessage::noteOff (1, note), 0);
    processor.processBlock (denseBuffer, release);
    for (int i = 0; i < denseBuffer.getNumSamples(); ++i)
        ok &= check (std::isfinite (denseBuffer.getSample (0, i)), "voice release output remains finite");

    processor.releaseResources();
    std::cout << (ok ? "Processor smoke tests passed\n" : "Processor smoke tests failed\n");
    return ok ? 0 : 1;
}
