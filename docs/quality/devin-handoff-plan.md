# Devin handoff: SEOUL DSP quality pass (Ableton-only host validation)

Date: 2026-09-20
Base commit: `4721efc`
JUCE: `8.0.14`

## Current state

The worktree contains the existing UI depth work and the DSP quality changes. Do not reset or revert the dirty tree. The current DSP changes are:

- cosine equal-power wavetable frame morphing;
- asymmetric saturation inside the existing 2x oversampling stage;
- JUCE `DelayLine<Lagrange3rd>` replacing the manual delay ring buffer;
- 100 Hz Linkwitz-Riley low/high crossover for mono-safe sub bass.

The current CTest suite passes 9/9. The follow-up realtime safety fixes are in commit `8c4f467`; this is still not final host or listening acceptance.

## First fixes completed

1. `processBlock()` now reuses a preallocated low-band scratch buffer.
2. DelayLine capacity is set from `ceil (sampleRate * 1.5) + 4` during `prepareToPlay()`.
3. DelayLine, crossover, reverb, scratch, and smoother initialization are reset safely.
4. `masterWidth == 0` mono behavior remains covered by smoke tests.
5. The unused reverb LFO field was removed; reverb diffusion remains unimplemented and must not be claimed.

The next owner should review the implementation and add any stronger realtime allocation instrumentation before final release.

## DSP verification

Run the following at 44.1, 48, 96, 176.4, and 192 kHz where supported:

- wavetable morph discontinuity, mip-boundary jump, alias energy;
- saturation 0/0.25/0.5/1.0 with THD, DC, and alias measurements;
- delay mix 0/0.5/1.0, feedback 0/0.9, and 30 ms/320 ms/1.5 s times;
- master width 0/1/2, including low-frequency side-energy and mono-sum checks;
- finite output, peak headroom, and PDC/oversampling latency consistency.

No allocation, lock, file I/O, or UI work is allowed on the audio thread.

## CPU and host gates

Release `CpuBench` after `8c4f467`: solo-unison8 `27.51x`, dense16-unison8 `1.76x` realtime on the current machine. The dense result is above the 1.0x hard floor but below the documented Phase 2 target of approximately 2.0x, so profile before further feature work.

Then run, against the exact binary whose hash is recorded:

1. fresh CMake build and `ctest --test-dir Build --output-on-failure`;
2. AudioQualityRunner with finite-output evidence;
3. pluginval strictness 10;
4. auval;
5. Ableton Live VST3 load, playback, bypass, automation, save/reopen, and CPU/dropout smoke;
6. human blind listening as a separate gate.

Current host scope is **Ableton Live only**. Do not run or report a new REAPER result for this release pass. Record binary SHA256, Ableton version, sample rate, and block size for every host result. Offline tests, pluginval, auval, Ableton smoke, and listening are separate evidence layers.

## Scope and deliverables

Own the DSP files and related tests: `Source/PluginProcessor.*`, `Source/DSP/SynthVoice.cpp`, `Source/DSP/WavetableOscillator.cpp`, and AudioQuality/ProcessorSmoke tests. Preserve the existing `PluginEditor.*`, `EditorTypes.h`, and `docs/design/*` work unless a build conflict requires a minimal adjustment.

Deliver the corrected source, Release CPU JSON, CTest/AudioQuality reports, pluginval/auval/Ableton logs, updated hashes, and a final report that labels code, host, and listening gates independently.

## Validation snapshot (2026-09-21)

- Build-Release CTest: 9/9 passed.
- Release AudioQuality foundation matrix: 54 fixtures, `finiteFailureCount=0`; report remains `passed=false` because `Tests/AudioQuality/golden/manifest.json` has no golden entries.
- Release CPU: solo-unison8 `27.51x`, dense16-unison8 `1.76x` realtime at 48 kHz / 64 samples.
- AU validation: `auval -v aumu Hwbl Eona` passed. Log: `Build-Release/host-validation-20260921/auval-seoul-dsp-release.log`.
- Release VST3 SHA256: `9de1464dab7244671f3c5dd4cecb68118b388df66eb468c4a38ff81b0dcb15c9`.
- pluginval is unavailable on this machine; the Ableton host gate remains pending for this exact Release hash.
- REAPER evidence in older documents is historical only and is excluded from the current release decision.
