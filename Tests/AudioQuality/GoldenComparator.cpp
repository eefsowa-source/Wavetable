#include "GoldenComparator.h"

#include "Metrics.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace audioquality
{
namespace
{
constexpr double floorPower = 1.0e-30;

std::vector<float> monoSamples (const juce::AudioBuffer<float>& buffer)
{
    const auto count = buffer.getNumSamples();
    std::vector<float> samples ((size_t) count, 0.0f);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < count; ++sample)
            samples[(size_t) sample] += buffer.getSample (channel, sample) / (float) juce::jmax (1, buffer.getNumChannels());
    return samples;
}

std::vector<double> spectrum (const std::vector<float>& input)
{
    int fftSize = 1;
    while (fftSize < (int) input.size() && fftSize < 65536)
        fftSize <<= 1;
    fftSize = juce::jmax (1024, fftSize);
    juce::dsp::FFT fft ((int) std::round (std::log2 ((double) fftSize)));
    std::vector<float> data ((size_t) fftSize * 2u, 0.0f);
    const auto used = juce::jmin ((int) input.size(), fftSize);
    for (int i = 0; i < used; ++i)
        data[(size_t) i] = input[(size_t) i] * (float) (0.5 * (1.0 - std::cos (2.0 * juce::MathConstants<double>::pi * i / (double) juce::jmax (1, used - 1))));
    fft.performRealOnlyForwardTransform (data.data());
    std::vector<double> result ((size_t) fftSize / 2u + 1u, 0.0);
    for (size_t k = 0; k < result.size(); ++k)
    {
        const auto re = (double) data[2 * k];
        const auto im = (k == 0 || k == result.size() - 1) ? 0.0 : (double) data[2 * k + 1];
        result[k] = std::sqrt (re * re + im * im);
    }
    return result;
}

double percentile (std::vector<double> values, double fraction)
{
    if (values.empty())
        return 0.0;
    std::sort (values.begin(), values.end());
    const auto index = (size_t) juce::jlimit (0, (int) values.size() - 1,
                                               (int) std::llround (fraction * (values.size() - 1)));
    return values[index];
}
}

GoldenComparison compareWithGolden (const juce::AudioBuffer<float>& candidate,
                                    const juce::AudioBuffer<float>& golden,
                                    double sampleRate,
                                    int maximumAlignmentSamples)
{
    GoldenComparison result;
    result.identityMatches = candidate.getNumChannels() == golden.getNumChannels()
                           && candidate.getNumSamples() == golden.getNumSamples();
    if (result.identityMatches)
        for (int channel = 0; channel < candidate.getNumChannels() && result.identityMatches; ++channel)
            for (int sample = 0; sample < candidate.getNumSamples(); ++sample)
                if (candidate.getSample (channel, sample) != golden.getSample (channel, sample))
                {
                    result.identityMatches = false;
                    break;
                }

    const auto count = juce::jmin (candidate.getNumSamples(), golden.getNumSamples());
    const auto channels = juce::jmin (candidate.getNumChannels(), golden.getNumChannels());
    const auto maxLag = juce::jmax (0, maximumAlignmentSamples);
    double bestCorrelation = -std::numeric_limits<double>::infinity();
    int bestLag = 0;
    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        const auto first = juce::jmax (0, -lag);
        const auto last = juce::jmin (count, count - lag);
        double dot = 0.0, candidateEnergy = 0.0, goldenEnergy = 0.0;
        for (int sample = first; sample < last; ++sample)
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto a = (double) candidate.getSample (channel, sample + lag);
                const auto b = (double) golden.getSample (channel, sample);
                dot += a * b;
                candidateEnergy += a * a;
                goldenEnergy += b * b;
            }
        const auto denominator = std::sqrt (candidateEnergy * goldenEnergy);
        const auto correlation = denominator > floorPower ? dot / denominator : 0.0;
        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }
    result.alignmentSamples = bestLag;
    const auto first = juce::jmax (0, -bestLag);
    const auto last = juce::jmin (count, count - bestLag);
    double squaredError = 0.0;
    int compared = 0;
    for (int sample = first; sample < last; ++sample)
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto difference = (double) candidate.getSample (channel, sample + bestLag)
                                  - (double) golden.getSample (channel, sample);
            squaredError += difference * difference;
            ++compared;
        }
    result.alignedErrorDbFS = compared > 0 && squaredError > floorPower
                            ? 10.0 * std::log10 (squaredError / (double) compared)
                            : -std::numeric_limits<double>::infinity();

    const auto candidateMetrics = measureAudio (candidate, sampleRate);
    const auto goldenMetrics = measureAudio (golden, sampleRate);
    result.loudnessDelta = std::abs (candidateMetrics.integratedLufs - goldenMetrics.integratedLufs);
    result.truePeakDeltaDb = std::abs (candidateMetrics.truePeakDbTP - goldenMetrics.truePeakDbTP);

    if (sampleRate > 0.0 && count > 0)
    {
        const auto candidateSpectrum = spectrum (monoSamples (candidate));
        const auto goldenSpectrum = spectrum (monoSamples (golden));
        const auto fftSize = juce::jmax (1, ((int) candidateSpectrum.size() - 1) * 2);
        const auto maxFrequency = juce::jmin (18000.0, 0.45 * sampleRate);
        std::vector<double> deltas;
        for (double centre = 40.0; centre <= maxFrequency; centre *= std::pow (2.0, 1.0 / 6.0))
        {
            const auto low = centre / std::pow (2.0, 1.0 / 12.0);
            const auto high = centre * std::pow (2.0, 1.0 / 12.0);
            const auto firstBin = juce::jmax (1, (int) std::floor (low * fftSize / sampleRate));
            const auto lastBin = juce::jmin ((int) candidateSpectrum.size() - 1,
                                             (int) std::ceil (high * fftSize / sampleRate));
            double candidatePower = 0.0, goldenPower = 0.0;
            for (int bin = firstBin; bin <= lastBin; ++bin)
            {
                candidatePower += candidateSpectrum[(size_t) bin];
                goldenPower += goldenSpectrum[(size_t) bin];
            }
            const auto candidateDb = 20.0 * std::log10 (std::max (candidatePower, floorPower));
            const auto goldenDb = 20.0 * std::log10 (std::max (goldenPower, floorPower));
            deltas.push_back (std::abs (candidateDb - goldenDb));
        }
        result.spectralMedianDeltaDb = percentile (deltas, 0.5);
        result.spectralP95DeltaDb = percentile (deltas, 0.95);
    }
    return result;
}
}
