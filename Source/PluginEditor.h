#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class WavetableEditorComponent : public juce::Component
{
public:
    explicit WavetableEditorComponent (HybridWavetableAudioProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    HybridWavetableAudioProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavetableEditorComponent)
};

// A compact, high-contrast skin keeps the synth readable on stage while giving
// the editor a distinctive cyberpunk identity.  All drawing stays in the GUI
// layer; parameter values and automation are unchanged.
class CyberpunkLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CyberpunkLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

class HybridWavetableAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit HybridWavetableAudioProcessorEditor (HybridWavetableAudioProcessor&);
    ~HybridWavetableAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    HybridWavetableAudioProcessor& processor;
    CyberpunkLookAndFeel lookAndFeel;
    juce::TextButton loadButton { "Load Audio" }, presetButton { "Save Preset" }, loadPresetButton { "Load Preset" };
    juce::TextButton midiLearnButton { "Learn CC" };
    juce::ComboBox midiLearnParameter, factoryPresetMenu;
    juce::Slider osc1, osc2, osc3, cutoff, resonance, drive, saturation, filterEnvAmount, output;
    juce::Slider osc1Detune, osc2Detune, osc3Detune, unisonKeyTrack;
    std::array<juce::Slider, 8> envelopeSliders;
    juce::ComboBox filterType, slope;
    juce::Label title, signatureLabel, wavetableSection, oscillatorSection, filterSection, envelopeSection, factoryPresetLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a1, a2, a3, ac, ar, ad, as, afe, ao;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ad1, ad2, ad3, akt;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> envelopeAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> at, asl;
    WavetableEditorComponent wavetableEditor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HybridWavetableAudioProcessorEditor)
};
