#pragma once
#include <JuceHeader.h>
#include "WavetableOscillator.h"
#include "RealtimeRandom.h"
#include "SaturationStage.h"
#include "SlopeFilter.h"
#include "UnisonBank.h"
#include <cstdint>

namespace SeoulDSPQuality
{
float filterEnvelopeCutoff (float baseCutoffHz, float envelopeAmount,
                            float envelopeSample, float sampleRate,
                            float lfoCutoffOctaves = 0.0f) noexcept;

// Maps the host's resonance parameter onto the section's Q.
//
// Why the mapping exists: the parameter's 0.1..1.0 range was handed straight to
// the TPT section as Q, so the most resonant setting the plug-in could reach was
// Q = 1 -- a 0 dB corner, no peak, and no ringing. Measured through the renderer
// at a 1 kHz corner (Tests/AudioQuality/FilterResonanceTests.cpp), the whole
// knob travelled Q 0.10 to 1.00, so the control was a damping trim rather than a
// resonance control. Serum, Vital, Massive and Surge XT all put a pronounced peak
// at the top of their resonance controls, so that range is the sound quality gap
// this upgrade closes.
//
// Why a curve instead of a wider parameter range: the parameter keeps its id,
// its 0.1..1.0 range and its stored numbers, so sessions, presets and MIDI CC
// assignments all keep working unchanged. Only the interpretation of the value
// moves. Every preset does get more resonance, which is the intended change and
// is recorded in docs/quality/c5-filter-resonance-range.md rather than hidden.
//
// The endpoints are chosen so both ends of the knob mean something: 0.5 is
// slightly past flat, where the filter is simply closed, and 20 is a resonant
// peak that sustains a pitched tail for tens of cycles without approaching the
// self-oscillation the reference instruments can reach.
float filterResonanceQ (float parameter) noexcept;
}

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
    int getSaturationLatencySamples() const noexcept { return saturationStage.getLatencySamples(); }
private:
    juce::AudioProcessorValueTreeState& params;
    OscillatorUnisonBank osc1Bank, osc2Bank, osc3Bank;
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
    // Four real slopes over the eon TPT primitives. Replaced the JUCE SVF pair,
    // whose two-stage cascade was the only way to change the slope and whose
    // resonance parameter was a Q of at most 1.0 (see SlopeFilter.h).
    SlopeFilter voiceFilterLeft, voiceFilterRight;
    // Slope and type are structural filter changes, not smoothed coefficients,
    // so a step would leave a discontinuity. On a change the previous
    // configuration keeps running for a short fade and the two outputs blend.
    SlopeFilter previousFilterLeft, previousFilterRight;
    int activeSlope = 1;
    int activeType = 0;
    int fadeSlope = 1;
    int fadeType = 0;
    int filterFadeSamplesRemaining = 0;
    int filterFadeLength = 240;
    juce::ADSR ampEnv, filterEnv;
    juce::ADSR::Parameters ampParams, filterParams;
    double sampleRate = 44100.0;
    float level = 0.0f, noteHz = 440.0f;
    int midiNote = 69;
    bool releasing = false;
    const std::atomic<const WavetableData*>* tableSource = nullptr;
    float lfo1Phase = 0.0f, lfo2Phase = 0.0f;
    // Antiderivative anti-aliasing plus half-band FIR oversampling, wrapped
    // tightly around the soft clip only. Kept per-voice (rather than moved to
    // the master bus) so saturation still happens pre-sum, preserving the
    // existing per-voice tone character.
    SaturationStage saturationStage;
    juce::AudioBuffer<float> preSaturationBuffer;
    std::vector<float> outputGainScratch;
    float driftPhase = 0.0f;
    RealtimeRandom random { 1u };

    float nextRandom01() noexcept;
};
