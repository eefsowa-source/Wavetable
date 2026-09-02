#pragma once
#include <JuceHeader.h>
#include "WavetableOscillator.h"
#include <cstdint>

class SynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice (juce::AudioProcessorValueTreeState&, std::uint32_t deterministicSeed = 1u);
    bool canPlaySound (juce::SynthesiserSound* sound) override { return dynamic_cast<SynthSound*> (sound) != nullptr; }
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void prepare (double sampleRate, int blockSize, const std::atomic<const WavetableData*>* wavetable);
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    int getSaturationOversamplingLatencySamples() const noexcept
    {
        return (int) saturationOversampling.getLatencyInSamples();
    }
private:
    juce::AudioProcessorValueTreeState& params;
    // Stage 2: Unison engine - 3개 OSC를 voiceCount 기반으로 확장
    WavetableOscillator osc1, osc2, osc3;  // 기본 3개 (호환성)
    // std::vector<WavetableOscillator> unisonOscs는 header의 Stage 2 섹션에서 정의
    // Parameter smoothing: eliminates zipper noise (50ms ramp)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedCutoff;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedResonance;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOsc1Level;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOsc2Level;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOsc3Level;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedSaturation;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFilterDrive;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedWavetable1;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedWavetable2;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedWavetable3;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFilterEnvAmount;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::StateVariableTPTFilter<float> filter2;
    juce::ADSR ampEnv, filterEnv;
    juce::ADSR::Parameters ampParams, filterParams;
    double sampleRate = 44100.0;
    float level = 0.0f, noteHz = 440.0f;
    int midiNote = 69;
    bool releasing = false;
    const std::atomic<const WavetableData*>* tableSource = nullptr;
    float lfo1Phase = 0.0f, lfo2Phase = 0.0f;
    // 2x oversampling wrapped tightly around the tanh saturation stage only,
    // so the nonlinearity's generated harmonics are folded back down cleanly
    // instead of aliasing against the block sample rate. Kept per-voice
    // (rather than moved to the master bus) so saturation still happens
    // pre-sum, preserving the existing per-voice tone character.
    juce::dsp::Oversampling<float> saturationOversampling { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, true };
    juce::AudioBuffer<float> preSaturationBuffer;
    std::vector<float> outputGainScratch;
    // Stage 2: Unison & Drift
    std::vector<WavetableOscillator> unisonOscs;
    float driftPhase = 0.0f;
    std::uint32_t randomState = 1u;

    float nextRandom01() noexcept;
};
