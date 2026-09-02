#include "../../Source/PluginProcessor.h"
#include "TestHarness.h"

int main()
{
    audioquality::TestHarness test;
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    HybridWavetableAudioProcessor processor;
    test.expect (processor.getName() == "SEOUL DSP", "processor constructs as SEOUL DSP");
    return test.result();
}
