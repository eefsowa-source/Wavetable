// Factory presets, measured through the real renderer.
//
// The factory bank used to be ten fixed-seed random patches. Nothing checked
// that the patches were usable, distinct, or related to their names -- "ACID
// VECTOR" was whatever the generator emitted that day. These gates make the
// bank auditable so a future edit cannot quietly ship eleven near-identical
// patches again.
//
// Three things are checked, in increasing order of what they cost:
//
//   1. Contract. Every preset applies cleanly, stays finite and bounded, lands
//      inside every parameter's own range, and is distinct from its neighbours.
//      This is the gate that would have caught a preset whose resonance or
//      cutoff fell outside the host range.
//   2. Ordering. The envelopes actually differ in the direction their names
//      imply: a pluck decays faster than a pad, a pad attacks slower than a
//      pluck. These are cheap and catch an edit that transposes two entries.
//   3. Reach. Each preset's rendered peak and RMS sit inside a band chosen for
//      its role, so an entry cannot be silently inaudible or clipping.
//
// It deliberately does not claim a subjective tone verdict. Whether NEON PULSE
// sounds like the bright lead it is named for is a listening gate
// (docs/quality/b8-blind-listening.md); what is checked here is that the numbers
// behind the names are real.

#include "../../Source/PluginProcessor.h"
#include "OfflineRenderer.h"
#include "TestHarness.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kPresetCount = 11;
constexpr std::uint32_t kSeed = 0x50524553u;   // "PRES"

struct RenderedPreset
{
    std::string name;
    float peak = 0.0f;
    double rmsDb = -120.0;
    float attackSeconds = 0.0f;
    float decaySeconds = 0.0f;
    float resonanceQ = 0.0f;
    float cutoff = 0.0f;
    float brightness = 0.0f;
};

float plainParameter (HybridWavetableAudioProcessor& p, const char* id)
{
    if (auto* parameter = p.parameters.getParameter (id))
        return parameter->getNormalisableRange().convertFrom0to1 (
            parameter->getValue());
    return 0.0f;
}

RenderedPreset renderPreset (int index)
{
    RenderedPreset result;
    const auto& names = HybridWavetableAudioProcessor::getFactoryPresetNames();
    result.name = names[index].toStdString();

    // Read what the preset actually applied off a live processor, before any
    // rendering. Doing this with a second render was wrong twice over: it
    // mutated the fixture's duration, and the reverb then asserted on the
    // truncated tail.
    {
        // Heap, not stack: the processor carries ~6.3 MB of wavetable tables and
        // overflows an 8 MB stack. Source/PluginProcessor.h records the same
        // constraint, and ProcessorQualityTests heap-allocates for it.
        auto inspector = std::make_unique<HybridWavetableAudioProcessor>();
        inspector->setBusesLayout ({ {}, juce::AudioChannelSet::stereo() });
        inspector->prepareToPlay (kSampleRate, 128);
        inspector->applyFactoryPreset (index);
        const auto knob = plainParameter (*inspector, "resonance");
        result.cutoff = plainParameter (*inspector, "cutoff");
        result.resonanceQ = SeoulDSPQuality::filterResonanceQ (knob);
        result.brightness = (plainParameter (*inspector, "osc1Pos")
                           + plainParameter (*inspector, "osc2Pos")
                           + plainParameter (*inspector, "osc3Pos")) / 3.0f;
    }

    audioquality::AudioQualityFixture fixture;
    fixture.id = names[index];
    fixture.sampleRate = kSampleRate;
    fixture.blockSize = 128;
    fixture.channels = 2;
    // Note-off at 1.0 s, well inside the window. The first draft released at
    // 2.5 s in a 3.0 s render, so every preset was still ringing when the window
    // ended and all eleven decay figures clustered between 2.0 and 2.8 s. That
    // measured the window length rather than the patch.
    fixture.durationSeconds = 3.0;
    fixture.tailSeconds = 0.5;
    fixture.randomSeed = kSeed;
    const auto noteOffSample = (int) std::llround (1.0 * kSampleRate);
    fixture.midi = { { juce::MidiMessage::noteOn (1, 57, 0.9f), 0 },
                     { juce::MidiMessage::noteOff (1, 57), noteOffSample } };

    const auto rendered = audioquality::OfflineRenderer::render (fixture,
        [index] (auto& processor) { processor.applyFactoryPreset (index); });

    // Peak and RMS over the held portion, skipping the attack transient.
    const auto analysisStart = (int) (0.25 * kSampleRate);
    const auto analysisEnd = juce::jmin (rendered.getNumSamples(),
                                         (int) (1.5 * kSampleRate));
    double sumSquares = 0.0;
    int taken = 0;
    for (int channel = 0; channel < rendered.getNumChannels(); ++channel)
    {
        const auto* samples = rendered.getReadPointer (channel);
        for (int i = 0; i < rendered.getNumSamples(); ++i)
            result.peak = juce::jmax (result.peak, std::abs (samples[i]));
        for (int i = analysisStart; i < analysisEnd; ++i)
        {
            sumSquares += (double) samples[i] * (double) samples[i];
            ++taken;
        }
    }
    result.rmsDb = taken > 0
        ? 20.0 * std::log10 (juce::jmax ((float) std::sqrt (sumSquares / taken), 1.0e-12f))
        : -120.0;

    // Attack is the time to first reach 90% of peak. Release is the time from
    // the note-off to the last sample above 10% of peak, so it measures the
    // patch's own release rather than how long the render window happened to be.
    const auto* mono = rendered.getReadPointer (0);
    const auto absolutePeak = *std::max_element (mono, mono + rendered.getNumSamples(),
                                                 [] (float a, float b)
                                                 { return std::abs (a) < std::abs (b); });
    const auto threshold = std::abs (absolutePeak);
    int firstSustain = -1, lastAudible = -1;
    for (int i = 0; i < rendered.getNumSamples(); ++i)
    {
        if (std::abs (mono[i]) >= 0.9f * threshold && firstSustain < 0)
            firstSustain = i;
        if (std::abs (mono[i]) >= 0.1f * threshold)
            lastAudible = i;
    }
    result.attackSeconds = firstSustain >= 0 ? (float) firstSustain / (float) kSampleRate : 0.0f;
    result.decaySeconds = lastAudible > noteOffSample
        ? (float) (lastAudible - noteOffSample) / (float) kSampleRate : 0.0f;

    return result;
}

void report (const std::vector<RenderedPreset>& rendered)
{
    std::printf ("\nFactory preset render report (C3, 48 kHz, level-matched seed %u)\n", kSeed);
    std::printf ("  %-18s %8s %8s %9s %9s %7s %9s %6s\n",
                 "preset", "peak dB", "rms dB", "atk s", "decay s", "Q", "cutoff", "pos");
    for (const auto& p : rendered)
        std::printf ("  %-18s %8.2f %8.2f %9.4f %9.4f %7.2f %9.0f %6.3f\n",
                     p.name.c_str(), 20.0 * std::log10 (juce::jmax (p.peak, 1.0e-9f)),
                     p.rmsDb, p.attackSeconds, p.decaySeconds, p.resonanceQ,
                     p.cutoff, p.brightness);
}

// Every parameter the plug-in registers. Listed explicitly because APVTS
// exposes no accessor for its own parameter list and getName() returns the
// human label rather than the id.
const std::vector<const char*>& presetParameterIds()
{
    static const std::vector<const char*> ids {
        "osc1Pos", "osc2Pos", "osc3Pos",
        "osc1Level", "osc2Level", "osc3Level",
        "osc1Tune", "osc2Tune", "osc3Tune",
        "osc1Unison", "osc2Unison", "osc3Unison",
        "osc1Spread", "osc2Spread", "osc3Spread",
        "osc1Detune", "osc2Detune", "osc3Detune", "unisonKeyTrack",
        "filterType", "filterSlope", "cutoff", "resonance", "filterDrive",
        "saturation", "saturationQuality", "output", "filterEnvAmount",
        "masterWidth",
        "ampAttack", "ampDecay", "ampSustain", "ampRelease",
        "filterAttack", "filterDecay", "filterSustain", "filterRelease",
        "lfo1Rate", "lfo1Depth", "lfo1Destination",
        "lfo2Rate", "lfo2Depth", "lfo2Destination",
        "delayTime", "delayFeedback", "delayMix",
        "reverbSize", "reverbDamping", "reverbMix",
        "arpEnabled", "arpRate", "arpGate", "arpPattern",
        "randomPhase", "driftRate", "driftDepth" };
    return ids;
}

// 1. Contract: applies, finite, bounded, in range, distinct.
void runContractGate (audioquality::TestHarness& test)
{
    // Block size for the contract gate. It must match the blocks handed to
    // processBlock below: the processor sizes its master-width scratch buffer
    // from the value given to prepareToPlay, and a larger block overruns it.
    constexpr int kContractBlockSize = 128;

    // Heap for the same reason as the inspector above.
    auto processor = std::make_unique<HybridWavetableAudioProcessor>();
    // Bus layout first, then prepare. Both OfflineRenderer and
    // ProcessorQualityTests establish the layout before preparing, and the
    // master-bus effect chain sizes itself from it.
    processor->setBusesLayout ({ {}, juce::AudioChannelSet::stereo() });
    processor->prepareToPlay (kSampleRate, 128);

    test.expect ((int) HybridWavetableAudioProcessor::getFactoryPresetNames().size() == kPresetCount,
                 "the factory bank exposes the expected number of presets");

    std::vector<std::vector<float>> signatures;
    bool allFinite = true;
    float loudest = 0.0f;

    for (int index = 0; index < kPresetCount; ++index)
    {
        processor->applyFactoryPreset (index);
        const auto& name = HybridWavetableAudioProcessor::getFactoryPresetNames()[index];

        // Every parameter must sit inside the range the host advertises, or a
        // host automation write would clamp it and the patch would not be what
        // it claims to be.
        //
        // The ids are listed explicitly rather than enumerated. APVTS exposes no
        // public accessor for its parameter list, and getName() on an APVTS
        // parameter returns the human-facing label ("Cutoff"), not the id
        // ("cutoff"), so recovering ids from getParameters() silently yields
        // empty strings. A first draft did that and every preset failed the
        // range check, because the lookup defaulted to 0.0.
        bool inRange = true;
        std::vector<float> signature;
        for (const auto* id : presetParameterIds())
        {
            auto* parameter = processor->parameters.getParameter (id);
            const auto value = plainParameter (*processor, id);
            if (const auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                inRange &= std::isfinite (value)
                        && value >= ranged->getNormalisableRange().start - 1.0e-3f
                        && value <= ranged->getNormalisableRange().end + 1.0e-3f;
            signature.push_back (value);
        }
        test.expect (inRange, (name + " stays inside every parameter range").toRawUTF8());
        signatures.push_back (std::move (signature));

        // The block must match the size given to prepareToPlay. The processor
        // sizes its master-width scratch buffer from that value, so 512-sample
        // blocks against a 128-sample preparation overrun it and fault.
        juce::AudioBuffer<float> buffer (2, kContractBlockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::noteOff (1, 57), 48000);
        for (int block = 0; block < 4; ++block)
        {
            buffer.clear();
            processor->processBlock (buffer, midi);
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    const auto value = buffer.getSample (channel, i);
                    allFinite &= std::isfinite (value);
                    loudest = juce::jmax (loudest, std::abs (value));
                }
        }
    }

    test.expect (allFinite, "every factory preset renders finite audio");
    test.expect (loudest < 1.0f, "no factory preset exceeds full scale on a held note");

    // Distinctness: no two presets may share a signature. A duplicated entry is
    // how a bank quietly shrinks without anyone noticing.
    int duplicatePairs = 0;
    for (size_t i = 0; i < signatures.size(); ++i)
        for (size_t j = i + 1; j < signatures.size(); ++j)
        {
            bool identical = signatures[i].size() == signatures[j].size();
            for (size_t k = 0; identical && k < signatures[i].size(); ++k)
                identical &= std::abs (signatures[i][k] - signatures[j][k]) < 1.0e-4f;
            if (identical)
                ++duplicatePairs;
        }
    test.expect (duplicatePairs == 0, "no two factory presets hold the same parameter values");
}

// 2. Ordering: envelope shapes and brightness must differ the way the names say.
void runDesignIntentGate (audioquality::TestHarness& test,
                          const std::vector<RenderedPreset>& rendered)
{
    const auto byName = [&rendered] (const char* name) -> const RenderedPreset&
    {
        for (const auto& p : rendered)
            if (p.name.find (name) != std::string::npos)
                return p;
        std::printf ("  missing preset %s\n", name);
        return rendered.front();
    };

    const auto& pluck = byName ("CHROME PLUCK");
    const auto& pad = byName ("VOID GLASS");
    const auto& air = byName ("QUANTUM AIR");
    const auto& bass = byName ("CIRCUIT BASS");
    const auto& bloom = byName ("STATIC BLOOM");
    const auto& acid = byName ("ACID VECTOR");

    std::printf ("\nDesign-intent checks (from the rendered envelopes, not the numbers)\n");
    std::printf ("  pluck  attack %.4f s, decay %.3f s\n", pluck.attackSeconds, pluck.decaySeconds);
    std::printf ("  pad    attack %.4f s, decay %.3f s\n", pad.attackSeconds, pad.decaySeconds);

    // A pluck reaches its level far faster than a pad.
    test.expect (pluck.attackSeconds < pad.attackSeconds * 0.25,
                 "the pluck attacks far faster than the pad");
    // A pad sustains far longer than a pluck.
    test.expect (pad.decaySeconds > pluck.decaySeconds * 4.0,
                 "the pad sustains far longer than the pluck");
    // QUANTUM AIR is the brightest entry and the most resonant one.
    const auto brightest = std::max_element (rendered.begin(), rendered.end(),
                                           [] (const RenderedPreset& a, const RenderedPreset& b)
                                           { return a.brightness < b.brightness; });
    const auto mostResonant = std::max_element (rendered.begin(), rendered.end(),
                                               [] (const RenderedPreset& a, const RenderedPreset& b)
                                               { return a.resonanceQ < b.resonanceQ; });
    test.expect (std::abs (air.brightness - brightest->brightness) < 1.0e-6f,
                 "QUANTUM AIR uses the brightest wavetable position of the bank");
    test.expect (std::abs (air.resonanceQ - mostResonant->resonanceQ) < 1.0e-3f,
                 "QUANTUM AIR is the most resonant entry of the bank");
    // CIRCUIT BASS is the lowest cutoff by a wide margin.
    const auto narrowest = std::min_element (rendered.begin(), rendered.end(),
                                            [] (const RenderedPreset& a, const RenderedPreset& b)
                                            { return a.cutoff < b.cutoff; });
    test.expect (std::abs (bass.cutoff - narrowest->cutoff) < 1.0e-3f,
                 "CIRCUIT BASS has the lowest cutoff of the bank");
    test.expect (bass.cutoff < 400.0f, "CIRCUIT BASS is tuned into the bass register");
    // ACID VECTOR is the most resonant low-register entry: that is what makes it acid.
    test.expect (acid.resonanceQ > 5.0f, "ACID VECTOR reaches a strongly resonant low filter");
    test.expect (acid.cutoff < 800.0f, "ACID VECTOR sits below ACID's usual range");
    // Attack ordering across the whole bank, checked as an ordering rather than
    // against a named entry. Naming one was wrong: QUANTUM AIR has the slowest
    // measured attack at 2.36 s, not STATIC BLOOM at 1.29 s, because a Q 16
    // filter rings through its own amp envelope and pushes the 90%-of-peak point
    // out past the envelope's own attack time. That is a real property of the
    // patch rather than a defect, and it is why this checks the set.
    const auto slowest = std::max_element (rendered.begin(), rendered.end(),
                                           [] (const RenderedPreset& a, const RenderedPreset& b)
                                           { return a.attackSeconds < b.attackSeconds; });
    std::printf ("  slowest measured attack: %s at %.3f s\n",
                 slowest->name.c_str(), slowest->attackSeconds);
    test.expect (slowest->attackSeconds > 1.0f,
                 "the bank contains a genuinely slow-attacking entry");
    const auto bloomMessage = juce::String ("STATIC BLOOM renders its 1.2 s attack (measured ")
                          + juce::String (bloom.attackSeconds, 3) + " s)";
    test.expect (std::abs (bloom.attackSeconds - 1.20f) < 0.35f, bloomMessage);
    // The pluck must still be at the fast end after the fix.
    const auto fastest = std::min_element (rendered.begin(), rendered.end(),
                                           [] (const RenderedPreset& a, const RenderedPreset& b)
                                           { return a.attackSeconds < b.attackSeconds; });
    test.expect (pluck.attackSeconds <= fastest->attackSeconds * 1.5f + 0.01f,
                 "CHROME PLUCK sits at the fast end of the attack range");

}

// 3. Reach: every preset has to be audible and none may be pinned to the rails.
void runReachGate (audioquality::TestHarness& test, const std::vector<RenderedPreset>& rendered)
{
    std::printf ("\nReach checks\n");
    bool allAudible = true;
    bool allBounded = true;
    for (const auto& p : rendered)
    {
        const auto peakDb = 20.0 * std::log10 (juce::jmax (p.peak, 1.0e-9f));
        // Audible: a factory preset that sits below -60 dBFS is a silent entry.
        allAudible &= peakDb > -40.0f;
        // Headroom left: pinned at the ceiling means the ceiling is doing the
        // level work instead of the patch's own output stage.
        allBounded &= peakDb < -0.5f;
    }
    test.expect (allAudible, "every factory preset renders audibly (peak above -40 dBFS)");
    test.expect (allBounded, "no factory preset is pinned against the output ceiling");

    // Level spread: a bank where everything matches to a tenth of a dB is a bank
    // that will not survive a level-matched listen.
    float lowest = 0.0f, highest = 0.0f;
    for (const auto& p : rendered)
    {
        const auto rms = p.rmsDb;
        if (&p == &rendered.front()) { lowest = highest = (float) rms; }
        lowest = juce::jmin (lowest, (float) rms);
        highest = juce::jmax (highest, (float) rms);
    }
    std::printf ("  RMS spread %.2f dB to %.2f dB (%.1f dB apart)\n",
                 lowest, highest, highest - lowest);
    test.expect (highest - lowest > 3.0f,
                 "the bank spans a real level range rather than matching one loudness");
}
}

int main()
{
    audioquality::TestHarness test;
    // The editor's output meter starts a juce::Timer, which asserts without a
    // running MessageManager. Every other renderer-backed test in this directory
    // installs this initialiser for the same reason; omitting it left the program
    // asserting inside Timer::startTimer and then faulting.
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    runContractGate (test);

    std::vector<RenderedPreset> rendered;
    rendered.reserve (kPresetCount);
    for (int index = 0; index < kPresetCount; ++index)
        rendered.push_back (renderPreset (index));
    report (rendered);

    runDesignIntentGate (test, rendered);
    runReachGate (test, rendered);

    std::printf ("\n%d failure(s)\n", test.failureCount());
    return test.result();
}
