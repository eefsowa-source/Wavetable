#include "Metrics.h"
#include "TestHarness.h"

#include <cmath>

using namespace audioquality;

namespace
{
void expectNear (TestHarness& test, double value, double expected, double tolerance, const juce::String& label)
{
    test.expect (std::isfinite (value) && std::abs (value - expected) <= tolerance,
                 label + " expected " + juce::String (expected, 4) + " got " + juce::String (value, 4));
}
}

int main()
{
    TestHarness test;
    constexpr double sampleRate = 48000.0;
    constexpr int count = 65536;
    constexpr int exactSineCount = 48000;

    juce::AudioBuffer<float> sine (1, exactSineCount);
    for (int i = 0; i < exactSineCount; ++i)
        sine.setSample (0, i, 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / sampleRate));
    const auto sineMetrics = measureAudio (sine, sampleRate);
    expectNear (test, sineMetrics.rmsDbFS, -9.0309, 0.02, "1 kHz sine RMS");
    test.expect (sineMetrics.dcDbFS < -120.0, "1 kHz sine DC");

    juce::AudioBuffer<float> constant (1, count);
    constant.clear();
    for (int i = 0; i < count; ++i)
        constant.setSample (0, i, 0.01f);
    expectNear (test, measureAudio (constant, sampleRate).dcDbFS, -40.0, 0.01, "constant DC");

    juce::AudioBuffer<float> pitch (1, count + (int) sampleRate / 10);
    for (int i = 0; i < pitch.getNumSamples(); ++i)
        pitch.setSample (0, i, std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
    const auto measuredPitch = estimateFundamentalHz (pitch.getReadPointer (0) + (int) sampleRate / 10,
                                                       count, sampleRate, 400.0, 480.0);
    test.expect (std::abs (centsError (measuredPitch, 440.0)) <= 0.2, "440 Hz pitch estimate");

    sine.setSample (0, exactSineCount / 2, std::numeric_limits<float>::quiet_NaN());
    test.expect (! measureAudio (sine, sampleRate).finite, "NaN is reported as non-finite");

    juce::AudioBuffer<float> saw (1, count);
    saw.clear();
    for (int i = 0; i < count; ++i)
    {
        double value = 0.0;
        for (int harmonic = 1; harmonic <= 4; ++harmonic)
            value += std::sin (2.0 * juce::MathConstants<double>::pi * 6000.0 * harmonic * i / sampleRate) / harmonic;
        saw.setSample (0, i, (float) (value * 0.7));
    }
    const auto legalAlias = measureInharmonicAliasDbc (saw.getReadPointer (0), count, sampleRate, 6000.0, 4);
    test.expect (legalAlias < -100.0, "legal-harmonic saw has no inharmonic alias");
    for (int i = 0; i < count; ++i)
    {
        const auto inharmonic = std::sin (2.0 * juce::MathConstants<double>::pi * 7300.0 * i / sampleRate)
                                * 0.0083;
        saw.addSample (0, i, (float) inharmonic);
    }
    const auto injectedAlias = measureInharmonicAliasDbc (saw.getReadPointer (0), count, sampleRate, 6000.0, 4);
    test.expect (injectedAlias >= -41.0 && injectedAlias <= -39.0,
                 "-40 dBc inharmonic tone is measured correctly");
    return test.result();
}
