# Plan C SQ-5 - filter resonance range (reference-synth comparison)

Measured: 2026-09-30
State: **measured + implemented + automated gates pass** (host and listening gates open)

## 1. Question

The reference set (Vital, Serum, Massive, Surge XT) all put a pronounced resonant
peak at the top of their resonance control, and that peak is where their character
lives. Does this synth's knob reach one?

`docs/quality/b4-filter-slopes.md` section 4 had recorded that the resonance range
was effectively meaningless and deferred widening it, because it changes every
existing preset. This task closes that deferral with a measurement.

## 2. Instrument (renderer-backed)

`Tests/AudioQuality/FilterResonanceTests.cpp` measures through the real
`HybridWavetableAudioProcessor::processBlock`. The knob's reach is a property of the
wrapper and its parameter plumbing, not of the TPT section: a DSP-core unit test
would read `R = 1/(2*q)` and call the range fine.

The gate calibrates itself before trusting any knob number. The section obeys
`|H(j*w0)| = Q`, so corner gain must equal `20*log10(Q)`:

| Q | measured | theory | error |
| ---: | ---: | ---: | ---: |
| 0.25 | -12.041 dB | -12.041 dB | +0.000 |
| 1.00 | +0.000 dB | +0.000 dB | +0.000 |
| 4.00 | +12.041 dB | +12.041 dB | +0.000 |
| 16.00 | +24.082 dB | +24.082 dB | +0.000 |

0.000 dB error. This is the same injected-line discipline `QualityOrderTests` uses
so its alias meter cannot pass vacuously.

Both renders are at the same frequency, because a reference tone elsewhere would
read the filter's own rolloff and the processor's 100 Hz mono-bass crossover into
the ratio. The first draft of this instrument normalised against a "passband" tone
at a quarter of the corner; for the high-pass that tone sits in the stopband, which
made HP read 24 dB louder than LP while the section itself is symmetric to the
sample. It also asserted a 2.7 dB calibration error at Q 0.25 because a quarter of
the corner is already 9.3 dB down, not the 12 dB Q implies.

The gate also verifies the probe never reaches the output ceiling, so the limiter
cannot compress the quantity being measured.

## 3. Before (RED)

The knob handed its 0.1..1.0 range straight to the section as Q:

| knob travel | parameter | measured Q |
| ---: | ---: | ---: |
| 0% | 0.100 | 0.10 |
| 50% | 0.550 | 0.55 |
| 100% | 1.000 | **1.00** |

The maximum is **Q = 1.00, a 0 dB corner**. No peak anywhere on the travel, no
ringing. The knob was a damping trim, not a resonance control.

## 4. Fix

`SeoulDSPQuality::filterResonanceQ` maps the parameter to `Q = 0.5 * 40^travel`,
reaching Q 20 at full travel.

* The parameter keeps its id, its 0.1..1.0 range and its stored values, so
  sessions, presets and MIDI CC assignments are untouched. Only the
  interpretation of the number moves.
* Exponential because Q is a ratio: equal steps of Q are equal steps of pitch in
  the ring. A linear knob would spend most of its travel in an inaudible octave.
* Endpoints: Q 0.5 is slightly past flat (a closed filter), Q 20 is a resonant peak
  that sustains a pitched tail without approaching self-oscillation.
* Smoothing moved to Q space, so the exponential stays at block rate and a 50 ms
  glide is a linear ramp in Q.

## 5. After (GREEN)

| knob travel | parameter | Q | corner gain |
| ---: | ---: | ---: | ---: |
| 0% | 0.100 | 0.50 | 0 dB |
| 40% | 0.460 | 2.19 | +12.8 dB |
| 80% | 0.820 | 9.56 | +25.6 dB |
| 100% | 1.000 | **20.00** | **+32.0 dB** |

Ringing, counted as sign changes within 60 dB of the peak: Q 0.5 -> 0 cycles,
Q 4 -> 8, Q 8 -> 17, Q 16 -> 35, Q 20 -> **44**.

## 6. Existing gates corrected, not loosened

### 6.1 The slope gate was reading a resonant skirt, not an asymptote

`ProcessorQualityTests` passed `resonance = 1.0` as "maximum resonance, minimal
damping". That is now Q = 20, where the peak's skirt swamps the rolloff being
measured, and slopes 1/2/3 all read **-3.2 dB/oct** instead of 12/18/24.

The slope did not change. Measured on the bare section at a 60 Hz corner:

| slope | Q 0.5 | Q 1 | Q 4 | Q 20 |
| ---: | ---: | ---: | ---: | ---: |
| 1 (12 dB) | 11.98 | 12.11 | 12.15 | 12.14 |
| 3 (24 dB) | 24.10 | 24.23 | 24.27 | **6.59** |

The gate now measures at a named Q = 1, knob 0.2691, derived from the mapping
(`Q = 0.5*40^travel` puts Q 1 at travel `ln2/ln40 = 0.1879`) rather than guessed. The
first guess of 0.7 was checked and corrected: it maps to Q 5.85.

Measured slopes after: **5.99 / 12.09 / 18.09 / 24.22 dB/oct**.

### 6.2 The filterDrive gate asserted a number about the source, not the drive

It compared driven THD against a fixed multiple of the 0 dB render. The probe
plays frame 0 of the shipped bank, so if that bank ever stops being a pure sine the
ratio measures the bank. It now requires monotonicity in the knob
(0 dB < +12 dB < +24 dB), which holds regardless of source spectrum.

### 6.3 The saturation ceiling gate was measuring filter skirt as alias

This one was found only by running the suite, and it is the most interesting of the
three. Widening the resonance range made `QualityOrder` fail: the Eco tier moved from
-153.35 to **-148.50 dBc** against a -150 dBc ceiling, with no change to the
saturation stage.

Cause: the probe tone is ~10 kHz (MIDI 123), so its third harmonic lands near
**30 kHz**, above the filter corner. `filterEnvelopeCutoff` clamps the corner to
`0.45*sr` = 21.6 kHz, so the filter always attenuates the harmonic whose fold is
being measured, and how much depends on the Q the filter is running. Raising the
corner to 21 kHz changed nothing, confirming it was Q and not level.

The absolute ceiling was folding that skirt into a number labelled "saturation
alias". Each tier is now gated against a floor rendered with the identical
configuration and only the nonlinearity bypassed:

| tier | measured | own bypass floor | margin |
| --- | ---: | ---: | ---: |
| Eco | -148.50 dBc | -152.88 dBc | **+4.38 dB** |
| Normal | -154.22 dBc | -152.81 dBc | -1.41 dB |
| High | -154.22 dBc | -152.81 dBc | -1.41 dB |

Normal and High sit *below* their own floor, i.e. at the instrument limit. Eco sits
+4.38 dB above it, which is real Eco-tier alias and agrees with
`c2-high-tier-verdict.md` (Eco -153.1 vs Normal -156.6 on the older probe). The gate
now bounds what a tier *adds*, at +6 dB.

## 7. Verification

Debug CTest **13/13 PASS** in an isolated worktree (`git worktree add`), so a
concurrent agent editing the main checkout could not contaminate the run.

* instrument calibration error 0.000 dB
* knob reach Q 0.50 -> 20.00, monotonic, loudest at top of travel
* all slope x type x Q combinations finite; the resonant section's corner gain
  tracks Q to 0.000 dB
* probe verified below the output ceiling across the whole travel

## 8. Guarantees and limits

Guaranteed:

* the resonance knob reaches a real resonant peak (+32 dB corner) and a sustained
  tail, monotonically, with the loudest setting at the top of the travel
* minimum resonance is a damped filter (Q 0.5)
* the four slope labels match measurement within 0.23 dB
* the saturation tiers' alias is now bounded relative to their own floor

Not guaranteed:

* **Listening.** This change is justified by measurement and CTest only. The
  level-matched blind listening gate (SQ-6) is open.
* **Existing preset tone.** Presets deliberately get more resonance. That is the
  intent, and it is recorded here rather than hidden, but how a stored session
  sounds is the listening gate's question.
* **IMD, alias, CPU** for this knob change. No separate measurement.
* **Host gates.** pluginval, auval and DAW load are separate gates and were not run
  in this commit.
* **Massive** is not installed on this machine and is not claimed as evidence.
  Serum, Vital and Surge XT are installed; only Serum and Vital have rendered
  reference material on disk.
