# Plan B integration matrices

Machine: macOS 15.7.9 (24G830), arm64. Source commit for this report:
`444261b60816a7edd424fb80f50eeb390110e5be`.

## Offline render safety matrix

`AudioQualityRunner` rendered two fixtures through the real
`HybridWavetableAudioProcessor` using the full matrix:

- sample rates: 44.1, 48, 88.2, 96, 176.4, and 192 kHz;
- block sizes: 16, 32, 64, 128, 256, 512, 1024, and 2048;
- channels: mono and stereo;
- 96 silence cases and 96 single-note cases;
- 192 cases total, finite failures: 0.

The single-note matrix stayed below -13.0605 dBFS sample peak and its worst
measured DC was -221.4172 dBFS. The 48 kHz block 64/128/512 subset was also
finite in all six cases; its worst peak was -13.0605 dBFS.

The silence run is archived at
`Build/quality-task7-silence-full/report.json`; the single-note run is at
`Build/quality-task7-single-note-full/report.json`. Both runner reports say
`passed: false` because the expanded matrix has no Golden entries. That is an
expected evidence boundary: the files prove the safety metrics and matrix
coverage, but they do not prove Golden identity.

## CPU matrix

`CpuBench` used 48 kHz, block 64, 2 seconds of audio, 3 measured runs after a
warm-up, 8/8/8 unison, maximum saturation and +12 dB filter drive.

| quality | slope | solo 1 voice median ms | solo realtime factor | dense 16 voice median ms | dense realtime factor |
| --- | ---: | ---: | ---: | ---: | ---: |
| Eco | 24 dB/oct | 92.654 | 21.586 | 1402.937 | 1.426 |
| Normal | 24 dB/oct | 93.498 | 21.391 | 1439.985 | 1.389 |
| High | 24 dB/oct | 121.095 | 16.516 | 1761.860 | 1.135 |

Normal dense voice medians for slopes 6/12/18/24 dB/oct were 1422.356,
1430.571, 1457.273, and 1439.985 ms in separate runs. These timings are
operational evidence only; extra CPU is not a sound-quality result.

The saturation stage keeps its fixed reported latency of 47 samples. The
output DC blocker and denormal guard add no host-reported latency.

## Host validation

The installed Release host results are recorded in
[b7-host-validation.md](b7-host-validation.md). VST3 pluginval strictness 10
and standalone AU `auval` passed. REAPER loaded the AU editor while its audio
device was closed. Ableton exposed the VST3 browser entry, but the current
pre-existing set did not isolate a distinct SEOUL DSP device, so clean
insertion remains pending. These host observations do not replace DAW
automation, state-recall, dropout, or level-matched listening gates.

## Artifact identity

| artifact | SHA-256 |
| --- | --- |
| Release VST3 executable | `2468304c0f8c2245bfd908b8e81f6e011e5ccb13d1f8a0c3e6bcd2e873d9c915` |
| Release AU executable | `74865c9e17a215c6a1079d7bdab7d5506bc70c7260aa88ea649011384322842b` |
| Release AudioQualityRunner | `db69cf43a6c186a26c54e2117e6f12a511137ef3c7dfc2b4735ac3f1adfe4900` |
| Release CpuBench | `ecb4c740975058b80aa215f5b72ebea975f7a61ba50d12d224260ed9cf71a667` |
