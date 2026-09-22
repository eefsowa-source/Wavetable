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
Track 2 and is not used as the synth reference. The existing MIDI clip was
copied to the Serum and Vital tracks for staging, but the render gate below
uses a separate clean MIDI-only fixture with no automation.

A separate clean one-note clip was then created in the Session View: C4,
1/16-note duration, velocity 100, with no automation. It was copied to the
Serum, Vital, and SEOUL DSP reference slots and launched one track at a time
at 48 kHz. Live reported these peak readings:

| reference | final track gain | observed peak |
| --- | ---: |
| Serum | 0.0 dB | -9.68 dBFS |
| Vital | 0.0 dB | -9.04 dBFS |
| SEOUL DSP | +4.0 dB | -9.43 dBFS |

The CPU meter remained around 20–24% during the checks. The final peak range
is 0.64 dB, with a maximum deviation of about 0.39 dB from the median. These
readings prove host playback and signal flow for the clean clip and establish
a practical level-matched starting point. They do not establish a quality
ranking or replace rendered-file analysis and blind listening.

## Rendered reference stems

The clean one-note fixture was rendered from Ableton Arrangement one track at
a time on 2026-09-22. The render selection started at `1.1.1` and covered the
one-bar fixture. Ableton Live was `12.4.5 (2026-08-19_225ce5e356)`, the set was
at `48.0 kHz`, and the export format was stereo WAV, `24-bit`, with triangular
dither, Normalize Off, Convert Mono Off, Return/Main Off, and Render as Loop
Off. The three files are retained under `Build/quality-reference-stems/` and
the complete file analysis is in
`docs/quality/b8-reference-stems-analysis.md`.

| stem | file size | duration | peak | RMS | SHA-256 |
| --- | ---: | ---: | ---: | ---: | --- |
| Serum | 576,080 bytes | 2.000 s | -9.684 dBFS | -29.452 dBFS | `8412251c194ac28bcc8667827e89a80bb7e53cd9004b285aadc70880590de7f9` |
| Vital | 576,080 bytes | 2.000 s | -9.040 dBFS | -28.619 dBFS | `26ce240fbc0dec1648a6016c3ad3458e8b7f19d792278d97f87e447f96171034` |
| SEOUL DSP | 576,080 bytes | 2.000 s | -9.425 dBFS | -23.122 dBFS | `c75c3b6baef97a7f78236a6c2c4d7f374a04c9f1cfedac050d99a813c146c8fc` |

All three renders are 2-channel, 48,000-frame files with a measured DC mean
within 0.000025 of zero. These values confirm the exported signal and the
level-matched peak range; they do not establish a sonic quality ranking. The
render capture did not record a verified Ableton buffer-size value, so buffer
size remains open for the next repeatable measurement pass.

## Comparison protocol

The first comparison pass should use a clean Ableton set with one reference
instrument and one SEOUL DSP instance on separate MIDI tracks. Use the same
sample rate, buffer, MIDI clip, note velocity, pitch, oscillator waveform,
phase-reset rule, and output gain. Match short-term loudness within 0.1 dB
before listening; otherwise the louder instrument will bias the result.

The clean MIDI-only fixture and separate solo renders are now complete. The
next gate is a level-matched blind listening pass over these exact files,
followed by the wider sample-rate and buffer matrix below.

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
