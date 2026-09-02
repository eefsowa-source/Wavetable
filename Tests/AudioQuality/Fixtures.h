#pragma once

#include "AudioQualityTypes.h"
#include <cmath>

namespace audioquality
{
inline AudioQualityFixture makeSingleNoteFixture (int note = 60)
{
    AudioQualityFixture fixture;
    fixture.id = "single-note-" + juce::String (note);
    fixture.midi.push_back ({ juce::MidiMessage::noteOn (1, note, 0.8f), 0 });
    fixture.midi.push_back ({ juce::MidiMessage::noteOff (1, note), (int) std::llround (fixture.durationSeconds * fixture.sampleRate * 0.5) });
    return fixture;
}
}
