#include "../Source/DSP/WavetableOscillator.h"
#include <iostream>
#include <juce_dsp/juce_dsp.h>

static bool expect (bool condition, const char* message)
{
    if (! condition) std::cerr << "FAIL: " << message << "\n";
    return condition;
}

static float maximumMipBoundaryJump (WavetableData& table, int harmonicCap)
{
    constexpr float sampleRate = 48000.0f;
    const auto boundaryFrequency = sampleRate / (2.0f * (float) harmonicCap);
    float maximumJump = 0.0f;

    for (int phaseIndex = 0; phaseIndex < 2048; ++phaseIndex)
    {
        const auto phase = (float) phaseIndex / 2048.0f;
        WavetableOscillator below;
        below.prepare (sampleRate);
        below.setPhase (phase);
        below.setFrequency (boundaryFrequency * 0.9999f);

        WavetableOscillator above;
        above.prepare (sampleRate);
        above.setPhase (phase);
        above.setFrequency (boundaryFrequency * 1.0001f);

        maximumJump = juce::jmax (maximumJump,
                                  std::abs (below.process (table) - above.process (table)));
    }

    return maximumJump;
}

int main()
{
    bool ok = true;
    WavetableData table;
    ok &= expect (std::abs (table.frames[0][0]) < 0.01f, "default table starts at zero crossing");
    ok &= expect (std::isfinite (table.frames[7][123]), "default table contains finite samples");

    WavetableOscillator osc;
    osc.prepare (48000.0); osc.setFrequency (440.0f); osc.setPosition (0.0f);
    float first = osc.process (table);
    osc.setPosition (1.0f);
    float last = osc.process (table);
    ok &= expect (std::isfinite (first) && std::isfinite (last), "oscillator output is finite");
    ok &= expect (first != last, "wavetable position changes the waveform");
    ok &= expect (WavetableData::mipHarmonicCaps.back() == 1,
                  "safest wavetable mip contains only the fundamental");

    WavetableOscillator aboveNyquist;
    aboveNyquist.prepare (48000.0);
    aboveNyquist.setFrequency (25000.0f);
    float aboveNyquistPeak = 0.0f;
    for (int sample = 0; sample < 128; ++sample)
        aboveNyquistPeak = juce::jmax (aboveNyquistPeak,
                                      std::abs (aboveNyquist.process (table)));
    ok &= expect (aboveNyquistPeak == 0.0f,
                  "oscillator suppresses a fundamental above Nyquist");

    bool everySelectedMipIsSafe = true;
    for (float frequency = 20.0f; frequency < 24000.0f; frequency *= 1.01f)
    {
        const auto increment = frequency / 48000.0f;
        const auto selection = table.selectMipLevels (increment);
        const auto maxSafeHarmonic = 0.5f / increment;
        everySelectedMipIsSafe &= (float) WavetableData::mipHarmonicCaps[(size_t) selection.detailedLevel]
                                  <= maxSafeHarmonic + 1.0e-4f;
        everySelectedMipIsSafe &= (float) WavetableData::mipHarmonicCaps[(size_t) selection.saferLevel]
                                  <= maxSafeHarmonic + 1.0e-4f;
    }
    ok &= expect (everySelectedMipIsSafe,
                  "every audible oscillator frequency selects only Nyquist-safe mips");

    juce::AudioBuffer<float> input (1, WavetableData::tableSize * WavetableData::numTables);
    for (int i = 0; i < input.getNumSamples(); ++i)
    {
        const auto phase = juce::MathConstants<float>::twoPi * i / (float) WavetableData::tableSize;
        input.setSample (0, i, std::sin (phase) * (0.5f + 0.03f * (float) (i / WavetableData::tableSize)));
    }
    table.loadFromAudio (input);
    ok &= expect (std::abs (table.frames[0][512] - 0.5f) < 0.01f, "audio import resamples into the table");
    ok &= expect (std::abs (table.frames[0][512] - table.frames[15][512]) > 0.1f, "audio import preserves frame movement");

    table.frames[0][0] = 0.37f;
    juce::AudioBuffer<float> empty (0, 16);
    table.loadFromAudio (empty);
    ok &= expect (std::abs (table.frames[0][0] - 0.37f) < 0.001f,
                  "empty audio input leaves the wavetable unchanged");

    // Rich spectra expose hard mip switches as a sudden timbre step when a
    // pitch bend crosses a harmonic-cap boundary. Frequencies immediately
    // either side of a boundary should therefore produce nearly identical
    // samples when evaluated at the same phase.
    {
        WavetableData richTable;
        for (int frame = 0; frame < WavetableData::numTables; ++frame)
            for (int sample = 0; sample < WavetableData::tableSize; ++sample)
            {
                const auto phase = juce::MathConstants<float>::twoPi
                                   * (float) sample / (float) WavetableData::tableSize;
                float value = 0.0f;
                for (int harmonic = 1; harmonic <= 64; ++harmonic)
                    value += std::sin (phase * (float) harmonic) / (float) harmonic;
                richTable.frames[(size_t) frame][(size_t) sample] = value * 0.45f;
            }
        richTable.regenerateMips();

        float worstBoundaryJump = 0.0f;
        for (const int cap : { 64, 32, 16, 8, 4, 2 })
            worstBoundaryJump = juce::jmax (worstBoundaryJump,
                                            maximumMipBoundaryJump (richTable, cap));

        ok &= expect (worstBoundaryJump < 0.01f,
                      "adjacent wavetable mip levels crossfade without a boundary jump");
    }

    // Anti-aliasing regression: a high note's rendered spectrum must not
    // carry meaningful energy above Nyquist/2, which is only possible if the
    // oscillator is selecting a band-limited mip level instead of the raw
    // (unfiltered) table for high playback rates.
    {
        constexpr double sr = 48000.0;
        constexpr int fftOrder = 13; // 8192-point FFT
        constexpr int fftSize = 1 << fftOrder;
        WavetableData highTable;
        WavetableOscillator highOsc;
        highOsc.prepare (sr);
        highOsc.setFrequency (6000.0f); // well above the safe range for a raw 2048-sample table
        highOsc.setPosition (0.0f);

        std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = highOsc.process (highTable);

        juce::dsp::FFT fft (fftOrder);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const int nyquistBin = fftSize / 2;
        const int halfNyquistBin = nyquistBin / 2;
        float bandEnergy = 0.0f, aboveHalfNyquistEnergy = 0.0f;
        for (int bin = 1; bin <= nyquistBin; ++bin)
        {
            bandEnergy += fftData[(size_t) bin];
            if (bin > halfNyquistBin)
                aboveHalfNyquistEnergy += fftData[(size_t) bin];
        }
        const float aboveHalfNyquistRatio = bandEnergy > 0.0f ? aboveHalfNyquistEnergy / bandEnergy : 0.0f;
        ok &= expect (aboveHalfNyquistRatio < 0.02f,
                      "high note selects a band-limited mip, leaving negligible energy above Nyquist/2");
    }

    std::cout << (ok ? "Wavetable DSP tests passed\n" : "Wavetable DSP tests failed\n");
    return ok ? 0 : 1;
}
