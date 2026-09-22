# Serum and Vital reference benchmark

Serum and Vital are useful installed Ableton references for SEOUL DSP because
they are established wavetable instruments with strong oscillator, filter,
unison, modulation, and high-register behavior. They are reference instruments
for measurable behavior and level-matched listening; their implementation or
factory content is not a production dependency.

## Installed Ableton evidence

On 2026-09-22, Ableton Live `12.4.5 (2026-08-19_225ce5e356)` exposed the
following entries in the Plug-Ins browser with the VST3 format selected:

- `Serum`
- `Serum 2`
- `SerumFX`
- `Serum 2 FX`
- `Vital`

The set was at `48.0 kHz`. The browser inventory was then exercised without
discarding or saving the user's set: a new `3-Serum` MIDI track contains the
`Serum` device, and a new `4-Vital` MIDI track contains the `Vital` device.
The first `Serum` search result was `SerumFX`, which remains on the pre-existing
Track 2 and is not used as the synth reference. No reference MIDI was played
and no reference audio was rendered from these devices yet.

## Comparison protocol

The first comparison pass should use a clean Ableton set with one reference
instrument and one SEOUL DSP instance on separate MIDI tracks. Use the same
sample rate, buffer, MIDI clip, note velocity, pitch, oscillator waveform,
phase-reset rule, and output gain. Match short-term loudness within 0.1 dB
before listening; otherwise the louder instrument will bias the result.

Capture the following cases at 44.1, 48, and 96 kHz with buffers 64 and 512:

1. A single oscillator playing a low, middle, and high register note to expose
   aliasing and pitch-dependent brightness.
2. A cutoff and resonance sweep at matched oscillator level to compare filter
   slope, resonance shape, self-oscillation, and drive behavior.
3. Unison at 1, 4, and 8 voices with the same detune and stereo spread to
   compare phase coherence, image width, peak growth, and CPU cost.
4. A fast envelope and short transient to compare attack stability, overshoot,
   denormal behavior, and tail cleanliness.
5. A 10 kHz test tone and a silence render for folded energy, DC, peak, and
   denormal or non-finite output checks.

Record the exact host version, sample rate, buffer, preset or waveform source,
parameter state, output gain, binary SHA-256, and rendered file SHA-256 for
each case. Use the measurements to identify regressions and use a blind,
level-matched listen for the final quality decision.

## Scope boundary

Serum and Vital are strong references for clean wavetable synthesis. They do
not establish analog transformer, tube, console, or mastering behavior, so
they cannot by themselves prove Brainworx or UAD parity. The current SEOUL DSP
Plan B already has offline alias, safety, CPU, plugin-contract, and host-load
evidence; this benchmark adds a reproducible commercial-synth comparison
layer before any further color algorithm is promoted.
