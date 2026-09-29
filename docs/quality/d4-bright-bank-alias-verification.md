# SQ-4 — the bright bank does not alias

Measured: 2026-09-30, immediately after `8c3e693`
State: **measured (renderer-backed) + automated gate**

## 1. Why this had to be checked

`d2-spectral-tilt-bank.md` replaced a two-partial default bank with a full
harmonic series. That necessarily puts far more energy near Nyquist than the
old wave did, so the obvious failure mode is that the new bank aliases on high
notes. A bright spectrum and an aliased one look identical in a total-energy
measurement, so peak/RMS/centroid numbers cannot answer this.

The measurement distinguishes them by structure rather than level:

- a **harmonic** partial sits exactly on `h * f0`;
- an **aliased** partial appears at a *fold* frequency, `|h*f0 - k*sr|`, and
  therefore lands between the harmonics, near the fundamental for the common
  low-order folds.

## 2. Method

Offline renders from `AudioQualityRunner` (`--fixture-group foundation`, default
patch, 48 kHz), BEFORE from `Build/quality-probe/` at commit `748f86b` and AFTER
from `Build/quality-sq/` at `8c3e693`. Same fixture, same patch, same code path;
only the table content differs.

Each render is analysed with a 32768-point Hann window taken after the attack.
Every bin within +/-8 bins of `h*f0` is marked harmonic; whatever power remains
is inharmonic (alias plus numerical noise). Separately, power is summed in
narrow bands around each *predicted fold* frequency to test aliasing directly.

## 3. Result

Inharmonic power relative to harmonic power:

| note | f0 | before | after | change |
| --- | ---: | ---: | ---: | ---: |
| 24 (A1) | 55.0 Hz | +19.6 dBc | +7.3 dBc | -12.3 |
| 96 (C7) | 2093.0 Hz | -66.3 dBc | -65.3 dBc | +1.0 |
| 108 (A7) | 4186.0 Hz | -62.0 dBc | -61.6 dBc | +0.4 |

Power sitting at *predicted fold* frequencies, relative to total power:

| note | before | after |
| --- | ---: | ---: |
| 96 (C7) | -112.6 dB | -84.4 dB |

## 4. Reading this honestly

**The new bank does not alias.** Across the top two octaves the inharmonic
content moved by about 1 dB, from -66.3 to -65.3 dBc at C7 and -62.0 to -61.6
dBc at A7. A bright wave and an aliased wave cannot differ by 1 dB: the total
energy above half-Nyquist really did rise by about 84 dB (see the raw
half-Nyquist ratio), and it is carried by partials sitting on exact integer
multiples of the fundamental. The mip bank is doing its job - it is passing
every harmonic that fits under Nyquist and removing the ones that do not.

The C7 fold band is -84.4 dBc. That is 28 dB *below* the inharmonic floor at the
same note, which says the small amount of fold-band energy is not the mechanism
producing the inharmonic content; it is the leakage of the Hann main lobe
around the very strong low-order harmonics. No fold is the dominant term.

**Note 24 is not an aliasing regression.** Its inharmonic energy is *lower*
after the change (19.6 -> 7.3 dBc, a 12.3 dB improvement), and 100% of it sits
below 500 Hz with none above 2 kHz. A 55 Hz note with a bright 1/h spectrum and
the plugin's resonant low-pass produces low-frequency sidebands around the
partials; that is the resonance range from commit `6a8b31c` becoming audible on
a bass note, not fold-back. Nothing here folds, because at 55 Hz the mip cap is
1024 and no harmonic approaches Nyquist in the first place.

## 5. Automated gate

`Tests/WavetableTests.cpp` renders the real `WavetableOscillator` at eight probe
frequencies across three bank positions, marks every bin within +/-8 bins of
`h*f0` as harmonic, and requires the remaining power to stay below -45 dBc.
Measured readings on the current bank: A7 -63.0 / -63.6 dBc, C7 -67.3 /
-64.6 dBc, C4 -62.6 / -64.3 dBc. The lowest is about -62 dBc, so the bar carries
17 dB of margin and still fails loudly if bank limiting ever breaks.

Two things that had to be fixed in the harness before it could measure anything,
both of which had been reporting clean audio as broken:

- **No window.** The analysis ran on an unwindowed render. A rectangular window
  leaks every harmonic across the spectrum, so the gaps between harmonics fill
  with leakage: the same render measured -18.4 dBc unwindowed and -64.2 dBc
  with a Hann window. A Hann window is now applied, and the comment in the
  file says why it is not optional.
- **A rounded divisor placed the mask wrong.** The harmonic mask index was
  computed as `round(frequency / round(sr / fftSize))` - frequency divided by an
  integer count of *hertz* rather than by the bin width. That misplaced every
  mask by roughly a factor of three. The divisor is now `sr / fftSize` as a
  double.

Known limit, stated rather than hidden: below about 200 Hz the probe is not
integer-cycle inside a 16384-sample window (55 Hz is 18.8 cycles), and the
frame-15 position reads about -19 dBc there. That is the harness, not the
oscillator - the same 55 Hz tone rendered through the real processor measures
-62.2 dBc. Low notes need a longer analysis window than this gate renders, so
they are excluded from the assertion instead of being asserted against a
number the harness cannot produce.

## 6. Not proven

- This is one fixture at 48 kHz. The `b8-reference-benchmark.md` matrix
  (44.1/48/96 kHz, buffers 64/512) has not been run against the new bank.
- No listening. A clean measurement does not say whether the brighter default
  sounds better, only that it is not broken.
- Surge XT and Massive remain unrendered; Massive is not installed.
- The sub-200 Hz region is measured only through the full processor path, not by
  the in-process oscillator gate, for the window-length reason in section 5.
