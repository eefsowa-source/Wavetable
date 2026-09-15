#include "../../Source/DSP/OutputSafety.h"
#include "TestHarness.h"

#include <cmath>

namespace
{
double rms (const juce::AudioBuffer<float>& buffer, int startSample)
{
    double sum = 0.0;
    int count = 0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = startSample; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = (double) buffer.getSample (channel, sample);
            sum += value * value;
            ++count;
        }
    return std::sqrt (sum / (double) juce::jmax (1, count));
}
}

int main()
{
    audioquality::TestHarness test;
    constexpr double sampleRate = 48000.0;

    OutputSafety dcSafety;
    dcSafety.prepare (sampleRate);
    juce::AudioBuffer<float> dc (2, (int) sampleRate);
    for (int sample = 0; sample < dc.getNumSamples(); ++sample)
    {
        dc.setSample (0, sample, 0.1f);
        dc.setSample (1, sample, -0.1f);
    }
    dcSafety.process (dc);
    double residual = 0.0;
    for (int sample = (int) sampleRate / 2; sample < dc.getNumSamples(); ++sample)
        residual += dc.getSample (0, sample);
    residual = std::abs (residual / (sampleRate / 2.0));
    test.expect (residual < 1.0e-4, "10 Hz output blocker removes DC below -80 dBFS");
    dcSafety.reset();
    juce::AudioBuffer<float> resetSilence (2, 128);
    resetSilence.clear();
    dcSafety.process (resetSilence);
    test.expect (resetSilence.getMagnitude (0, resetSilence.getNumSamples()) == 0.0f,
                 "reset clears DC blocker history before transport restart");

    OutputSafety sineSafety;
    sineSafety.prepare (sampleRate);
    juce::AudioBuffer<float> processed (2, (int) sampleRate);
    juce::AudioBuffer<float> reference (2, (int) sampleRate);
    for (int sample = 0; sample < processed.getNumSamples(); ++sample)
    {
        const auto value = 0.5f * std::sin (juce::MathConstants<float>::twoPi
                                           * 1000.0f * (float) sample / (float) sampleRate);
        for (int channel = 0; channel < 2; ++channel)
        {
            processed.setSample (channel, sample, value);
            reference.setSample (channel, sample, value);
        }
    }
    sineSafety.process (processed);
    const auto gainErrorDb = juce::Decibels::gainToDecibels ((float) (rms (processed, 4800)
                                                                       / rms (reference, 4800)));
    test.expect (std::abs (gainErrorDb) < 0.01f,
                 "output DC blocker is transparent at 1 kHz");
    return test.result();
}
