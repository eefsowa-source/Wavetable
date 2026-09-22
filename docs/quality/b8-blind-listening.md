# Serum/Vital/SEOUL DSP blind listening pack

This pack is the next gate after the Ableton render check. It is intended to
separate level-matched listening from the existing file and signal metrics.
It does not establish a statistical preference or Brainworx/UAD parity.

## Source and level matching

The source stems were rendered in Ableton Live `12.4.5
(2026-08-19_225ce5e356)` at 48 kHz as stereo 24-bit WAV. Their measured
integrated loudness was:

| source | integrated loudness | applied gain |
| --- | ---: | ---: |
| Serum | -21.2 LUFS | +1.0 dB |
| Vital | -20.2 LUFS | 0.0 dB |
| SEOUL DSP | -17.1 LUFS | -3.1 dB |

The target was the median `-20.2 LUFS`, measured with FFmpeg `ebur128`. The
gain operation used double-precision volume processing and wrote stereo
24-bit PCM without metadata.

## Blinded files

The files are in the ignored local directory
`Build/quality-reference-blind/`. The label map is kept separately in the
local `mapping.local.txt` file and is intentionally excluded from this
committed record.

| label | bytes | channels | sample rate | frames | SHA-256 |
| --- | ---: | ---: | ---: | ---: | --- |
| A | 576,102 | 2 | 48,000 Hz | 96,000 | `1ef409e9d95f4751e5bddd3a018348b8cf7d1d4a8d30eb8b9342a651c40ec279` |
| B | 576,102 | 2 | 48,000 Hz | 96,000 | `02029a4c529d96006eca44053f4f23814f9fbd87d6767d6d11d166951e206205` |
| C | 576,102 | 2 | 48,000 Hz | 96,000 | `17880ba49e7edb7cb93eddf347f7edc95d55a9cb8dcc51b410fed1a4bd3e789c` |

## Listening protocol

Listen to A, B, and C at the same monitor level in Ableton or a simple audio
player. Use short repeated passes and record:

1. preferred label for clean tone and pitch stability;
2. preferred label for high-frequency smoothness;
3. preferred label for transient and release cleanliness;
4. any audible aliasing, zipper noise, stereo instability, or level bias;
5. confidence from 1 to 5.

The next DSP change is eligible only if it improves an identified artifact or
preference at matched loudness and still passes the existing finite, DC, alias,
latency, and CPU gates. The current pack is a single clean one-note fixture;
the wider filter, unison, transient, and high-note matrix remains a separate
measurement gate.
