#include "SynthVoice.h"
#include <Dsp/RtGuard.h>

#include <cstring>

namespace SeoulDSPQuality
{
float filterEnvelopeCutoff (float baseCutoffHz, float envelopeAmount,
                            float envelopeSample, float sampleRate,
                            float lfoCutoffOctaves) noexcept
{
    const auto modulationOctaves = envelopeAmount * 4.0f * juce::jlimit (0.0f, 1.0f, envelopeSample)
                                 + juce::jlimit (-4.0f, 4.0f, lfoCutoffOctaves);
    return juce::jlimit (20.0f, 0.45f * sampleRate,
                         baseCutoffHz * std::exp2 (modulationOctaves));
}

float filterResonanceQ (float parameter) noexcept
{
    constexpr float minimumQ = 0.5f;
    constexpr float maximumQ = 20.0f;
    const auto travel = juce::jlimit (0.0f, 1.0f,
                                      (parameter - 0.1f) / 0.9f);
    // Exponential rather than linear: Q is a ratio, so equal steps of Q sound
    // like equal steps of pitch in the ring, and a linear knob would spend most
    // of its travel in the inaudible bottom octave.
    return minimumQ * std::pow (maximumQ / minimumQ, travel);
}
}

SynthVoice::SynthVoice (juce::AudioProcessorValueTreeState& p, std::uint32_t deterministicSeed)
    : params (p), random (deterministicSeed)
{
}

float SynthVoice::nextRandom01() noexcept
{
    return random.nextUnitFloat();
}

void SynthVoice::prepare (double sr, int blockSize, const std::atomic<const WavetableData*>* wt)
{
    sampleRate = sr;
    tableSource = wt;
    osc1Bank.prepare (sr); osc2Bank.prepare (sr); osc3Bank.prepare (sr);
    voiceFilterLeft.reset();
    voiceFilterRight.reset();
    previousFilterLeft.reset();
    previousFilterRight.reset();
    filterFadeLength = juce::jmax (1, (int) (0.005 * sr));
    filterFadeSamplesRemaining = 0;
    activeSlope = (int) (params.getRawParameterValue ("filterSlope") != nullptr
                             ? params.getRawParameterValue ("filterSlope")->load() : 1.0f);
    activeType = (int) (params.getRawParameterValue ("filterType") != nullptr
                            ? params.getRawParameterValue ("filterType")->load() : 0.0f);
    fadeSlope = activeSlope;
    fadeType = activeType;
    ampEnv.setSampleRate (sr); filterEnv.setSampleRate (sr);
    saturationStage.prepare (blockSize);
    preSaturationBuffer.setSize (2, juce::jmax (1, blockSize), false, true, true);
    outputGainScratch.assign ((size_t) juce::jmax (1, blockSize), 0.0f);
    // Initialize smoothed parameters with 50ms ramp time.  Starting both the
    // current and target values from the host state avoids a startup ramp from
    // the default zero value.
    const float rampTimeMs = 50.0f;
    const auto initialiseSmoother = [this, sr, rampTimeMs] (auto& smoother, const char* id, float fallback)
    {
        smoother.reset (sr, rampTimeMs / 1000.0f);
        const auto* parameter = params.getRawParameterValue (id);
        auto value = parameter != nullptr ? parameter->load() : fallback;
        // Resonance is smoothed in Q rather than in raw parameter units, which
        // moves the exponential mapping off the per-sample path and out of the
        // audio callback's cost. It also makes a 50 ms glide a linear ramp in Q,
        // which is what a resonant sweep sweeping through a note wants.
        if (std::strcmp (id, "resonance") == 0)
            value = SeoulDSPQuality::filterResonanceQ (value);
        smoother.setCurrentAndTargetValue (value);
    };
    initialiseSmoother (smoothedCutoff, "cutoff", 12000.0f);
    initialiseSmoother (smoothedResonance, "resonance", 0.25f);
    initialiseSmoother (smoothedOsc1Level, "osc1Level", 0.75f);
    initialiseSmoother (smoothedOsc2Level, "osc2Level", 0.75f);
    initialiseSmoother (smoothedOsc3Level, "osc3Level", 0.75f);
    initialiseSmoother (smoothedSaturation, "saturation", 0.15f);
    initialiseSmoother (smoothedFilterDrive, "filterDrive", 0.0f);
    initialiseSmoother (smoothedWavetable1, "osc1Pos", 0.0f);
    initialiseSmoother (smoothedWavetable2, "osc2Pos", 0.35f);
    initialiseSmoother (smoothedWavetable3, "osc3Pos", 0.67f);
    initialiseSmoother (smoothedFilterEnvAmount, "filterEnvAmount", 0.5f);
}

void SynthVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    midiNote = midiNoteNumber;
    noteHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    level = velocity; releasing = false;
    auto value = [this] (const char* id, float fallback) { if (auto* p = params.getRawParameterValue (id)) return p->load(); return fallback; };
    // 작업 4: Start phase randomization
    float randomPhaseAmount = value ("randomPhase", 0.0f);
    if (randomPhaseAmount > 0.0f) {
        const auto maxPhase = juce::jlimit (0.0f, 1.0f, randomPhaseAmount);
        osc1Bank.setRandomPhases (random, maxPhase);
        osc2Bank.setRandomPhases (random, maxPhase);
        osc3Bank.setRandomPhases (random, maxPhase);
    } else {
        osc1Bank.resetPhases(); osc2Bank.resetPhases(); osc3Bank.resetPhases();
    }
    ampParams.attack = value ("ampAttack", 0.01f); ampParams.decay = value ("ampDecay", 0.25f); ampParams.sustain = value ("ampSustain", 0.8f); ampParams.release = value ("ampRelease", 0.35f);
    filterParams.attack = value ("filterAttack", 0.01f); filterParams.decay = value ("filterDecay", 0.25f); filterParams.sustain = value ("filterSustain", 0.8f); filterParams.release = value ("filterRelease", 0.35f);
    ampEnv.setParameters (ampParams); filterEnv.setParameters (filterParams);
    ampEnv.noteOn(); filterEnv.noteOn();
    // A stale fade from the previous note must not bleed into this one.
    filterFadeSamplesRemaining = 0;
    activeSlope = (int) value ("filterSlope", 1.0f);
    activeType = (int) value ("filterType", 0.0f);
    fadeSlope = activeSlope;
    fadeType = activeType;
    
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff) { ampEnv.noteOff(); filterEnv.noteOff(); releasing = true; }
    else { ampEnv.reset(); filterEnv.reset(); clearCurrentNote(); }
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& output, int start, int count)
{
    eon::ScopedDenormalsOff noDenormals;
    if (! isVoiceActive()) return;
    auto* out = output.getWritePointer (0, start);
    auto* outRight = output.getNumChannels() > 1 ? output.getWritePointer (1, start) : nullptr;
    const auto* wt1 = params.getRawParameterValue ("osc1Pos");
    const auto* wt2 = params.getRawParameterValue ("osc2Pos");
    const auto* wt3 = params.getRawParameterValue ("osc3Pos");
    const auto* cutoff = params.getRawParameterValue ("cutoff");
    const auto* resonance = params.getRawParameterValue ("resonance");
    const auto* drive = params.getRawParameterValue ("filterDrive");
    const auto* sat = params.getRawParameterValue ("saturation");
    const auto* envAmount = params.getRawParameterValue ("filterEnvAmount");
    const auto* lfo1Rate = params.getRawParameterValue ("lfo1Rate");
    const auto* lfo1Depth = params.getRawParameterValue ("lfo1Depth");
    const auto* lfo1Destination = params.getRawParameterValue ("lfo1Destination");
    const auto* lfo2Rate = params.getRawParameterValue ("lfo2Rate");
    const auto* lfo2Depth = params.getRawParameterValue ("lfo2Depth");
    const auto* lfo2Destination = params.getRawParameterValue ("lfo2Destination");
    const auto* osc1Spread = params.getRawParameterValue ("osc1Spread");
    const auto* osc2Spread = params.getRawParameterValue ("osc2Spread");
    const auto* osc3Spread = params.getRawParameterValue ("osc3Spread");
    const auto type = (int) params.getRawParameterValue ("filterType")->load();
    const auto slope = (int) params.getRawParameterValue ("filterSlope")->load();

    // A slope or type change swaps the filter structure. Snapshot the running
    // filter and blend from it for a few milliseconds so the swap is not a step.
    if (slope != activeSlope || type != activeType)
    {
        previousFilterLeft = voiceFilterLeft;
        previousFilterRight = voiceFilterRight;
        fadeSlope = activeSlope;
        fadeType = activeType;
        activeSlope = slope;
        activeType = type;
        filterFadeSamplesRemaining = filterFadeLength;
    }
    
    // Update smoothed parameter targets
    smoothedCutoff.setTargetValue (cutoff->load());
    smoothedResonance.setTargetValue (SeoulDSPQuality::filterResonanceQ (resonance->load()));
    smoothedOsc1Level.setTargetValue (params.getRawParameterValue ("osc1Level")->load());
    smoothedOsc2Level.setTargetValue (params.getRawParameterValue ("osc2Level")->load());
    smoothedOsc3Level.setTargetValue (params.getRawParameterValue ("osc3Level")->load());
    smoothedSaturation.setTargetValue (juce::jlimit (0.0f, 1.0f, sat->load()));
    smoothedFilterDrive.setTargetValue (drive->load());
    smoothedWavetable1.setTargetValue (wt1->load());
    smoothedWavetable2.setTargetValue (wt2->load());
    smoothedWavetable3.setTargetValue (wt3->load());
    smoothedFilterEnvAmount.setTargetValue (envAmount != nullptr ? envAmount->load() : 0.5f);
    
    const auto ratioForSemitones = [] (float semitones) { return std::exp2 (semitones / 12.0f); };
    const float osc1Ratio = ratioForSemitones (params.getRawParameterValue ("osc1Tune")->load());
    const float osc2Ratio = ratioForSemitones (params.getRawParameterValue ("osc2Tune")->load());
    const float osc3Ratio = ratioForSemitones (params.getRawParameterValue ("osc3Tune")->load());
    const auto countFor = [this] (const char* id)
    {
        const auto* parameter = params.getRawParameterValue (id);
        return juce::jlimit (1, 8, parameter != nullptr ? juce::roundToInt (parameter->load()) : 1);
    };
    const int osc1Count = countFor ("osc1Unison");
    const int osc2Count = countFor ("osc2Unison");
    const int osc3Count = countFor ("osc3Unison");
    const float osc1Detune = params.getRawParameterValue ("osc1Detune") != nullptr ? params.getRawParameterValue ("osc1Detune")->load() : 12.0f;
    const float osc2Detune = params.getRawParameterValue ("osc2Detune") != nullptr ? params.getRawParameterValue ("osc2Detune")->load() : 12.0f;
    const float osc3Detune = params.getRawParameterValue ("osc3Detune") != nullptr ? params.getRawParameterValue ("osc3Detune")->load() : 12.0f;
    const float keyTrack = params.getRawParameterValue ("unisonKeyTrack") != nullptr ? params.getRawParameterValue ("unisonKeyTrack")->load() : 0.0f;
    const float keyTrackScale = juce::jlimit (0.5f, 2.0f, std::exp2 (keyTrack * (float) (midiNote - 60) / 48.0f));
    
    // Note: filter cutoff & resonance are updated per-sample from the smoothed values.
    const auto* table = tableSource != nullptr
                          ? tableSource->load (std::memory_order_acquire)
                          : nullptr;
    if (table == nullptr) return;
    const float lfo1DepthValue = lfo1Depth->load();
    const float lfo2DepthValue = lfo2Depth->load();
    // 작업 3: Analog drift LFO (저속 진동수 변조)
    const float driftRateHz = params.getRawParameterValue ("driftRate")->load();
    const float driftDepthValue = params.getRawParameterValue ("driftDepth")->load();
    const int lfo1Target = (int) lfo1Destination->load();
    const int lfo2Target = (int) lfo2Destination->load();
    const bool lfo1Active = lfo1DepthValue > 0.0001f;
    const bool lfo2Active = lfo2DepthValue > 0.0001f;
    const float lfo1Increment = lfo1Rate->load() / (float) sampleRate;
    const float lfo2Increment = lfo2Rate->load() / (float) sampleRate;
    const float driftIncrement = driftRateHz / (float) sampleRate;
    const float osc1SpreadValue = -osc1Spread->load();
    const float osc2SpreadValue = osc2Spread->load();
    const float osc3SpreadValue = ((midiNote & 1) == 0 ? -1.0f : 1.0f) * osc3Spread->load();
    const float osc1DetuneScaled = osc1Detune * keyTrackScale;
    const float osc2DetuneScaled = osc2Detune * keyTrackScale;
    const float osc3DetuneScaled = osc3Detune * keyTrackScale;
    auto* preSatLeft = preSaturationBuffer.getWritePointer (0);
    auto* preSatRight = preSaturationBuffer.getWritePointer (1);
    for (int i = 0; i < count; ++i)
    {
        // 작업 3: Drift LFO advance
        driftPhase += driftIncrement;
        driftPhase -= std::floor (driftPhase);
        const float driftMod = driftDepthValue > 0.0f ? std::sin (juce::MathConstants<float>::twoPi * driftPhase) * driftDepthValue * 0.02f : 0.0f;  // ±2%
        
        // Advance smoothed parameter values
        const float cutoffHz = smoothedCutoff.getNextValue();
        const float resonance = smoothedResonance.getNextValue();
        const float l1 = smoothedOsc1Level.getNextValue();
        const float l2 = smoothedOsc2Level.getNextValue();
        const float l3 = smoothedOsc3Level.getNextValue();
        const float saturation = smoothedSaturation.getNextValue();
        // Filter drive is no longer a linear pre-gain here: SlopeFilter owns it
        // and clips its first section's drive node. See SlopeFilter.h.
        const float filterDriveDb = smoothedFilterDrive.getNextValue();
        const float wavetable1 = smoothedWavetable1.getNextValue();
        const float wavetable2 = smoothedWavetable2.getNextValue();
        const float wavetable3 = smoothedWavetable3.getNextValue();
        const float filterEnvAmount = smoothedFilterEnvAmount.getNextValue();
        
        const float lfo1 = lfo1Active ? std::sin (juce::MathConstants<float>::twoPi * lfo1Phase) : 0.0f;
        const float lfo2 = lfo2Active ? std::sin (juce::MathConstants<float>::twoPi * lfo2Phase) : 0.0f;
        lfo1Phase += lfo1Increment;
        lfo2Phase += lfo2Increment;
        lfo1Phase -= std::floor (lfo1Phase);
        lfo2Phase -= std::floor (lfo2Phase);
        float pitchSemitones = driftMod * 12.0f;
        float lfoCutoffOctaves = 0.0f;
        float wavetableMod = 0.0f;
        const auto applyLfo = [&] (float value, float depth, int destination)
        {
            const float amount = value * depth;
            switch (destination)
            {
                case 0: pitchSemitones += amount * 12.0f; break;
                case 1: lfoCutoffOctaves += amount * 4.0f; break;
                default: wavetableMod += amount * 0.25f; break;
            }
        };
        applyLfo (lfo1, lfo1DepthValue, lfo1Target);
        applyLfo (lfo2, lfo2DepthValue, lfo2Target);
        const float pitchRatio = ratioForSemitones (pitchSemitones);
        const float env = ampEnv.getNextSample();
        const float fenv = filterEnv.getNextSample();
        const float modulatedCutoff = SeoulDSPQuality::filterEnvelopeCutoff (cutoffHz, filterEnvAmount,
                                                                              fenv, (float) sampleRate,
                                                                              lfoCutoffOctaves);
        voiceFilterLeft.setParams (slope, type, modulatedCutoff, resonance, filterDriveDb, sampleRate);
        voiceFilterRight.setParams (slope, type, modulatedCutoff, resonance, filterDriveDb, sampleRate);
        if (filterFadeSamplesRemaining > 0)
        {
            previousFilterLeft.setParams (fadeSlope, fadeType, modulatedCutoff, resonance, filterDriveDb, sampleRate);
            previousFilterRight.setParams (fadeSlope, fadeType, modulatedCutoff, resonance, filterDriveDb, sampleRate);
        }
        float osc1Left = 0.0f, osc1Right = 0.0f;
        float osc2Left = 0.0f, osc2Right = 0.0f;
        float osc3Left = 0.0f, osc3Right = 0.0f;
        const auto positionOffset = wavetableMod;
        osc1Bank.processStereo (*table, osc1Count, noteHz * osc1Ratio * pitchRatio,
                                wavetable1 + positionOffset, osc1DetuneScaled,
                                osc1SpreadValue, osc1Left, osc1Right);
        osc2Bank.processStereo (*table, osc2Count, noteHz * osc2Ratio * pitchRatio,
                                wavetable2 + positionOffset, osc2DetuneScaled,
                                osc2SpreadValue, osc2Left, osc2Right);
        osc3Bank.processStereo (*table, osc3Count, noteHz * osc3Ratio * pitchRatio,
                                wavetable3 + positionOffset, osc3DetuneScaled,
                                osc3SpreadValue, osc3Left, osc3Right);
        float left = (osc1Left * l1 + osc2Left * l2 + osc3Left * l3) / 3.0f;
        float right = (osc1Right * l1 + osc2Right * l2 + osc3Right * l3) / 3.0f;
        const float filterInputLeft = left;
        const float filterInputRight = right;
        float filteredLeft = voiceFilterLeft.process (filterInputLeft);
        float filteredRight = 0.0f;
        if (outRight != nullptr)
            filteredRight = voiceFilterRight.process (filterInputRight);
        if (filterFadeSamplesRemaining > 0)
        {
            const float t = 1.0f - (float) filterFadeSamplesRemaining / (float) filterFadeLength;
            const float previousLeft = previousFilterLeft.process (filterInputLeft);
            filteredLeft = previousLeft + (filteredLeft - previousLeft) * t;
            if (outRight != nullptr)
            {
                const float previousRight = previousFilterRight.process (filterInputRight);
                filteredRight = previousRight + (filteredRight - previousRight) * t;
            }
            --filterFadeSamplesRemaining;
        }
        left = filteredLeft;
        if (outRight != nullptr)
            right = filteredRight;
        const float vcaGain = env * level * 0.25f;
        const float vcaLeft = left * vcaGain;
        const float vcaRight = right * vcaGain;
        outputGainScratch[(size_t) i] = 1.0f;
        preSatLeft[i] = vcaLeft * (1.0f + saturation * 8.0f);
        preSatRight[i] = outRight != nullptr ? vcaRight * (1.0f + saturation * 8.0f) : 0.0f;
    }

    const float saturation = smoothedSaturation.getCurrentValue();
    const float satMakeup = 1.0f - saturation * 0.18f;
    const auto qualityIndex = (int) params.getRawParameterValue ("saturationQuality")->load();
    // Plan C SQ-2: the High (4x) tier was removed. Swept across probe pitches
    // whose third harmonic folds just above Nyquist (see docs/quality/c2), it
    // never beat Normal (2x) by more than 0.33 dB and lost by up to 1.4 dB, while
    // costing about a quarter more CPU on a dense chord. The parameter keeps its
    // third choice so existing sessions and presets still recall, but it renders
    // as Normal.
    const int requestedQuality = juce::jlimit (0, 2, qualityIndex);
    saturationStage.setQuality (static_cast<SaturationStage::Quality> (requestedQuality >= 2 ? 1 : requestedQuality));
    // Saturation only matters once drive is actually applied, but a block inside
    // a ramp keeps the stage running so smoothed attacks receive the same
    // antialiasing as steady state does.
    const bool saturationBypassed = saturation <= 0.005f && ! smoothedSaturation.isSmoothing();
    if (saturationBypassed)
        saturationStage.processBypass (preSatLeft, outRight != nullptr ? preSatRight : nullptr, count);
    else
        saturationStage.process (preSatLeft, outRight != nullptr ? preSatRight : nullptr, count);
    for (int i = 0; i < count; ++i)
    {
        out[i] += preSatLeft[i] * satMakeup * outputGainScratch[(size_t) i];
        if (outRight != nullptr)
            outRight[i] += preSatRight[i] * satMakeup * outputGainScratch[(size_t) i];
    }
    if (releasing && ampEnv.isActive() == false) clearCurrentNote();
}
