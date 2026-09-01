#include "../Source/DSP/WavetableOscillator.h"
#include <iostream>
#include <juce_dsp/juce_dsp.h>

static bool expect (bool condition, const char* message)
{
    if (! condition) std::cerr << "FAIL: " << message << "\n";
    return condition;
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
