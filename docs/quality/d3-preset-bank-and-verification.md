# Factory preset bank and verification state

Measured: 2026-09-30
State: **implemented + Debug 14/14 + Release 14/14** (host and listening gates open)

## 1. What changed

Ten of the eleven factory presets were fixed-seed random numbers wearing names
that promised characters the generator never aimed for. "ACID VECTOR" was
whatever the PRNG produced that day. Nothing checked that a preset was
distinct, audible, or inside its parameter ranges, so one could ship silent or
duplicated and no gate would notice.

Each entry is now designed against the bank in `d2-spectral-tilt-bank.md`:
frame 0 is round (harmonic exponent 2.6), frame 7 is sawtooth-like (1.44),
frame 15 is bright and even-weighted (0.70, even partials +60%). Resonance is
written as a Q target and converted through `knobForQ()`, the inverse of the
mapping added in commit 6a8b31c, rather than guessed.

## 2. The resonance consequence that came with it

The random bank stored `resonance` in 0.15..0.9. Under the widened range that
became **Q 0.72..16.9**: every random preset became dramatically more resonant.
That is the intended consequence of the range change, but no one had listened to
it. The curated bank now uses the knob's travel as the bank's stylistic range:

| preset | Q | cutoff | character |
| --- | ---: | ---: | --- |
| 03 VOID GLASS | 0.70 | 900 Hz | closed, soft pad |
| 11 FIFTHS | 0.90 | 12 kHz | the historical default |
| 04 LASER PAD | 1.10 | 2.4 kHz | bright open pad |
| 07 STATIC BLOOM | 1.60 | 1.4 kHz | filtered swell |
| 06 NIGHT DRIVE | 2.00 | 1.4 kHz | overdriven lead |
| 08 GHOST FM | 2.20 | 1.1 kHz | band-passed hollow tone |
| 01 NEON PULSE | 2.60 | 3.2 kHz | the bright lead |
| 09 CIRCUIT BASS | 4.00 | 150 Hz | tuned sub bass |
| 02 CHROME PLUCK | 4.50 | 5.2 kHz | short metallic strike |
| 05 ACID VECTOR | 9.00 | 260 Hz | resonant bass line |
| 10 QUANTUM AIR | 16.00 | 7.0 kHz | pure resonant sweep |

## 3. Measured render report

C3 (A3, 220 Hz), 48 kHz, seed 1347568979, note-off at 1.0 s. Produced by
`FactoryPresetTests`.

| preset | peak dBFS | RMS dBFS | attack s | release s | Q |
| --- | ---: | ---: | ---: | ---: | ---: |
| 01 NEON PULSE | -8.36 | -27.30 | 0.028 | 0.242 | 2.60 |
| 02 CHROME PLUCK | -8.82 | -41.95 | 0.037 | 0.074 | 4.50 |
| 03 VOID GLASS | -20.41 | -30.69 | 0.878 | 2.257 | 0.70 |
| 04 LASER PAD | -14.18 | -30.26 | 0.674 | 1.620 | 1.10 |
| 05 ACID VECTOR | -9.29 | -21.81 | 0.047 | 0.150 | 9.00 |
| 06 NIGHT DRIVE | -6.57 | -21.14 | 0.056 | 0.224 | 2.00 |
| 07 STATIC BLOOM | -17.66 | -33.24 | 1.260 | 2.497 | 1.60 |
| 08 GHOST FM | -31.05 | -47.47 | 0.601 | 0.802 | 2.20 |
| 09 CIRCUIT BASS | -5.84 | -24.80 | 0.134 | 0.053 | 4.00 |
| 10 QUANTUM AIR | -24.70 | -40.82 | 0.210 | 1.701 | 16.00 |
| 11 FIFTHS | -7.85 | -23.78 | 0.028 | 0.288 | 0.90 |

RMS spans 26.3 dB. CHROME PLUCK attacks 23x faster than VOID GLASS and
releases 30x sooner; CIRCUIT BASS releases in 53 ms against VOID GLASS's 2.26 s.

## 4. Output headroom had to become per-preset

A resonant filter puts a peak at its corner, so a high-Q entry needs more output
attenuation than a closed one. The first draft of the curated bank left:

| preset | first draft peak | cause |
| --- | ---: | --- |
| 06 NIGHT DRIVE | **-0.01 dBFS** | Q 2.0 peaking over drive 19 dB |
| 09 CIRCUIT BASS | **-0.84 dBFS** | Q 4.0 peaking over drive 8 dB |
| 05 ACID VECTOR | -3.29 dBFS | Q 9.0 peaking over drive 14 dB |

The first two were pinned against the output ceiling, which means the ceiling was
doing the level work instead of the patch's own output stage. Their `output`
values were lowered by 8, 5 and 6 dB respectively; the measured peaks are now
-6.57, -5.84 and -9.29 dBFS.

## 5. Two of my own gate assumptions were wrong

**Decay did not discriminate.** The first fixture released the note at 2.5 s in a
3.0 s render, so all eleven presets were still ringing when the window ended and
every decay figure clustered between 2.0 and 2.8 s. The measurement was reporting
the window length. The note-off now sits at 1.0 s and release is timed from
there, which separates the pluck (0.074 s) from the pad (2.257 s).

**STATIC BLOOM is not the slowest attacker.** The gate asserted it was, and it
is not: QUANTUM AIR measures 2.36 s against STATIC BLOOM's 1.26 s. With Q 16 the
filter rings through its own amp envelope, pushing the 90%-of-peak point out past
the envelope's own attack time. That is a real property of the patch rather than
a defect, so the gate checks the ordering across the set instead of naming an
entry that was assumed to hold the extreme.

## 6. Gates

`Tests/AudioQuality/FactoryPresetTests.cpp` (new CTest target `FactoryPreset`):

1. **Contract** - every preset applies inside every parameter's advertised range,
   renders finite audio below full scale, and no two hold identical values.
2. **Ordering** - envelopes differ in the direction the names imply. QUANTUM AIR
   has the brightest position and highest Q, CIRCUIT BASS the lowest cutoff,
   CHROME PLUCK sits at the fast end, and the bank contains a genuinely slow
   attacker.
3. **Reach** - every preset is audible above -40 dBFS, none is pinned against the
   output ceiling, and the RMS spread exceeds 3 dB.

A note on how the range check reads parameter ids: they are listed explicitly
rather than enumerated. `AudioProcessorValueTreeState` exposes no accessor for its
own parameter list, and `getName()` on an APVTS parameter returns the human label
("Cutoff") rather than the id ("cutoff"), so recovering ids from
`getParameters()` silently yields empty strings and every lookup defaults to 0.0.

## 7. Verification

- Debug CTest **14/14 PASS**.
- Release CTest **14/14 PASS**, run per-binary (the whole-suite invocation was
  killed repeatedly by a concurrent agent's Release build competing for the CPU).
- Release `QualityOrder` tier line: `Eco -148.52 dBc (+4.40 vs floor) | Normal
  -154.28 (-1.46) | High -154.28 (-1.46)`, all within the +6 dB over-floor bound.
- Release `ProcessorQuality`: slope 3 measures 24.22 dB/oct against a 24 label;
  worst-case chord peak -0.009 dBFS.

## 8. Not proven

- **No listening.** Whether NEON PULSE sounds like a bright lead, or ACID VECTOR
  like an acid line, is a listening gate (`b8-blind-listening.md`). What is
  measured here is that the numbers behind the names are real and ordered.
- **No host evidence.** No pluginval, auval, REAPER or Ableton run on this binary.
- **Preset tone is a judgement.** The Q and cutoff assignments are designed from
  the bank's measured spectrum and the resonance range, not from listening. If
  an entry does not sound like its name, the number is defensible and the name
  is what should change.
