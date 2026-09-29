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

    // Safety ceiling (Plan C SQ-1). Below the threshold the stage must stay
    // transparent; above it the output must remain inside full scale so a legal
    // patch cannot clip the host, and the clip flag must report it.
    {
        OutputSafety ceilingSafety;
        ceilingSafety.prepare (sampleRate);
        juce::AudioBuffer<float> quiet (2, 4096);
        juce::AudioBuffer<float> quietReference (2, 4096);
        for (int sample = 0; sample < quiet.getNumSamples(); ++sample)
        {
            const auto value = 0.5f * std::sin (juce::MathConstants<float>::twoPi
                                                 * 250.0f * (float) sample / (float) sampleRate);
            for (int channel = 0; channel < 2; ++channel)
            {
                quiet.setSample (channel, sample, value);
                quietReference.setSample (channel, sample, value);
            }
        }
        ceilingSafety.process (quiet);
        const auto quietGainErrorDb = juce::Decibels::gainToDecibels ((float) (rms (quiet, 0)
                                                                                 / rms (quietReference, 0)));
        test.expect (std::abs (quietGainErrorDb) < 0.02f,
                     "safety ceiling is transparent below the threshold");
        test.expect (std::abs (ceilingSafety.peakLevel() - quiet.getMagnitude (0, quiet.getNumSamples())) < 1.0e-6f,
                     "the reported peak matches the processed buffer below the threshold");
        test.expect (! ceilingSafety.clipActive(),
                     "safety ceiling leaves the clip flag clear below the threshold");

        juce::AudioBuffer<float> hot (2, 4096);
        for (int sample = 0; sample < hot.getNumSamples(); ++sample)
        {
            const auto value = 8.0f * std::sin (juce::MathConstants<float>::twoPi
                                                * 250.0f * (float) sample / (float) sampleRate);
            for (int channel = 0; channel < 2; ++channel)
                hot.setSample (channel, sample, value);
        }
        ceilingSafety.process (hot);
        const auto hotPeak = hot.getMagnitude (0, hot.getNumSamples());
        std::printf ("  safety ceiling hot peak = %.6f\n", hotPeak);
        test.expect (hotPeak < 1.0f,
                     juce::String ("safety ceiling keeps a hot signal inside full scale (peak ")
                         + juce::String (hotPeak, 4) + ")");
        test.expect (ceilingSafety.peakLevel() > 0.99f && ceilingSafety.peakLevel() < 1.0f,
                     "the reported peak tracks the limited output");
        test.expect (ceilingSafety.clipActive(),
                     "safety ceiling raises the clip flag when it limits");
        ceilingSafety.clearClipFlag();
        test.expect (! ceilingSafety.clipActive(),
                     "clearClipFlag resets the clip indicator");
    }
    return test.result();
}
