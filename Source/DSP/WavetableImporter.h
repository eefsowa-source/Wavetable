#pragma once

#include <JuceHeader.h>

struct WavetableData;

struct WavetableImportOptions
{
    bool removeDc = true;
    bool alignCyclicPhase = true;
    bool normaliseEachFrame = false;
    float bankPeakCeiling = 0.98f;
};

class WavetableImporter
{
public:
    static WavetableData import (const juce::AudioBuffer<float>& source,
                                 const WavetableImportOptions& options = {});
};
