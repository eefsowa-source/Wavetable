#include "PluginEditor.h"

namespace
{
// Skin-aware helpers shared by the editor painting code. All presentation
// decisions flow from the active seoului::Skin; nothing is hard-coded to the
// default palette so the 30 skins stay visually distinct.
juce::Colour skinColour (std::uint32_t argb) { return juce::Colour (argb); }
}

CyberpunkLookAndFeel::CyberpunkLookAndFeel()
{
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0a0d14));
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void CyberpunkLookAndFeel::setSkin (const seoului::Skin& newSkin)
{
    currentSkin = newSkin;
    setColour (juce::Slider::rotarySliderFillColourId, c (newSkin.accent));
    setColour (juce::Slider::rotarySliderOutlineColourId, c (newSkin.panelLine));
    setColour (juce::Slider::textBoxTextColourId, c (newSkin.text));
    setColour (juce::Slider::textBoxBackgroundColourId, c (newSkin.backgroundBottom));
    setColour (juce::Slider::textBoxOutlineColourId, c (newSkin.panelLine));
    setColour (juce::Label::textColourId, c (newSkin.text));
    setColour (juce::ComboBox::backgroundColourId, c (newSkin.backgroundBottom));
    setColour (juce::ComboBox::outlineColourId, c (newSkin.panelLine));
    setColour (juce::ComboBox::textColourId, c (newSkin.text));
    setColour (juce::TextButton::buttonColourId, c (newSkin.panel));
    setColour (juce::TextButton::buttonOnColourId, c (newSkin.panel).brighter (0.25f));
    setColour (juce::TextButton::textColourOffId, c (newSkin.text));
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
    const auto accent = skinColour (warm ? currentSkin.accentAlt : currentSkin.accent);
    const auto body = c (currentSkin.panel).brighter (0.1f);
    const auto outline = c (currentSkin.panelLine);

    g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.0f).withStyle ("Bold").withKerningFactor (0.05f)));
    g.setColour (c (currentSkin.textDim));
    g.drawFittedText (slider.getName().toUpperCase(), labelBounds.toNearestInt(), juce::Justification::centred, 1, 0.72f);

    if (currentSkin.glow)
    {
        g.setColour (accent.withAlpha (0.08f));
        g.fillEllipse (knob.expanded (7.0f));
    }
    g.setColour (body);
    g.fillEllipse (knob);
    g.setColour (outline);
    g.drawEllipse (knob, 1.5f);

    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const float arcRadius = knob.reduced (4.0f).getWidth() * 0.5f;
    const auto centre = knob.getCentre();

    switch (currentSkin.knobStyle)
    {
        case 1: // dot ring: value shown as accent dots around the dial
        {
            constexpr int dotCount = 24;
            for (int dot = 0; dot < dotCount; ++dot)
            {
                const float t = (float) dot / (float) (dotCount - 1);
                const float dotAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
                const bool lit = t <= sliderPosProportional;
                g.setColour (lit ? accent : outline.withAlpha (0.5f));
                const auto p = centre + juce::Point<float> (std::cos (dotAngle), std::sin (dotAngle)) * (arcRadius + 1.0f);
                g.fillEllipse (p.x - 1.8f, p.y - 1.8f, 3.6f, 3.6f);
            }
            break;
        }
        case 2: // bar meter across the top of the dial
        {
            const auto track = juce::Rectangle<float> (knob.getX(), knob.getY() - 6.0f, knob.getWidth(), 3.5f);
            g.setColour (outline.withAlpha (0.6f));
            g.fillRoundedRectangle (track, 1.6f);
            g.setColour (accent);
            g.fillRoundedRectangle (track.withWidth (track.getWidth() * sliderPosProportional), 1.6f);
            break;
        }
        case 3: // thin needle, terminal style
        {
            juce::Path trackArc;
            trackArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour (outline.withAlpha (0.5f));
            g.strokePath (trackArc, juce::PathStrokeType (1.6f));
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour (accent);
            g.strokePath (valueArc, juce::PathStrokeType (1.6f));
            break;
        }
        case 4: // split gauge: two opposing fill arcs
        {
            const float mid = (rotaryStartAngle + rotaryEndAngle) * 0.5f;
            juce::Path lowerArc, upperArc;
            if (sliderPosProportional < 0.5f)
            {
                lowerArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                        mid - sliderPosProportional * (mid - rotaryStartAngle) * 2.0f, mid, true);
                g.setColour (accent);
                g.strokePath (lowerArc, juce::PathStrokeType (4.0f));
            }
            else
            {
                upperArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, mid,
                                        mid + (sliderPosProportional - 0.5f) * (rotaryEndAngle - mid) * 2.0f, true);
                g.setColour (accent);
                g.strokePath (upperArc, juce::PathStrokeType (4.0f));
            }
            g.setColour (outline.withAlpha (0.45f));
            juce::Path baseArc;
            baseArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.strokePath (baseArc, juce::PathStrokeType (1.2f));
            break;
        }
        default: // classic arc
        {
            juce::Path trackArc;
            trackArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour (outline.withAlpha (0.55f));
            g.strokePath (trackArc, juce::PathStrokeType (3.0f));
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour (accent);
            g.strokePath (valueArc, juce::PathStrokeType (4.0f));
            break;
        }
    }

    juce::Line<float> pointer (centre, centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * (knob.getWidth() * 0.29f));
    g.setColour (skinColour (currentSkin.text).withAlpha (0.92f));
    g.drawLine (pointer, 2.0f);
    g.setColour (accent);
    g.fillEllipse (centre.x - 2.5f, centre.y - 2.5f, 5.0f, 5.0f);

    // Values stay out of the way until the performer is actively changing a control.
    if (slider.isMouseButtonDown())
    {
        g.setColour (skinColour (currentSkin.backgroundBottom).withAlpha (0.94f));
        g.fillRoundedRectangle (valueBounds, 3.0f);
        g.setColour (accent.withAlpha (0.9f));
        g.drawRoundedRectangle (valueBounds, 3.0f, 1.0f);
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f).withStyle ("Bold")));
        g.setColour (skinColour (currentSkin.text));
        g.drawFittedText (slider.getTextFromValue (slider.getValue()), valueBounds.toNearestInt(), juce::Justification::centred, 1);
    }
}

void CyberpunkLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour, bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    auto fill = down ? c (currentSkin.accent).withAlpha (0.25f) : backgroundColour.withAlpha (0.96f);
    if (highlighted) fill = fill.brighter (0.16f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (highlighted ? c (currentSkin.accent) : c (currentSkin.panelLine));
    g.drawRoundedRectangle (r, 4.0f, down ? 2.0f : 1.0f);
    const auto edge = c (currentSkin.accent);
    g.setColour (edge);
    g.fillRect (r.getX(), r.getY(), 3.0f, r.getHeight());
    g.fillRect (r.getRight() - 3.0f, r.getBottom() - 3.0f, 3.0f, 3.0f);
}

void CyberpunkLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool down)
{
    g.setColour (down ? skinColour (currentSkin.text) : (highlighted ? c (currentSkin.accent) : c (currentSkin.text)));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f).withStyle ("Bold")));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (8, 0), juce::Justification::centred, 1);
}

void CyberpunkLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (1.0f, 1.0f, (float) width - 2.0f, (float) height - 2.0f);
    g.setColour (isButtonDown ? c (currentSkin.panel).brighter (0.3f) : c (currentSkin.backgroundBottom));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.isMouseOver() ? c (currentSkin.accent) : c (currentSkin.panelLine));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    arrow.addTriangle ((float) width - 18.0f, height * 0.42f, (float) width - 8.0f, height * 0.42f, (float) width - 13.0f, height * 0.64f);
    g.setColour (c (currentSkin.accentAlt));
    g.fillPath (arrow);
}

void CyberpunkLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (1, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setBorderSize (juce::BorderSize<int> (0, 12, 0, 24));
    const auto textHeight = box.getWidth() < 120 ? 10.0f : 12.0f;
    label.setFont (juce::Font (juce::FontOptions{}.withHeight (textHeight)));
    // ComboBox owns a child Label, so explicitly propagate the skin's text
    // colour here; otherwise the default black Label text disappears on the
    // near-black panel in headless snapshots and some hosts.
    label.setColour (juce::Label::textColourId, findColour (juce::ComboBox::textColourId));
    label.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}

void WavetableEditorComponent::paint (juce::Graphics& g)
{
    jassert (skin != nullptr);
    const auto& s = *skin;
    auto area = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (skinColour (s.panel).withAlpha (0.97f));
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (skinColour (s.panelLine));
    g.drawRoundedRectangle (area, 8.0f, 1.0f);
    auto wave = area.reduced (18.0f, 34.0f);
    g.setColour (skinColour (s.panelLine).withAlpha (0.4f));
    for (int x = (int) wave.getX(); x < wave.getRight(); x += 32) g.drawVerticalLine (x, wave.getY(), wave.getBottom());
    for (int y = (int) wave.getY(); y < wave.getBottom(); y += 24) g.drawHorizontalLine (y, wave.getX(), wave.getRight());
    const auto accent = skinColour (s.accent);
    const auto accentAlt = skinColour (s.accentAlt);
    g.setColour (accent.withAlpha (0.22f));
    g.drawHorizontalLine ((int) wave.getCentreY(), wave.getX(), wave.getRight());
    juce::Path p;
    const auto& frame = processor.wavetable.frames[0];
    for (int x = 0; x < wave.getWidth(); ++x)
    {
        const int i = (x * WavetableData::tableSize) / juce::jmax (1, (int) wave.getWidth());
        const float y = juce::jmap (frame[(size_t) i], -1.0f, 1.0f, wave.getBottom(), wave.getY());
        if (x == 0) p.startNewSubPath (wave.getX() + x, y); else p.lineTo (wave.getX() + x, y);
    }
    g.setColour (accent.withAlpha (0.14f));
    g.strokePath (p, juce::PathStrokeType (8.0f));
    g.setColour (accent);
    g.strokePath (p, juce::PathStrokeType (2.0f));
    for (int h = 1; h <= 16; ++h)
    {
        float re = 0.0f, im = 0.0f;
        for (int i = 0; i < WavetableData::tableSize; ++i)
        {
            const float ph = juce::MathConstants<float>::twoPi * h * i / (float) WavetableData::tableSize;
            re += frame[(size_t) i] * std::cos (ph);
            im += frame[(size_t) i] * std::sin (ph);
        }
        const float mag = juce::jlimit (0.0f, 1.0f, std::sqrt (re * re + im * im) / (float) WavetableData::tableSize * 2.0f);
        const float x = wave.getX() + (h - 1) * wave.getWidth() / 16.0f;
        g.setColour (accentAlt.withAlpha (0.18f));
        g.fillRect (x, wave.getBottom() - mag * 30.0f, wave.getWidth() / 28.0f, mag * 30.0f);
        g.setColour (accentAlt);
        g.fillRect (x, wave.getBottom() - mag * 30.0f, wave.getWidth() / 28.0f, 2.0f);
    }
    g.setColour (accent);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f).withStyle ("Bold")));
    g.drawText ("WAVETABLE // MORPH EDITOR", 18, 9, 260, 18, juce::Justification::left);
    g.setColour (skinColour (s.textDim));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    g.drawText ("DRAG: DRAW   SHIFT+DRAG: HARMONICS   |   2048 SAMPLES", 290, 9, getWidth() - 308, 18, juce::Justification::right);
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
        for (int n = 0; n < WavetableData::tableSize; ++n)
            processor.wavetable.frames[0][(size_t) n] += amount * 0.003f * std::sin (juce::MathConstants<float>::twoPi * harmonic * n / (float) WavetableData::tableSize);
    }
    else if (i >= 0 && i < WavetableData::tableSize)
    {
        processor.wavetable.frames[0][(size_t) i] = juce::jmap (e.position.y, wave.getBottom(), wave.getY(), -1.0f, 1.0f);
    }
    // Rebuild only frame 0's band-limited mips (the frame this editor
    // touches) so high notes stay alias-free after a live edit, without
    // paying for all 16 frames' FFTs on every mouse-drag callback.
    processor.wavetable.regenerateMipsForFrame (0);
    processor.publishWavetable();
    repaint();
}

HybridWavetableAudioProcessorEditor::HybridWavetableAudioProcessorEditor (HybridWavetableAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), wavetableEditor (p)
{
    setSize (1120, 760);
    setResizable (true, true);
    setResizeLimits (1040, 760, 1800, 1100);
    setLookAndFeel (&lookAndFeel);

    title.setText ("SEOUL DSP", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions { "Baskerville", "Regular", 30.0f }.withKerningFactor (0.035f)));
    addAndMakeVisible (title);
    signatureLabel.setText ("aoi yume", juce::dontSendNotification);
    signatureLabel.setFont (juce::Font (juce::FontOptions { "Snell Roundhand", "Regular", 18.0f }.withHorizontalScale (0.94f)));
    addAndMakeVisible (signatureLabel);

    auto setupSection = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f).withStyle ("Bold")));
        addAndMakeVisible (label);
    };
    setupSection (oscillatorSection, "OSCILLATORS // TRIPLE WAVETABLE");
    setupSection (filterSection, "FILTER // NON-LINEAR");
    setupSection (envelopeSection, "ENVELOPES // AMP + FILTER");

    osc1.setRange (0.0, 1.0, 0.001); osc2.setRange (0.0, 1.0, 0.001); osc3.setRange (0.0, 1.0, 0.001);
    osc1Detune.setRange (0.0, 50.0, 0.01); osc2Detune.setRange (0.0, 50.0, 0.01); osc3Detune.setRange (0.0, 50.0, 0.01);
    unisonKeyTrack.setRange (-1.0, 1.0, 0.001);
    cutoff.setRange (20.0, 20000.0, 1.0); cutoff.setSkewFactorFromMidPoint (1000.0);
    resonance.setRange (0.1, 1.0, 0.001); drive.setRange (-12.0, 24.0, 0.1);
    saturation.setRange (0.0, 1.0, 0.001); filterEnvAmount.setRange (-1.0, 1.0, 0.001); output.setRange (-60.0, 6.0, 0.1);
    osc1.setName ("Osc 1 Wavetable"); osc2.setName ("Osc 2 Wavetable"); osc3.setName ("Osc 3 Wavetable");
    osc1Detune.setName ("Osc 1 Detune"); osc2Detune.setName ("Osc 2 Detune"); osc3Detune.setName ("Osc 3 Detune");
    unisonKeyTrack.setName ("Key Track"); cutoff.setName ("Cutoff"); resonance.setName ("Resonance");
    drive.setName ("Filter Drive"); saturation.setName ("Saturation"); filterEnvAmount.setName ("Filter Env"); output.setName ("Output");
    for (auto* s : { &osc1, &osc2, &osc3, &osc1Detune, &osc2Detune, &osc3Detune, &unisonKeyTrack, &cutoff, &resonance, &drive, &saturation, &filterEnvAmount, &output })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setNumDecimalPlacesToDisplay (2);
        s->setTooltip (s->getName());
        addAndMakeVisible (s);
    }
    addAndMakeVisible (filterType);
    filterType.addItemList ({ "Low-pass", "High-pass", "Band-pass" }, 1);
    filterType.setTooltip ("Filter type");
    addAndMakeVisible (slope);
    slope.addItemList ({ "8 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct" }, 1);
    slope.setTooltip ("Filter slope");
    addAndMakeVisible (wavetableEditor);

    midiLearnParameter.addItemList (processor.getMidiLearnTargets(), 1);
    midiLearnParameter.setSelectedItemIndex (0, juce::dontSendNotification);
    midiLearnParameter.setTooltip ("Choose a parameter for MIDI CC learn");
    addAndMakeVisible (midiLearnParameter);
    addAndMakeVisible (midiLearnButton);
    midiLearnButton.setTooltip ("Listen for the next MIDI CC");
    midiLearnButton.onClick = [this] { processor.beginMidiLearn (midiLearnParameter.getSelectedItemIndex()); };

    factoryPresetLabel.setText ("FACTORY // RANDOM BANK", juce::dontSendNotification);
    factoryPresetLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f).withStyle ("Bold")));
    addAndMakeVisible (factoryPresetLabel);
    factoryPresetMenu.addItemList (processor.getFactoryPresetNames(), 1);
    factoryPresetMenu.setTextWhenNothingSelected ("SELECT FACTORY PRESET");
    factoryPresetMenu.setTooltip ("Apply one of the 10 built-in random-generated presets");
    factoryPresetMenu.onChange = [this] { processor.applyFactoryPreset (factoryPresetMenu.getSelectedItemIndex()); };
    addAndMakeVisible (factoryPresetMenu);

    // The single 90-entry UI type selector covers all three banks.
    uiTypeLabel.setText ("UI TYPE", juce::dontSendNotification);
    uiTypeLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f).withStyle ("Bold")));
    addAndMakeVisible (uiTypeLabel);
    for (int bank = 0; bank < 3; ++bank)
    {
        uiTypeMenu.addSeparator();
        for (int i = 0; i < 30; ++i)
        {
            const int type = bank * 30 + i;
            const juce::String prefix = bank == 0 ? "A" : (bank == 1 ? "B" : "C");
            const juce::String name = bank == 0 ? juce::String (seoului::skins()[(size_t) i].name)
                                    : bank == 1 ? juce::String (seoului::layouts()[(size_t) i].name)
                                                : juce::String (seoului::combined()[(size_t) i].name);
            uiTypeMenu.addItem (prefix + " // " + name, type + 1);
        }
    }
    uiTypeMenu.setTooltip ("Select one of the 90 editor UI types (saved with the session)");
    uiTypeMenu.onChange = [this] { applyUiType (uiTypeMenu.getSelectedItemIndex()); };
    addAndMakeVisible (uiTypeMenu);

    const char* envelopeNames[] = { "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release", "Filter Attack", "Filter Decay", "Filter Sustain", "Filter Release" };
    const char* envelopeIDs[] = { "ampAttack", "ampDecay", "ampSustain", "ampRelease", "filterAttack", "filterDecay", "filterSustain", "filterRelease" };
    for (size_t i = 0; i < envelopeSliders.size(); ++i)
    {
        auto& s = envelopeSliders[i];
        s.setName (envelopeNames[i]);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        const bool sustain = (i == 2 || i == 6);
        s.setRange (sustain ? 0.0 : 0.001, sustain ? 1.0 : 10.0, 0.001);
        if (! sustain) s.setSkewFactorFromMidPoint (0.25);
        s.setNumDecimalPlacesToDisplay (2);
        s.setTooltip (envelopeNames[i]);
        addAndMakeVisible (s);
        envelopeAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, envelopeIDs[i], s);
    }

    addAndMakeVisible (loadButton);
    loadButton.setTooltip ("Import WAV, AIFF, M4A, CAF, MP3, FLAC or OGG");
    loadButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Load audio", juce::File(), "*");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, chooser] (const juce::FileChooser& c) { if (c.getResult().existsAsFile()) processor.loadAudioFile (c.getResult()); });
    };
    addAndMakeVisible (presetButton);
    presetButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Save preset", juce::File(), "*.preset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, chooser] (const juce::FileChooser& c)
                              {
                                  if (c.getResult() != juce::File())
                                  {
                                      juce::MemoryBlock d;
                                      processor.getStateInformation (d);
                                      c.getResult().replaceWithData (d.getData(), d.getSize());
                                  }
                              });
    };
    addAndMakeVisible (loadPresetButton);
    loadPresetButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Load preset", juce::File(), "*.preset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, chooser] (const juce::FileChooser& c)
                              {
                                  if (c.getResult().existsAsFile())
                                  {
                                      juce::MemoryBlock d;
                                      c.getResult().loadFileAsData (d);
                                      processor.setStateInformation (d.getData(), (int) d.getSize());
                                  }
                              });
    };

    a1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc1Pos", osc1);
    a2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc2Pos", osc2);
    a3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc3Pos", osc3);
    ad1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc1Detune", osc1Detune);
    ad2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc2Detune", osc2Detune);
    ad3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "osc3Detune", osc3Detune);
    akt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "unisonKeyTrack", unisonKeyTrack);
    ac = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "cutoff", cutoff);
    ar = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "resonance", resonance);
    ad = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "filterDrive", drive);
    as = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "saturation", saturation);
    afe = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "filterEnvAmount", filterEnvAmount);
    ao = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, "output", output);
    at = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.parameters, "filterType", filterType);
    asl = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.parameters, "filterSlope", slope);

    applyUiType (processor.getUiType());
}

HybridWavetableAudioProcessorEditor::~HybridWavetableAudioProcessorEditor() { setLookAndFeel (nullptr); }

void HybridWavetableAudioProcessorEditor::applyUiType (int type)
{
    uiType = seoului::clampType (type);
    // Keep the selector text in sync even when the change arrives from state
    // restoration or offline snapshot tooling rather than the combo itself.
    uiTypeMenu.setSelectedItemIndex (uiType, juce::dontSendNotification);
    activeSkin = &seoului::skins()[(size_t) seoului::skinIndexForType (uiType)];
    activeLayout = seoului::layouts()[(size_t) seoului::layoutIndexForType (uiType)];
    lookAndFeel.setSkin (*activeSkin);
    // setLookAndFeel with the same instance does not broadcast, so nudge
    // children (combo-box labels in particular) to re-read the new colours.
    sendLookAndFeelChange();
    wavetableEditor.setSkin (*activeSkin);
    title.setColour (juce::Label::textColourId, juce::Colour (activeSkin->text));
    signatureLabel.setColour (juce::Label::textColourId, juce::Colour (activeSkin->accentAlt));
    oscillatorSection.setColour (juce::Label::textColourId, juce::Colour (activeSkin->accent));
    filterSection.setColour (juce::Label::textColourId, juce::Colour (activeSkin->accentAlt));
    envelopeSection.setColour (juce::Label::textColourId, juce::Colour (activeSkin->accentAlt));
    factoryPresetLabel.setColour (juce::Label::textColourId, juce::Colour (activeSkin->accent));
    uiTypeLabel.setColour (juce::Label::textColourId, juce::Colour (activeSkin->accentAlt));
    resized();
    repaint();
}

void HybridWavetableAudioProcessorEditor::paint (juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    const auto& s = *activeSkin;
    const auto bgTop = juce::Colour (s.backgroundTop);
    const auto bgBottom = juce::Colour (s.backgroundBottom);
    const auto accent = juce::Colour (s.accent);
    const auto accentAlt = juce::Colour (s.accentAlt);
    const auto panelColour = juce::Colour (s.panel);
    const auto lineColour = juce::Colour (s.panelLine);

    juce::ColourGradient sky (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) h, false);
    g.setGradientFill (sky);
    g.fillRect (getLocalBounds());

    switch (s.decoration)
    {
        case 0: // Seoul skyline with window lights, river line and N Tower mast
        {
            const float horizon = (float) h - 18.0f;
            const int buildingHeights[] = { 48, 84, 58, 112, 72, 96, 42, 76, 126, 64, 88, 54, 104, 68 };
            const int buildingWidths[] = { 44, 56, 38, 62, 48, 52, 34, 58, 68, 42, 50, 36, 60, 46 };
            int buildingX = -18;
            for (int i = 0; i < 14; ++i)
            {
                const int buildingY = (int) horizon - buildingHeights[i];
                g.setColour (lineColour.withAlpha (0.55f));
                g.fillRect (buildingX, buildingY, buildingWidths[i], buildingHeights[i]);
                for (int wy = buildingY + 12; wy < (int) horizon - 8; wy += 14)
                    for (int wx = buildingX + 8; wx < buildingX + buildingWidths[i] - 5; wx += 14)
                        if (((i * 5 + wy + wx) % 7) < 3)
                        {
                            const auto light = ((i + wy / 14) % 4 == 0) ? accentAlt : accent;
                            g.setColour (light.withAlpha (0.42f));
                            g.fillRect (wx, wy, 5, 2);
                        }
                buildingX += buildingWidths[i] - 2;
            }
            const float towerX = w * 0.79f;
            g.setColour (accent.withAlpha (0.5f));
            g.drawLine (towerX, horizon - 126.0f, towerX, horizon - 20.0f, 1.4f);
            g.fillEllipse (towerX - 15.0f, horizon - 91.0f, 30.0f, 10.0f);
            g.drawLine (towerX, horizon - 126.0f, towerX, horizon - 141.0f, 1.0f);
            g.setColour (accentAlt.withAlpha (0.4f));
            g.drawHorizontalLine ((int) horizon - 12, 0.0f, (float) w);
            break;
        }
        case 1: // drafting grid
        {
            g.setColour (lineColour.withAlpha (0.2f));
            for (int x = 0; x < w; x += 24) g.drawVerticalLine (x, 0.0f, (float) h);
            for (int y = 0; y < h; y += 24) g.drawHorizontalLine (y, 0.0f, (float) w);
            break;
        }
        case 2: // CRT scanlines
        {
            g.setColour (lineColour.withAlpha (0.16f));
            for (int y = 0; y < h; y += 4) g.drawHorizontalLine (y, 0.0f, (float) w);
            break;
        }
        case 3: // aurora bands
        {
            for (int band = 0; band < 3; ++band)
            {
                juce::Path bandPath;
                const float baseY = h * (0.18f + 0.16f * (float) band);
                const float amplitude = 26.0f + 12.0f * (float) band;
                bandPath.startNewSubPath (0.0f, baseY);
                for (int x = 0; x <= w; x += 40)
                    bandPath.lineTo ((float) x, baseY + amplitude * std::sin ((float) x * 0.011f + (float) band * 1.7f));
                bandPath.lineTo ((float) w, 0.0f);
                bandPath.lineTo (0.0f, 0.0f);
                bandPath.closeSubPath();
                g.setColour ((band % 2 == 0 ? accent : accentAlt).withAlpha (0.05f));
                g.fillPath (bandPath);
            }
            break;
        }
        default: // minimal: soft vignette only
        {
            g.setColour (bgBottom.withAlpha (0.35f));
            g.drawEllipse ((float) w * 0.5f - 260.0f, (float) h * 0.5f - 260.0f, 520.0f, 520.0f, 60.0f);
            break;
        }
    }

    auto area = getLocalBounds().toFloat().reduced (10.0f);
    auto panelBox = [&g, &s, panelColour, lineColour] (juce::Rectangle<float> r, juce::Colour accentColour)
    {
        g.setColour (panelColour.withAlpha (0.84f));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (lineColour);
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
        g.setColour (accentColour.withAlpha (0.9f));
        g.fillRect (r.getX(), r.getY(), 4.0f, r.getHeight());
        g.setColour (accentColour.withAlpha (0.45f));
        g.fillRect (r.getX() + 18.0f, r.getY(), juce::jmin (120.0f, r.getWidth() - 24.0f), 2.0f);
    };
    panelBox (area.withTop (92.0f).withHeight (232.0f), accent);
    panelBox (area.withTop (338.0f).withHeight (190.0f), accentAlt);
    panelBox (area.withTop (540.0f).withHeight (area.getBottom() - 540.0f), accentAlt);
    g.setColour (accent.withAlpha (0.8f));
    g.fillRect (20.0f, 82.0f, (float) w - 40.0f, 1.0f);
}

void HybridWavetableAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    const auto& layout = activeLayout;
    const int skeleton = layout.skeleton;
    const int density = layout.density;
    title.setBounds (28, 12, 250, 42);
    signatureLabel.setBounds (154, 48, 130, 22);

    const bool compact = w < 1120;
    const int gap = juce::jmax (3, 6 - density);
    const int rowHeight = juce::jmax (26, 30 - density * 2);

    // Top tool row: preset / midi learn / audio / UI type selector.
    int topX = compact ? 420 : 430;
    const int factoryWidth = compact ? 160 : 180;
    const int midiWidth = 120;
    const int learnWidth = compact ? 70 : 80;
    const int loadWidth = compact ? 78 : 92;
    const int saveWidth = compact ? 78 : 92;
    const int typeWidth = juce::jmax (150, (compact ? 210 : 250) - density * 8);
    const int externalWidth = juce::jmax (80, w - (topX + factoryWidth + typeWidth + midiWidth + learnWidth + loadWidth + saveWidth + gap * 6 + 8));
    int x = topX;
    factoryPresetMenu.setBounds (x, 28, factoryWidth, rowHeight);
    x += factoryWidth + gap;
    uiTypeMenu.setBounds (x, 28, typeWidth, rowHeight);
    x += typeWidth + gap;
    midiLearnParameter.setBounds (x, 28, midiWidth, rowHeight);
    x += midiWidth + gap;
    midiLearnButton.setBounds (x, 28, learnWidth, rowHeight);
    x += learnWidth + gap;
    loadButton.setBounds (x, 28, loadWidth, rowHeight);
    x += loadWidth + gap;
    presetButton.setBounds (x, 28, saveWidth, rowHeight);
    x += saveWidth + gap;
    loadPresetButton.setBounds (x, 28, externalWidth, rowHeight);
    factoryPresetLabel.setBounds (topX, 7, factoryWidth, 16);
    uiTypeLabel.setBounds (topX + factoryWidth + gap, 7, typeWidth, 16);

    // Content geometry scales with density; skeletons rearrange the panels
    // while keeping the same controls visible and usable everywhere.
    const int contentTop = 92;
    const int contentBottom = h - 16;
    const int contentHeight = contentBottom - contentTop;
    const bool sideLayout = (skeleton == 1 || skeleton == 4); // sidecar / rail
    const int waveHeight = sideLayout ? contentHeight : juce::jmax (170, 232 - density * 10);
    const int waveWidth = sideLayout ? juce::jmax (300, (int) (w * (skeleton == 4 ? 0.26f : 0.34f)) - 40) : w - 40;
    const int controlsWidth = sideLayout ? w - waveWidth - 20 : w;

    wavetableEditor.setBounds (20, contentTop, waveWidth, waveHeight);

    if (sideLayout)
    {
        // Wave editor lives on the left rail; knobs and envelopes flow right.
        const int rightX = 20 + waveWidth + 10;
        oscillatorSection.setBounds (rightX + 10, contentTop, controlsWidth - 20, 20);
        filterSection.setBounds (rightX + 10 + (controlsWidth - 20) / 2, contentTop, controlsWidth / 2 - 20, 20);
        const int knobY = contentTop + 24;
        const int knobWidth = juce::jmax (56, (controlsWidth - 60) / 13);
        auto placeKnobRow = [knobY, knobWidth, rightX] (juce::Slider& s, int index) { s.setBounds (rightX + 20 + index * knobWidth, knobY, knobWidth - 5, 132); };
        placeKnobRow (osc1, 0); placeKnobRow (osc2, 1); placeKnobRow (osc3, 2);
        placeKnobRow (osc1Detune, 3); placeKnobRow (osc2Detune, 4); placeKnobRow (osc3Detune, 5);
        placeKnobRow (unisonKeyTrack, 6); placeKnobRow (cutoff, 7); placeKnobRow (resonance, 8);
        placeKnobRow (drive, 9); placeKnobRow (saturation, 10); placeKnobRow (filterEnvAmount, 11); placeKnobRow (output, 12);
        const int filterX = rightX + 20 + 13 * knobWidth + 6;
        const bool filterFits = filterX + 150 < rightX + controlsWidth;
        filterType.setBounds (filterFits ? filterX : rightX + 20, knobY + 12, 150, 26);
        slope.setBounds (filterFits ? filterX : rightX + 20, knobY + 46, 150, 26);
        const int envY = knobY + 150;
        const int envCell = juce::jmax (86, (controlsWidth - 60) / 8);
        for (size_t i = 0; i < envelopeSliders.size(); ++i)
            envelopeSliders[i].setBounds (rightX + 20 + (int) i * envCell, envY, envCell - 5, contentBottom - envY - 6);
        envelopeSection.setBounds (rightX + 10, envY - 22, 360, 20);
    }
    else if (skeleton == 2) // twin: wave and knobs share vertical halves
    {
        const int halfHeight = (contentHeight - 16) / 2;
        wavetableEditor.setBounds (20, contentTop, w - 40, halfHeight);
        const int lowerTop = contentTop + halfHeight + 16;
        const int knobY = lowerTop + 20;
        const int knobWidth = juce::jmax (56, (w - 90) / 13);
        auto placeKnobRow = [knobY, knobWidth, halfHeight] (juce::Slider& s, int index) { s.setBounds (30 + index * knobWidth, knobY, knobWidth - 5, juce::jmin (132, halfHeight - 30)); };
        placeKnobRow (osc1, 0); placeKnobRow (osc2, 1); placeKnobRow (osc3, 2);
        placeKnobRow (osc1Detune, 3); placeKnobRow (osc2Detune, 4); placeKnobRow (osc3Detune, 5);
        placeKnobRow (unisonKeyTrack, 6); placeKnobRow (cutoff, 7); placeKnobRow (resonance, 8);
        placeKnobRow (drive, 9); placeKnobRow (saturation, 10); placeKnobRow (filterEnvAmount, 11); placeKnobRow (output, 12);
        const int filterX = 30 + 13 * knobWidth + 6;
        const bool filterFits = filterX + 150 < w - 20;
        filterType.setBounds (filterFits ? filterX : w - 170, knobY + 12, 150, 26);
        slope.setBounds (filterFits ? filterX : w - 170, knobY + 46, 150, 26);
        oscillatorSection.setBounds (30, lowerTop, 340, 20);
        filterSection.setBounds (w - 380, lowerTop, 350, 20);
        const int envY = knobY + juce::jmin (140, halfHeight - 24);
        const int envCell = juce::jmax (86, (w - 60) / 8);
        for (size_t i = 0; i < envelopeSliders.size(); ++i)
            envelopeSliders[i].setBounds (30 + (int) i * envCell, envY, envCell - 5, contentBottom - envY - 4);
        envelopeSection.setBounds (30, envY - 22, 360, 20);
    }
    else if (skeleton == 3) // stage: large wave, knobs underneath in one band
    {
        wavetableEditor.setBounds (20, contentTop, w - 40, contentHeight - 180 - gap);
        const int knobY = contentBottom - 176;
        const int knobWidth = juce::jmax (54, (w - 100) / 13);
        auto placeKnobRow = [knobY, knobWidth] (juce::Slider& s, int index) { s.setBounds (34 + index * knobWidth, knobY, knobWidth - 5, 128); };
        placeKnobRow (osc1, 0); placeKnobRow (osc2, 1); placeKnobRow (osc3, 2);
        placeKnobRow (osc1Detune, 3); placeKnobRow (osc2Detune, 4); placeKnobRow (osc3Detune, 5);
        placeKnobRow (unisonKeyTrack, 6); placeKnobRow (cutoff, 7); placeKnobRow (resonance, 8);
        placeKnobRow (drive, 9); placeKnobRow (saturation, 10); placeKnobRow (filterEnvAmount, 11); placeKnobRow (output, 12);
        const int filterX = 34 + 13 * knobWidth + 4;
        const bool filterFits = filterX + 150 < w - 20;
        filterType.setBounds (filterFits ? filterX : w - 170, knobY + 8, 150, 26);
        slope.setBounds (filterFits ? filterX : w - 170, knobY + 42, 150, 26);
        oscillatorSection.setBounds (34, knobY - 24, 340, 20);
        filterSection.setBounds (w - 380, knobY - 24, 350, 20);
        const int envY = knobY + 128 + 2;
        const int envCell = juce::jmax (82, (w - 60) / 8);
        for (size_t i = 0; i < envelopeSliders.size(); ++i)
            envelopeSliders[i].setBounds (34 + (int) i * envCell, envY, envCell - 5, contentBottom - envY - 2);
        envelopeSection.setBounds (34, envY - 22, 360, 20);
    }
    else if (skeleton == 5) // grid: knob grid two rows, envelopes right column
    {
        wavetableEditor.setBounds (20, contentTop, w - 320 - gap, juce::jmax (170, contentHeight * 2 / 5));
        const int gridX = w - 300;
        oscillatorSection.setBounds (gridX, contentTop, 290, 20);
        const int cellW = 96;
        const int cellH = 92 - density * 4;
        const std::array<juce::Slider*, 13> knobs { &osc1, &osc2, &osc3, &osc1Detune, &osc2Detune, &osc3Detune, &unisonKeyTrack, &cutoff, &resonance, &drive, &saturation, &filterEnvAmount, &output };
        for (size_t i = 0; i < knobs.size(); ++i)
            knobs[i]->setBounds (gridX + (int) (i % 3) * cellW, contentTop + 24 + (int) (i / 3) * cellH, cellW - 8, cellH - 6);
        filterType.setBounds (gridX + 2 * cellW - 8 + 12, contentTop + 24 + 4 * cellH + 2, 90, 26);
        slope.setBounds (gridX + 2 * cellW - 8 + 12, contentTop + 24 + 4 * cellH + 36, 90, 26);
        filterSection.setBounds (gridX + 200, contentTop, 100, 20);
        const int envTop = contentTop + juce::jmax (180, contentHeight * 2 / 5) + 12;
        envelopeSection.setBounds (24, envTop, 360, 20);
        const int envCell = juce::jmax (82, (w - 350) / 8);
        for (size_t i = 0; i < envelopeSliders.size(); ++i)
            envelopeSliders[i].setBounds (24 + (int) i * envCell, envTop + 24, envCell - 5, contentBottom - envTop - 30);
    }
    else // stack: the classic vertical band arrangement
    {
        wavetableEditor.setBounds (20, contentTop, w - 40, juce::jmax (170, 232 - density * 10));
        const int waveHeightUsed = juce::jmax (170, 232 - density * 10);
        const int knobBandTop = contentTop + waveHeightUsed + 14;
        oscillatorSection.setBounds (30, knobBandTop, 340, 20);
        filterSection.setBounds (390, knobBandTop, w - 420, 20);
        const int knobY = knobBandTop + 24;
        const int knobWidth = juce::jmax (56, (w - 220) / 13);
        auto placeKnobRow = [knobY, knobWidth, density] (juce::Slider& s, int index) { s.setBounds (30 + index * knobWidth, knobY, knobWidth - 6, 140 - density * 6); };
        placeKnobRow (osc1, 0); placeKnobRow (osc2, 1); placeKnobRow (osc3, 2);
        placeKnobRow (osc1Detune, 3); placeKnobRow (osc2Detune, 4); placeKnobRow (osc3Detune, 5);
        placeKnobRow (unisonKeyTrack, 6); placeKnobRow (cutoff, 7); placeKnobRow (resonance, 8);
        placeKnobRow (drive, 9); placeKnobRow (saturation, 10); placeKnobRow (filterEnvAmount, 11); placeKnobRow (output, 12);
        const int filterX = 30 + 13 * knobWidth + 6;
        const bool filterFits = filterX + 150 < w - 20;
        filterType.setBounds (filterFits ? filterX : w - 170, knobY + 12, 150, 26);
        slope.setBounds (filterFits ? filterX : w - 170, knobY + 46, 150, 26);
        const int envY = knobY + 148 - density * 6 + 6;
        envelopeSection.setBounds (30, envY - 22, 360, 20);
        const int envCell = juce::jmax (86, (w - 60) / 8);
        for (size_t i = 0; i < envelopeSliders.size(); ++i)
            envelopeSliders[i].setBounds (30 + (int) i * envCell, envY, envCell - 5, contentBottom - envY - 4);
    }
}
