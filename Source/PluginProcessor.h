#pragma once
#include <JuceHeader.h>
#include "DSP/SynthVoice.h"

class HybridWavetableAudioProcessor : public juce::AudioProcessor
{
public:
    HybridWavetableAudioProcessor();
    ~HybridWavetableAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SEOUL DSP"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // Return delay + reverb tail time for proper host bounce
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    WavetableData wavetable;
    void publishWavetable();
    void loadAudioFile (const juce::File&);
    static const juce::StringArray& getMidiLearnTargets();
    static const juce::StringArray& getFactoryPresetNames();
    void applyFactoryPreset (int index);
    void beginMidiLearn (int targetIndex) noexcept;

private:
    static constexpr int wavetableBufferCount = 3;
    int acquireWavetableForAudio() noexcept;
    void releaseWavetableForAudio (int slot) noexcept;
    void processArpeggiator (juce::MidiBuffer& midi, int numSamples) noexcept;
    void processEffects (juce::AudioBuffer<float>& buffer) noexcept;

    juce::Synthesiser synth;
    std::array<WavetableData, wavetableBufferCount> wavetableBuffers;
    std::array<std::atomic<int>, wavetableBufferCount> wavetableReaders {};
    std::atomic<int> activeWavetableSlot { 0 };
    std::atomic<const WavetableData*> audioWavetable { nullptr };

    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition = 0;
    juce::dsp::Reverb reverb;

    std::array<float, 128> heldNoteVelocity {};
    int arpActiveNote = -1, arpStep = 0, arpSamplesUntilStep = 0, arpSamplesUntilGateOff = -1;
    std::uint32_t arpRandomState = 0x53454f55u;
    juce::MidiBuffer arpeggiatedMidi;
    std::array<std::atomic<int>, 128> midiCCAssignments;
    std::atomic<int> midiLearnTarget { -1 };
    double currentSampleRate = 44100.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HybridWavetableAudioProcessor)
};

