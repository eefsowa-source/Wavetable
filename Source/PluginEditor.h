#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "EditorTypes.h"
#include <deque>

#if defined(MELATONIN_INSPECTOR_ENABLED)
#include <melatonin_inspector/melatonin_inspector.h>
#endif

class WavetableEditorComponent : public juce::Component
{
public:
    explicit WavetableEditorComponent (HybridWavetableAudioProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    // Presentation state is pushed by the owning editor; audio behaviour and
    // the control set never change with the selected type.
    void setSkin (const seoului::Skin& skinToUse) { skin = &skinToUse; repaint(); }

    // Edit-history snapshots of frame 0 (the only frame this editor draws).
    void pushUndoSnapshot();
    void undo();
    void redo();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
private:
    void restoreFrame (const std::array<float, WavetableData::tableSize>& frame);

    HybridWavetableAudioProcessor& processor;
    const seoului::Skin* skin = &seoului::skins()[(size_t) 0];
    std::deque<std::array<float, WavetableData::tableSize>> undoStack, redoStack;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavetableEditorComponent)
};

// Rotary slider that drops to quarter sensitivity while Shift or Alt is held.
// JUCE re-reads the drag sensitivity on every mouse event, so toggling the
// modifier mid-drag scales deltas without a value jump.
class FineSlider final : public juce::Slider
{
public:
    using juce::Slider::Slider;
    void mouseDrag (const juce::MouseEvent& e) override
    {
        setMouseDragSensitivity (250 * ((e.mods.isShiftDown() || e.mods.isAltDown()) ? 4 : 1));
        juce::Slider::mouseDrag (e);
    }
};

// Compact output level meter with a clip indicator (Plan C UX-2). The audio
// thread publishes the peak and clip flag as atomics; this polls them on a
// timer, so nothing here touches the audio callback.
class OutputMeterComponent : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit OutputMeterComponent (HybridWavetableAudioProcessor& p) : processor (p)
    {
        setTooltip ("Output level. Click to clear the clip indicator.");
        startTimerHz (30);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        processor.clearOutputClip();
        clipShown = false;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0e1116));
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (juce::Colour (0xff2a3038));
        g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);

        auto label = area.removeFromLeft (28.0f);
        g.setColour (juce::Colour (0xff9aa4b2));
        g.setFont (juce::Font (juce::FontOptions {}.withHeight (10.0f).withStyle ("Bold")));
        g.drawText ("OUT", label, juce::Justification::centred);

        auto led = area.removeFromRight (38.0f).reduced (3.0f);
        g.setColour (clipShown ? juce::Colour (0xffff5a4a) : juce::Colour (0xff232a33));
        g.fillRoundedRectangle (led, 3.0f);
        g.setColour (juce::Colour (0xff0b0e12));
        g.drawText ("CLIP", led, juce::Justification::centred);

        auto bar = area.reduced (4.0f, 6.0f);
        g.setColour (juce::Colour (0xff1b2129));
        g.fillRoundedRectangle (bar, 2.0f);
        const auto proportion = juce::jlimit (0.0f, 1.0f, (displayDb + 60.0f) / 60.0f);
        if (proportion > 0.0f)
        {
            g.setColour (displayDb > -0.5f ? juce::Colour (0xffff5a4a) : juce::Colour (0xff39b27b));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * proportion), 2.0f);
        }
    }

private:
    void timerCallback() override
    {
        const auto peak = processor.outputPeakLevel();
        const auto peakDb = peak > 1.0e-6f ? juce::Decibels::gainToDecibels (peak) : -100.0f;
        // Fast attack, slow release, so a short peak stays visible.
        const auto next = juce::jmax (peakDb, displayDb - 1.5f);
        const auto clip = processor.outputClipActive();
        const bool changed = std::abs (next - displayDb) > 0.25f || clip != clipShown;
        displayDb = next;
        clipShown = clip;
        if (changed)
            repaint();
    }

    HybridWavetableAudioProcessor& processor;
    float displayDb = -100.0f;
    bool clipShown = false;
};

// The editor renders one of 90 user-selectable types: 30 skins, 30 layouts
// (6 skeletons x 5 densities) and 30 curated skin+layout pairings. The look
// and feel is parameterised by the active skin so every knob, button and
// combo restyles together; audio parameters stay identical in all types.
class CyberpunkLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CyberpunkLookAndFeel();
    void setSkin (const seoului::Skin& newSkin);

    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void setAlwaysShowValues (bool shouldShow) { alwaysShowValues = shouldShow; }

private:
    seoului::Skin currentSkin = seoului::skins()[(size_t) 0];
    bool alwaysShowValues = false;
    juce::Colour c (std::uint32_t argb) const { return juce::Colour (argb); }
};

class HybridWavetableAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit HybridWavetableAudioProcessorEditor (HybridWavetableAudioProcessor&);
    ~HybridWavetableAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    // Pushes the processor's uiType into the presentation state. Used by the
    // editor itself, host-side state changes and offline snapshot tooling.
    void applyUiType (int type);

    // A/B compare: swaps between two in-memory parameter snapshots. Session
    // state and the preset schema are untouched.
    void toggleAB();

private:
    // Rebuilds the item menu for one bank (0 skins, 1 layouts, 2 curated).
    void populateUiTypeItems (int bank);
    void applyFactoryPresetStep (int step);

    HybridWavetableAudioProcessor& processor;
   CyberpunkLookAndFeel lookAndFeel;

   juce::TextButton loadButton { "Load Audio" }, presetButton { "Save Preset" }, loadPresetButton { "Load Preset" };
   juce::TextButton valuesButton { "Values" }, undoButton { "Undo" }, redoButton { "Redo" };
   juce::TextButton presetPrevButton { "<" }, presetNextButton { ">" }, abButton { "A" };
   juce::ComboBox scaleMenu;
   juce::TextButton midiLearnButton { "Learn CC" };
   juce::ComboBox midiLearnParameter, factoryPresetMenu, uiBankMenu, uiTypeMenu;
   FineSlider osc1, osc2, osc3, cutoff, resonance, drive, saturation, filterEnvAmount, output;
   FineSlider osc1Detune, osc2Detune, osc3Detune, unisonKeyTrack;
   std::array<FineSlider, 8> envelopeSliders;
juce::ComboBox filterType, slope;
juce::Label title, signatureLabel, wavetableSection, oscillatorSection, filterSection, envelopeSection, factoryPresetLabel, uiTypeLabel;
juce::Label midiLearnLabel, filterTypeLabel, slopeLabel;
std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a1, a2, a3, ac, ar, ad, as, afe, ao;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ad1, ad2, ad3, akt;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> envelopeAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> at, asl;
    WavetableEditorComponent wavetableEditor;
    OutputMeterComponent outputMeter;

    const seoului::Skin* activeSkin = &seoului::skins()[(size_t) 0];
    seoului::Layout activeLayout = seoului::layouts()[(size_t) 0];
    int uiType = 0;
    int uiTypeBank = -1; // bank currently loaded into uiTypeMenu
    juce::ValueTree presetA, presetB;
    bool showingB = false;

#if defined(MELATONIN_INSPECTOR_ENABLED)
    melatonin::Inspector inspector { *this, false };
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HybridWavetableAudioProcessorEditor)
};
