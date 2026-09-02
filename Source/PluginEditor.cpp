#include "PluginEditor.h"

namespace
{
const juce::Colour cyan (0xff55f5ff);
const juce::Colour magenta (0xffff4fd8);
const juce::Colour amber (0xffffa45b);
const juce::Colour ink (0xff0a0d14);
const juce::Colour panel (0xff111a28);
const juce::Colour panelRaised (0xff182538);
const juce::Colour line (0xff28465d);
}

CyberpunkLookAndFeel::CyberpunkLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId, cyan);
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff35516b));
    setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd8f8ff));
    setColour (juce::Slider::textBoxBackgroundColourId, ink);
    setColour (juce::Slider::textBoxOutlineColourId, line);
    setColour (juce::Label::textColourId, juce::Colour (0xffd3e7f0));
    setColour (juce::ComboBox::backgroundColourId, ink);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffd8f8ff));
    setColour (juce::TextButton::buttonColourId, panelRaised);
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff294b62));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd8f8ff));
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void CyberpunkLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                               float sliderPosProportional, float rotaryStartAngle,
                                               float rotaryEndAngle, juce::Slider& slider)
{
    const auto componentBounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto labelBounds = componentBounds.reduced (2.0f).withHeight (17.0f);
    const auto valueBounds = componentBounds.reduced (4.0f).withTop (componentBounds.getBottom() - 23.0f).withHeight (18.0f);
    const auto dialBounds = componentBounds.withTrimmedTop (18.0f).withTrimmedBottom (25.0f).reduced (10.0f);
    const auto diameter = juce::jmin (dialBounds.getWidth(), dialBounds.getHeight());
    const auto knob = juce::Rectangle<float> (dialBounds.getCentreX() - diameter * 0.36f,
                                               dialBounds.getCentreY() - diameter * 0.36f,
                                               diameter * 0.72f, diameter * 0.72f);
    const bool warm = slider.getName().containsIgnoreCase ("drive") || slider.getName().containsIgnoreCase ("saturation");
    const auto accent = warm ? amber : cyan;

    g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.0f).withStyle ("Bold").withKerningFactor (0.05f)));
    g.setColour (juce::Colour (0xffb8d2de));
    g.drawFittedText (slider.getName().toUpperCase(), labelBounds.toNearestInt(), juce::Justification::centred, 1, 0.72f);

    g.setColour (accent.withAlpha (0.08f)); g.fillEllipse (knob.expanded (7.0f));
    g.setColour (juce::Colour (0xff1b2c42)); g.fillEllipse (knob);
    g.setColour (juce::Colour (0xff5f7d99)); g.drawEllipse (knob, 1.5f);
    const auto arcBounds = knob.reduced (4.0f);
    juce::Path arc;
    arc.addCentredArc (knob.getCentreX(), knob.getCentreY(), arcBounds.getWidth() * 0.5f, arcBounds.getHeight() * 0.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff263e55)); g.strokePath (arc, juce::PathStrokeType (3.0f));
    juce::Path valueArc;
    valueArc.addCentredArc (knob.getCentreX(), knob.getCentreY(), arcBounds.getWidth() * 0.5f, arcBounds.getHeight() * 0.5f, 0.0f, rotaryStartAngle, rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle), true);
    g.setColour (accent); g.strokePath (valueArc, juce::PathStrokeType (4.0f));
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    juce::Line<float> pointer (knob.getCentre(), knob.getCentre() + juce::Point<float> (std::cos (angle), std::sin (angle)) * (knob.getWidth() * 0.29f));
    g.setColour (juce::Colours::white.withAlpha (0.92f)); g.drawLine (pointer, 2.0f);
    g.setColour (accent); g.fillEllipse (knob.getCentreX() - 2.5f, knob.getCentreY() - 2.5f, 5.0f, 5.0f);

    // Values stay out of the way until the performer is actively changing a control.
    if (slider.isMouseButtonDown())
    {
        g.setColour (ink.withAlpha (0.94f));
        g.fillRoundedRectangle (valueBounds, 3.0f);
        g.setColour (accent.withAlpha (0.9f));
        g.drawRoundedRectangle (valueBounds, 3.0f, 1.0f);
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f).withStyle ("Bold")));
        g.setColour (juce::Colours::white);
        g.drawFittedText (slider.getTextFromValue (slider.getValue()), valueBounds.toNearestInt(), juce::Justification::centred, 1);
    }
}

void CyberpunkLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour, bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    auto fill = down ? juce::Colour (0xff315a71) : backgroundColour.withAlpha (0.96f);
    if (highlighted) fill = fill.brighter (0.16f);
    g.setColour (fill); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (highlighted ? cyan : line); g.drawRoundedRectangle (r, 4.0f, down ? 2.0f : 1.0f);
    g.setColour (highlighted ? magenta : cyan); g.fillRect (r.getX(), r.getY(), 3.0f, r.getHeight()); g.fillRect (r.getRight() - 3.0f, r.getBottom() - 3.0f, 3.0f, 3.0f);
}

void CyberpunkLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool down)
{
    g.setColour (down ? juce::Colours::white : (highlighted ? cyan : juce::Colour (0xffd8f8ff)));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f).withStyle ("Bold")));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (8, 0), juce::Justification::centred, 1);
}

void CyberpunkLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (1.0f, 1.0f, (float) width - 2.0f, (float) height - 2.0f);
    g.setColour (isButtonDown ? juce::Colour (0xff203d51) : ink); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.isMouseOver() ? cyan : line); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow; arrow.addTriangle ((float) width - 18.0f, height * 0.42f, (float) width - 8.0f, height * 0.42f, (float) width - 13.0f, height * 0.64f);
    g.setColour (magenta); g.fillPath (arrow);
}

void CyberpunkLookAndFeel::positionComboBoxText (juce::ComboBox&, juce::Label& label)
{
    label.setJustificationType (juce::Justification::centredLeft);
    label.setBorderSize (juce::BorderSize<int> (0, 12, 0, 24));
    label.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
}

void WavetableEditorComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (panel.withAlpha (0.97f)); g.fillRoundedRectangle (area, 8.0f); g.setColour (line); g.drawRoundedRectangle (area, 8.0f, 1.0f);
    auto wave = area.reduced (18.0f, 34.0f);
    g.setColour (juce::Colour (0xff1c3850).withAlpha (0.55f));
    for (int x = (int) wave.getX(); x < wave.getRight(); x += 32) g.drawVerticalLine (x, wave.getY(), wave.getBottom());
    for (int y = (int) wave.getY(); y < wave.getBottom(); y += 24) g.drawHorizontalLine (y, wave.getX(), wave.getRight());
    g.setColour (cyan.withAlpha (0.22f)); g.drawHorizontalLine ((int) wave.getCentreY(), wave.getX(), wave.getRight());
    juce::Path p; const auto& frame = processor.wavetable.frames[0];
    for (int x = 0; x < wave.getWidth(); ++x)
    {
        const int i = (x * WavetableData::tableSize) / juce::jmax (1, (int) wave.getWidth());
        const float y = juce::jmap (frame[(size_t) i], -1.0f, 1.0f, wave.getBottom(), wave.getY());
        if (x == 0) p.startNewSubPath (wave.getX() + x, y); else p.lineTo (wave.getX() + x, y);
    }
    g.setColour (cyan.withAlpha (0.14f)); g.strokePath (p, juce::PathStrokeType (8.0f)); g.setColour (cyan); g.strokePath (p, juce::PathStrokeType (2.0f));
    for (int h = 1; h <= 16; ++h)
    {
        float re = 0.0f, im = 0.0f;
        for (int i = 0; i < WavetableData::tableSize; ++i) { const float ph = juce::MathConstants<float>::twoPi * h * i / (float) WavetableData::tableSize; re += frame[(size_t) i] * std::cos (ph); im += frame[(size_t) i] * std::sin (ph); }
        const float mag = juce::jlimit (0.0f, 1.0f, std::sqrt (re * re + im * im) / (float) WavetableData::tableSize * 2.0f);
        const float x = wave.getX() + (h - 1) * wave.getWidth() / 16.0f;
        g.setColour (magenta.withAlpha (0.18f)); g.fillRect (x, wave.getBottom() - mag * 30.0f, wave.getWidth() / 28.0f, mag * 30.0f);
        g.setColour (magenta); g.fillRect (x, wave.getBottom() - mag * 30.0f, wave.getWidth() / 28.0f, 2.0f);
    }
    g.setColour (cyan); g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f).withStyle ("Bold"))); g.drawText ("WAVETABLE // MORPH EDITOR", 18, 9, 260, 18, juce::Justification::left);
    g.setColour (juce::Colour (0xff91aabb)); g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f))); g.drawText ("DRAG: DRAW   SHIFT+DRAG: HARMONICS   |   2048 SAMPLES", 290, 9, getWidth() - 308, 18, juce::Justification::right);
}

void WavetableEditorComponent::mouseDown (const juce::MouseEvent& e) { mouseDrag (e); }
void WavetableEditorComponent::mouseDrag (const juce::MouseEvent& e)
{
    auto wave = getLocalBounds().toFloat().reduced (18.0f, 34.0f);
    const int i = juce::jlimit (0, WavetableData::tableSize - 1, (int) ((e.position.x - wave.getX()) / wave.getWidth() * WavetableData::tableSize));
    if (e.mods.isShiftDown())
    {
        const int harmonic = juce::jlimit (1, 16, 1 + (int) (e.position.x / (float) getWidth() * 16.0f));
        const float amount = juce::jmap (e.position.y, wave.getBottom(), wave.getY(), -1.0f, 1.0f);
        for (int n = 0; n < WavetableData::tableSize; ++n) processor.wavetable.frames[0][(size_t) n] += amount * 0.003f * std::sin (juce::MathConstants<float>::twoPi * harmonic * n / (float) WavetableData::tableSize);
    }
    else if (i >= 0 && i < WavetableData::tableSize) processor.wavetable.frames[0][(size_t) i] = juce::jmap (e.position.y, wave.getBottom(), wave.getY(), -1.0f, 1.0f);
    // Rebuild only frame 0's band-limited mips (the frame this editor
    // touches) so high notes stay alias-free after a live edit, without
    // paying for all 16 frames' FFTs on every mouse-drag callback.
    processor.wavetable.regenerateMipsForFrame (0);
    processor.publishWavetable(); repaint();
}

HybridWavetableAudioProcessorEditor::HybridWavetableAudioProcessorEditor (HybridWavetableAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), wavetableEditor (p)
{
    setSize (1120, 760); setResizable (true, true); setResizeLimits (1040, 760, 1800, 1100); setLookAndFeel (&lookAndFeel);
    title.setText ("SEOUL DSP", juce::dontSendNotification); title.setFont (juce::Font (juce::FontOptions { "Baskerville", "Regular", 30.0f }.withKerningFactor (0.035f))); title.setColour (juce::Label::textColourId, juce::Colour (0xfff2dfc7)); addAndMakeVisible (title);
    signatureLabel.setText ("aoi yume", juce::dontSendNotification); signatureLabel.setFont (juce::Font (juce::FontOptions { "Snell Roundhand", "Regular", 18.0f }.withHorizontalScale (0.94f))); signatureLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb8e5df)); addAndMakeVisible (signatureLabel);
    auto setupSection = [this] (juce::Label& label, const juce::String& text, juce::Colour colour) { label.setText (text, juce::dontSendNotification); label.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f).withStyle ("Bold"))); label.setColour (juce::Label::textColourId, colour); addAndMakeVisible (label); };
    setupSection (oscillatorSection, "OSCILLATORS // TRIPLE WAVETABLE", cyan); setupSection (filterSection, "FILTER // NON-LINEAR", magenta); setupSection (envelopeSection, "ENVELOPES // AMP + FILTER", amber);
    osc1.setRange (0.0, 1.0, 0.001); osc2.setRange (0.0, 1.0, 0.001); osc3.setRange (0.0, 1.0, 0.001); osc1Detune.setRange (0.0, 50.0, 0.01); osc2Detune.setRange (0.0, 50.0, 0.01); osc3Detune.setRange (0.0, 50.0, 0.01); unisonKeyTrack.setRange (-1.0, 1.0, 0.001); cutoff.setRange (20.0, 20000.0, 1.0); cutoff.setSkewFactorFromMidPoint (1000.0); resonance.setRange (0.1, 1.0, 0.001); drive.setRange (-12.0, 24.0, 0.1); saturation.setRange (0.0, 1.0, 0.001); filterEnvAmount.setRange (-1.0, 1.0, 0.001); output.setRange (-60.0, 6.0, 0.1);
    osc1.setName ("Osc 1 Wavetable"); osc2.setName ("Osc 2 Wavetable"); osc3.setName ("Osc 3 Wavetable"); osc1Detune.setName ("Osc 1 Detune (cents)"); osc2Detune.setName ("Osc 2 Detune (cents)"); osc3Detune.setName ("Osc 3 Detune (cents)"); unisonKeyTrack.setName ("Unison Key Track"); cutoff.setName ("Cutoff"); resonance.setName ("Resonance"); drive.setName ("Filter Drive"); saturation.setName ("Saturation"); filterEnvAmount.setName ("Filter Env"); output.setName ("Output");
    for (auto* s : { &osc1, &osc2, &osc3, &osc1Detune, &osc2Detune, &osc3Detune, &unisonKeyTrack, &cutoff, &resonance, &drive, &saturation, &filterEnvAmount, &output }) { s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); s->setNumDecimalPlacesToDisplay (2); s->setTooltip (s->getName()); addAndMakeVisible (s); }
    addAndMakeVisible (filterType); filterType.addItemList ({ "Low-pass", "High-pass", "Band-pass" }, 1); filterType.setTooltip ("Filter type"); addAndMakeVisible (slope); slope.addItemList ({ "8 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct" }, 1); slope.setTooltip ("Filter slope"); addAndMakeVisible (wavetableEditor);
    midiLearnParameter.addItemList (processor.getMidiLearnTargets(), 1); midiLearnParameter.setSelectedItemIndex (0, juce::dontSendNotification); midiLearnParameter.setTooltip ("Choose a parameter for MIDI CC learn"); addAndMakeVisible (midiLearnParameter); addAndMakeVisible (midiLearnButton); midiLearnButton.setTooltip ("Listen for the next MIDI CC"); midiLearnButton.onClick = [this] { processor.beginMidiLearn (midiLearnParameter.getSelectedItemIndex()); };
    factoryPresetLabel.setText ("FACTORY // RANDOM BANK", juce::dontSendNotification); factoryPresetLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f).withStyle ("Bold"))); factoryPresetLabel.setColour (juce::Label::textColourId, amber); addAndMakeVisible (factoryPresetLabel);
    factoryPresetMenu.addItemList (processor.getFactoryPresetNames(), 1); factoryPresetMenu.setTextWhenNothingSelected ("SELECT FACTORY PRESET"); factoryPresetMenu.setTooltip ("Apply one of the 10 built-in random-generated presets"); factoryPresetMenu.onChange = [this] { processor.applyFactoryPreset (factoryPresetMenu.getSelectedItemIndex()); }; addAndMakeVisible (factoryPresetMenu);
    const char* envelopeNames[] = { "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release", "Filter Attack", "Filter Decay", "Filter Sustain", "Filter Release" }; const char* envelopeIDs[] = { "ampAttack", "ampDecay", "ampSustain", "ampRelease", "filterAttack", "filterDecay", "filterSustain", "filterRelease" };
    for (size_t i = 0; i < envelopeSliders.size(); ++i) { auto& s = envelopeSliders[i]; s.setName (envelopeNames[i]); s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); const bool sustain = (i == 2 || i == 6); s.setRange (sustain ? 0.0 : 0.001, sustain ? 1.0 : 10.0, 0.001); if (! sustain) s.setSkewFactorFromMidPoint (0.25); s.setNumDecimalPlacesToDisplay (2); s.setTooltip (envelopeNames[i]); addAndMakeVisible (s); envelopeAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, envelopeIDs[i], s); }
    addAndMakeVisible (loadButton); loadButton.setTooltip ("Import WAV, AIFF, M4A, CAF, MP3, FLAC or OGG"); loadButton.onClick = [this] { auto chooser = std::make_shared<juce::FileChooser> ("Load audio", juce::File(), "*"); chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser] (const juce::FileChooser& c) { if (c.getResult().existsAsFile()) processor.loadAudioFile (c.getResult()); }); };
    addAndMakeVisible (presetButton); presetButton.onClick = [this] { auto chooser = std::make_shared<juce::FileChooser> ("Save preset", juce::File(), "*.preset"); chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser] (const juce::FileChooser& c) { if (c.getResult() != juce::File()) { juce::MemoryBlock d; processor.getStateInformation (d); c.getResult().replaceWithData (d.getData(), d.getSize()); } }); };
    addAndMakeVisible (loadPresetButton); loadPresetButton.onClick = [this] { auto chooser = std::make_shared<juce::FileChooser> ("Load preset", juce::File(), "*.preset"); chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser] (const juce::FileChooser& c) { if (c.getResult().existsAsFile()) { juce::MemoryBlock d; c.getResult().loadFileAsData (d); processor.setStateInformation (d.getData(), (int) d.getSize()); } }); };
    a1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc1Pos", osc1); a2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc2Pos", osc2); a3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc3Pos", osc3); ad1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc1Detune", osc1Detune); ad2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc2Detune", osc2Detune); ad3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc3Detune", osc3Detune); akt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "unisonKeyTrack", unisonKeyTrack); ac = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "cutoff", cutoff); ar = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "resonance", resonance); ad = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "filterDrive", drive); as = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "saturation", saturation); afe = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "filterEnvAmount", filterEnvAmount); ao = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "output", output); at = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.parameters, "filterType", filterType); asl = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.parameters, "filterSlope", slope);
}

HybridWavetableAudioProcessorEditor::~HybridWavetableAudioProcessorEditor() { setLookAndFeel (nullptr); }

void HybridWavetableAudioProcessorEditor::paint (juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    g.fillAll (ink);
    juce::ColourGradient sky (juce::Colour (0xff101b2b), 0.0f, 0.0f,
                              juce::Colour (0xff071018), 0.0f, (float) h, false);
    g.setGradientFill (sky);
    g.fillRect (getLocalBounds());
    g.setColour (juce::Colour (0xff102130).withAlpha (0.22f)); for (int x = 0; x < getWidth(); x += 24) g.drawVerticalLine (x, 0.0f, (float) getHeight()); for (int y = 0; y < getHeight(); y += 24) g.drawHorizontalLine (y, 0.0f, (float) getWidth());
    // A low-contrast Seoul skyline sits behind the panels: blocky towers,
    // restrained window lights, a Han River line, and the N Seoul Tower mast.
    // It is intentionally atmospheric so controls remain the visual priority.
    const float horizon = (float) h - 18.0f;
    const int buildingHeights[] = { 48, 84, 58, 112, 72, 96, 42, 76, 126, 64, 88, 54, 104, 68 };
    const int buildingWidths[] = { 44, 56, 38, 62, 48, 52, 34, 58, 68, 42, 50, 36, 60, 46 };
    int buildingX = -18;
    for (int i = 0; i < 14; ++i)
    {
        const int buildingY = (int) horizon - buildingHeights[i];
        g.setColour (juce::Colour (0xff16344a).withAlpha (0.82f));
        g.fillRect (buildingX, buildingY, buildingWidths[i], buildingHeights[i]);
        for (int wy = buildingY + 12; wy < (int) horizon - 8; wy += 14)
            for (int wx = buildingX + 8; wx < buildingX + buildingWidths[i] - 5; wx += 14)
                if (((i * 5 + wy + wx) % 7) < 3)
                {
                    const auto light = ((i + wy / 14) % 4 == 0) ? magenta : cyan;
                    g.setColour (light.withAlpha (0.48f));
                    g.fillRect (wx, wy, 5, 2);
                }
        buildingX += buildingWidths[i] - 2;
    }
    const float towerX = w * 0.79f;
    g.setColour (cyan.withAlpha (0.52f));
    g.drawLine (towerX, horizon - 126.0f, towerX, horizon - 20.0f, 1.4f);
    g.fillEllipse (towerX - 15.0f, horizon - 91.0f, 30.0f, 10.0f);
    g.drawLine (towerX, horizon - 126.0f, towerX, horizon - 141.0f, 1.0f);
    g.setColour (magenta.withAlpha (0.44f));
    g.drawHorizontalLine ((int) horizon - 12, 0.0f, (float) w);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f).withStyle ("Bold")));
    g.setColour (cyan.withAlpha (0.58f));
    g.drawText ("SEOUL // HANGANG NIGHT", 24, h - 34, 190, 16, juce::Justification::left);

    auto area = getLocalBounds().toFloat().reduced (10.0f);
    auto panelBox = [&g] (juce::Rectangle<float> r, juce::Colour accent) { g.setColour (panel.withAlpha (0.84f)); g.fillRoundedRectangle (r, 7.0f); g.setColour (line); g.drawRoundedRectangle (r, 7.0f, 1.0f); g.setColour (accent.withAlpha (0.9f)); g.fillRect (r.getX(), r.getY(), 4.0f, r.getHeight()); g.setColour (accent.withAlpha (0.45f)); g.fillRect (r.getX() + 18.0f, r.getY(), juce::jmin (120.0f, r.getWidth() - 24.0f), 2.0f); };
    panelBox (area.withTop (92.0f).withHeight (232.0f), cyan); panelBox (area.withTop (338.0f).withHeight (190.0f), magenta); panelBox (area.withTop (540.0f).withHeight (area.getBottom() - 540.0f), amber); g.setColour (cyan.withAlpha (0.8f)); g.fillRect (20.0f, 82.0f, (float) getWidth() - 40.0f, 1.0f);
}

void HybridWavetableAudioProcessorEditor::resized ()
{
    const int w = getWidth(); title.setBounds (28, 12, 250, 42); signatureLabel.setBounds (154, 48, 130, 22);
    const bool compact = w < 1120;
    const int gap = compact ? 4 : 6;
    const int topX = compact ? 420 : 430;
    const int factoryWidth = compact ? 145 : 180;
    const int midiWidth = compact ? 105 : 120;
    const int learnWidth = compact ? 70 : 80;
    const int loadWidth = compact ? 78 : 92;
    const int saveWidth = compact ? 78 : 92;
    const int externalWidth = juce::jmax (80, w - (topX + factoryWidth + midiWidth + learnWidth + loadWidth + saveWidth + gap * 5 + 8));
    int x = topX;
    factoryPresetMenu.setBounds (x, 28, factoryWidth, 28); x += factoryWidth + gap;
    midiLearnParameter.setBounds (x, 28, midiWidth, 28); x += midiWidth + gap;
    midiLearnButton.setBounds (x, 28, learnWidth, 28); x += learnWidth + gap;
    loadButton.setBounds (x, 28, loadWidth, 28); x += loadWidth + gap;
    presetButton.setBounds (x, 28, saveWidth, 28); x += saveWidth + gap;
    loadPresetButton.setBounds (x, 28, externalWidth, 28);
    factoryPresetLabel.setBounds (topX, 7, factoryWidth, 16);
    wavetableEditor.setBounds (20, 92, w - 40, 232); oscillatorSection.setBounds (30, 348, 340, 20); filterSection.setBounds (390, 348, w - 420, 20); envelopeSection.setBounds (30, 550, 360, 20);
    const int y = 372; const int knobAreaRight = w - 190; const int knobWidth = juce::jmax (62, (knobAreaRight - 30) / 13); auto placeKnob = [knobWidth, y] (juce::Slider& s, int index) { s.setBounds (30 + index * knobWidth, y, knobWidth - 6, 145); }; placeKnob (osc1, 0); placeKnob (osc2, 1); placeKnob (osc3, 2); placeKnob (osc1Detune, 3); placeKnob (osc2Detune, 4); placeKnob (osc3Detune, 5); placeKnob (unisonKeyTrack, 6); placeKnob (cutoff, 7); placeKnob (resonance, 8); placeKnob (drive, 9); placeKnob (saturation, 10); placeKnob (filterEnvAmount, 11); placeKnob (output, 12); filterType.setBounds (w - 170, y + 12, 150, 28); slope.setBounds (w - 170, y + 58, 150, 28);
    const int envY = 574; const int cell = juce::jmax (104, (w - 60) / 8); for (size_t i = 0; i < envelopeSliders.size(); ++i) envelopeSliders[i].setBounds (30 + (int) i * cell, envY, cell - 6, 149);
}
