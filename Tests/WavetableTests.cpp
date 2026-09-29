#include "../Source/DSP/WavetableOscillator.h"
#include <iostream>
#include <juce_dsp/juce_dsp.h>

namespace
{
// Power spectrum of one frame, in dB relative to the strongest partial.
// The table is periodic over tableSize samples, so partial h sits exactly on
// FFT bin h and no window is needed (an integer number of cycles per window).
std::array<double, WavetableData::tableSize / 2> frameSpectrumDb (
    const std::array<float, WavetableData::tableSize>& frame)
{
    std::array<float, WavetableData::tableSize * 2> buffer {};
    std::copy (frame.begin(), frame.end(), buffer.begin());
    juce::dsp::FFT fft (11); // 2048-point
    fft.performRealOnlyForwardTransform (buffer.data());

    std::array<double, WavetableData::tableSize / 2> spectrum {};
    double strongest = 0.0;
    for (int bin = 1; bin < (int) spectrum.size(); ++bin)
    {
        // Real-only layout stores bin k as (re, im) at indices 2k, 2k+1.
        const auto real = buffer[(size_t) (2 * bin)];
        const auto imag = buffer[(size_t) (2 * bin + 1)];
        const auto magnitude = std::sqrt (real * real + imag * imag);
        spectrum[(size_t) bin] = magnitude * magnitude;
        strongest = juce::jmax (strongest, spectrum[(size_t) bin]);
    }
    for (auto& bin : spectrum)
        bin = 10.0 * std::log10 (bin / strongest + 1.0e-30);
    return spectrum;
}
}

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

    // Spectral-content gate. The previous default bank was
    // sin(x)*(1-0.35m) + 0.25*sin(2x)*m, i.e. exactly two partials, which made
    // the shipped patch a near-sine. Against the Serum/Vital reference stems on
    // the same C4 fixture that measured a 264 Hz centroid where the references
    // measure 1418 Hz. These gates pin the content that closed that gap so it
    // cannot silently return; see docs/quality/d1-reference-spectrum-gap.md.
    {
        // Frame 7 is the centre of the bank. Measured: H1 0, H2 -6.5, H4 -15.2,
        // H8 -23.9, H16 -32.5 dB. That is a real harmonic series, where the old
        // two-partial table measured H3 and above at the -180 dB floor.
        // Self-calibrating floor: a pure sine has, by definition, no H8 or H16.
        // Measuring it puts this float32 transform's numerical floor on the same
        // footing as the bank, so the per-frame gates assert real harmonic
        // content instead of an arbitrary absolute dB.
        std::array<float, WavetableData::tableSize> pureSine {};
        for (int i = 0; i < WavetableData::tableSize; ++i)
            pureSine[(size_t) i] = std::sin (juce::MathConstants<float>::twoPi
                                             * (float) i / (float) WavetableData::tableSize);
        const auto sineFloor = frameSpectrumDb (pureSine);
        const auto centre = frameSpectrumDb (table.frames[7]);
        ok &= expect (centre[16] < -25.0,
                      "default bank frame 7 carries real energy out to the 16th partial");
        ok &= expect (centre[2] < -3.0,
                      "default bank frame 7 is not a single-partial wave");

        // Every frame must carry a harmonic series rather than being a lone
        // sine. The bank deliberately spans soft (frame 0, H8 -47.0 dB) to
        // bright (frame 15, H8 -8.6 dB), so no single absolute threshold fits
        // the range; each frame must instead clear the measured sine floor by
        // 20 dB. This is the regression that started here, where H3 and above
        // sat on the numerical floor for every frame.
        for (int frame = 0; frame < WavetableData::numTables; ++frame)
        {
            const auto spectrum = frameSpectrumDb (table.frames[(size_t) frame]);
            ok &= expect (spectrum[8] > sineFloor[8] + 20.0,
                          "every default bank frame carries energy past the 8th partial");
            ok &= expect (spectrum[16] > sineFloor[16] + 20.0,
                          "every default bank frame carries energy past the 16th partial");
        }

        // Anchored to the measured ends of the bank with stated margin: frame 7
        // measures H16 -32.5 dB, frame 15 measures H16 -12.8 dB.
        ok &= expect (frameSpectrumDb (table.frames[15])[16] < -10.0,
                      "the brightest bank frame reaches well past the 16th partial");
        ok &= expect (frameSpectrumDb (table.frames[7])[16] < -25.0,
                      "the middle bank frame reaches past the 16th partial");

        // The morph axis has to actually sweep spectrum rather than repeat one
        // waveform: the bright end must carry more upper-harmonic energy than
        // the soft end. Measured H16 is -62.6 dB at frame 0 and -12.8 dB at
        // frame 15, so the bank spans about 50 dB at that partial.
        ok &= expect (frameSpectrumDb (table.frames[15])[16]
                          > frameSpectrumDb (table.frames[0])[16] + 30.0,
                      "the default bank sweeps from a soft wave to a bright one");

        // Peak-normalised bank: the position knob must be a timbre control and
        // not a level control, so all 16 frames share one peak.
        float lowestPeak = 1.0f, highestPeak = 0.0f;
        for (const auto& frame : table.frames)
        {
            float peak = 0.0f;
            for (const auto sample : frame)
                peak = juce::jmax (peak, std::abs (sample));
            lowestPeak = juce::jmin (lowestPeak, peak);
            highestPeak = juce::jmax (highestPeak, peak);
        }
        ok &= expect (highestPeak > 0.99f && highestPeak <= 1.0f,
                      "default bank frames are peak normalised to unity");
        ok &= expect (highestPeak - lowestPeak < 1.0e-4f,
                      "all default bank frames share the same peak level");

        // The bank must sweep spectrum rather than repeat one waveform, and it
        // must stay DC-free: a DC offset would be re-introduced at every note.
        const auto first = frameSpectrumDb (table.frames[0]);
        double bestMatch = 0.0;
        for (int frame = 0; frame < WavetableData::numTables; ++frame)
        {
            const auto other = frameSpectrumDb (table.frames[(size_t) frame]);
            double difference = 0.0;
            for (int bin = 1; bin < 64; ++bin)
                difference += std::abs (first[(size_t) bin] - other[(size_t) bin]);
            bestMatch = juce::jmax (bestMatch, difference);
        }
        ok &= expect (bestMatch > 20.0,
                      "default bank sweeps distinct spectra across its frames");

        for (int frame = 0; frame < WavetableData::numTables; ++frame)
        {
            double mean = 0.0;
            for (const auto sample : table.frames[(size_t) frame])
                mean += sample;
            mean /= (double) WavetableData::tableSize;
            ok &= expect (std::abs (mean) < 1.0e-5,
                          "default bank frames carry no DC offset");
        }
    }

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
