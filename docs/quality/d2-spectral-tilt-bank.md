# SQ-2 — the default wavetable bank, before and after

Measured: 2026-09-30
State: **implemented + Release 13/13, Debug 13/13** (host and listening gates open)

## 1. What changed

`WavetableData::WavetableData()` built all 16 frames from
`sin(x)*(1-0.35m) + 0.25*sin(2x)*m`, a wave with exactly two partials. The bank
is now a harmonic series whose tilt sweeps across the morph axis:

```
rolloff(h) = 0.70 + 1.90 * (1 - morph)^1.5      exponent on h
even bias  = 1.00 + 0.60 * morph
frame      = peak-normalised sum over h of  evenWeight(h) * sin(h*x) / h^rolloff
```

512 partials, exactly DC-free by construction, peak-normalised so the position
knob is a timbre control and not a level control.

The (1 - morph)^1.5 curve makes the bank monotonic in brightness: soft and round
at frame 0 (exponent 2.6), sawtooth-like near frame 7 (1.44), bright and slightly
even-weighted at frame 15 (0.70, even partials +60%). Frames 11-15 land within a
few dB of the rolloff both references measured, and frame 7's H2..H8 track them
closely.

## 2. Measured result

Same fixture as `d1-reference-spectrum-gap.md`: a clean C4 (261.6256 Hz), one
note, no automation, 48 kHz, 16384-point Hann window taken after the attack.
The BEFORE column is `Build/quality-probe/` (commit 748f86b), AFTER is
`Build/quality-sq1/` (this change), both produced by `AudioQualityRunner`.

| render | centroid | H1 | H2 | H3 | H4 | H8 | H16 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| before | 264 Hz | -2.3 | -23.1 | -53.4 | -66.0 | -147.4 | -164.4 |
| **after** | **364 Hz** | -3.0 | -12.3 | -18.9 | -20.5 | **-29.2** | **-35.9** |
| Vital (Ableton) | 2628 Hz | -51.0 | -17.8 | -46.8 | -23.9 | -29.8 | -35.1 |
| Serum (Ableton) | 11714 Hz | -44.1 | -41.2 | -43.8 | -43.3 | -36.1 | -37.2 |

The upper-harmonic structure now matches Vital closely: H8 -29.2 vs -29.8 dB,
H16 -35.9 vs -35.1 dB, H4 -20.5 vs -23.9 dB. H16 moved 128 dB, from the float
floor to within 0.8 dB of the reference.

## 3. Reading the centroid honestly

The centroid columns are not comparable across the four renders and should not
be quoted as a ranking. The Ableton stems were captured with the instruments'
own default presets, which are detuned multi-voice stacks with the filter open
and an envelope moving, so their centroids (2628 Hz and 11714 Hz) measure those
presets, not a comparable single-oscillator tone. The BEFORE/AFTER pair is the
controlled comparison: same fixture, same patch, same code path, only the table
content differs. Per-partial values are the meaningful comparison, and they are
what this change is justified by.

## 4. Gate changes this required

Two existing gates depended on frame 0 being a pure sine, which the old
two-partial bank happened to guarantee and the new bank does not.

`QualityOrderTests`: the folded-alias proxy band is `|3*f0 - 48000| +/- 600 Hz`.
At the 10 kHz probe, 3 * 10002.18 = 30006.5 Hz, which folds to exactly
17993.47 Hz - the centre of the band. The new frame 0 carries H3 at -24.8 dB, so
the table's own third harmonic arrived in the measurement band before the
saturation stage did anything, and the metric could no longer separate
oscillator harmonics from nonlinearity folds. Measured with the new bank and
the old premise, the product gate read -113.80 dBc against a -150 dBc ceiling.
With the probe installing its own sine table it reads -154.28 dBc. Note that
`f1 + 2*f1 = 3*f1` is an identity, so the H2 sum-frequency cannot be avoided by
changing probe pitch; loading a clean source is the only sound fix.

`ProcessorQualityTests`: the drive gate compares THD at 0 / +12 / +24 dB. Frame
0 alone contributes about 18% THD, which both raised the 0 dB reference and made
the +12 dB point noisy (17.77 -> 17.75 -> 20.81, non-monotone). With a sine
source it reads 0.0034% -> 4.85% -> 13.80%, which is the drive stage measured on
its own.

Both probes now install their own sine table rather than inheriting whatever the
shipped bank contains, so a future change to the default bank cannot silently
redefine what those gates measure.

## 5. Regression coverage added

`Tests/WavetableTests.cpp` gained a spectral block that would have caught the
original defect: every frame must clear a self-calibrated numerical floor (a
pure sine measured in the same transform, +20 dB) at H8 and H16; frame 7 must
reach H16 below -25 dB; frame 15 below -10 dB; the bright end must exceed the
soft end by 30 dB at H16; all frames must share one peak; and no frame may carry
DC. The measured bank satisfies these with margin.

## 6. Gates

- Release CTest **13/13 PASS**; Debug CTest **13/13 PASS**.
- Pre-existing `FilterResonance` / `ProcessorQuality` slope and resonance
  behaviour unchanged (those gates come from commit 6a8b31c).

## 7. Not proven

- No host evidence: no pluginval, auval, REAPER or Ableton run on this binary.
- No listening evidence. The change is justified by measurement alone.
- Surge XT and Massive are not yet rendered through the same fixture, so this
  record compares against Serum and Vital only.
- Golden promotion remains blocked, as it was before this change.
