# Devin handoff: SEOUL DSP DSP quality pass

Date: 2026-09-20
Base commit: `4721efc`
JUCE: `8.0.14`

## Current state

The worktree contains the existing UI depth work and the DSP quality changes. Do not reset or revert the dirty tree. The current DSP changes are:

- cosine equal-power wavetable frame morphing;
- asymmetric saturation inside the existing 2x oversampling stage;
- JUCE `DelayLine<Lagrange3rd>` replacing the manual delay ring buffer;
- 100 Hz Linkwitz-Riley low/high crossover for mono-safe sub bass.

The current CTest suite passes 9/9. This is a WIP checkpoint, not a final host or listening acceptance.

## First fixes required

1. Remove the `lowBand.makeCopyOf (b)` allocation from `processBlock()`. Preallocate low/high scratch buffers in `prepareToPlay()` and reuse them in the callback.
2. Set the DelayLine maximum length from `ceil (sampleRate * 1.5)` during `prepareToPlay()`. The current constructor value of 96000 is insufficient for 1.5 seconds at 96 kHz and above.
3. Reset DelayLine and crossover state in `reset()`.
4. Preserve exact mono-collapse behavior at `masterWidth == 0`; apply the low mono/high stereo split only for non-zero width.
5. Remove unused `reverbLfoPhase`, or implement a measured, allocation-free reverb modulation stage. Do not claim reverb diffusion unless it is present in the audio path.

## DSP verification

Run the following at 44.1, 48, 96, 176.4, and 192 kHz where supported:

- wavetable morph discontinuity, mip-boundary jump, alias energy;
- saturation 0/0.25/0.5/1.0 with THD, DC, and alias measurements;
- delay mix 0/0.5/1.0, feedback 0/0.9, and 30 ms/320 ms/1.5 s times;
- master width 0/1/2, including low-frequency side-energy and mono-sum checks;
- finite output, peak headroom, and PDC/oversampling latency consistency.

No allocation, lock, file I/O, or UI work is allowed on the audio thread.

## CPU and host gates

Build Release and run `CpuBench` with 48 kHz, 64 samples, solo-unison8 and dense16-unison8. Use the documented Phase 2 result as the comparison point. Treat dense16 below 1.0x realtime as a hard failure and target approximately 2.0x realtime.

Then run, against the exact binary whose hash is recorded:

1. fresh CMake build and `ctest --test-dir Build --output-on-failure`;
2. AudioQualityRunner with finite-output evidence;
3. pluginval strictness 10;
4. auval;
5. REAPER smoke in a disposable profile;
6. Ableton Live load, playback, bypass, automation, save/reopen, and CPU/dropout smoke;
7. human blind listening as a separate gate.

Record binary SHA256, host version, sample rate, and block size for every host result. Offline tests, pluginval, host smoke, and listening are separate evidence layers.

## Scope and deliverables

Own the DSP files and related tests: `Source/PluginProcessor.*`, `Source/DSP/SynthVoice.cpp`, `Source/DSP/WavetableOscillator.cpp`, and AudioQuality/ProcessorSmoke tests. Preserve the existing `PluginEditor.*`, `EditorTypes.h`, and `docs/design/*` work unless a build conflict requires a minimal adjustment.

Deliver the corrected source, Release CPU JSON, CTest/AudioQuality reports, pluginval/auval/REAPER/Ableton logs, updated hashes, and a final report that labels code, host, and listening gates independently.
