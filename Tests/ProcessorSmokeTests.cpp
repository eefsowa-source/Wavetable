#include "../Source/PluginProcessor.h"
#include <iostream>

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
