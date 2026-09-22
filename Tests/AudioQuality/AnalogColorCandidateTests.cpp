// Conditional Plan B Task 5 evaluation.
//
// These stages are candidates only. They are deliberately rendered through
// the pinned eon_dsp oversampler here, but are not part of SynthVoice until a
// quality gate or a level-matched listening comparison accepts them.

#include "Dsp/Oversampling.h"
#include "Dsp/Stages.h"
#include "Dsp/Transformer.h"
#include "Dsp/Triode.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr double probeHz = 10000.0;
constexpr int blockSize = 127;
constexpr int totalSamples = 144000;
constexpr int warmupSamples = 48000;
constexpr int analysisSamples = totalSamples - warmupSamples;

enum class Mode { off, triode, transformer };

struct Stats
{
    double rms = 0.0;
    double dc = 0.0;
    double peak = 0.0;
    double fundamental = 0.0;
    double foldedThirdDbc = -std::numeric_limits<double>::infinity();
    double thd = 0.0;
};

double amplitudeAt (const std::vector<float>& signal, double frequencyHz)
{
    double real = 0.0;
    double imag = 0.0;
    for (int i = 0; i < analysisSamples; ++i)
    {
        const auto sample = (double) signal[(size_t) (warmupSamples + i)];
        const auto phase = 2.0 * 3.14159265358979323846 * frequencyHz
                         * (double) i / sampleRate;
        real += sample * std::cos (phase);
        imag -= sample * std::sin (phase);
    }
    return 2.0 * std::hypot (real, imag) / (double) analysisSamples;
}

double foldedFrequency (double frequencyHz)
{
    auto folded = std::fmod (frequencyHz, sampleRate);
    if (folded > sampleRate * 0.5)
        folded = sampleRate - folded;
    return std::abs (folded);
}

Stats measure (const std::vector<float>& signal)
{
    Stats result;
    double sum = 0.0;
    double sumSquares = 0.0;
    for (int i = warmupSamples; i < totalSamples; ++i)
    {
        const auto value = (double) signal[(size_t) i];
        sum += value;
        sumSquares += value * value;
        result.peak = std::max (result.peak, std::abs (value));
    }
    result.rms = std::sqrt (sumSquares / (double) analysisSamples);
    result.dc = std::abs (sum / (double) analysisSamples);
    result.fundamental = amplitudeAt (signal, probeHz);
    const auto foldedThird = amplitudeAt (signal, foldedFrequency (3.0 * probeHz));
    result.foldedThirdDbc = 20.0 * std::log10 (std::max (foldedThird, 1.0e-20)
                                               / std::max (result.fundamental, 1.0e-20));

    double harmonicPower = 0.0;
    for (int harmonic = 2; harmonic <= 10; ++harmonic)
    {
        const auto amplitude = amplitudeAt (signal, foldedFrequency (harmonic * probeHz));
        harmonicPower += amplitude * amplitude;
    }
    result.thd = std::sqrt (harmonicPower) / std::max (result.fundamental, 1.0e-20);
    return result;
}

class Candidate
{
public:
    void prepare()
    {
        oversampler.setStages (2); // 4x; the candidate must earn more CPU.
        oversampler.prepare (blockSize);
        scratch.assign ((size_t) blockSize * (size_t) oversampler.factor(), 0.0f);
        triode.inVolts = 1.8f;
        triode.reset();
        transformer.reset();
        outputDc.prepare (sampleRate, 18.0);
        outputDc.reset();
    }

    void process (float* input, int count, Mode mode)
    {
        if (mode == Mode::off)
            return;

        oversampler.up (input, count, scratch.data());
        const auto upCount = count * oversampler.factor();
        for (int i = 0; i < upCount; ++i)
        {
            const auto x = scratch[(size_t) i];
            if (mode == Mode::triode)
                scratch[(size_t) i] = triode.process (x) * 0.35f;
            else
                scratch[(size_t) i] = transformer.process (x) * 0.38f;
        }
        oversampler.down (scratch.data(), input, count);
        for (int i = 0; i < count; ++i)
            input[i] = outputDc.process (input[i]);
    }

private:
    eon::Oversampler oversampler;
    eon::TriodeStage triode;
    eon::JilesAtherton transformer;
    eon::DCBlocker outputDc;
    std::vector<float> scratch;
};

std::vector<float> render (Mode mode)
{
    std::vector<float> output ((size_t) totalSamples, 0.0f);
    std::vector<float> block ((size_t) blockSize, 0.0f);
    Candidate candidate;
    candidate.prepare();
    for (int offset = 0; offset < totalSamples; offset += blockSize)
    {
        const auto count = std::min (blockSize, totalSamples - offset);
        for (int i = 0; i < count; ++i)
            block[(size_t) i] = 0.35f * (float) std::sin (2.0 * 3.14159265358979323846
                                                          * probeHz * (double) (offset + i)
                                                          / sampleRate);
        candidate.process (block.data(), count, mode);
        std::copy_n (block.begin(), count, output.begin() + offset);
    }
    return output;
}

bool finiteAndBounded (const Stats& stats)
{
    return std::isfinite (stats.rms) && std::isfinite (stats.dc)
        && std::isfinite (stats.peak) && stats.peak < 2.0;
}
}

int main()
{
    const auto baseline = measure (render (Mode::off));
    const auto triode = measure (render (Mode::triode));
    const auto transformer = measure (render (Mode::transformer));

    std::printf ("analog color baseline: rms %.6f dc %.3g peak %.6f alias %.2f dBc thd %.4f\n",
                 baseline.rms, baseline.dc, baseline.peak,
                 baseline.foldedThirdDbc, baseline.thd);
    std::printf ("analog color triode:   rms %.6f dc %.3g peak %.6f alias %.2f dBc thd %.4f\n",
                 triode.rms, triode.dc, triode.peak,
                 triode.foldedThirdDbc, triode.thd);
    std::printf ("analog color transformer: rms %.6f dc %.3g peak %.6f alias %.2f dBc thd %.4f\n",
                 transformer.rms, transformer.dc, transformer.peak,
                 transformer.foldedThirdDbc, transformer.thd);

    bool ok = finiteAndBounded (baseline) && finiteAndBounded (triode)
           && finiteAndBounded (transformer);
    if (! ok)
        std::fprintf (stderr, "analog color candidate produced non-finite or excessive output\n");

    // This is a rejection gate, not an adoption gate. The existing linear
    // path is already at the measurement floor, so extra harmonic colour is
    // not an objective quality improvement. Transformer aliasing is also too
    // high for the product's absolute folded-energy ceiling.
    const bool triodeImprovesAlias = triode.foldedThirdDbc < baseline.foldedThirdDbc - 3.0;
    const bool transformerClearsAlias = transformer.foldedThirdDbc <= -100.0;
    const bool rejectionExpected = ! triodeImprovesAlias && ! transformerClearsAlias;
    if (rejectionExpected)
        std::printf ("analog color decision: reject candidate for production path\n");
    else
        std::fprintf (stderr, "analog color decision gate unexpectedly accepted a candidate\n");

    return ok && rejectionExpected ? 0 : 1;
}
