# Ableton reference stem analysis

This record covers the rendered comparison stems for the SEOUL DSP benchmark.
It is signal and file evidence, not a listening result or a product quality
ranking.

## Render provenance

- Host: Ableton Live `12.4.5 (2026-08-19_225ce5e356)`
- Set: `qa-stage2-ableton-recall`
- Sample rate: `48.0 kHz`
- Selection: Arrangement, starting at `1.1.1`, one-bar clean fixture
- Output: Main, stereo WAV, `24-bit`
- Dither: Triangular
- Normalize: Off
- Convert Mono: Off
- Render as Loop: Off
- Return/Main: Off
- Track state: one of Serum, Vital, or SEOUL DSP soloed per render

The render capture did not record a verified Ableton buffer-size value. CPU
meter readings during the host checks were approximately 20–24%.

## Measured files

| file | channels | sample rate | frames | duration | peak dBFS | RMS dBFS | DC mean | SHA-256 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `Build/quality-reference-stems/serum.wav` | 2 | 48,000 Hz | 96,000 | 2.000000 s | -9.684 | -29.452 | -2.47257631e-05 | `8412251c194ac28bcc8667827e89a80bb7e53cd9004b285aadc70880590de7f9` |
| `Build/quality-reference-stems/vital.wav` | 2 | 48,000 Hz | 96,000 | 2.000000 s | -9.040 | -28.619 | 1.34355829e-05 | `26ce240fbc0dec1648a6016c3ad3458e8b7f19d792278d97f87e447f96171034` |
| `Build/quality-reference-stems/seoul-dsp.wav` | 2 | 48,000 Hz | 96,000 | 2.000000 s | -9.425 | -23.122 | -1.35165950e-09 | `c75c3b6baef97a7f78236a6c2c4d7f374a04c9f1cfedac050d99a813c146c8fc` |

Each WAV is 576,080 bytes. The peak range is 0.644 dB, consistent with the
level-matching record. The higher SEOUL DSP RMS is a measured output
difference, not evidence by itself of better loudness, saturation, or fidelity.

The files can now be used for a level-matched blind listening pass. Any claim
about Brainworx or UAD parity requires an additional reference set and a
documented listening protocol; these renders alone cannot establish it.
