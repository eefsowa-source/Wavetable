# SEOUL DSP Sound Quality and Realism Design

Date: 2026-09-01  
Status: Approved  
Product: SEOUL DSP JUCE wavetable synthesizer

## 1. Decision

SEOUL DSP will follow a staged `Plan A -> Plan B` strategy.

- Plan A is mandatory foundation work. It restores truth between parameters and DSP,
  creates deterministic sound-quality evidence, and improves the existing architecture
  without invalidating preset state.
- Plan B is the maximum-quality destination. It modularizes the voice engine and adds
  higher-quality oscillator, unison, filter, nonlinear, effects, and output stages.
- Plan B must not begin until Plan A acceptance gates pass. Otherwise known defects can
  be copied into the new architecture and contaminate the listening reference.

This design covers DSP sound quality, realism, reproducible measurement, listening, and
host evidence. It does not redesign the GUI, add unrelated workflow features, or claim
equivalence to proprietary synth internals.

## 2. Current Evidence and Problems

The current Debug CTest suite passes `WavetableDSP` and `ProcessorSmoke`, but those tests
do not prove audible quality or DAW behavior. Source inspection found these concrete gaps:

1. Random phase passes radians in the range 0 to 2pi into an oscillator phase setter that
   accepts normalized phase from 0 to 1. Most generated values therefore clamp to 1.
2. `osc2Unison` and `osc3Unison` are registered parameters with no DSP consumer.
3. Wavetable-position and filter-envelope smoothers receive targets but are not advanced.
   Wavetable control remains unsmoothed, and filter-envelope depth can remain at zero.
4. Eight-voice Osc 1 unison spans approximately -1.92 to +1.44 semitones and has a
   downward-centred distribution, despite being described as a small detune range.
5. All Osc 1 unison copies share the same pan gain, so spread does not distribute copies
   symmetrically across stereo.
6. Filter Drive is linear gain before a clean TPT filter; saturation occurs later, outside
   the filter feedback path. It is not currently nonlinear filter drive.
7. Delay mix is `dry + delayed * mix`; full wet is impossible and level rises with mix.
8. The existing aliasing test uses a harmonically simple default table and does not measure
   energy folded into forbidden inharmonic bins.
9. `auval`, pluginval, CTest, DAW insertion, UI rendering, and user listening are separate
   validation gates and must be reported separately.

## 3. Quality Architecture

### 3.1 Data flow

```text
Fixture Manifest
      |
      v
Deterministic Offline Renderer ---> WAV + render metadata
      |                                  |
      +----------------------+-----------+
                             v
                  Objective Metrics Engine
                      |              |
                      v              v
              Absolute Gates   Golden Comparator
                      |              |
                      +-------+------+
                              v
                        Quality Report

Installed AU/VST3 ---> Host Audit ---> Listening Report
```

### 3.2 Components

The implementation plan will preserve these boundaries:

- `AudioQualityFixture`: preset state, MIDI events, random seed, sample rate, block size,
  channel layout, render duration, and expected tail duration.
- `OfflineRenderer`: constructs the real `HybridWavetableAudioProcessor`, calls
  `prepareToPlay()` and `processBlock()`, and produces deterministic float audio.
- `AudioMetrics`: pure analysis functions with no processor or GUI dependency.
- `GoldenComparator`: aligns renders, validates manifest identity, and compares approved
  waveform, loudness, transient, and spectral metrics.
- `QualityReport`: writes machine-readable JSON and a concise human-readable summary.
- `HostAudit`: records installed-artifact identity, auval/pluginval logs, Logic/Ableton
  insertion, real-time playback, offline bounce, state restore, UI render, and listening.

Suggested source layout:

```text
Tests/AudioQuality/AudioQualityRunner.cpp
Tests/AudioQuality/Fixtures.h
Tests/AudioQuality/Metrics.h
Tests/AudioQuality/Metrics.cpp
Tests/AudioQuality/GoldenComparator.h
Tests/AudioQuality/GoldenComparator.cpp
Tests/AudioQuality/fixture-manifest.json
Tests/AudioQuality/golden/
docs/quality/host-audit.md
```

Application DSP source must not depend on test code. Test-only determinism is provided
through a small seedable random interface whose production implementation is also
real-time safe and allocation-free.

## 4. Fixture and Matrix Design

### 4.1 Mandatory audio fixtures

1. Silence: no MIDI and zero effect tails.
2. Single-note sine-like table: MIDI 24, 60, 96, and 108.
3. Rich saw table: chromatic sweep from MIDI 24 through 108.
4. Rich pulse table: duty cycles 25%, 50%, and 75%.
5. Transient: repeated 10 ms, 100 ms, and 1 s notes.
6. Release: 100 ms note followed by the maximum amp and effect tail.
7. Unison: 1, 2, 4, and 8 copies for each oscillator, isolated one oscillator at a time.
8. Drift: off, subtle, and maximum, rendered for at least 20 seconds.
9. Automation: cutoff, resonance, wavetable position, drive, saturation, delay time,
   feedback, and wet mix step and ramp sequences.
10. Dense load: 32-note musical chord and 128-note stress chord.
11. State: save, restore, then render the same deterministic fixture.
12. Effects: dry, delay-only, reverb-only, and delay-plus-reverb impulse/tone bursts.

### 4.2 Sample-rate and block-size policy

- Per build: 48 kHz, block sizes 64, 128, and 512, mono and stereo.
- DSP-change gate: 44.1, 48, 88.2, 96, 176.4, and 192 kHz with block sizes 16,
  32, 64, 128, 256, 512, 1024, and 2048.
- Release gate: full Cartesian matrix plus offline rendering with irregular final blocks.
- All fixture MIDI events include offsets at block start, block end minus one sample, and
  at least one interior non-aligned position.

## 5. Acceptance Gates

### 5.1 Hard correctness gates

- Every output sample is finite.
- With drift disabled, steady-state pitch error is at most 1 cent.
- Mean pitch offset of any symmetric unison configuration is at most 0.2 cent.
- A released voice reaches digital silence no later than declared amp release plus
  250 ms; effect-only tails may continue up to the declared plug-in tail.
- Mono output and stereo output folded to mono remain free of missing-note or
  phase-cancellation failures.
- State restore reproduces the deterministic Golden render within the Golden tolerances.
- Audio callback performs no heap allocation, blocking lock, file I/O, GUI call, or global
  non-real-time-safe random-number operation.

### 5.2 Signal-quality gates

- Default and factory-preset fixtures peak at or below -1 dBTP.
- With no note, no intentional noise, and no effect tail, RMS is at or below -120 dBFS.
- DC level after the output safety stage is at or below -80 dBFS.
- For rich-table clean oscillator tests, inharmonic alias energy is below -60 dBc for at
  least 95% of tested notes and never exceeds -45 dBc.
- A deterministic unchanged render has aligned sample error below -120 dBFS RMS.
- An approved intentional DSP change may update a Golden only when its report contains
  before/after metrics and the listening result.
- Golden RMS difference for short technical fixtures and integrated-loudness difference
  for clips at least 10 seconds long are at most 0.25 dB/LU; true-peak difference is at
  most 0.5 dB unless the change explicitly targets gain calibration.
- Log-frequency spectral-envelope median deviation is at most 0.5 dB and the 95th
  percentile at most 2 dB for changes not intended to alter timbre.

### 5.3 Performance gates

- At 48 kHz and block size 128 on the development Mac, the 32-note musical fixture has
  p95 processing time below 25% of the block deadline and maximum below 80%.
- Each performance report records hardware model, OS, architecture, build type, sample
  format, quality mode, and plug-in/source identity so results are not compared across
  unidentified machines or artifacts.
- The 128-note fixture is a bounded stress test: no dropout, allocation, non-finite sample,
  or buffer overrun. It is not a promise that 128 maximum-quality unison voices meet a
  real-time production CPU budget.
- Plan B quality modes publish their measured CPU multiplier relative to Plan A High mode.

### 5.4 Listening gates

- Test clips are 10 to 25 seconds and level-aligned to within 0.1 dB.
- The listener can switch freely among reference, hidden reference, and candidate.
- Presentation order is randomized and names are hidden.
- At least five critical excerpts cover high-note aliasing, unison bass clarity, filter
  resonance sweep, transient/release behavior, and dense-chord clarity.
- The candidate must have no newly introduced critical artifact and must be preferred or
  judged equivalent on every critical excerpt before a Golden is promoted.
- Listening results are recorded as evidence, not represented as statistically significant
  when only one listener participates.

## 6. Plan A: Correctness and Production Quality

### A0. Establish trustworthy measurement

- Add the deterministic renderer, metric engine, fixture manifest, and initial reports.
- Replace the existing alias check with expected-harmonic masking on rich tables.
- Capture the current build as a diagnostic baseline, not an approved quality Golden.
- Add hard regressions for every current defect listed in Section 2.

### A1. Restore parameter-to-DSP truth

- Use normalized oscillator phase consistently.
- Replace `std::rand()` with a per-voice deterministic, allocation-free PRNG.
- Initialize every smoother from the current parameter and advance every active smoother.
- Make wavetable position smoothing and filter-envelope amount audible and testable.
- Implement Osc 1, 2, and 3 unison or remove a parameter only through explicit state
  migration. The chosen design is to implement all three.
- Compute detune in cents, symmetric around zero, with optional key tracking.
- Use paired equal-power pan positions and energy-preserving normalization.
- Preserve all existing parameter IDs and add a state version for changed interpretation.
- Convert delay to true dry/wet with smoothing and latency-aware mixing.

### A2. Improve the existing signal path

- Crossfade adjacent mip levels based on harmonic headroom.
- Replace nearest-neighbour audio import sampling with band-limited resampling.
- Remove DC and phase-align imported frames before mip generation. Apply only a single
  bank-wide safety gain by default so intentional relative level movement between frames
  is preserved; per-frame normalisation requires an explicit import option.
- Use curve-controlled/exponential amp and filter envelopes.
- Express pitch and filter modulation in cents/octaves instead of linear Hz multipliers.
- Provide Eco 2x and High 4x saturation quality modes. A runtime quality change is applied
  only during safe processor re-preparation and reports its new latency before audio resumes.
- Add output DC blocking, calibrated headroom, and optional safety limiting.
- Smooth delay time, feedback, wet mix, reverb mix, and master width.
- Calibrate factory presets to the signal-quality gates.

### A3. Release evidence

- Fresh universal Release build of Standalone, AU, and VST3.
- Full CTest and quality matrix.
- pluginval strictness 5 in regular validation and strictness 10 for release.
- Targeted `auval` for the installed current component identity.
- Logic AU and Ableton VST3 insertion, MIDI playback, automation, state restore,
  real-time playback, offline bounce, and actual UI rendering.
- Listening gate in Section 5.4.

Plan A acceptance means every A0-A3 gate passes and no parameter remains visibly exposed
without a verified DSP effect.

## 7. Plan B: Maximum-Quality Voice Architecture

Plan B begins only after Plan A acceptance and reuses its test harness and Goldens.

### B1. Isolate DSP modules

```text
VoiceParameterSnapshot
        -> OscillatorBank
        -> VoiceFilterModel
        -> CurvedVCAAndDrive
        -> VoiceOutput
        -> MasterCharacter
        -> MasterFX
        -> OutputSafety
```

- Snapshot atomic parameter values once per block or note as appropriate.
- Remove string-based parameter lookup and avoid repeated expensive pitch conversions in
  the inner sample/unison loops.
- Give each module a prepare, reset, and process contract with preallocated storage.
- Keep the state-migration layer outside the audio callback.

### B2. Premium oscillator and unison

- Multi-resolution band-limited wavetable banks with continuous level selection.
- High-quality arbitrary-ratio import resampling and per-frame spectral analysis.
- Optional higher-order integrated wavetable path if measured alias results justify its
  complexity over the crossfaded mip implementation.
- Per-oscillator 1 to 8 unison copies, keytracked cents detune, selectable pan laws,
  phase distribution, and energy normalization.
- Independent per-voice/per-oscillator filtered random-walk pitch drift plus much slower
  gain and phase variation. Defaults remain subtle and intentional noise remains off.

### B3. Premium filter and nonlinear stages

- Retain a clean TPT SVF mode for fast modulation.
- Add nonlinear SEM/OTA and zero-delay-feedback ladder modes.
- Put model-specific nonlinearity inside the filter feedback loop with resonance-level
  compensation.
- Use selectable 2x/4x/8x oversampling around stateful nonlinear blocks.
- Evaluate first- or second-order ADAA for memoryless tanh-style stages only; reject it if
  numerical edge cases, latency, or measured aliasing are worse than oversampling.
- Every filter mode gets cutoff tracking, self-oscillation, gain, aliasing, and modulation
  regression fixtures.

### B4. Premium effects and output

- Tempo-synchronised stereo and ping-pong delay with third-order fractional-delay
  interpolation, feedback filtering, diffusion, and optional feedback saturation.
- Ensemble/chorus with independent modulation phases and mono-compatibility checks.
- Plate or FDN reverb with pre-delay, damping, modulation, and freeze-safe feedback.
- Latency-compensated equal-power dry/wet mixing.
- Output DC blocking, true-peak monitoring, and quality-selectable soft clip/limiting.

### B5. Quality modes

- Eco: bounded oscillator quality, 2x nonlinear oversampling, reduced maximum unison.
- High: full crossfaded mip engine, 4x nonlinear oversampling, full unison.
- Ultra/offline: highest measured-safe interpolation and 8x nonlinear oversampling.
- Changing quality mode is prepared off the audio thread while processing is suspended,
  then reports the new latency before audio resumes. If the host cannot safely re-prepare,
  the change remains pending until the next processor preparation instead of switching
  inside the audio callback.

Plan B acceptance adds these requirements to all Plan A gates:

- Each premium mode has a documented CPU multiplier and latency.
- High and Ultra improve at least one intended objective metric or listening result; a mode
  that only raises CPU is not retained.
- Existing Plan A presets load through migration and remain recognisably equivalent unless
  explicitly tagged as recalibrated.

## 8. Failure and Evidence Policy

- Missing fixture, Golden, metric, or metadata is a failed/incomplete run, never a pass.
- Nondeterministic deterministic-fixture output is a failure with the first divergent block
  and metric reported.
- Metrics and Golden files are updated only through an explicit review action.
- A threshold change must include the old value, new value, reason, and before/after result.
- CTest success is source/test evidence only.
- pluginval/auval success is wrapper/host-contract evidence, not sound-quality evidence.
- DAW launch without insertion and audible playback is not DAW validation.
- UI rendering, user listening, installation, signing, and notarization remain separate.

## 9. External Technical References

- JUCE `SmoothedValue`: smoothing, progression, and logarithmic/multiplicative use.
  https://docs.juce.com/master/classjuce_1_1SmoothedValue.html
- JUCE `Oversampling`: factors, filtering choices, CPU/latency, and latency reporting.
  https://docs.juce.com/develop/classjuce_1_1dsp_1_1Oversampling.html
- JUCE TPT state-variable filter: 12 dB/oct clean filter designed for fast modulation.
  https://docs.juce.com/master/juce__StateVariableTPTFilter_8h.html
- Higher-Order Integrated Wavetable Synthesis: wavetable interpolation as sample-rate
  conversion and aliasing/image-error analysis.
  https://www.dafx.de/paper-archive/2012/papers/dafx12_submission_69.pdf
- Interpolation Filters for Antiderivative Antialiasing: ADAA trade-offs for memoryless
  and stateful nonlinearities.
  https://dafx.de/paper-archive/2024/papers/DAFx24_paper_33.pdf
- The Art of VA Filter Design: TPT and virtual-analog filter structures.
  https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.2.pdf
- ITU-R BS.1116-3: hidden-reference subjective evaluation of small impairments.
  https://www.itu.int/rec/R-REC-BS.1116-3-201502-I/en
- Apple Audio Unit validation and host testing boundaries.
  https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/AudioUnitDevelopmentFundamentals/AudioUnitDevelopmentFundamentals.html
- Tracktion pluginval validation and CI usage.
  https://github.com/Tracktion/pluginval

## 10. Completion Boundary

This design is complete when the user approves this written specification. The next step
is a separate implementation plan that breaks Plan A into test-first, reviewable tasks.
Plan B planning follows only after Plan A implementation and acceptance evidence, unless
the user explicitly requests one combined long-horizon implementation plan.
