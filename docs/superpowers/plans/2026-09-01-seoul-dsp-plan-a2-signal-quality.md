# SEOUL DSP Plan A2 Signal Quality Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Improve oscillator continuity, imported-table fidelity, modulation behavior, nonlinear processing, effects, and output safety while preserving the accepted A0+A1 evidence contract.

**Architecture:** `WavetableData` owns precomputed, immutable mip and import data; `WavetableOscillator` performs allocation-free continuous frame and mip selection. Voice envelopes and modulation use perceptually meaningful curve/cents/octave domains. Nonlinear quality is selected during processor preparation and never reallocates in the callback. Master effects and output safety remain processor-level stages with preallocated state and explicit latency.

**Tech Stack:** C++20, JUCE 8.0.14, JUCE FFT/WindowedSincInterpolator/Oversampling/DSP, CMake/CTest, the accepted Plan A0+A1 quality runner and Goldens.

---

## Entry and retention rules

- Start only from a clean A0+A1 acceptance commit with `docs/quality/a0-a1-acceptance.md` and a passing full matrix.
- Run each algorithm against its focused fixture, then the 48 kHz per-build gate, before continuing.
- Keep an algorithm only when it improves its intended objective metric or passes the recorded critical listening comparison without a new artifact. Extra CPU alone is not a quality result.
- Preserve all A0+A1 parameter IDs, state migration, deterministic seeds, Golden identity checks, and realtime constraints.
- Do not promote new Goldens until Task 10; intermediate intentional differences belong under `Build/quality-a2-*`.

### Task 1: Record the A2 baseline and add feature-isolation fixtures

**Files:**
- Modify: `Tests/AudioQuality/fixture-manifest.json`
- Modify: `Tests/AudioQuality/Fixtures.h`
- Create: `docs/quality/a2-baseline.md`
- Verify: `Build/quality-a1/`

- [ ] **Step 1: Verify the A0+A1 entry gate**

Run:

```bash
git status --short
ctest --test-dir Build --output-on-failure
test -f docs/quality/a0-a1-acceptance.md
```

Expected: clean worktree, every test passes, and the acceptance record exists. Stop A2 if any condition fails.

- [ ] **Step 2: Add isolated A2 fixtures**

Add deterministic fixture IDs for:

- `mip-boundary-up` and `mip-boundary-down`: pitch ramps through every current harmonic-cap boundary;
- `import-44k1-short`, `import-96k-long`, `import-dc-offset`, and `import-phase-shifted`;
- `amp-curve-pluck`, `amp-curve-pad`, `filter-curve-sweep`;
- `pitch-lfo-cents`, `filter-lfo-octaves`, `drift-20s`;
- `drive-sine-sweep`, `saturation-two-tone`, and `saturation-high-note`;
- `delay-time-ramp`, `delay-feedback-ramp`, `reverb-mix-ramp`, `width-ramp`;
- `output-dc`, `output-transient`, and `output-overload`.

Each fixture must isolate the named subsystem: disable other oscillators/effects/modulation and state those parameter values in the manifest.

- [ ] **Step 3: Capture immutable before metrics**

Run:

```bash
Build/AudioQualityRunner_artefacts/Debug/AudioQualityRunner \
  --manifest Tests/AudioQuality/fixture-manifest.json \
  --fixture-group a2-isolation --output-dir Build/quality-a2-before --write-audio
shasum -a 256 Build/quality-a2-before/report.json \
  > Build/quality-a2-before/report.sha256
```

Expected: the runner completes. `docs/quality/a2-baseline.md` records the A0+A1 commit, report path/hash, machine identity, and known weaknesses without approving new Goldens.

- [ ] **Step 4: Commit the A2 fixture contract**

Commit: `git add Tests/AudioQuality docs/quality/a2-baseline.md && git commit -m "test: define Plan A2 isolation fixtures"`

### Task 2: Crossfade adjacent wavetable mip levels

**Files:**
- Modify: `Source/DSP/WavetableOscillator.h`
- Modify: `Source/DSP/WavetableOscillator.cpp`
- Modify: `Tests/WavetableTests.cpp`
- Modify: `Tests/AudioQuality/ProcessorQualityTests.cpp`

- [ ] **Step 1: Write RED boundary-continuity and alias tests**

Sweep oscillator frequency slowly across every frequency at which `maxSafeHarmonic` crosses one of `mipHarmonicCaps`. For each boundary, compare adjacent 2,048-sample windows and fail if either occurs:

- RMS changes by more than `0.5 dB` after compensating the intended pitch movement;
- spectral-envelope median changes by more than `1.0 dB` between adjacent windows;
- the maximum single-sample residual against a crossfaded reference exceeds `0.1`.

Keep the rich-table inharmonic-alias gates from A0+A1 active.

Run: `ctest --test-dir Build -R 'WavetableDSP|ProcessorQuality' --output-on-failure`

Expected: RED at one or more hard level-selection boundaries.

- [ ] **Step 2: Return a continuous mip selection**

Replace `tableForFrame()` with:

```cpp
struct MipSelection
{
    int detailedLevel = 0;
    int saferLevel = 0;
    float saferMix = 0.0f;
};

MipSelection selectMipLevels (float increment) const noexcept;
const std::array<float, tableSize>& tableForFrameAndLevel (int frame, int level) const noexcept;
```

Compute the continuous harmonic requirement in log2 space. Select the adjacent cap pair surrounding it and use a smoothstep transition over the final `0.35 octave` before the detailed level becomes unsafe:

```cpp
const float smoothstep = x * x * (3.0f - 2.0f * x);
```

At the most detailed and safest extremes, select one level with mix zero. Never select a table whose harmonic cap exceeds Nyquist outside the transition guard.

- [ ] **Step 3: Interpolate frame and mip dimensions in one sample path**

For frame indices A/B and mip levels detailed/safer, perform four existing Hermite sample reads, crossfade detailed-to-safer for each frame, then crossfade frame A-to-B. Advance oscillator phase once after all reads.

- [ ] **Step 4: Verify metric improvement and commit**

Run:

```bash
ctest --test-dir Build -R 'WavetableDSP|ProcessorQuality' --output-on-failure
Build/AudioQualityRunner_artefacts/Debug/AudioQualityRunner \
  --manifest Tests/AudioQuality/fixture-manifest.json \
  --fixture-group mip-boundary --output-dir Build/quality-a2-mip --write-audio
```

Expected: all continuity gates pass and aliasing remains within A0+A1 limits. If alias energy worsens by over `3 dB` on any note, tighten the guard or revert rather than accepting the crossfade.

Commit: `git add Source/DSP Tests && git commit -m "feat: crossfade wavetable mip boundaries"`

### Task 3: Replace nearest-neighbour import with band-limited cyclic resampling

**Files:**
- Create: `Source/DSP/WavetableImporter.h`
- Create: `Source/DSP/WavetableImporter.cpp`
- Create: `Tests/AudioQuality/WavetableImporterTests.cpp`
- Modify: `Source/DSP/WavetableOscillator.h`
- Modify: `Source/DSP/WavetableOscillator.cpp`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write RED import fidelity tests**

Generate source buffers at 44.1 and 96 kHz containing a known fundamental plus legal partials. Test:

- output frame length is exactly 2,048;
- every sample is finite;
- DC is below `-100 dBFS` after import;
- phase-shifted copies align to within one target sample;
- legal partial amplitude error is below `0.25 dB` through `0.4*target Nyquist`;
- no new inharmonic component exceeds `-80 dBc`;
- a 16-frame source preserves relative per-frame RMS within `0.1 dB`.

Expected: RED because current import floors to the nearest source sample and leaves DC/phase untreated.

- [ ] **Step 2: Separate import from realtime wavetable playback**

Define:

```cpp
struct WavetableImportOptions
{
    bool removeDc = true;
    bool alignCyclicPhase = true;
    bool normaliseEachFrame = false;
    float bankPeakCeiling = 0.98f;
};

class WavetableImporter
{
public:
    static WavetableData import (const juce::AudioBuffer<float>& source,
                                 const WavetableImportOptions& options);
};
```

Keep this API off the audio thread. `loadAudioFile()` calls it on its existing UI/file path, then publishes the completed immutable table.

- [ ] **Step 3: Implement cyclic windowed-sinc resampling**

Use JUCE `WindowedSincInterpolator` for arbitrary input-frame length to 2,048 samples. Provide at least 16 wrapped source samples before and after each extracted frame so the interpolator sees a periodic boundary. Do not zero-pad a cyclic frame. Use the interpolator speed ratio `inputFrameLength / 2048.0` and verify the returned input consumption.

- [ ] **Step 4: Remove DC, align phase, and apply one bank gain**

For each frame:

1. subtract its arithmetic mean;
2. find the cyclic lag relative to frame 0 with maximum FFT cross-correlation;
3. rotate by that lag, preserving the waveform rather than windowing it;
4. store the unnormalised frame.

After every frame is processed, calculate the maximum absolute sample over the whole bank and apply one gain only when that maximum exceeds `bankPeakCeiling`. Per-frame normalization occurs only when `normaliseEachFrame` is explicitly true.

- [ ] **Step 5: Regenerate mips once and verify GREEN**

Call `regenerateMips()` once after preprocessing all frames. Do not rebuild per frame inside the import loop.

Run:

```bash
cmake --build Build --target WavetableImporterTests WavetableTests -j 4
ctest --test-dir Build -R 'WavetableImporter|WavetableDSP|ProcessorSmoke' --output-on-failure
```

Expected: all fidelity and legacy WAV/AIFF smoke tests pass. Update the old nearest-neighbour sample-value assertion to a spectral/fundamental assertion because exact individual samples intentionally change.

Commit: `git add CMakeLists.txt Source Tests && git commit -m "feat: add band limited cyclic wavetable import"`

### Task 4: Add curve-controlled amp and filter envelopes

**Files:**
- Create: `Source/DSP/CurvedEnvelope.h`
- Create: `Source/DSP/CurvedEnvelope.cpp`
- Create: `Tests/AudioQuality/CurvedEnvelopeTests.cpp`
- Modify: `Source/DSP/SynthVoice.h`
- Modify: `Source/DSP/SynthVoice.cpp`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write RED duration, continuity, and curve tests**

For attack/decay/release values `1 ms`, `10 ms`, `100 ms`, `1 s`, and `10 s` at 44.1/48/96/192 kHz, assert segment duration error is at most one sample. Require no discontinuity over `1e-6` at stage boundaries and finite output after retrigger. For curve `-1`, `0`, and `+1`, assert distinct midpoint levels while endpoints remain exact.

- [ ] **Step 2: Implement an allocation-free state machine**

Expose:

```cpp
struct CurvedEnvelopeParameters
{
    float attackSeconds = 0.01f;
    float decaySeconds = 0.25f;
    float sustain = 0.8f;
    float releaseSeconds = 0.35f;
    float attackCurve = 0.35f;
    float decayCurve = -0.25f;
    float releaseCurve = -0.35f;
};
```

The envelope stores current value, segment start/end, exact integer segment length, and sample index. Map normalized progress through a numerically stable exponential family; curve zero is exactly linear. Start release from the current level and permit sample-accurate retrigger without resetting to zero.

- [ ] **Step 3: Add parameter IDs and migrate old states**

Add `ampAttackCurve`, `ampDecayCurve`, `ampReleaseCurve`, `filterAttackCurve`, `filterDecayCurve`, and `filterReleaseCurve`, each range `-1..1` with the defaults above. Preserve all old ADSR IDs and values. Increment `stateSchemaVersion` to 3; old states receive only the new curve defaults.

Add compact curve controls within the existing envelope panel without changing the editor's main sections.

- [ ] **Step 4: Replace JUCE ADSRs and verify**

Replace both `juce::ADSR` instances in `SynthVoice`. Keep voice-clear timing tied to amp envelope completion; filter-envelope completion must not kill a voice.

Run: `ctest --test-dir Build -R 'CurvedEnvelope|ProcessorQuality|ProcessorSmoke' --output-on-failure`

Expected: timing/curve tests pass, state v1/v2 migration still passes, and transient/release fixtures contain no new click.

Commit: `git add CMakeLists.txt Source Tests && git commit -m "feat: add curved synthesis envelopes"`

### Task 5: Express pitch movement in cents and drift as filtered random walk

**Files:**
- Create: `Source/DSP/PitchModulation.h`
- Create: `Source/DSP/PitchModulation.cpp`
- Create: `Tests/AudioQuality/PitchModulationTests.cpp`
- Modify: `Source/DSP/SynthVoice.h`
- Modify: `Source/DSP/SynthVoice.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write RED unit and 20-second drift tests**

Assert `1200 cents == 2x`, `-1200 cents == 0.5x`, and zero cents is exact unity. With drift off, require `<= 1 cent` pitch error. At subtle default depth, require mean offset `<= 0.2 cent`, 95% of instantaneous movement within `+/- 5 cents`, and no periodic spectral line that dominates the drift band. Same seed must reproduce exactly; different seed must decorrelate.

- [ ] **Step 2: Centralize pitch conversion**

Use one conversion only:

```cpp
inline float ratioFromCents (float cents) noexcept
{
    return std::exp2 (cents / 1200.0f);
}
```

Accumulate oscillator tuning, pitch LFO, unison detune, and drift in cents before one conversion per lane/sample. Remove the current percent-frequency drift and semitone conversions inside nested loops.

- [ ] **Step 3: Replace the drift sine with a bounded filtered random walk**

At a control rate of 200 Hz, add zero-mean PRNG steps to state, apply a one-pole low-pass derived from `driftRate`, softly reflect at `+/- driftDepthCents`, and linearly interpolate between control points at audio rate. Map the current `driftDepth` range `0..1` to `0..20 cents`; keep default zero. Use the per-voice `RealtimeRandom` stream, not another global generator.

- [ ] **Step 4: Verify pitch quality and commit**

Run: `ctest --test-dir Build -R 'PitchModulation|ProcessorQuality' --output-on-failure`

Expected: unit conversion, off-pitch, mean-offset, bounded drift, reproducibility, and decorrelation tests pass at every DSP-change sample rate.

Commit: `git add CMakeLists.txt Source/DSP Tests/AudioQuality && git commit -m "feat: map pitch modulation in cents"`

### Task 6: Make nonlinear character quality-selectable and truthful

**Files:**
- Create: `Source/DSP/VoiceNonlinearStage.h`
- Create: `Source/DSP/VoiceNonlinearStage.cpp`
- Create: `Tests/AudioQuality/VoiceNonlinearStageTests.cpp`
- Modify: `Source/DSP/SynthVoice.h`
- Modify: `Source/DSP/SynthVoice.cpp`
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write RED transfer, alias, and latency tests**

For Filter Drive at `-12, 0, +12, +24 dB`, require a nonlinear transfer at positive drive, monotonic finite output, DC below `-100 dBFS`, and level compensation within `+/- 1.0 dB` for a `-18 dBFS` sine. For saturation, compare 2x and 4x alias energy on sine sweep and two-tone fixtures. Require High 4x to improve alias energy by at least `6 dB` on the designated stress fixture or it is not retained.

Assert reported processor latency equals the active oversampler latency after every `prepareToPlay()`.

- [ ] **Step 2: Define quality selection outside the audio callback**

Add a `qualityMode` choice parameter with `Eco 2x` and `High 4x`, default `High 4x`. Increment `stateSchemaVersion` to 4; states from schema 1-3 receive High 4x without changing their other values. `requestedQualityMode` follows the parameter, but `activeQualityMode` changes only in `prepareToPlay()` or an explicit non-realtime `applyPendingQualityMode()` invoked while processing is suspended.

`processBlock()` must never construct, destroy, or call `initProcessing()` on an oversampler. If the requested mode differs while audio is running, continue with the active mode and expose a pending-state flag to the editor.

- [ ] **Step 3: Centralize nonlinear processing**

`VoiceNonlinearStage` owns preconstructed 2x and 4x oversamplers and preallocated buffers. Prepare both outside the callback, reset both, then process only the active one. Apply:

- Filter Drive as normalized tanh before the clean TPT filter, with gain compensation;
- Saturation as normalized tanh after the filter/VCA, preserving the current signal order;
- denormal protection and finite guards.

Plan B, not A2, owns nonlinearity inside a filter feedback loop. Update the UI/help text so A2 claims `pre-filter nonlinear drive`, not nonlinear filter feedback.

- [ ] **Step 4: Report latency and verify retention**

After preparing the active mode, call `setLatencySamples()` with its exact latency before audio resumes. Render `Build/quality-a2-nonlinear` and compare to the baseline.

Expected: High meets the `>= 6 dB` designated alias improvement, neither mode violates hard gates, and mode changes do not affect allocation instrumentation inside the callback. If High misses the improvement gate, keep Eco only and document the rejection.

Commit: `git add CMakeLists.txt Source Tests/AudioQuality && git commit -m "feat: add prepared nonlinear quality modes"`

### Task 7: Upgrade master effects modulation and interpolation

**Files:**
- Create: `Source/DSP/MasterEffects.h`
- Create: `Source/DSP/MasterEffects.cpp`
- Create: `Tests/AudioQuality/MasterEffectsTests.cpp`
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write RED delay and automation tests**

Require delay impulse time error `<= 1 sample`, continuous output during a 30 ms-to-1.5 s time sweep, feedback decay matching the requested gain within `0.5 dB`, equal-power endpoints, and silence remaining below `-120 dBFS`. Sweep delay time, feedback, wet mix, reverb mix, and width at block sizes 16/128/2048; reject any click residual above `-50 dBFS` after subtracting a control-rate-smoothed reference.

- [ ] **Step 2: Extract preallocated master effects**

Move delay buffer, write index, reverb, and associated smoothers into `MasterEffects`. Its contract is:

```cpp
class MasterEffects
{
public:
    void prepare (double sampleRate, int maximumBlockSize, int channels);
    void reset() noexcept;
    void setTargets (float delaySeconds, float feedback, float delayMix,
                     float reverbSize, float damping, float reverbMix,
                     float width) noexcept;
    void process (juce::AudioBuffer<float>&) noexcept;
    double tailSeconds() const noexcept;
};
```

- [ ] **Step 3: Use third-order fractional-delay interpolation**

Read four wrapped delay samples and use a four-point Hermite interpolator. Clamp the requested delay so all four taps remain valid. Keep feedback below `0.9` and apply a 20 Hz DC blocker inside the feedback path.

Smooth delay time in seconds, feedback, mixes, and width at sample rate. Smooth reverb size/damping at control rate and update the JUCE reverb once per block.

- [ ] **Step 4: Verify and commit**

Run: `ctest --test-dir Build -R 'MasterEffects|ProcessorQuality|ProcessorSmoke' --output-on-failure`

Expected: all effect endpoint, interpolation, automation, mono/stereo, and tail tests pass with zero callback allocation.

Commit: `git add CMakeLists.txt Source Tests/AudioQuality && git commit -m "feat: harden master effects interpolation and smoothing"`

### Task 8: Add output DC blocking, calibrated headroom, and optional limiting

**Files:**
- Create: `Source/DSP/OutputSafety.h`
- Create: `Source/DSP/OutputSafety.cpp`
- Create: `Tests/AudioQuality/OutputSafetyTests.cpp`
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write RED DC, peak, latency, and release tests**

Inject DC values `+/-0.1`, impulses, sustained sines, and a 32-note overload. Require DC below `-80 dBFS` after 500 ms, finite output, linked stereo gain, no overshoot over `-1 dBTP` when limiting is enabled, and exact reported latency. With limiting disabled, require output to differ from the pre-safety signal only by the always-on DC blocker; headroom is calibrated in source/factory levels rather than hidden inside this stage.

- [ ] **Step 2: Implement an always-on transparent DC blocker**

Use one state per channel with a 10 Hz first-order high-pass coefficient calculated in `prepare()`. Reset on processor reset/reprepare. Process after effects and before the optional limiter.

- [ ] **Step 3: Implement a prepared optional true-peak safety limiter**

Add `safetyLimiter` boolean, default off. Increment `stateSchemaVersion` to 5; states from schema 1-4 receive limiter off. The limiter uses a preallocated 1 ms lookahead ring, stereo-linked gain, instantaneous attack, 50 ms release, and a 4x oversampled peak detector. Set the internal ceiling to `-1.2 dBFS` so measured 4x true peak stays at or below `-1 dBTP` in the technical fixtures. Report the lookahead plus detector latency only when enabled at preparation time; a runtime toggle remains pending until safe reprepare.

- [ ] **Step 4: Verify and commit**

Run: `ctest --test-dir Build -R 'OutputSafety|ProcessorQuality|ProcessorSmoke' --output-on-failure`

Expected: DC/true-peak/latency tests pass, limiter-off defaults do not conceal factory calibration failures, and no callback allocation is observed.

Commit: `git add CMakeLists.txt Source Tests/AudioQuality && git commit -m "feat: add output safety stage"`

### Task 9: Calibrate defaults and every factory preset

**Files:**
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Tests/AudioQuality/fixture-manifest.json`
- Create: `docs/quality/factory-preset-calibration.md`

- [ ] **Step 1: Render all presets without the safety limiter**

For each of the 10 factory presets, render notes 24/60/96, a 4-note chord, and the 32-note musical chord with the limiter disabled. Record short-fixture RMS, long-fixture LUFS, true peak, DC, aliasing, and release tail.

- [ ] **Step 2: Calibrate at the source, not by hiding failures**

Adjust oscillator levels, unison detune/spread, filter drive, saturation makeup, effect returns, and preset output so:

- every default/factory fixture is `<= -1 dBTP` without the safety limiter;
- silence is `<= -120 dBFS RMS`;
- DC is `<= -80 dBFS`;
- clean rich-table aliasing meets the A0+A1 gate;
- preset-to-preset integrated loudness stays within a practical `6 LU` span unless a quieter preset is intentionally documented.

Do not normalize every preset to one LUFS number and do not enable the limiter to force a pass.

- [ ] **Step 3: Record each intentional tonal change**

`docs/quality/factory-preset-calibration.md` contains old/new parameter values, before/after metrics, and a one-line listening result for each modified preset.

- [ ] **Step 4: Verify and commit**

Run:

```bash
Build/AudioQualityRunner_artefacts/Debug/AudioQualityRunner \
  --manifest Tests/AudioQuality/fixture-manifest.json \
  --fixture-group factory --output-dir Build/quality-a2-factory --write-audio
```

Expected: all factory gates pass with limiter off.

Commit: `git add Source/PluginProcessor.cpp Tests/AudioQuality docs/quality && git commit -m "fix: calibrate factory sound levels"`

### Task 10: Run the complete A2 gate and promote reviewed Goldens

**Files:**
- Modify after review: `Tests/AudioQuality/golden/manifest.json`
- Modify after review: `Tests/AudioQuality/golden/*.wav`
- Create: `docs/quality/a2-acceptance.md`
- Verify: `Build/quality-a2-final/`

- [ ] **Step 1: Run all tests and the full matrix**

Run:

```bash
cmake -S . -B Build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build Build -j 4
ctest --test-dir Build --output-on-failure
Build/AudioQualityRunner_artefacts/Debug/AudioQualityRunner \
  --manifest Tests/AudioQuality/fixture-manifest.json \
  --matrix full --output-dir Build/quality-a2-final --write-audio
```

Expected: all A0+A1 hard/signal gates remain green at 44.1/48/88.2/96/176.4/192 kHz, block sizes 16 through 2048, mono/stereo, and irregular offline blocks.

- [ ] **Step 2: Run the per-build CPU gate**

At 48 kHz, block 128, Release build, measure the 32-note musical fixture. Require p95 processing time below 25% of the block deadline and maximum below 80%. The 128-note stress fixture must remain finite and allocation-free but is not required to meet the production deadline.

Record hardware model, OS, architecture, build type, sample format, active quality mode, plug-in/source identity, and the Eco/High CPU ratio.

- [ ] **Step 3: Conduct the five critical hidden-reference comparisons**

Prepare level-aligned 10-to-25-second excerpts for high-note aliasing, unison bass clarity, filter sweep, transient/release, and dense-chord clarity. Randomize candidate/reference/hidden-reference labels. A2 must have no new critical artifact and be preferred or equivalent on all five. Record a single-listener result as evidence, not statistical significance.

- [ ] **Step 4: Promote only reviewed intentional changes**

For each changed Golden, attach the before/after metric record and listening decision to `docs/quality/a2-acceptance.md`, then replace the WAV and SHA-256. Unchanged fixtures must remain sample-identical within the existing threshold.

- [ ] **Step 5: Commit A2 acceptance**

Run:

```bash
git add Source Tests CMakeLists.txt docs/quality
git commit -m "test: accept Plan A2 signal quality"
git status --short
```

Expected: clean worktree. The acceptance document records the commit hash, final report hash/path, CPU metadata, quality-mode latency, and listening result.

## A2 completion boundary

A2 is complete only when all accepted A0+A1 evidence still passes, mip transitions and imports meet their focused gates, High quality demonstrates a measured benefit, defaults and factory presets pass without the optional limiter, the callback remains allocation-free, and every changed Golden has explicit objective and listening approval. Plan A3 validates the resulting Release artifacts; it does not repair A2 failures.
