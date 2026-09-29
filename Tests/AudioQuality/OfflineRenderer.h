#pragma once

#include "AudioQualityTypes.h"
#include "../../Source/PluginProcessor.h"
#include <functional>

namespace audioquality
{
class OfflineRenderer
{
public:
    static juce::AudioBuffer<float> render (const AudioQualityFixture& fixture,
                                             const std::function<void (HybridWavetableAudioProcessor&)>& configure = {},
                                             const std::function<void (HybridWavetableAudioProcessor&, int blockIndex)>& onBlock = {});
};
}
