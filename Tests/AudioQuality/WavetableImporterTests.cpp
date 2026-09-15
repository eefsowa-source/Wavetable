#include "../../Source/DSP/WavetableOscillator.h"
#include "TestHarness.h"

#include <cmath>

namespace
{
float mean (const std::array<float, WavetableData::tableSize>& frame)
{
    double sum = 0.0;
    for (const auto sample : frame)
        sum += sample;
    return (float) (sum / (double) frame.size());
}

float rms (const std::array<float, WavetableData::tableSize>& frame)
{
    double sum = 0.0;
    for (const auto sample : frame)
        sum += (double) sample * (double) sample;
    return (float) std::sqrt (sum / (double) frame.size());
}

float rmsDifference (const std::array<float, WavetableData::tableSize>& a,
                     const std::array<float, WavetableData::tableSize>& b)
{
    const auto scaleA = 1.0f / juce::jmax (rms (a), 1.0e-12f);
    const auto scaleB = 1.0f / juce::jmax (rms (b), 1.0e-12f);
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        const auto difference = (double) a[i] * scaleA - (double) b[i] * scaleB;
        sum += difference * difference;
    }
    return (float) std::sqrt (sum / (double) a.size());
}
}

int main()
{
    audioquality::TestHarness test;

    {
        juce::AudioBuffer<float> source (1, 3001);
        for (int i = 0; i < source.getNumSamples(); ++i)
        {
            const auto phase = juce::MathConstants<float>::twoPi
                               * (float) i / (float) source.getNumSamples();
            source.setSample (0, i, 0.2f + 0.6f * std::sin (phase));
        }

        WavetableData imported;
        imported.loadFromAudio (source);
        test.expect (std::abs (mean (imported.frames[0])) < 1.0e-5f,
                     "import removes DC before mip generation");
        double resamplingError = 0.0;
        for (int i = 0; i < WavetableData::tableSize; ++i)
        {
            const auto expected = 0.6f * std::sin (juce::MathConstants<float>::twoPi
                                                   * (float) i
                                                   / (float) WavetableData::tableSize);
            const auto difference = (double) imported.frames[0][(size_t) i] - expected;
            resamplingError += difference * difference;
        }
        resamplingError = std::sqrt (resamplingError / (double) WavetableData::tableSize);
        test.expect (resamplingError < 1.0e-4,
                     "band-limited cyclic resampling preserves a legal partial");
        test.expect (std::all_of (imported.frames[0].begin(), imported.frames[0].end(),
                                 [] (float sample) { return std::isfinite (sample); }),
                     "arbitrary-length import stays finite");
    }

    {
        constexpr int sourceFrameSize = 3073;
        juce::AudioBuffer<float> source (1, sourceFrameSize * WavetableData::numTables);
        for (int frame = 0; frame < WavetableData::numTables; ++frame)
        {
            const auto amplitude = 0.2f + 0.02f * (float) frame;
            const auto phaseOffset = frame * 31;
            for (int i = 0; i < sourceFrameSize; ++i)
            {
                const auto phase = juce::MathConstants<float>::twoPi
                                   * (float) (i + phaseOffset) / (float) sourceFrameSize;
                source.setSample (0, frame * sourceFrameSize + i,
                                  amplitude * (std::sin (phase) + 0.25f * std::sin (3.0f * phase)));
            }
        }

        WavetableData imported;
        imported.loadFromAudio (source);
        test.expect (rmsDifference (imported.frames[0], imported.frames[15]) < 0.01f,
                     "cyclic import phase-aligns frames before morphing");

        const auto inputRatio = (0.2f + 0.02f * 15.0f) / 0.2f;
        const auto outputRatio = rms (imported.frames[15]) / rms (imported.frames[0]);
        const auto ratioErrorDb = 20.0f * std::log10 (outputRatio / inputRatio);
        test.expect (std::abs (ratioErrorDb) < 0.1f,
                     "bank import preserves relative frame levels");
    }

    {
        constexpr int sourceFrameSize = WavetableData::tableSize;
        juce::AudioBuffer<float> source (1, sourceFrameSize * WavetableData::numTables);
        std::array<float, sourceFrameSize> richCycle {};
        constexpr std::array<int, 8> harmonics { 389, 20, 111, 81, 256, 28, 308, 380 };
        constexpr std::array<float, 8> amplitudes {
            0.3996854055f, 0.4461417416f, 0.9960167835f, 0.0920760027f,
            0.9661462355f, 0.2534283855f, 0.7980482895f, 0.2309347435f };
        constexpr std::array<float, 8> phases {
            2.4594817299f, 4.4769251164f, 5.8087789054f, 2.7413795380f,
            3.8305131900f, 2.1831244951f, 1.3160048734f, 5.9083670641f };
        for (int i = 0; i < sourceFrameSize; ++i)
        {
            const auto phase = juce::MathConstants<float>::twoPi
                               * (float) i / (float) sourceFrameSize;
            for (size_t partial = 0; partial < harmonics.size(); ++partial)
                richCycle[(size_t) i] += amplitudes[partial]
                                        * std::sin ((float) harmonics[partial] * phase
                                                    + phases[partial]);
        }
        for (int frame = 0; frame < WavetableData::numTables; ++frame)
        {
            const auto shift = frame == 0 ? 0 : 1018;
            for (int i = 0; i < sourceFrameSize; ++i)
                source.setSample (0, frame * sourceFrameSize + i,
                                  richCycle[(size_t) ((i + shift) % sourceFrameSize)]);
        }

        WavetableData imported;
        imported.loadFromAudio (source);
        test.expect (rmsDifference (imported.frames[0], imported.frames[15]) < 0.01f,
                     "phase alignment resolves high-harmonic cyclic shifts");
    }

    return test.result();
}
