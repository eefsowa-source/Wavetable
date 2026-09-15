#include "WavetableImporter.h"
#include "WavetableOscillator.h"

#include <cmath>
#include <limits>

namespace
{
double sinc (double x) noexcept
{
    if (std::abs (x) < 1.0e-12)
        return 1.0;
    const auto radians = juce::MathConstants<double>::pi * x;
    return std::sin (radians) / radians;
}

int wrappedIndex (int index, int size) noexcept
{
    index %= size;
    return index < 0 ? index + size : index;
}

void resampleCyclic (const float* source, int sourceSize,
                     std::array<float, WavetableData::tableSize>& destination)
{
    constexpr double lobes = 16.0;
    const auto speedRatio = (double) sourceSize / (double) WavetableData::tableSize;
    const auto cutoff = juce::jmin (1.0, 1.0 / speedRatio);
    const auto support = (int) std::ceil (lobes / cutoff);

    for (int output = 0; output < WavetableData::tableSize; ++output)
    {
        const auto sourcePosition = (double) output * speedRatio;
        const auto centre = (int) std::floor (sourcePosition);
        double weighted = 0.0;
        double weightSum = 0.0;

        for (int offset = -support; offset <= support; ++offset)
        {
            const auto inputIndex = centre + offset;
            const auto distance = sourcePosition - (double) inputIndex;
            const auto weight = cutoff * sinc (distance * cutoff)
                                * sinc (distance * cutoff / lobes);
            weighted += (double) source[wrappedIndex (inputIndex, sourceSize)] * weight;
            weightSum += weight;
        }

        destination[(size_t) output] = (float) (weighted / juce::jmax (weightSum, 1.0e-12));
    }
}

void removeMean (std::array<float, WavetableData::tableSize>& frame) noexcept
{
    double sum = 0.0;
    for (const auto sample : frame)
        sum += sample;
    const auto average = (float) (sum / (double) frame.size());
    for (auto& sample : frame)
        sample -= average;
}

int bestCyclicLag (const std::array<float, WavetableData::tableSize>& reference,
                   const std::array<float, WavetableData::tableSize>& candidate) noexcept
{
    constexpr int fftOrder = 11;
    using Complex = juce::dsp::Complex<float>;
    std::array<Complex, WavetableData::tableSize> referenceInput {};
    std::array<Complex, WavetableData::tableSize> candidateInput {};
    std::array<Complex, WavetableData::tableSize> referenceSpectrum {};
    std::array<Complex, WavetableData::tableSize> candidateSpectrum {};
    std::array<Complex, WavetableData::tableSize> crossSpectrum {};
    std::array<Complex, WavetableData::tableSize> correlation {};
    for (int sample = 0; sample < WavetableData::tableSize; ++sample)
    {
        referenceInput[(size_t) sample] = { reference[(size_t) sample], 0.0f };
        candidateInput[(size_t) sample] = { candidate[(size_t) sample], 0.0f };
    }

    juce::dsp::FFT fft (fftOrder);
    fft.perform (referenceInput.data(), referenceSpectrum.data(), false);
    fft.perform (candidateInput.data(), candidateSpectrum.data(), false);
    for (int bin = 0; bin < WavetableData::tableSize; ++bin)
        crossSpectrum[(size_t) bin] = std::conj (referenceSpectrum[(size_t) bin])
                                     * candidateSpectrum[(size_t) bin];
    fft.perform (crossSpectrum.data(), correlation.data(), true);

    double bestCorrelation = -std::numeric_limits<double>::infinity();
    int bestLag = 0;
    for (int lag = 0; lag < WavetableData::tableSize; ++lag)
    {
        const auto value = (double) correlation[(size_t) lag].real();
        if (value > bestCorrelation)
        {
            bestCorrelation = value;
            bestLag = lag;
        }
    }
    return bestLag;
}

void rotateBy (std::array<float, WavetableData::tableSize>& frame, int lag) noexcept
{
    const auto original = frame;
    for (int sample = 0; sample < WavetableData::tableSize; ++sample)
        frame[(size_t) sample] = original[(size_t) wrappedIndex (sample + lag,
                                                                 WavetableData::tableSize)];
}
}

WavetableData WavetableImporter::import (const juce::AudioBuffer<float>& source,
                                         const WavetableImportOptions& options)
{
    WavetableData result (WavetableData::EmptyTag {});
    if (source.getNumChannels() == 0 || source.getNumSamples() == 0)
        return result;

    const bool hasMultipleFrames = source.getNumSamples()
                                   >= WavetableData::tableSize * WavetableData::numTables;
    const auto sourceFrameSize = hasMultipleFrames
                                     ? source.getNumSamples() / WavetableData::numTables
                                     : source.getNumSamples();

    for (int frame = 0; frame < WavetableData::numTables; ++frame)
    {
        const auto sourceFrame = hasMultipleFrames ? frame : 0;
        resampleCyclic (source.getReadPointer (0, sourceFrame * sourceFrameSize),
                        sourceFrameSize, result.frames[(size_t) frame]);
        if (options.removeDc)
            removeMean (result.frames[(size_t) frame]);
    }

    if (options.alignCyclicPhase && hasMultipleFrames)
        for (int frame = 1; frame < WavetableData::numTables; ++frame)
            rotateBy (result.frames[(size_t) frame],
                      bestCyclicLag (result.frames[0], result.frames[(size_t) frame]));

    if (options.normaliseEachFrame)
        for (auto& frame : result.frames)
        {
            float peak = 0.0f;
            for (const auto sample : frame)
                peak = juce::jmax (peak, std::abs (sample));
            if (peak > 0.0f)
                for (auto& sample : frame)
                    sample *= options.bankPeakCeiling / peak;
        }

    float bankPeak = 0.0f;
    for (const auto& frame : result.frames)
        for (const auto sample : frame)
            bankPeak = juce::jmax (bankPeak, std::abs (sample));
    if (bankPeak > options.bankPeakCeiling && bankPeak > 0.0f)
    {
        const auto gain = options.bankPeakCeiling / bankPeak;
        for (auto& frame : result.frames)
            for (auto& sample : frame)
                sample *= gain;
    }

    result.regenerateMips();
    return result;
}
