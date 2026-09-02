#include "SynthVoice.h"

namespace SeoulDSPQuality
{
float filterEnvelopeCutoff (float baseCutoffHz, float envelopeAmount,
                            float envelopeSample, float sampleRate,
                            float lfoCutoffOctaves) noexcept
{
    const auto multiplier = juce::jlimit (0.05f, 4.0f,
                                          1.0f + envelopeAmount * (envelopeSample * 2.0f - 1.0f)
                                          + lfoCutoffOctaves);
    return juce::jlimit (20.0f, 0.45f * sampleRate, baseCutoffHz * multiplier);
}
}

SynthVoice::SynthVoice (juce::AudioProcessorValueTreeState& p, std::uint32_t deterministicSeed)
    : params (p), random (deterministicSeed)
{
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
}

float SynthVoice::nextRandom01() noexcept
{
    return random.nextUnitFloat();
}

void SynthVoice::prepare (double sr, int blockSize, const std::atomic<const WavetableData*>* wt)
{
    sampleRate = sr;
    tableSource = wt;
    osc1.prepare (sr); osc2.prepare (sr); osc3.prepare (sr);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockSize, 2 };
    filter.prepare (spec);
    filter2.prepare (spec);
    filter.reset(); filter2.reset();
    ampEnv.setSampleRate (sr); filterEnv.setSampleRate (sr);
    saturationOversampling.initProcessing ((size_t) juce::jmax (1, blockSize));
    saturationOversampling.reset();
    preSaturationBuffer.setSize (2, juce::jmax (1, blockSize), false, true, true);
    outputGainScratch.assign ((size_t) juce::jmax (1, blockSize), 0.0f);
    // 작업 1: Initialize unison oscillators
    unisonOscs.resize (16);  // Max 16 voices
    for (auto& osc : unisonOscs) osc.prepare (sr);
    
    // Initialize smoothed parameters with 50ms ramp time
    const float rampTimeMs = 50.0f;
    smoothedCutoff.reset (sr, rampTimeMs / 1000.0f);
    smoothedResonance.reset (sr, rampTimeMs / 1000.0f);
    smoothedOsc1Level.reset (sr, rampTimeMs / 1000.0f);
    smoothedOsc2Level.reset (sr, rampTimeMs / 1000.0f);
    smoothedOsc3Level.reset (sr, rampTimeMs / 1000.0f);
    smoothedSaturation.reset (sr, rampTimeMs / 1000.0f);
    smoothedFilterDrive.reset (sr, rampTimeMs / 1000.0f);
    smoothedWavetable1.reset (sr, rampTimeMs / 1000.0f);
    smoothedWavetable2.reset (sr, rampTimeMs / 1000.0f);
    smoothedWavetable3.reset (sr, rampTimeMs / 1000.0f);
    smoothedFilterEnvAmount.reset (sr, rampTimeMs / 1000.0f);
}

void SynthVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    midiNote = midiNoteNumber;
    noteHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    osc1.setFrequency (noteHz); osc2.setFrequency (noteHz * 1.002f); osc3.setFrequency (noteHz * 0.997f);
    level = velocity; releasing = false;
    auto value = [this] (const char* id, float fallback) { if (auto* p = params.getRawParameterValue (id)) return p->load(); return fallback; };
    // 작업 4: Start phase randomization
    float randomPhaseAmount = value ("randomPhase", 0.0f);
    if (randomPhaseAmount > 0.0f) {
        const auto maxPhase = juce::jlimit (0.0f, 1.0f, randomPhaseAmount);
        osc1.setPhase (nextRandom01() * maxPhase);
        osc2.setPhase (nextRandom01() * maxPhase);
        osc3.setPhase (nextRandom01() * maxPhase);
    } else {
        osc1.reset(); osc2.reset(); osc3.reset();
    }
    ampParams.attack = value ("ampAttack", 0.01f); ampParams.decay = value ("ampDecay", 0.25f); ampParams.sustain = value ("ampSustain", 0.8f); ampParams.release = value ("ampRelease", 0.35f);
    filterParams.attack = value ("filterAttack", 0.01f); filterParams.decay = value ("filterDecay", 0.25f); filterParams.sustain = value ("filterSustain", 0.8f); filterParams.release = value ("filterRelease", 0.35f);
    ampEnv.setParameters (ampParams); filterEnv.setParameters (filterParams);
    ampEnv.noteOn(); filterEnv.noteOn();
    
    // 작업 1: Initialize unison oscillators
    int voiceCount = (int)value ("osc1Unison", 1.0f);
    voiceCount = juce::jlimit (1, 16, voiceCount);
    for (int i = 0; i < voiceCount; ++i) {
        unisonOscs[i].setFrequency (noteHz);
        if (randomPhaseAmount > 0.0f) {
            const auto maxPhase = juce::jlimit (0.0f, 1.0f, randomPhaseAmount);
            unisonOscs[i].setPhase (nextRandom01() * maxPhase);
        } else {
            unisonOscs[i].reset();
        }
    }
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff) { ampEnv.noteOff(); filterEnv.noteOff(); releasing = true; }
    else { ampEnv.reset(); filterEnv.reset(); clearCurrentNote(); }
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& output, int start, int count)
{
    if (! isVoiceActive()) return;
    auto value = [this] (const char* id, float fallback) { if (auto* p = params.getRawParameterValue (id)) return p->load(); return fallback; };
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
    
    // Update smoothed parameter targets
    smoothedCutoff.setTargetValue (cutoff->load());
    smoothedResonance.setTargetValue (resonance->load());
    smoothedOsc1Level.setTargetValue (params.getRawParameterValue ("osc1Level")->load());
    smoothedOsc2Level.setTargetValue (params.getRawParameterValue ("osc2Level")->load());
    smoothedOsc3Level.setTargetValue (params.getRawParameterValue ("osc3Level")->load());
    smoothedSaturation.setTargetValue (juce::jlimit (0.0f, 1.0f, sat->load()));
    smoothedFilterDrive.setTargetValue (drive->load());
    smoothedWavetable1.setTargetValue (wt1->load());
    smoothedWavetable2.setTargetValue (wt2->load());
    smoothedWavetable3.setTargetValue (wt3->load());
    smoothedFilterEnvAmount.setTargetValue (envAmount != nullptr ? envAmount->load() : 0.5f);
    
    osc1.setPosition (wt1->load()); osc2.setPosition (wt2->load()); osc3.setPosition (wt3->load());
    const auto ratioForSemitones = [] (float semitones) { return std::pow (2.0f, semitones / 12.0f); };
    const float osc1Ratio = ratioForSemitones (params.getRawParameterValue ("osc1Tune")->load());
    const float osc2Ratio = ratioForSemitones (params.getRawParameterValue ("osc2Tune")->load());
    const float osc3Ratio = ratioForSemitones (params.getRawParameterValue ("osc3Tune")->load());
    osc1.setFrequency (noteHz * osc1Ratio);
    osc2.setFrequency (noteHz * osc2Ratio);
    osc3.setFrequency (noteHz * osc3Ratio);
    
    // Note: filter cutoff & resonance will be updated per-sample using smoothed values
    filter.setType (type == 1 ? juce::dsp::StateVariableTPTFilterType::highpass : type == 2 ? juce::dsp::StateVariableTPTFilterType::bandpass : juce::dsp::StateVariableTPTFilterType::lowpass);
    filter2.setType (type == 1 ? juce::dsp::StateVariableTPTFilterType::highpass : type == 2 ? juce::dsp::StateVariableTPTFilterType::bandpass : juce::dsp::StateVariableTPTFilterType::lowpass);
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
    const auto panGains = [] (float pan, float& left, float& right)
    {
        const auto clampedPan = juce::jlimit (-1.0f, 1.0f, pan);
        left = std::sqrt (0.5f * (1.0f - clampedPan));
        right = std::sqrt (0.5f * (1.0f + clampedPan));
    };
    float osc1Left = 0.0f, osc1Right = 0.0f;
    float osc2Left = 0.0f, osc2Right = 0.0f;
    float osc3Left = 0.0f, osc3Right = 0.0f;
    panGains (-juce::jlimit (0.0f, 1.0f, osc1Spread->load()), osc1Left, osc1Right);
    panGains ( juce::jlimit (0.0f, 1.0f, osc2Spread->load()), osc2Left, osc2Right);
    const float osc3Pan = (midiNote & 1) == 0 ? -osc3Spread->load() : osc3Spread->load();
    panGains (osc3Pan, osc3Left, osc3Right);
    auto* preSatLeft = preSaturationBuffer.getWritePointer (0);
    auto* preSatRight = preSaturationBuffer.getWritePointer (1);
    for (int i = 0; i < count; ++i)
    {
        // 작업 3: Drift LFO advance
        const float driftIncrement = driftRateHz / (float) sampleRate;
        driftPhase += driftIncrement;
        driftPhase -= std::floor (driftPhase);
        const float driftMod = driftDepthValue > 0.0f ? std::sin (juce::MathConstants<float>::twoPi * driftPhase) * driftDepthValue * 0.02f : 0.0f;  // ±2%
        
        // Advance smoothed parameter values
        smoothedCutoff.getNextValue();
        smoothedResonance.getNextValue();
        const float l1 = smoothedOsc1Level.getNextValue();
        const float l2 = smoothedOsc2Level.getNextValue();
        const float l3 = smoothedOsc3Level.getNextValue();
        const float saturation = smoothedSaturation.getNextValue();
        const float inGain = juce::Decibels::decibelsToGain (smoothedFilterDrive.getNextValue());
        
        const float lfo1 = lfo1Active ? std::sin (juce::MathConstants<float>::twoPi * lfo1Phase) : 0.0f;
        const float lfo2 = lfo2Active ? std::sin (juce::MathConstants<float>::twoPi * lfo2Phase) : 0.0f;
        lfo1Phase += lfo1Increment;
        lfo2Phase += lfo2Increment;
        lfo1Phase -= std::floor (lfo1Phase);
        lfo2Phase -= std::floor (lfo2Phase);
        float pitchSemitones = driftMod * 12.0f, cutoffMod = 0.0f;  // 작업 3: Include drift in pitch
        float wavetableMod = 0.0f;
        const auto applyLfo = [&] (float value, float depth, int destination)
        {
            const float amount = value * depth;
            switch (destination)
            {
                case 0: pitchSemitones += amount * 12.0f; break;
                case 1: cutoffMod += amount; break;
                default: wavetableMod += amount * 0.25f; break;
            }
        };
        applyLfo (lfo1, lfo1DepthValue, lfo1Target);
        applyLfo (lfo2, lfo2DepthValue, lfo2Target);
        if (pitchSemitones != 0.0f)
        {
            const float pitchRatio = ratioForSemitones (pitchSemitones);
            osc1.setFrequency (noteHz * osc1Ratio * pitchRatio);
            osc2.setFrequency (noteHz * osc2Ratio * pitchRatio);
            osc3.setFrequency (noteHz * osc3Ratio * pitchRatio);
        }
        if (wavetableMod != 0.0f)
        {
            osc1.setPosition (smoothedWavetable1.getCurrentValue() + wavetableMod);
            osc2.setPosition (smoothedWavetable2.getCurrentValue() + wavetableMod);
            osc3.setPosition (smoothedWavetable3.getCurrentValue() + wavetableMod);
        }
        const float env = ampEnv.getNextSample();
        const float fenv = filterEnv.getNextSample();
        const float envDepth = smoothedFilterEnvAmount.getCurrentValue();
        const float modulatedCutoff = smoothedCutoff.getCurrentValue() * juce::jlimit (0.05f, 4.0f, 1.0f + envDepth * (fenv * 2.0f - 1.0f) + cutoffMod);
        filter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, modulatedCutoff));
        filter.setResonance (smoothedResonance.getCurrentValue());
        filter2.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, modulatedCutoff));
        filter2.setResonance (smoothedResonance.getCurrentValue());
        const float osc1Value = osc1.process (*table) * l1;
        const float osc2Value = osc2.process (*table) * l2;
        const float osc3Value = osc3.process (*table) * l3;
        
        // 작업 1: Unison processing - render unisonOscs with detuning
        int voiceCount = (int)value ("osc1Unison", 1.0f);
        voiceCount = juce::jlimit (1, 16, voiceCount);
        float unisonLeft = 0.0f, unisonRight = 0.0f;
        if (voiceCount > 1) {
            const float detuneAmount = 0.02f;  // ±2% detuning range
            for (int v = 0; v < voiceCount; ++v) {
                float detuneSemitones = (v - voiceCount/2.0f) * 2.0f * detuneAmount * 12.0f;
                float detuneRatio = ratioForSemitones (detuneSemitones);
                unisonOscs[v].setFrequency (noteHz * osc1Ratio * detuneRatio * (pitchSemitones != 0.0f ? ratioForSemitones(pitchSemitones) : 1.0f));
                unisonOscs[v].setPosition (smoothedWavetable1.getCurrentValue() + wavetableMod);
                float uniValue = unisonOscs[v].process (*table) * l1 / voiceCount;
                unisonLeft += uniValue * osc1Left;
                unisonRight += uniValue * osc1Right;
            }
        }
        
        float left = ((osc1Value * osc1Left + osc2Value * osc2Left + osc3Value * osc3Left) + unisonLeft) / (3.0f + (voiceCount > 1 ? 1.0f : 0.0f)) * inGain;
        float right = ((osc1Value * osc1Right + osc2Value * osc2Right + osc3Value * osc3Right) + unisonRight) / (3.0f + (voiceCount > 1 ? 1.0f : 0.0f)) * inGain;
        left = filter.processSample (0, left);
        if (outRight != nullptr)
            right = filter.processSample (1, right);
        if (slope >= 2)
        {
            left = filter2.processSample (0, left);
            if (outRight != nullptr)
                right = filter2.processSample (1, right);
        }
        const float vcaGain = env * level * 0.25f;
        const float vcaLeft = left * vcaGain;
        const float vcaRight = right * vcaGain;
        outputGainScratch[(size_t) i] = 1.0f;
        preSatLeft[i] = vcaLeft * (1.0f + saturation * 8.0f);
        preSatRight[i] = outRight != nullptr ? vcaRight * (1.0f + saturation * 8.0f) : 0.0f;
    }

    const float saturation = smoothedSaturation.getCurrentValue();
    const float satMakeup = 1.0f - saturation * 0.18f;
    juce::dsp::AudioBlock<float> preSatBlock (preSaturationBuffer);
    auto preSatSub = preSatBlock.getSubBlock (0, (size_t) count);
    auto oversampledBlock = saturationOversampling.processSamplesUp (preSatSub);
    for (size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
    {
        auto* channelData = oversampledBlock.getChannelPointer (ch);
        for (size_t n = 0; n < oversampledBlock.getNumSamples(); ++n)
            channelData[n] = std::tanh (channelData[n]);
    }
    saturationOversampling.processSamplesDown (preSatSub);
    for (int i = 0; i < count; ++i)
    {
        out[i] += preSatLeft[i] * satMakeup * outputGainScratch[(size_t) i];
        if (outRight != nullptr)
            outRight[i] += preSatRight[i] * satMakeup * outputGainScratch[(size_t) i];
    }
    if (releasing && ampEnv.isActive() == false) clearCurrentNote();
}
