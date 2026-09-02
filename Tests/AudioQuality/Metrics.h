#pragma once

#include "AudioQualityTypes.h"

namespace audioquality
{
AudioMetrics measureAudio (const juce::AudioBuffer<float>& buffer, double sampleRate);
double estimateFundamentalHz (const float* samples, int count, double sampleRate,
                              double minimumHz, double maximumHz);
double centsError (double measuredHz, double expectedHz);
double measureInharmonicAliasDbc (const float* samples, int count, double sampleRate,
                                  double fundamentalHz, int maximumExpectedHarmonic);
}
