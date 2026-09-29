#pragma once
#include <JuceHeader.h>
#include <Dsp/Rng.h>
#include "DSP/SynthVoice.h"
#include "DSP/OutputSafety.h"
#include "EditorTypes.h"
#include <cstdint>

struct DelayMixGains
{
    float dry = 1.0f;
    float wet = 0.0f;
};

class HybridWavetableAudioProcessor : public juce::AudioProcessor
{
public:
    explicit HybridWavetableAudioProcessor (std::uint32_t deterministicSeed = 0);
    ~HybridWavetableAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
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

    // Output meter and clip state. Safe to poll from the editor: the audio
    // thread only stores atomics.
    float outputPeakLevel() const noexcept { return outputSafety.peakLevel(); }
    bool outputClipActive() const noexcept { return outputSafety.clipActive(); }
    void clearOutputClip() noexcept { outputSafety.clearClipFlag(); }
    static const juce::StringArray& getMidiLearnTargets();
    static const juce::StringArray& getFactoryPresetNames();
    void applyFactoryPreset (int index);
    void beginMidiLearn (int targetIndex) noexcept;
    int getUiType() const noexcept { return uiType.load (std::memory_order_acquire); }
    void setUiType (int type) noexcept;
    static DelayMixGains calculateDelayMixGains (float mix) noexcept;

private:
    static constexpr int wavetableBufferCount = 3;
    int acquireWavetableForAudio() noexcept;
    void releaseWavetableForAudio (int slot) noexcept;
    void processArpeggiator (juce::MidiBuffer& midi, int numSamples) noexcept;
    void processEffects (juce::AudioBuffer<float>& buffer) noexcept;

    juce::Synthesiser synth;
    // Mip-mapped tables are large; keep the publication pool off the stack so
    // console tests and hosts with small thread stacks remain safe.
    std::unique_ptr<std::array<WavetableData, wavetableBufferCount>> wavetableBuffers;
    std::array<std::atomic<int>, wavetableBufferCount> wavetableReaders {};
    std::atomic<int> activeWavetableSlot { 0 };
    std::atomic<const WavetableData*> audioWavetable { nullptr };

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine { 96000 };
    juce::dsp::LinkwitzRileyFilter<float> monoBassLowpass, monoBassHighpass;
    // Reused by the master-width crossover; never allocate from processBlock().
    juce::AudioBuffer<float> lowBandScratch;
    juce::dsp::Reverb reverb;
    OutputSafety outputSafety;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDelayTime;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDelayFeedback;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDelayMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedReverbMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMasterWidth;
    bool effectSmoothersNeedInitialisation = true;

    std::array<float, 128> heldNoteVelocity {};
    int arpActiveNote = -1, arpStep = 0, arpSamplesUntilStep = 0, arpSamplesUntilGateOff = -1;
    eon::Rng arpRandom { 0x53454f55u };
    juce::MidiBuffer arpeggiatedMidi;
    std::array<std::atomic<int>, 128> midiCCAssignments;
    std::atomic<int> midiLearnTarget { -1 };
    // Presentation-only editor selection; never read from the audio thread.
    std::atomic<int> uiType { 0 };
    double currentSampleRate = 44100.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HybridWavetableAudioProcessor)
};
