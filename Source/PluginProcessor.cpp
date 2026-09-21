#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace
{
    constexpr std::array<const char*, 16> midiLearnIDs {
        "osc1Pos", "osc2Pos", "osc3Pos", "cutoff", "resonance",
        "filterDrive", "saturation", "output", "ampAttack", "ampDecay",
        "ampSustain", "ampRelease", "filterAttack", "filterDecay",
        "filterSustain", "filterRelease" };
    constexpr int wavetableStateVersion = 1;
    constexpr int factoryPresetCount = 11;

    std::uint32_t splitMix32 (std::uint32_t value) noexcept
    {
        value += 0x9e3779b9u;
        value = (value ^ (value >> 16)) * 0x85ebca6bu;
        value = (value ^ (value >> 13)) * 0xc2b2ae35u;
        value ^= value >> 16;
        return value != 0u ? value : 1u;
    }

    struct FactoryPreset
    {
        float osc1Pos = 0.0f, osc2Pos = 0.0f, osc3Pos = 0.0f;
        float osc1Level = 0.75f, osc2Level = 0.75f, osc3Level = 0.75f;
        float osc1Tune = 0.0f, osc2Tune = 7.0f, osc3Tune = -7.0f;
        int filterType = 0, filterSlope = 0;
        float cutoff = 12000.0f, resonance = 0.25f, filterDrive = 0.0f;
        float saturation = 0.15f, output = -6.0f;
        float ampAttack = 0.01f, ampDecay = 0.25f, ampSustain = 0.8f, ampRelease = 0.35f;
        float filterAttack = 0.01f, filterDecay = 0.25f, filterSustain = 0.8f, filterRelease = 0.35f;
    };

    const std::array<FactoryPreset, factoryPresetCount>& getFactoryPresets()
    {
        static const auto presets = []
        {
            std::array<FactoryPreset, factoryPresetCount> values;
            std::uint32_t state = 0xE0A2026u;
            const auto next01 = [&state]
            {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                return (float) (state & 0x00ffffffu) / 16777215.0f;
            };
            const auto range = [&next01] (float lo, float hi) { return lo + (hi - lo) * next01(); };
            const auto time = [&next01] (float lo, float hi) { return std::exp (juce::jmap (next01(), std::log (lo), std::log (hi))); };

            for (auto& preset : values)
            {
                // Random bank keeps the factory fifth-stack character; only the
                // curated entries below override tuning/levels explicitly.
                preset.osc1Pos = next01();
                preset.osc2Pos = next01();
                preset.osc3Pos = next01();
                preset.filterType = juce::jlimit (0, 2, (int) (next01() * 3.0f));
                preset.filterSlope = juce::jlimit (0, 3, (int) (next01() * 4.0f));
                preset.cutoff = std::exp (juce::jmap (next01(), std::log (120.0f), std::log (16000.0f)));
                preset.resonance = range (0.15f, 0.9f);
                preset.filterDrive = range (-6.0f, 18.0f);
                preset.saturation = range (0.02f, 0.85f);
                preset.output = range (-12.0f, -1.0f);
                preset.ampAttack = time (0.005f, 0.8f);
                preset.ampDecay = time (0.05f, 2.5f);
                preset.ampSustain = range (0.25f, 1.0f);
                preset.ampRelease = time (0.05f, 3.0f);
                preset.filterAttack = time (0.005f, 0.6f);
                preset.filterDecay = time (0.05f, 2.0f);
                preset.filterSustain = range (0.15f, 1.0f);
                preset.filterRelease = time (0.05f, 2.5f);
            }
            // Curated preset: the historical default state (root + fifth up +
            // fifth down) preserved as a factory entry before the default
            // tuning changed to unison pitch.
            values[10].filterSlope = 3;
            return values;
        }();
        return presets;
    }
}

static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    using P = juce::AudioParameterFloat; using C = juce::AudioParameterChoice; using B = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<P> ("osc1Pos", "Osc 1 Wavetable", 0.0f, 1.0f, 0.0f));
    l.add (std::make_unique<P> ("osc2Pos", "Osc 2 Wavetable", 0.0f, 1.0f, 0.35f));
    l.add (std::make_unique<P> ("osc3Pos", "Osc 3 Wavetable", 0.0f, 1.0f, 0.67f));
    for (int osc = 1; osc <= 3; ++osc)
    {
        const auto prefix = "osc" + juce::String (osc);
        l.add (std::make_unique<P> (prefix + "Level", prefix + " Level", 0.0f, 1.0f, 0.75f));
        l.add (std::make_unique<P> (prefix + "Tune", prefix + " Tune", -24.0f, 24.0f, 0.0f));
        l.add (std::make_unique<P> (prefix + "Unison", prefix + " Unison", 1.0f, 8.0f, 1.0f));
        l.add (std::make_unique<P> (prefix + "Spread", prefix + " Spread", 0.0f, 1.0f, 0.2f));
        l.add (std::make_unique<P> (prefix + "Detune", prefix + " Detune", 0.0f, 50.0f, 12.0f));
    }
    l.add (std::make_unique<P> ("unisonKeyTrack", "Unison Key Track", -1.0f, 1.0f, 0.0f));
    l.add (std::make_unique<C> ("filterType", "Filter Type", juce::StringArray { "Low-pass", "High-pass", "Band-pass" }, 0));
    l.add (std::make_unique<C> ("filterSlope", "Filter Slope", juce::StringArray { "12 dB/oct", "12 dB/oct", "24 dB/oct", "24 dB/oct" }, 3));
    l.add (std::make_unique<P> ("cutoff", "Cutoff", 20.0f, 20000.0f, 12000.0f));
    l.add (std::make_unique<P> ("resonance", "Resonance", 0.1f, 1.0f, 0.25f));
    l.add (std::make_unique<P> ("filterDrive", "Filter Drive", -12.0f, 24.0f, 0.0f));
    l.add (std::make_unique<P> ("saturation", "Saturation", 0.0f, 1.0f, 0.15f));
    l.add (std::make_unique<P> ("output", "Output", -60.0f, 6.0f, -6.0f));
    l.add (std::make_unique<P> ("filterEnvAmount", "Filter Envelope Amount", -1.0f, 1.0f, 0.5f));
    l.add (std::make_unique<P> ("masterWidth", "Master Width", 0.0f, 2.0f, 1.0f));
    l.add (std::make_unique<P> ("ampAttack", "Amp Attack", 0.001f, 10.0f, 0.01f));
    l.add (std::make_unique<P> ("ampDecay", "Amp Decay", 0.001f, 10.0f, 0.25f));
    l.add (std::make_unique<P> ("ampSustain", "Amp Sustain", 0.0f, 1.0f, 0.8f));
    l.add (std::make_unique<P> ("ampRelease", "Amp Release", 0.001f, 10.0f, 0.35f));
    l.add (std::make_unique<P> ("filterAttack", "Filter Attack", 0.001f, 10.0f, 0.01f));
    l.add (std::make_unique<P> ("filterDecay", "Filter Decay", 0.001f, 10.0f, 0.25f));
    l.add (std::make_unique<P> ("filterSustain", "Filter Sustain", 0.0f, 1.0f, 0.8f));
    l.add (std::make_unique<P> ("filterRelease", "Filter Release", 0.001f, 10.0f, 0.35f));
    l.add (std::make_unique<P> ("lfo1Rate", "LFO 1 Rate", 0.05f, 20.0f, 1.0f));
    l.add (std::make_unique<P> ("lfo1Depth", "LFO 1 Depth", 0.0f, 1.0f, 0.0f));
    l.add (std::make_unique<C> ("lfo1Destination", "LFO 1 Destination", juce::StringArray { "Pitch", "Cutoff", "Wavetable" }, 1));
    l.add (std::make_unique<P> ("lfo2Rate", "LFO 2 Rate", 0.05f, 20.0f, 0.25f));
    l.add (std::make_unique<P> ("lfo2Depth", "LFO 2 Depth", 0.0f, 1.0f, 0.0f));
    l.add (std::make_unique<C> ("lfo2Destination", "LFO 2 Destination", juce::StringArray { "Pitch", "Cutoff", "Wavetable" }, 2));
    l.add (std::make_unique<P> ("delayTime", "Delay Time", 0.03f, 1.5f, 0.32f));
    l.add (std::make_unique<P> ("delayFeedback", "Delay Feedback", 0.0f, 0.9f, 0.35f));
    l.add (std::make_unique<P> ("delayMix", "Delay Mix", 0.0f, 1.0f, 0.0f));
    l.add (std::make_unique<P> ("reverbSize", "Reverb Size", 0.0f, 1.0f, 0.45f));
    l.add (std::make_unique<P> ("reverbDamping", "Reverb Damping", 0.0f, 1.0f, 0.5f));
    l.add (std::make_unique<P> ("reverbMix", "Reverb Mix", 0.0f, 1.0f, 0.0f));
    l.add (std::make_unique<B> ("arpEnabled", "Arpeggiator Enabled", false));
    l.add (std::make_unique<P> ("arpRate", "Arpeggiator Rate", 0.5f, 24.0f, 8.0f));
    l.add (std::make_unique<P> ("arpGate", "Arpeggiator Gate", 0.05f, 1.0f, 0.72f));
    // Stage 2: Character & Expressiveness (작업 4, 1, 3)
    l.add (std::make_unique<P> ("randomPhase", "Random Phase", 0.0f, 1.0f, 0.0f));
    l.add (std::make_unique<P> ("driftRate", "Drift Rate", 0.05f, 2.0f, 0.3f));
    l.add (std::make_unique<P> ("driftDepth", "Drift Depth", 0.0f, 1.0f, 0.0f));
    l.add (std::make_unique<C> ("arpPattern", "Arpeggiator Pattern", juce::StringArray { "Up", "Down", "Up/Down", "Random" }, 0));
    return l;
}

HybridWavetableAudioProcessor::HybridWavetableAudioProcessor (std::uint32_t deterministicSeed)
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", makeLayout())
{
    auto seed = deterministicSeed;
    if (seed == 0u)
        seed = (std::uint32_t) juce::Random::getSystemRandom().nextInt();
    if (seed == 0u)
        seed = 1u;
    wavetableBuffers = std::make_unique<std::array<WavetableData, wavetableBufferCount>>();
    for (auto& table : *wavetableBuffers)
        table = wavetable;
    for (auto& reader : wavetableReaders)
        reader.store (0, std::memory_order_relaxed);
    audioWavetable.store (&(*wavetableBuffers)[0], std::memory_order_release);
    for (auto& mapping : midiCCAssignments)
        mapping.store (-1, std::memory_order_relaxed);
    // A generous pool keeps allocation off the real-time thread while allowing
    // dense chords; JUCE's voice stealing remains CPU-safe under load.
    for (int i = 0; i < 128; ++i)
        synth.addVoice (new SynthVoice (parameters, splitMix32 (seed + (std::uint32_t) i)));
    synth.addSound (new SynthSound());
}

DelayMixGains HybridWavetableAudioProcessor::calculateDelayMixGains (float mix) noexcept
{
    const auto normalized = juce::jlimit (0.0f, 1.0f, mix);
    return { std::cos (0.5f * juce::MathConstants<float>::pi * normalized),
             std::sin (0.5f * juce::MathConstants<float>::pi * normalized) };
}

void HybridWavetableAudioProcessor::prepareToPlay (double sr, int block)
{
    currentSampleRate = sr; synth.setCurrentPlaybackSampleRate (sr);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i))) v->prepare (sr, block, &audioWavetable);
    if (auto* firstVoice = dynamic_cast<SynthVoice*> (synth.getVoice (0)))
        setLatencySamples (firstVoice->getSaturationOversamplingLatencySamples());
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, block), 2 };
    delayLine.setMaximumDelayInSamples (juce::jmax (4, (int) std::ceil (sr * 1.5) + 4));
    delayLine.prepare (spec);
    delayLine.reset();
    monoBassLowpass.prepare (spec);
    monoBassLowpass.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    monoBassLowpass.setCutoffFrequency (100.0f);
    monoBassHighpass.prepare (spec);
    monoBassHighpass.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    monoBassHighpass.setCutoffFrequency (100.0f);
    lowBandScratch.setSize (2, juce::jmax (1, block), false, true, true);
    lowBandScratch.clear();
    reverb.prepare (spec);
    reverb.reset();
    outputSafety.prepare (sr);
    arpeggiatedMidi.ensureSize (2048);
    const float rampSeconds = 0.05f;
    const auto initialiseEffectSmoother = [this, sr, rampSeconds] (auto& smoother, const char* id, float fallback)
    {
        smoother.reset (sr, rampSeconds);
        const auto* parameter = parameters.getRawParameterValue (id);
        smoother.setCurrentAndTargetValue (parameter != nullptr ? parameter->load() : fallback);
    };
    initialiseEffectSmoother (smoothedDelayTime, "delayTime", 0.32f);
    initialiseEffectSmoother (smoothedDelayFeedback, "delayFeedback", 0.35f);
    initialiseEffectSmoother (smoothedDelayMix, "delayMix", 0.0f);
    initialiseEffectSmoother (smoothedReverbMix, "reverbMix", 0.0f);
    initialiseEffectSmoother (smoothedMasterWidth, "masterWidth", 1.0f);
    effectSmoothersNeedInitialisation = true;
}
void HybridWavetableAudioProcessor::releaseResources() {}
void HybridWavetableAudioProcessor::reset()
{
    delayLine.reset();
    monoBassLowpass.reset();
    monoBassHighpass.reset();
    lowBandScratch.clear();
    reverb.reset();
    outputSafety.reset();
    effectSmoothersNeedInitialisation = true;
}
bool HybridWavetableAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{ return l.getMainOutputChannelSet() == juce::AudioChannelSet::mono() || l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
void HybridWavetableAudioProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer& m)
{
    juce::ScopedNoDenormals noDenormals;
    if (effectSmoothersNeedInitialisation)
    {
        const auto initialiseNow = [this] (auto& smoother, const char* id)
        {
            if (const auto* parameter = parameters.getRawParameterValue (id))
                smoother.setCurrentAndTargetValue (parameter->load());
        };
        initialiseNow (smoothedDelayTime, "delayTime");
        initialiseNow (smoothedDelayFeedback, "delayFeedback");
        initialiseNow (smoothedDelayMix, "delayMix");
        initialiseNow (smoothedReverbMix, "reverbMix");
        initialiseNow (smoothedMasterWidth, "masterWidth");
        effectSmoothersNeedInitialisation = false;
    }
    for (const auto metadata : m)
    {
        const auto message = metadata.getMessage();
        if (! message.isController())
            continue;

        const int cc = message.getControllerNumber();
        const int target = midiLearnTarget.exchange (-1, std::memory_order_acq_rel);
        if (target >= 0 && target < (int) midiLearnIDs.size())
        {
            midiCCAssignments[(size_t) cc].store (target, std::memory_order_release);
            continue;
        }

        const int mappedTarget = midiCCAssignments[(size_t) cc].load (std::memory_order_acquire);
        if (mappedTarget >= 0 && mappedTarget < (int) midiLearnIDs.size())
            if (auto* parameter = parameters.getParameter (midiLearnIDs[(size_t) mappedTarget]))
                parameter->setValueNotifyingHost (message.getControllerValue() / 127.0f);
    }

    processArpeggiator (m, b.getNumSamples());
    const int wavetableSlot = acquireWavetableForAudio();
    audioWavetable.store (&(*wavetableBuffers)[(size_t) wavetableSlot], std::memory_order_release);
    b.clear();
    synth.renderNextBlock (b, m, 0, b.getNumSamples());
    releaseWavetableForAudio (wavetableSlot);
    b.applyGain (juce::Decibels::decibelsToGain (parameters.getRawParameterValue ("output")->load()));
    if (b.getNumChannels() > 1)
    {
        smoothedMasterWidth.setTargetValue (juce::jlimit (0.0f, 2.0f,
                                                          parameters.getRawParameterValue ("masterWidth")->load()));
        // Elliptical Sub-Bass Crossover: Keep low end (<100Hz) pure mono to prevent phase cancellation
        const auto samples = b.getNumSamples();
        for (int channel = 0; channel < 2; ++channel)
            lowBandScratch.copyFrom (channel, 0, b, channel, 0, samples);
        auto lowBlock = juce::dsp::AudioBlock<float> (lowBandScratch).getSubBlock (0, (size_t) samples);
        juce::dsp::AudioBlock<float> highBlock (b);
        monoBassLowpass.process (juce::dsp::ProcessContextReplacing<float> (lowBlock));
        monoBassHighpass.process (juce::dsp::ProcessContextReplacing<float> (highBlock));

        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const auto width = smoothedMasterWidth.getNextValue();
            if (width <= 0.0001f)
            {
                const auto monoSum = 0.5f * (b.getSample (0, i) + b.getSample (1, i)
                                              + lowBandScratch.getSample (0, i) + lowBandScratch.getSample (1, i));
                b.setSample (0, i, monoSum);
                b.setSample (1, i, monoSum);
            }
            else
            {
                const auto highMid = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
                const auto highSide = 0.5f * (b.getSample (0, i) - b.getSample (1, i)) * width;
                const auto lowMono = 0.5f * (lowBandScratch.getSample (0, i) + lowBandScratch.getSample (1, i));
                b.setSample (0, i, lowMono + highMid + highSide);
                b.setSample (1, i, lowMono + highMid - highSide);
            }
        }
    }
    processEffects (b);
    outputSafety.process (b);
}

int HybridWavetableAudioProcessor::acquireWavetableForAudio() noexcept
{
    // If publication races this callback, retry after releasing the obsolete
    // slot. This is a bounded lock-free reader-side critical section.
    for (;;)
    {
        const int slot = activeWavetableSlot.load (std::memory_order_acquire);
        wavetableReaders[(size_t) slot].fetch_add (1, std::memory_order_acq_rel);
        if (activeWavetableSlot.load (std::memory_order_acquire) == slot)
            return slot;
        wavetableReaders[(size_t) slot].fetch_sub (1, std::memory_order_release);
    }
}

void HybridWavetableAudioProcessor::releaseWavetableForAudio (int slot) noexcept
{
    wavetableReaders[(size_t) slot].fetch_sub (1, std::memory_order_release);
}


void HybridWavetableAudioProcessor::processEffects (juce::AudioBuffer<float>& buffer) noexcept
{
    const int channels = juce::jmin (2, buffer.getNumChannels());
    smoothedDelayTime.setTargetValue (juce::jlimit (0.03f, 1.5f,
                                                    parameters.getRawParameterValue ("delayTime")->load()));
    smoothedDelayFeedback.setTargetValue (juce::jlimit (0.0f, 0.9f,
                                                        parameters.getRawParameterValue ("delayFeedback")->load()));
    smoothedDelayMix.setTargetValue (juce::jlimit (0.0f, 1.0f,
                                                   parameters.getRawParameterValue ("delayMix")->load()));
    smoothedReverbMix.setTargetValue (juce::jlimit (0.0f, 1.0f,
                                                    parameters.getRawParameterValue ("reverbMix")->load()));
    float reverbMix = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float delaySamplesExact = (float) (smoothedDelayTime.getNextValue() * currentSampleRate);
        const float feedback = juce::jlimit (0.0f, 0.9f, smoothedDelayFeedback.getNextValue());
        const float mix = juce::jlimit (0.0f, 1.0f, smoothedDelayMix.getNextValue());
        reverbMix = juce::jlimit (0.0f, 1.0f, smoothedReverbMix.getNextValue());
        const auto gains = calculateDelayMixGains (mix);
        for (int channel = 0; channel < channels; ++channel)
        {
            const float dry = buffer.getSample (channel, sample);
            const float delayed = delayLine.popSample (channel, delaySamplesExact);
            delayLine.pushSample (channel, dry + delayed * feedback);
            buffer.setSample (channel, sample, dry * gains.dry + delayed * gains.wet);
        }
    }

    if (channels == 2)
    {
        juce::dsp::Reverb::Parameters settings;
        settings.roomSize = juce::jlimit (0.0f, 1.0f, parameters.getRawParameterValue ("reverbSize")->load());
        settings.damping = juce::jlimit (0.0f, 1.0f, parameters.getRawParameterValue ("reverbDamping")->load());
        settings.wetLevel = reverbMix;
        settings.dryLevel = 1.0f;
        settings.width = 1.0f;
        reverb.setParameters (settings);
        juce::dsp::AudioBlock<float> block (buffer);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (block));
    }
}

double HybridWavetableAudioProcessor::getTailLengthSeconds() const
{
    return 4.5;
}


void HybridWavetableAudioProcessor::processArpeggiator (juce::MidiBuffer& midi, int numSamples) noexcept
{
    const bool enabled = parameters.getRawParameterValue ("arpEnabled")->load() >= 0.5f;
    if (! enabled)
    {
        arpeggiatedMidi.clear();
        if (arpActiveNote >= 0)
            arpeggiatedMidi.addEvent (juce::MidiMessage::noteOff (1, arpActiveNote), 0);
        for (const auto metadata : midi)
            arpeggiatedMidi.addEvent (metadata.getMessage(), metadata.samplePosition);
        arpActiveNote = -1;
        arpStep = 0;
        arpSamplesUntilStep = 0;
        arpSamplesUntilGateOff = -1;
        heldNoteVelocity.fill (0.0f);
        midi.swapWith (arpeggiatedMidi);
        return;
    }

    arpeggiatedMidi.clear();
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
            heldNoteVelocity[(size_t) message.getNoteNumber()] = message.getFloatVelocity();
        else if (message.isNoteOff())
            heldNoteVelocity[(size_t) message.getNoteNumber()] = 0.0f;
        else
            arpeggiatedMidi.addEvent (message, metadata.samplePosition);
    }

    std::array<int, 128> notes {};
    int noteCount = 0;
    for (int note = 0; note < 128; ++note)
        if (heldNoteVelocity[(size_t) note] > 0.0f)
            notes[(size_t) noteCount++] = note;
    if (noteCount == 0)
    {
        if (arpActiveNote >= 0)
            arpeggiatedMidi.addEvent (juce::MidiMessage::noteOff (1, arpActiveNote), 0);
        arpActiveNote = -1;
        arpSamplesUntilStep = 0;
        arpSamplesUntilGateOff = -1;
        midi.swapWith (arpeggiatedMidi);
        return;
    }

    const float rate = juce::jlimit (0.5f, 24.0f, parameters.getRawParameterValue ("arpRate")->load());
    const int stepSamples = juce::jmax (1, (int) std::round (currentSampleRate / rate));
    const float gate = juce::jlimit (0.05f, 1.0f, parameters.getRawParameterValue ("arpGate")->load());
    const int gateSamples = juce::jlimit (1, stepSamples, (int) std::round (stepSamples * gate));
    const int pattern = (int) parameters.getRawParameterValue ("arpPattern")->load();
    int offset = 0;
    int samplesUntilStep = juce::jmax (0, arpSamplesUntilStep);
    int samplesUntilGateOff = arpSamplesUntilGateOff;
    while (offset < numSamples)
    {
        const int untilGate = samplesUntilGateOff >= 0 ? samplesUntilGateOff : std::numeric_limits<int>::max();
        const int nextEvent = juce::jmin (samplesUntilStep, untilGate);
        const int remaining = numSamples - offset;
        if (nextEvent >= remaining)
        {
            samplesUntilStep -= remaining;
            if (samplesUntilGateOff >= 0)
                samplesUntilGateOff -= remaining;
            break;
        }

        offset += nextEvent;
        samplesUntilStep -= nextEvent;
        if (samplesUntilGateOff >= 0)
            samplesUntilGateOff -= nextEvent;

        if (samplesUntilGateOff == 0)
        {
            if (arpActiveNote >= 0)
                arpeggiatedMidi.addEvent (juce::MidiMessage::noteOff (1, arpActiveNote), offset);
            arpActiveNote = -1;
            samplesUntilGateOff = -1;
        }

        if (samplesUntilStep == 0)
        {
            if (arpActiveNote >= 0)
                arpeggiatedMidi.addEvent (juce::MidiMessage::noteOff (1, arpActiveNote), offset);

            int index = arpStep++ % noteCount;
            if (pattern == 1)
                index = noteCount - 1 - index;
            else if (pattern == 2 && noteCount > 1)
            {
                const int cycle = (arpStep - 1) % (noteCount * 2 - 2);
                index = cycle < noteCount ? cycle : (noteCount * 2 - 2 - cycle);
            }
            else if (pattern == 3)
            {
                arpRandomState = arpRandomState * 1664525u + 1013904223u;
                index = (int) (arpRandomState % (std::uint32_t) noteCount);
            }
            arpActiveNote = notes[(size_t) index];
            arpeggiatedMidi.addEvent (juce::MidiMessage::noteOn (1, arpActiveNote, heldNoteVelocity[(size_t) arpActiveNote]), offset);
            samplesUntilStep = stepSamples;
            samplesUntilGateOff = gateSamples;
        }
    }
    arpSamplesUntilStep = samplesUntilStep;
    arpSamplesUntilGateOff = samplesUntilGateOff;
    midi.swapWith (arpeggiatedMidi);
}
void HybridWavetableAudioProcessor::getStateInformation (juce::MemoryBlock& d)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("stateSchemaVersion", 2);
    xml->setAttribute ("uiType", getUiType());

    juce::MemoryOutputStream tableData;
    tableData.writeInt (wavetableStateVersion);
    tableData.writeInt (WavetableData::numTables);
    tableData.writeInt (WavetableData::tableSize);
    for (const auto& frame : wavetable.frames)
        for (const auto sample : frame)
            tableData.writeFloat (sample);

    auto* tableElement = xml->createNewChildElement ("WAVETABLE");
    tableElement->addTextElement (tableData.getMemoryBlock().toBase64Encoding());

    auto* midiElement = xml->createNewChildElement ("MIDILEARN");
    for (size_t cc = 0; cc < midiCCAssignments.size(); ++cc)
        if (const int target = midiCCAssignments[cc].load (std::memory_order_acquire); target >= 0)
            midiElement->setAttribute ("cc" + juce::String ((int) cc), target);

    copyXmlToBinary (*xml, d);
}

void HybridWavetableAudioProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    // The custom payloads are text-bearing XML nodes, which have no direct
    // ValueTree representation. Strip them before handing the parameter tree
    // to JUCE so state restore stays assertion-free in debug builds.
    std::unique_ptr<juce::XmlElement> parameterXml (new juce::XmlElement (*xml));
    if (auto* table = parameterXml->getChildByName ("WAVETABLE"))
        parameterXml->removeChildElement (table, true);
    if (auto* midi = parameterXml->getChildByName ("MIDILEARN"))
        parameterXml->removeChildElement (midi, true);
    auto parameterState = juce::ValueTree::fromXml (*parameterXml);
    if (! parameterState.isValid())
        return;
    const auto stateSchemaVersion = xml->getIntAttribute ("stateSchemaVersion", 1);
    if (stateSchemaVersion < 2)
    {
        // Version-1 sessions predate the unison quality controls.  Keep every
        // existing value and explicitly fill only the newly introduced fields.
        const auto hasParameter = [&parameterState] (const char* id)
        {
            for (int index = 0; index < parameterState.getNumChildren(); ++index)
            {
                const auto parameter = parameterState.getChild (index);
                if (parameter.hasType ("PARAM") && parameter.getProperty ("id").toString() == id)
                    return true;
            }
            return false;
        };
        const auto setDefaultIfMissing = [&parameterState, &hasParameter] (const char* id, float value)
        {
            if (! hasParameter (id))
            {
                juce::ValueTree parameter ("PARAM");
                parameter.setProperty ("id", id, nullptr);
                parameter.setProperty ("value", value, nullptr);
                parameterState.addChild (parameter, -1, nullptr);
            }
        };
        setDefaultIfMissing ("osc1Detune", 12.0f);
        setDefaultIfMissing ("osc2Detune", 12.0f);
        setDefaultIfMissing ("osc3Detune", 12.0f);
        setDefaultIfMissing ("unisonKeyTrack", 0.0f);
    }
    parameters.replaceState (parameterState);

    // The editor type is presentation-only state carried on the root element.
    // Clamping keeps hand-edited or hostile sessions inside the 90-type range.
    setUiType (xml->getIntAttribute ("uiType", 0));

    if (const auto* tableElement = xml->getChildByName ("WAVETABLE"))
    {
        juce::MemoryBlock tableData;
        if (tableData.fromBase64Encoding (tableElement->getAllSubText()))
        {
            juce::MemoryInputStream input (tableData, false);
            const int version = input.readInt();
            const int numTables = input.readInt();
            const int tableSize = input.readInt();
            if (version == wavetableStateVersion && numTables == WavetableData::numTables && tableSize == WavetableData::tableSize)
            {
                auto restored = std::make_unique<WavetableData>();
                for (auto& frame : restored->frames)
                    for (auto& sample : frame)
                        sample = input.readFloat();
                // The frames above were overwritten after WavetableData's
                // constructor already built mips for the default sine
                // table, so the restored table needs its own mip rebuild
                // before it is published to the audio thread.
                restored->regenerateMips();
                wavetable = *restored;
                publishWavetable();
            }
        }
    }

    for (auto& mapping : midiCCAssignments)
        mapping.store (-1, std::memory_order_release);
    if (const auto* midiElement = xml->getChildByName ("MIDILEARN"))
        for (int cc = 0; cc < (int) midiCCAssignments.size(); ++cc)
            midiCCAssignments[(size_t) cc].store (midiElement->getIntAttribute ("cc" + juce::String (cc), -1), std::memory_order_release);
}
void HybridWavetableAudioProcessor::loadAudioFile (const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
    if (r == nullptr || r->lengthInSamples <= 0 || r->numChannels == 0)
        return;

    // Read one bounded, contiguous source region per frame. Long files use a
    // centred region from each sixteenth so the bank retains movement across
    // the file without allocating for the complete recording.
    constexpr int importCapacity = WavetableData::tableSize * WavetableData::numTables;
    constexpr int maximumSourceSamplesPerFrame = WavetableData::tableSize * 32;
    const bool hasMultipleFrames = r->lengthInSamples >= importCapacity;
    const auto sourceSegmentLength = hasMultipleFrames
                                         ? r->lengthInSamples / WavetableData::numTables
                                         : r->lengthInSamples;
    const auto sourceSamplesPerFrame = (int) juce::jmin ((juce::int64) maximumSourceSamplesPerFrame,
                                                         sourceSegmentLength);
    const int importSamples = sourceSamplesPerFrame
                              * (hasMultipleFrames ? WavetableData::numTables : 1);
    juce::AudioBuffer<float> data (1, importSamples);
    data.clear();
    const int framesToRead = hasMultipleFrames ? WavetableData::numTables : 1;
    for (int frame = 0; frame < framesToRead; ++frame)
    {
        const auto segmentStart = hasMultipleFrames ? (juce::int64) frame * sourceSegmentLength : 0;
        const auto centredOffset = juce::jmax ((juce::int64) 0,
                                               (sourceSegmentLength - sourceSamplesPerFrame) / 2);
        r->read (&data, frame * sourceSamplesPerFrame, sourceSamplesPerFrame,
                 segmentStart + centredOffset, true, false);
    }
    wavetable.loadFromAudio (data);
    publishWavetable();
}
void HybridWavetableAudioProcessor::publishWavetable()
{
    const int active = activeWavetableSlot.load (std::memory_order_acquire);
    for (int offset = 1; offset < wavetableBufferCount; ++offset)
    {
        const int candidate = (active + offset) % wavetableBufferCount;
        if (wavetableReaders[(size_t) candidate].load (std::memory_order_acquire) == 0)
        {
            (*wavetableBuffers)[(size_t) candidate] = wavetable;
            activeWavetableSlot.store (candidate, std::memory_order_release);
            return;
        }
    }
    // Under pathological GUI publication pressure all spare tables can be in
    // flight. Dropping this update is safer than touching audio-owned memory;
    // the next editor move republishes the current editable table.
}
const juce::StringArray& HybridWavetableAudioProcessor::getMidiLearnTargets()
{
    static const juce::StringArray names { "Osc 1 Position", "Osc 2 Position", "Osc 3 Position", "Cutoff", "Resonance",
                                           "Filter Drive", "Saturation", "Output", "Amp Attack", "Amp Decay",
                                           "Amp Sustain", "Amp Release", "Filter Attack", "Filter Decay",
                                           "Filter Sustain", "Filter Release" };
    return names;
}
const juce::StringArray& HybridWavetableAudioProcessor::getFactoryPresetNames()
{
    static const juce::StringArray names {
        "RND 01 // NEON PULSE", "RND 02 // CHROME PLUCK", "RND 03 // VOID GLASS",
        "RND 04 // LASER PAD", "RND 05 // ACID VECTOR", "RND 06 // NIGHT DRIVE",
        "RND 07 // STATIC BLOOM", "RND 08 // GHOST FM", "RND 09 // CIRCUIT BASS",
        "RND 10 // QUANTUM AIR", "STACK 11 // FIFTHS" };
    return names;
}
void HybridWavetableAudioProcessor::applyFactoryPreset (int index)
{
    if (index < 0 || index >= factoryPresetCount)
        return;

    const auto& preset = getFactoryPresets()[(size_t) index];
    const auto setParameter = [this] (const char* id, float value)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (parameters.getParameter (id)))
            parameter->setValueNotifyingHost (parameter->getNormalisableRange().convertTo0to1 (value));
    };
    setParameter ("osc1Pos", preset.osc1Pos);
    setParameter ("osc2Pos", preset.osc2Pos);
    setParameter ("osc3Pos", preset.osc3Pos);
    setParameter ("osc1Level", preset.osc1Level);
    setParameter ("osc2Level", preset.osc2Level);
    setParameter ("osc3Level", preset.osc3Level);
    setParameter ("osc1Tune", preset.osc1Tune);
    setParameter ("osc2Tune", preset.osc2Tune);
    setParameter ("osc3Tune", preset.osc3Tune);
    setParameter ("filterType", (float) preset.filterType);
    setParameter ("filterSlope", (float) preset.filterSlope);
    setParameter ("cutoff", preset.cutoff);
    setParameter ("resonance", preset.resonance);
    setParameter ("filterDrive", preset.filterDrive);
    setParameter ("saturation", preset.saturation);
    setParameter ("output", preset.output);
    setParameter ("ampAttack", preset.ampAttack);
    setParameter ("ampDecay", preset.ampDecay);
    setParameter ("ampSustain", preset.ampSustain);
    setParameter ("ampRelease", preset.ampRelease);
    setParameter ("filterAttack", preset.filterAttack);
    setParameter ("filterDecay", preset.filterDecay);
    setParameter ("filterSustain", preset.filterSustain);
    setParameter ("filterRelease", preset.filterRelease);
}
void HybridWavetableAudioProcessor::beginMidiLearn (int targetIndex) noexcept
{
    midiLearnTarget.store (juce::jlimit (0, (int) midiLearnIDs.size() - 1, targetIndex), std::memory_order_release);
}
void HybridWavetableAudioProcessor::setUiType (int type) noexcept
{
    uiType.store (seoului::clampType (type), std::memory_order_release);
}
juce::AudioProcessorEditor* HybridWavetableAudioProcessor::createEditor() { return new HybridWavetableAudioProcessorEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HybridWavetableAudioProcessor(); }
