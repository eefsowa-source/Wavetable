# SQ status against the reference-synth objective

Updated: 2026-09-30

Objective: raise sound quality with reference to Vital, Serum, Massive, Surge XT.

## Closed this session

- **SQ-1** default wavetable bank had only two partials; now a full harmonic
  series. H16 -164.4 -> -35.9 dB against Vital -35.1 dB. See
  [`d1-reference-spectrum-gap.md`](d1-reference-spectrum-gap.md) (diagnosis) and
  [`d2-spectral-tilt-bank.md`](d2-spectral-tilt-bank.md) (measured result).
- Two gates that silently assumed frame 0 was a pure sine now install their own
  sine source, so they measure the stage they are named for.

## Reference availability on this machine

Measured this session, not assumed:

| reference | state |
| --- | --- |
| Serum, Serum 2 | installed; Serum has an existing Ableton stem |
| Vital | installed; has an existing Ableton stem |
| Surge XT | installed (VST3 + standalone), **no stem rendered yet** |
| Massive | **not installed** |

So only Serum and Vital can be cited as recorded evidence today. Surge XT needs
one Ableton render of the same fixture; Massive is unavailable and should not
be claimed until it is.

## Also landed by concurrent work in this repo (not this task)

Commits `6a8b31c`, `2207417`, `8ef48ae`, `890899a` widen the resonance range to
Q 0.5-20 and repair the gates that change invalidated. Those are a separate
change with their own evidence and are not attributed to SQ-1.

## Open

1. Host evidence on the current binary: pluginval, `auval`, REAPER and Ableton
   load. None has been run on `8c3e693`.
2. Listening. Every quality claim here is measurement only.
3. Surge XT stem on the same fixture.
4. Golden promotion, still blocked as before.
