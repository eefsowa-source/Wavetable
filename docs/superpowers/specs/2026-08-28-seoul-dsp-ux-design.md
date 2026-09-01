# SEOUL DSP UX Design

## Approved direction

SEOUL DSP is a practical cyberpunk wavetable synthesizer for Ableton Live and Logic Pro. The primary view keeps the waveform editor, three oscillator position controls, filter controls, and amp/filter envelopes visible without overlapping labels. The product identity is `SEOUL DSP`; the small cursive maker mark remains `aoi yume`.

## Interaction rules

- Every rotary control has a visible name.
- Numeric values are shown only while the control is being dragged.
- Preset load/save and the ten generated factory presets are available inside the plug-in editor.
- Audio import accepts JUCE basic formats (including WAV, AIFF, M4A/CAF where the host codec is available).
- Resizing preserves readable spacing; controls compress before panels overlap.

## DSP foundation

The parameter/state layer uses stable IDs for oscillator level, tune, unison, spread, filter-envelope amount, voice mode, glide, and stereo width. Realtime voice rendering remains preallocated and allocation-free. Filter resonance, drive, saturation, and envelope modulation are part of the core signal path.

## Verification gates

Source/build, unit/state tests, pluginval, `auval`, DAW loading, automation, preset restoration, and real audio import are reported separately so unavailable host checks are not overstated.

## 2026-08-28 targeted DSP repair

### Scope

This repair makes the already-registered oscillator spread, master width, and
arpeggiator gate controls behave in the audio engine. It does not add new
parameters, change stored parameter IDs, alter the editor layout, or attempt
host-cache migration.

### Stereo signal path

`SynthVoice` will render a genuine stereo signal directly into the processor
buffer. Each oscillator receives a deterministic left/right placement derived
from its existing `osc{N}Spread` parameter: oscillator 1 moves left,
oscillator 2 moves right, and oscillator 3 alternates its direction by MIDI
note parity so it broadens chords without a global shared state. Both channels
retain their own filter state. The processor then applies `masterWidth` with a
mid/side transform: `0` folds to mono, `1` preserves the voice stereo image,
and `2` doubles its side component. No delay line, allocation, lock, UI call,
or filesystem access is introduced into the audio callback.

### Arpeggiator event contract

While enabled, the arpeggiator converts held incoming MIDI notes into scheduled
note-on and note-off events. A step lasts `sampleRate / arpRate` samples; the
active note is released after that duration multiplied by `arpGate`, clamped to
at least one sample. A new step always releases any still-active arpeggiated
note before starting its replacement. Disabling the arpeggiator immediately
emits a note-off for its active generated note and clears its held-note and
timer state, while normal incoming MIDI continues to pass through.

### Regression coverage

The processor smoke test will assert that a spread voice produces distinct
left/right output, that master width zero collapses those outputs to identical
channels, that a partial arpeggiator gate releases its note before the next
step, and that disabling the arpeggiator sends its active note-off. Tests use
the real processor/MIDI path and do not inspect private implementation state.
