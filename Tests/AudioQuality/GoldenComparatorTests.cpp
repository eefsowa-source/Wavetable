#include "GoldenComparator.h"
#include "TestHarness.h"

#include <cmath>

using namespace audioquality;

namespace
{
juce::AudioBuffer<float> makeTone (double gain = 1.0)
{
    juce::AudioBuffer<float> buffer (1, 65536);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample (0, i, (float) (gain * std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / 48000.0)));
    return buffer;
}
}

int main()
{
    TestHarness test;
    const auto golden = makeTone();
    const auto exact = compareWithGolden (golden, golden, 48000.0);
    test.expect (exact.identityMatches && exact.alignedErrorDbFS < -120.0,
                 "exact golden comparison is identical");

    auto shifted = makeTone();
    for (int i = shifted.getNumSamples() - 1; i >= 10; --i)
        shifted.setSample (0, i, shifted.getSample (0, i - 10));
    shifted.setSample (0, 0, 0.0f);
    shifted.setSample (0, 1, 0.0f);
    shifted.setSample (0, 2, 0.0f);
    shifted.setSample (0, 3, 0.0f);
    shifted.setSample (0, 4, 0.0f);
    shifted.setSample (0, 5, 0.0f);
    shifted.setSample (0, 6, 0.0f);
    shifted.setSample (0, 7, 0.0f);
    shifted.setSample (0, 8, 0.0f);
    shifted.setSample (0, 9, 0.0f);
    const auto aligned = compareWithGolden (shifted, golden, 48000.0);
    test.expect (aligned.alignmentSamples == 10 && aligned.alignedErrorDbFS < -100.0,
                 "10 sample shift aligns");

    auto farShift = makeTone();
    for (int i = farShift.getNumSamples() - 1; i >= 33; --i)
        farShift.setSample (0, i, farShift.getSample (0, i - 33));
    for (int i = 0; i < 33; ++i)
        farShift.setSample (0, i, 0.0f);
    const auto unaligned = compareWithGolden (farShift, golden, 48000.0);
    test.expect (unaligned.alignmentSamples != -33 && unaligned.alignedErrorDbFS > -80.0,
                 "33 sample shift exceeds alignment bound");

    const auto gainSmall = compareWithGolden (makeTone (std::pow (10.0, 0.2 / 20.0)), golden, 48000.0);
    test.expect (gainSmall.loudnessDelta <= 0.25, "+0.2 dB gain is inside normal tolerance");
    const auto gainLarge = compareWithGolden (makeTone (std::pow (10.0, 0.3 / 20.0)), golden, 48000.0);
    test.expect (gainLarge.loudnessDelta > 0.25, "+0.3 dB gain exceeds normal tolerance");

    auto tilted = makeTone();
    for (int i = 0; i < tilted.getNumSamples(); ++i)
        tilted.setSample (0, i, tilted.getSample (0, i) * (i < tilted.getNumSamples() / 2 ? 0.5f : 2.0f));
    const auto tilt = compareWithGolden (tilted, golden, 48000.0);
    test.expect (tilt.spectralP95DeltaDb > 2.0, "spectral tilt exceeds p95 limit");
    return test.result();
}
