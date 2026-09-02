#include "Metrics.h"

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace audioquality
{
namespace
{
constexpr double floorPower = 1.0e-30;

double toDb (double value)
{
    if (! std::isfinite (value) || value <= floorPower)
        return -std::numeric_limits<double>::infinity();
    return 20.0 * std::log10 (value);
}

double nextPowerOfTwoAtMost (int count)
{
    if (count <= 0)
        return 0;
    int n = 1;
    while (n <= count / 2 && n < 65536)
        n <<= 1;
    return n;
}

struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    double process (double x) noexcept
    {
        const auto y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

Biquad makeHighPass (double sampleRate)
{
    // 60 Hz, Q=0.5.  This is the BS.1770 high-pass corner and remains
    // numerically well behaved at both supported audit rates.
    const auto w0 = 2.0 * juce::MathConstants<double>::pi * 60.0 / sampleRate;
    const auto c = std::cos (w0);
    const auto s = std::sin (w0);
    const auto alpha = s / (2.0 * 0.5);
    const auto a0 = 1.0 + alpha;
    return { (1.0 + c) / (2.0 * a0), -(1.0 + c) / a0,
             (1.0 + c) / (2.0 * a0), -2.0 * c / a0, (1.0 - alpha) / a0 };
}

Biquad makeHighShelf (double sampleRate)
{
    // +4 dB shelf at 4 kHz, the second K-weighting section.
    const auto A = std::pow (10.0, 4.0 / 40.0);
    const auto w0 = 2.0 * juce::MathConstants<double>::pi * 4000.0 / sampleRate;
    const auto c = std::cos (w0);
    const auto s = std::sin (w0);
    const auto alpha = s / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / 0.9 - 1.0) + 2.0);
    const auto beta = 2.0 * std::sqrt (A) * alpha;
    const auto a0 = (A + 1.0) - (A - 1.0) * c + beta;
    return { A * ((A + 1.0) + (A - 1.0) * c + beta) / a0,
             -2.0 * A * ((A - 1.0) + (A + 1.0) * c) / a0,
             A * ((A + 1.0) + (A - 1.0) * c - beta) / a0,
             2.0 * ((A - 1.0) - (A + 1.0) * c) / a0,
             ((A + 1.0) - (A - 1.0) * c - beta) / a0 };
}

void fftMagnitude (const float* samples, int count, double sampleRate,
                   std::vector<double>& magnitudes, int& fftSize)
{
    fftSize = (int) nextPowerOfTwoAtMost (count);
    if (fftSize < 1024)
    {
        fftSize = 1;
        while (fftSize < std::max (count, 1))
            fftSize <<= 1;
        fftSize = juce::jmin (fftSize, 65536);
    }

    const auto order = (int) std::round (std::log2 ((double) fftSize));
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) fftSize * 2u, 0.0f);
    const auto used = juce::jmin (count, fftSize);
    for (int i = 0; i < used; ++i)
    {
        const auto phase = (double) i / (double) juce::jmax (1, used - 1);
        const auto window = 0.5 * (1.0 - std::cos (2.0 * juce::MathConstants<double>::pi * phase));
        data[(size_t) i] = std::isfinite (samples[i]) ? samples[i] * (float) window : 0.0f;
    }
    fft.performRealOnlyForwardTransform (data.data());
    const auto bins = fftSize / 2 + 1;
    magnitudes.assign ((size_t) bins, 0.0);
    for (int k = 0; k < bins; ++k)
    {
        const auto re = (double) data[(size_t) 2 * (size_t) k];
        const auto im = (k == 0 || k == fftSize / 2) ? 0.0 : (double) data[(size_t) 2 * (size_t) k + 1];
        magnitudes[(size_t) k] = re * re + im * im;
    }
    (void) sampleRate;
}
}

AudioMetrics measureAudio (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    AudioMetrics result;
    const auto channels = buffer.getNumChannels();
    const auto samples = buffer.getNumSamples();
    if (channels <= 0 || samples <= 0)
        return result;

    double maxAbs = 0.0;
    double sumSquares = 0.0;
    double dcMax = 0.0;
    for (int channel = 0; channel < channels; ++channel)
    {
        double sum = 0.0;
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto value = (double) buffer.getSample (channel, sample);
            if (! std::isfinite (value))
                result.finite = false;
            const auto safe = std::isfinite (value) ? value : 0.0;
            maxAbs = std::max (maxAbs, std::abs (safe));
            sumSquares += safe * safe;
            sum += safe;
        }
        dcMax = std::max (dcMax, std::abs (sum / (double) samples));
    }

    result.samplePeakDbFS = toDb (maxAbs);
    result.rmsDbFS = toDb (std::sqrt (sumSquares / (double) (channels * samples)));
    result.dcDbFS = toDb (dcMax);

    double truePeak = maxAbs;
    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample + 1 < samples; ++sample)
        {
            const auto a = (double) buffer.getSample (channel, sample);
            const auto b = (double) buffer.getSample (channel, sample + 1);
            if (! std::isfinite (a) || ! std::isfinite (b))
                continue;
            for (int phase = 1; phase < 4; ++phase)
                truePeak = std::max (truePeak, std::abs (a + (b - a) * (double) phase / 4.0));
        }
    result.truePeakDbTP = toDb (truePeak);

    // Offline BS.1770-style integrated loudness.  The two cascaded filters
    // are reset per channel and use 400 ms blocks with 75% overlap.
    if (sampleRate > 0.0)
    {
        const auto blockSize = juce::jmax (1, (int) std::llround (0.400 * sampleRate));
        const auto hop = juce::jmax (1, blockSize / 4);
        std::vector<double> weighted ((size_t) samples * (size_t) channels, 0.0);
        for (int channel = 0; channel < channels; ++channel)
        {
            auto hp = makeHighPass (sampleRate);
            auto shelf = makeHighShelf (sampleRate);
            for (int sample = 0; sample < samples; ++sample)
                weighted[(size_t) sample * (size_t) channels + (size_t) channel]
                    = shelf.process (hp.process ((double) buffer.getSample (channel, sample)));
        }

        std::vector<double> blockLoudness;
        for (int start = 0; start + blockSize <= samples; start += hop)
        {
            double energy = 0.0;
            for (int i = 0; i < blockSize; ++i)
                for (int channel = 0; channel < channels; ++channel)
                {
                    const auto value = weighted[(size_t) (start + i) * (size_t) channels + (size_t) channel];
                    energy += value * value;
                }
            energy /= (double) (blockSize * channels);
            const auto lufs = -0.691 + 10.0 * std::log10 (std::max (energy, floorPower));
            if (lufs > -70.0)
                blockLoudness.push_back (lufs);
        }
        if (! blockLoudness.empty())
        {
            const auto meanEnergy = std::accumulate (blockLoudness.begin(), blockLoudness.end(), 0.0,
                                                     [] (double, double lufs) { return std::pow (10.0, (lufs + 0.691) / 10.0); });
            const auto ungated = -0.691 + 10.0 * std::log10 (std::max (meanEnergy / blockLoudness.size(), floorPower));
            const auto relativeGate = ungated - 10.0;
            double gatedEnergy = 0.0;
            int gatedCount = 0;
            for (const auto lufs : blockLoudness)
                if (lufs >= relativeGate)
                {
                    gatedEnergy += std::pow (10.0, (lufs + 0.691) / 10.0);
                    ++gatedCount;
                }
            if (gatedCount > 0)
                result.integratedLufs = -0.691 + 10.0 * std::log10 (std::max (gatedEnergy / gatedCount, floorPower));
        }
    }
    return result;
}

double estimateFundamentalHz (const float* samples, int count, double sampleRate,
                              double minimumHz, double maximumHz)
{
    if (samples == nullptr || count <= 0 || sampleRate <= 0.0 || minimumHz >= maximumHz)
        return 0.0;
    std::vector<double> magnitudes;
    int fftSize = 0;
    fftMagnitude (samples, count, sampleRate, magnitudes, fftSize);
    const auto minBin = juce::jlimit (1, fftSize / 2 - 1, (int) std::ceil (minimumHz * fftSize / sampleRate));
    const auto maxBin = juce::jlimit (minBin, fftSize / 2 - 1, (int) std::floor (maximumHz * fftSize / sampleRate));
    auto best = minBin;
    for (int bin = minBin + 1; bin <= maxBin; ++bin)
        if (magnitudes[(size_t) bin] > magnitudes[(size_t) best])
            best = bin;
    double offset = 0.0;
    if (best > 0 && best + 1 < (int) magnitudes.size())
    {
        const auto left = std::log (std::max (magnitudes[(size_t) best - 1], floorPower));
        const auto centre = std::log (std::max (magnitudes[(size_t) best], floorPower));
        const auto right = std::log (std::max (magnitudes[(size_t) best + 1], floorPower));
        const auto denominator = left - 2.0 * centre + right;
        if (std::abs (denominator) > floorPower)
            offset = 0.5 * (left - right) / denominator;
    }
    return (best + offset) * sampleRate / (double) fftSize;
}

double centsError (double measuredHz, double expectedHz)
{
    if (measuredHz <= 0.0 || expectedHz <= 0.0)
        return std::numeric_limits<double>::infinity();
    return 1200.0 * std::log2 (measuredHz / expectedHz);
}

double measureInharmonicAliasDbc (const float* samples, int count, double sampleRate,
                                  double fundamentalHz, int maximumExpectedHarmonic)
{
    if (samples == nullptr || count <= 0 || sampleRate <= 0.0 || fundamentalHz <= 0.0)
        return -std::numeric_limits<double>::infinity();
    std::vector<double> magnitudes;
    int fftSize = 0;
    fftMagnitude (samples, count, sampleRate, magnitudes, fftSize);
    const auto bins = (int) magnitudes.size();
    std::vector<bool> legal ((size_t) bins, false);
    const auto binWidth = sampleRate / (double) fftSize;
    for (int harmonic = 1; harmonic <= maximumExpectedHarmonic; ++harmonic)
    {
        const auto frequency = fundamentalHz * harmonic;
        if (frequency >= sampleRate * 0.5)
            break;
        const auto centre = frequency / binWidth;
        const auto first = juce::jmax (1, (int) std::ceil (centre - 1.5));
        const auto last = juce::jmin (bins - 1, (int) std::floor (centre + 1.5));
        for (int bin = first; bin <= last; ++bin)
            legal[(size_t) bin] = true;
    }
    double legalPower = 0.0;
    double inharmonicPower = 0.0;
    for (int bin = 1; bin < bins; ++bin)
    {
        if (legal[(size_t) bin])
            legalPower += magnitudes[(size_t) bin];
        else
            inharmonicPower += magnitudes[(size_t) bin];
    }
    if (legalPower <= floorPower)
        return -std::numeric_limits<double>::infinity();
    return 10.0 * std::log10 (std::max (inharmonicPower, floorPower) / legalPower);
}
}
