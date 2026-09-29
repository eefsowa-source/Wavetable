# SQ-1 — spectrum gap against the reference wavetable synths

Measured: 2026-09-30
State: **diagnosis (renderer-backed); fix and measured result in d2-spectral-tilt-bank.md**

## 1. Question

The recorded Ableton stems (`b8-reference-stems-analysis.md`) held only peak/RMS.
Those cannot say whether the instrument is *thin* or *bright*. The benchmark
protocol in `b8-reference-benchmark.md` asks for a spectral comparison, so this
task measures one on the exact same fixture: a clean C4 (261.6256 Hz), one note,
no automation, 48 kHz.

References used: Serum, Vital (Ableton stems already on disk) and Surge XT
(installed as `/Applications/Surge XT.app`, not yet rendered through Ableton).
Massive is not installed on this machine, so it is not claimed as evidence here.

## 2. Method

Analysis window: samples 9600..25583 of the render (16384 points, Hann), taken
after the attack so the envelope is in steady state. Power-weighted spectral
centroid over 0..24 kHz, plus per-harmonic energy in a +/-4-bin band around
`h * 261.6256 Hz`, in dB relative to total bin power.

The same script was run on the project's own offline render
(`AudioQualityRunner --fixture-group foundation`, fixture `single-note-60-clean`,
default patch, seed 1397051221), so the comparison does not depend on the host.

## 3. Measurement

| render | centroid | H1 | H2 | H3 | H4 | H8 | H16 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Serum (Ableton) | 1418 Hz | -47.1 | -4.8 | -47.0 | -10.8 | -16.8 | -22.9 |
| Vital (Ableton) | 1420 Hz | -69.4 | -2.3 | -72.5 | -8.3 | -14.3 | -20.3 |
| SEOUL DSP (offline, this project) | **264 Hz** | -0.0 | **-20.5** | **-52.1** | **-64.6** | -144.5 | -151.3 |

Both references carry a full harmonic series with even-harmonic emphasis and a
centroid near 1.4 kHz. The project renders a near-sine: H1 dominates at 0 dB and
everything above H2 has collapsed (H4 is 44 dB below the references' H4, H8 is
128 dB down). The reference stems even show a suppressed *fundamental* (H1 at
-47/-69 dB) with the second harmonic loudest, which is what a detuned five-voice
stack does; the project's H1-dominant balance is the opposite signature.

## 4. Root cause

It is not the filter, the drive, or the saturation stage. The built-in table
content is the cause. `WavetableData::WavetableData()` builds every one of the 16
frames from

```
sin(x) * (1 - 0.35 m) + 0.25 * sin(2x) * m        m = frame / 15
```

which contains **exactly two partials, H1 and H2** (measured: H3 and above are at
-180 dB, i.e. the float64 numerical floor, for every frame). A wavetable built
from two partials can only ever sound like a sine with a touch of second
harmonic, and the bank-limiting cannot invent content that is not there.

The mip bank is correct and is not the cause: its caps
`{1024, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1}` are strictly decreasing, the
selected level always satisfies `cap <= 0.5 / increment` (asserted in
`Tests/WavetableTests.cpp` over 20 Hz..24 kHz), and at C4 the cap is 64, so
harmonics up to H64 would pass through untouched.

This is why the offline render peaks at -15 dBFS with 6.8 dB more RMS than the
references while still being spectrally empty: it is a loud sine, not a bright
wavetable tone.

## 5. Fix

Replace the two-partial default frames with a 16-frame bank built from a
harmonic series with the sawtooth-style `1/h` rolloff and a morph that sweeps
the spectral tilt, so frame 0 is a sine-like start, the middle is a full saw,
and frame 15 is a bright, thin wave. Each frame is DC-free by construction and
peak-normalised to a constant level so moving the wavetable knob does not also
move the volume. See `Source/DSP/WavetableOscillator.cpp` and
`docs/quality/d2-spectral-tilt-bank.md` for the measured result: H16 moves from
-164.4 dB (float floor) to -35.9 dB, within 0.8 dB of Vital.

## 6. What this does not prove

- No listening result. The change is justified by the measured spectral gap and
  the offline/CTest gates only; SQ-6 listening is still open.
- The centroid figures in section 3 are not a ranking across the four renders;
  see section 3 of `d2-spectral-tilt-bank.md` for why the Ableton stems are not
  patch-comparable.
- Surge XT is named as a reference target but no rendered Surge XT stem exists
  yet, and Massive is absent from the machine. Neither is claimed as evidence.
- Serum/Vital numbers come from one shared fixture in one host session. They
  establish the gap, not a quality ranking.
