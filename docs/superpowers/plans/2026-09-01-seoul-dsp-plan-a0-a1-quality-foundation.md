# SEOUL DSP Plan A0+A1 Quality Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build trustworthy, deterministic sound-quality measurement and repair every known parameter-to-DSP mismatch before changing the synth's tone architecture.

**Architecture:** Test-only analysis code lives under `Tests/AudioQuality` and renders the real `HybridWavetableAudioProcessor`; production DSP does not depend on the test harness. `SynthVoice` receives a seedable, allocation-free PRNG and three fixed-capacity unison banks. Existing parameter IDs remain stable, new detune controls receive new IDs, and the serialized state gains an explicit schema version. All active smoothers advance at audio rate, while master effects own separately prepared smoothers.

**Tech Stack:** C++20, JUCE 8.0.14, CMake 3.22+, CTest, JUCE FFT/audio formats/JSON, macOS universal builds.

---

## Execution rules

- Work from `/Users/sungha/Desktop/EON LLM wiki/EON Audio Plugin/Wavetable`.
- This directory is not currently a Git repository. Task 1 establishes the baseline required by the commit steps below. If repository initialization is declined, execute every code/test step but record commit hashes as `unavailable: no repository`.
- Never update a Golden merely to make a failing change pass. First store the before/after metrics and listening decision.
- Build success, CTest, plugin validation, host load, UI rendering, and listening are separate evidence classes.
- Do not allocate, lock, perform file I/O, use global random state, or call the GUI from `processBlock()` or `renderNextBlock()`.

### Task 1: Establish a recoverable project baseline

**Files:**
- Create: `.gitignore`
- Verify: `CMakeLists.txt`
- Verify: `Source/`
- Verify: `Tests/`

- [x] **Step 1: Verify the current non-repository boundary**

Run: `git rev-parse --show-toplevel`

Expected: before initialization, Git exits non-zero with `not a git repository`.

- [x] **Step 2: Add build and release artifacts to `.gitignore`**

Create exactly:

```gitignore
.DS_Store
Build/
build/
dist/
*.zip
*.pkg
*.tmp
```

- [x] **Step 3: Initialize the repository and capture the pre-quality baseline**

Run:

```bash
git init -b main
git add .gitignore CMakeLists.txt README.md Source Tests docs
git commit -m "chore: establish SEOUL DSP quality baseline"
git status --short
```

Expected: the commit succeeds and `git status --short` prints nothing.

### Task 2: Add the audio-quality support library and test executables

**Files:**
- Modify: `CMakeLists.txt`
- Create: `Tests/AudioQuality/TestHarness.h`
- Create: `Tests/AudioQuality/AudioQualityTypes.h`
- Create: `Tests/AudioQuality/Metrics.h`
- Create: `Tests/AudioQuality/Metrics.cpp`
- Create: `Tests/AudioQuality/MetricsTests.cpp`
- Create: `Tests/AudioQuality/ProcessorQualityTests.cpp`

- [x] **Step 1: Add a minimal test harness and metric contracts**

`TestHarness.h` must expose `expect(bool, const juce::String&)`, count failures, print each failure to `stderr`, and return `0` only when no failures occurred.

Define these stable data contracts in `AudioQualityTypes.h`:

```cpp
struct MidiEventSpec
{
    juce::MidiMessage message;
    int sampleOffset = 0;
};

struct AudioQualityFixture
{
    juce::String id;
    double sampleRate = 48000.0;
    int blockSize = 128;
    int channels = 2;
    double durationSeconds = 2.0;
    double tailSeconds = 0.5;
    std::uint32_t randomSeed = 0x53454f55u;
    std::vector<MidiEventSpec> midi;
};

struct AudioMetrics
{
    bool finite = true;
    double samplePeakDbFS = -std::numeric_limits<double>::infinity();
    double truePeakDbTP = -std::numeric_limits<double>::infinity();
    double rmsDbFS = -std::numeric_limits<double>::infinity();
    double dcDbFS = -std::numeric_limits<double>::infinity();
    double integratedLufs = -std::numeric_limits<double>::infinity();
};
```

Declare pure functions in `Metrics.h`:

```cpp
AudioMetrics measureAudio (const juce::AudioBuffer<float>&, double sampleRate);
double estimateFundamentalHz (const float* samples, int count, double sampleRate,
                              double minimumHz, double maximumHz);
double centsError (double measuredHz, double expectedHz);
double measureInharmonicAliasDbc (const float* samples, int count, double sampleRate,
                                  double fundamentalHz, int maximumExpectedHarmonic);
```

- [x] **Step 2: Write RED tests for known mathematical signals**

In `MetricsTests.cpp`, generate rather than load fixtures so tests have no file dependency:

- 1 kHz sine at amplitude `0.5`: RMS `-9.0309 dBFS +/- 0.02 dB` and DC below `-120 dBFS`.
- Constant `0.01`: DC `-40.0 dBFS +/- 0.01 dB`.
- 440 Hz sine: pitch error `<= 0.2 cent` after discarding 100 ms.
- One injected NaN: `finite == false`.
- 6 kHz saw synthesized from only legal harmonics: alias result below `-100 dBc`.
- Add an inharmonic 7.3 kHz tone at `-40 dB`: alias result between `-41` and `-39 dBc`.

Run:

```bash
cmake -S . -B Build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build Build --target AudioQualityMetricsTests -j 4
Build/AudioQualityMetricsTests_artefacts/Debug/AudioQualityMetricsTests
```

Expected: compilation or assertions fail because metric bodies and the CMake target do not exist yet.

- [x] **Step 3: Implement the metric algorithms**

Use these definitions:

- sample peak: maximum absolute sample across all channels;
- RMS: square root of the mean squared sample across all channels;
- DC: maximum absolute per-channel mean;
- true peak: maximum absolute sample after test-only 4x oversampling with a linear-phase half-band filter;
- pitch: Hann window, FFT magnitude, strongest bin in the requested range, then parabolic interpolation of log magnitudes;
- inharmonic alias: Hann-windowed FFT power excluding DC and masks of `+/- 1.5` bins around every legal harmonic below Nyquist, reported as `10*log10(inharmonicPower/legalPower)`;
- integrated LUFS: BS.1770-style K weighting, 400 ms blocks with 75% overlap, absolute gate at `-70 LKFS`, then relative gate `10 LU` below the ungated mean.

Clamp logarithm inputs to `1.0e-30`; return negative infinity for exact silence. Unit-test the K-weighting coefficients at 48 and 96 kHz for finiteness and stability rather than hard-coding one sample rate.

- [x] **Step 4: Add the CMake targets**

Create a static `AudioQualitySupport` library containing `Metrics.cpp`. Link it to `juce::juce_audio_basics`, `juce::juce_audio_formats`, `juce::juce_core`, and `juce::juce_dsp`.

Create:

```cmake
juce_add_console_app(AudioQualityMetricsTests PRODUCT_NAME "Audio Quality Metrics Tests")
target_sources(AudioQualityMetricsTests PRIVATE Tests/AudioQuality/MetricsTests.cpp)
target_link_libraries(AudioQualityMetricsTests PRIVATE AudioQualitySupport)
add_test(NAME AudioQualityMetrics COMMAND AudioQualityMetricsTests)

juce_add_console_app(ProcessorQualityTests PRODUCT_NAME "Processor Quality Tests")
target_sources(ProcessorQualityTests PRIVATE
  Tests/AudioQuality/ProcessorQualityTests.cpp
  Source/PluginProcessor.cpp Source/PluginEditor.cpp
  Source/DSP/WavetableOscillator.cpp Source/DSP/SynthVoice.cpp)
target_link_libraries(ProcessorQualityTests PRIVATE
  AudioQualitySupport juce::juce_audio_utils juce::juce_audio_processors
  juce::juce_dsp juce::juce_gui_extra)
add_test(NAME ProcessorQuality COMMAND ProcessorQualityTests)
```

Apply the same C++20, JUCE network-disable definitions, and macOS deployment target already used by the existing tests.

- [x] **Step 5: Verify GREEN and commit**

Run:

```bash
cmake --build Build --target AudioQualityMetricsTests ProcessorQualityTests -j 4
Build/AudioQualityMetricsTests_artefacts/Debug/AudioQualityMetricsTests
ctest --test-dir Build -R 'AudioQualityMetrics|ProcessorQuality' --output-on-failure
```

Expected: metric tests pass; the initial processor-quality executable exits `0` with a deliberate single assertion that the processor constructs and reports `SEOUL DSP`.

Commit: `git add CMakeLists.txt Tests/AudioQuality && git commit -m "test: add audio quality metric foundation"`

### Task 3: Build a deterministic real-processor renderer and fixture manifest

**Files:**
- Create: `Tests/AudioQuality/OfflineRenderer.h`
- Create: `Tests/AudioQuality/OfflineRenderer.cpp`
- Create: `Tests/AudioQuality/OfflineRendererTests.cpp`
- Create: `Tests/AudioQuality/Fixtures.h`
- Create: `Tests/AudioQuality/fixture-manifest.json`
- Modify: `CMakeLists.txt`
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/DSP/SynthVoice.h`
- Modify: `Source/DSP/SynthVoice.cpp`

- [x] **Step 1: Write RED determinism and MIDI-offset tests**

Declare:

```cpp
class OfflineRenderer
{
public:
    static juce::AudioBuffer<float> render (const AudioQualityFixture& fixture,
                                             const std::function<void (HybridWavetableAudioProcessor&)>& configure);
};
```

Test two renders with the same seed and state for exact sample equality. Test a different seed with `randomPhase=1` for a different first 1,024 samples. Add note-on events at sample offsets `0`, `blockSize - 1`, and `blockSize / 3`, plus an irregular final block, and verify the first non-silent sample occurs no earlier than each event.

Run: `cmake --build Build --target OfflineRendererTests -j 4`

Expected: RED because the renderer and seed injection do not exist.

- [x] **Step 2: Add a production-safe seed path**

Change the processor constructor to:

```cpp
explicit HybridWavetableAudioProcessor (std::uint32_t deterministicSeed = 0);
```

`0` means generate one seed in the non-realtime processor constructor using `juce::Random::getSystemRandom()`; a non-zero value means use the supplied deterministic seed. Derive one stable non-zero seed per synth voice with SplitMix32 and pass it to `SynthVoice`. Never reseed from the audio callback.

This step only adds seed plumbing. The global `std::rand()` replacement is completed in Task 6.

- [x] **Step 3: Implement exact block scheduling**

`OfflineRenderer::render` must:

1. construct the processor with `fixture.randomSeed`;
2. set mono or stereo output buses;
3. call `prepareToPlay(sampleRate, blockSize)`;
4. apply `configure` before the first audio block;
5. sort MIDI events by absolute sample offset;
6. translate each event to a block-local offset;
7. process `duration + tail` samples, using a shorter final block when necessary;
8. copy only the valid final-block samples into the result;
9. call `releaseResources()`.

Do not call `processBlock()` with a full-size buffer and then truncate it; the irregular final block is part of the contract.

- [x] **Step 4: Add the mandatory manifest entries**

`fixture-manifest.json` must have a top-level `schemaVersion: 1`. Every entry includes a `groups` string array so the runner can select stable sets such as `foundation`, `automation`, `factory`, and `release`. Add entries for:

```json
{
  "id": "single-note-60-clean",
  "sampleRate": 48000,
  "blockSize": 128,
  "channels": 2,
  "durationSeconds": 2.0,
  "tailSeconds": 0.5,
  "seed": 1397051221,
  "preset": "technical-clean",
  "midi": [
    { "type": "noteOn", "note": 60, "velocity": 0.8, "sample": 0 },
    { "type": "noteOff", "note": 60, "velocity": 0.0, "sample": 48000 }
  ]
}
```

Also add IDs for silence, notes 24/96/108, rich-saw chromatic, pulse 25/50/75, transient 10 ms/100 ms/1 s, maximum release, oscillator-isolated unison 1/2/4/8, drift off/subtle/maximum, parameter automation, 32-note chord, 128-note stress, state restore, delay impulse, reverb impulse, and combined effects.

Add `OfflineRenderer.cpp` to the real-processor test sources and create an `OfflineRendererTests` CMake target/test linked to the processor implementation and JUCE modules in the same way as `ProcessorQualityTests`.

- [x] **Step 5: Verify GREEN and commit**

Run:

```bash
cmake --build Build --target OfflineRendererTests ProcessorQualityTests -j 4
Build/OfflineRendererTests_artefacts/Debug/OfflineRendererTests
ctest --test-dir Build -R 'OfflineRenderer|ProcessorQuality' --output-on-failure
```

Expected: identical-seed renders are sample-identical, different-seed randomized-phase renders differ, all MIDI-offset tests pass, and no sample is non-finite.

Commit: `git add CMakeLists.txt Source Tests/AudioQuality && git commit -m "test: add deterministic offline renderer"`

### Task 4: Add Golden comparison and auditable quality reports

**Files:**
- Create: `Tests/AudioQuality/GoldenComparator.h`
- Create: `Tests/AudioQuality/GoldenComparator.cpp`
- Create: `Tests/AudioQuality/GoldenComparatorTests.cpp`
- Create: `Tests/AudioQuality/QualityReport.h`
- Create: `Tests/AudioQuality/QualityReport.cpp`
- Create: `Tests/AudioQuality/AudioQualityRunner.cpp`
- Create: `Tests/AudioQuality/golden/manifest.json`
- Create: `docs/quality/README.md`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Define the comparison contract and write RED tests**

```cpp
struct GoldenComparison
{
    bool identityMatches = false;
    double alignedErrorDbFS = 0.0;
    double loudnessDelta = 0.0;
    double truePeakDeltaDb = 0.0;
    double spectralMedianDeltaDb = 0.0;
    double spectralP95DeltaDb = 0.0;
};

GoldenComparison compareWithGolden (const juce::AudioBuffer<float>& candidate,
                                    const juce::AudioBuffer<float>& golden,
                                    double sampleRate,
                                    int maximumAlignmentSamples = 32);
```

Tests must cover exact equality, a 10-sample shift that aligns, a 33-sample shift that fails, `+0.2 dB` gain that stays inside the normal tolerance, `+0.3 dB` gain that fails, and a deliberate spectral tilt that exceeds the p95 limit.

- [x] **Step 2: Implement bounded alignment and spectral comparison**

Choose the lag in `[-32, 32]` with maximum normalized cross-correlation, crop the common region, then compute aligned RMS error. Compare 1/6-octave log-frequency magnitudes from 40 Hz to `min(18 kHz, 0.45*sampleRate)`, using a floor of `-120 dBFS`. Report median and 95th percentile absolute deviation.

Enforce:

- identical deterministic render: aligned error `<= -120 dBFS RMS`;
- short fixture RMS delta `<= 0.25 dB`;
- clip at least 10 seconds: integrated loudness delta `<= 0.25 LU`;
- true-peak delta `<= 0.5 dB`;
- spectral median `<= 0.5 dB`, p95 `<= 2 dB` unless the fixture explicitly allows timbral change.

- [x] **Step 3: Make reports self-identifying**

The runner accepts only:

```text
--manifest <path> --output-dir <path>
  [--fixture <id> | --fixture-group <name>]
  [--matrix per-build|full] [--write-audio]
```

No fixture selector means every manifest entry. `--matrix per-build` expands each selected fixture over 48 kHz, block sizes 64/128/512, and mono/stereo. `--matrix full` expands over 44.1/48/88.2/96/176.4/192 kHz, block sizes 16/32/64/128/256/512/1024/2048, mono/stereo, boundary MIDI offsets, and an irregular final block.

Each JSON report must contain fixture ID, manifest schema, source identity, executable SHA-256, sample rate, block size, channel count, seed, parameter state hash, duration, tail, every metric, threshold, pass/fail, and first divergent block. Encode a mathematically valid silent value as the string `-inf` plus `metricState: silence`; never emit invalid JSON infinity. The Markdown summary lists failures first. Missing fixture, Golden, metadata, or metric is a failed run.

Add `GoldenComparator.cpp`, `QualityReport.cpp`, and `OfflineRenderer.cpp` to a new `AudioQualityRunner` target alongside the real processor sources. Add `GoldenComparatorTests` as its own CTest target linked to `AudioQualitySupport`.

Do not add a `--promote` switch to the runner; Golden promotion remains a reviewed file operation in Task 5.

- [x] **Step 4: Capture the current build as diagnostic baseline only**

Run:

```bash
cmake --build Build --target AudioQualityRunner -j 4
test ! -e Build/quality-baseline
Build/AudioQualityRunner_artefacts/Debug/AudioQualityRunner \
  --manifest Tests/AudioQuality/fixture-manifest.json \
  --output-dir Build/quality-baseline --write-audio
```

Expected: the command completes and reports the known A1 defects as failures. Store generated audio only under `Build/quality-baseline`; do not copy it into `Tests/AudioQuality/golden`.

- [x] **Step 5: Verify comparator tests and commit**

Run: `ctest --test-dir Build -R 'GoldenComparator|AudioQualityMetrics|OfflineRenderer' --output-on-failure`

Expected: all comparison infrastructure tests pass independently of product-quality failures.

Commit: `git add CMakeLists.txt Tests/AudioQuality docs/quality && git commit -m "test: add auditable golden comparison reports"`

### Task 5: Lock every known A1 defect with RED processor tests

**Files:**
- Modify: `Tests/AudioQuality/ProcessorQualityTests.cpp`
- Modify: `Tests/ProcessorSmokeTests.cpp`

- [x] **Step 1: Add phase-domain and determinism regressions**

Assert that `randomPhase=1` produces at least 16 distinct normalized phase starts across deterministic seeds, every phase is in `[0, 1)`, and repeated renders with the same seed match exactly. The test must fail if phases collapse to the clamped value `1.0`.

- [x] **Step 2: Add parameter-effect regressions**

For each `osc1Unison`, `osc2Unison`, and `osc3Unison`, isolate that oscillator, render with count 1 and count 4, and require a spectral/stereo change while preserving finite output. Automate `osc1Pos`, `osc2Pos`, `osc3Pos`, and `filterEnvAmount` independently; require the post-ramp render to differ from the pre-ramp render and contain no one-sample discontinuity above `0.25` full scale.

- [x] **Step 3: Add unison correctness regressions**

Expose a testable pure layout function, then assert for counts 1, 2, 4, and 8:

- mean cents offset `<= 0.2 cent`;
- paired cents offsets sum to zero;
- paired pan positions sum to zero;
- every pan lies in `[-1, 1]`;
- equal-power channel gains are finite;
- normalization is `1/sqrt(count)` within `1e-6`.

- [x] **Step 4: Add filter-envelope and dry/wet regressions**

At a base cutoff of 1 kHz, `filterEnvAmount=+1` must reach approximately +4 octaves at envelope peak before clamping to Nyquist-safe cutoff; `-1` must move approximately -4 octaves. For delay, mix 0 must equal dry, mix 1 must contain no direct sample at time zero, and mix 0.5 must use equal-power gains.

- [x] **Step 5: Run the focused tests and preserve RED evidence**

Run:

```bash
cmake --build Build --target ProcessorQualityTests -j 4
Build/ProcessorQualityTests_artefacts/Debug/ProcessorQualityTests \
  2>&1 | tee Build/quality-baseline/a1-red.txt
```

Expected: non-zero exit. The log must name failures for phase domain, Osc 2/3 unison, smoother advancement, symmetric unison, octave-domain filter envelope, and true delay dry/wet.

Commit: `git add Tests/AudioQuality Tests/ProcessorSmokeTests.cpp && git commit -m "test: lock parameter to DSP quality regressions"`

### Task 6: Replace global random phase with normalized per-voice PRNG

**Files:**
- Create: `Source/DSP/RealtimeRandom.h`
- Modify: `Source/DSP/SynthVoice.h`
- Modify: `Source/DSP/SynthVoice.cpp`
- Modify: `Tests/AudioQuality/ProcessorQualityTests.cpp`

- [x] **Step 1: Implement a fixed-state generator**

Use xorshift32 with a non-zero state:

```cpp
class RealtimeRandom
{
public:
    explicit RealtimeRandom (std::uint32_t seed) noexcept
        : state (seed != 0 ? seed : 0x6d2b79f5u) {}

    float nextUnitFloat() noexcept
    {
        auto x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return static_cast<float> (x >> 8) * (1.0f / 16777216.0f);
    }

private:
    std::uint32_t state;
};
```

- [x] **Step 2: Use normalized phase consistently**

Replace every `std::rand()` phase assignment with:

```cpp
const float phase = random.nextUnitFloat() * randomPhaseAmount;
oscillator.setPhase (phase);
```

Keep `WavetableOscillator::setPhase()` normalized. Do not multiply by `2*pi` anywhere outside trigonometric evaluation.

- [x] **Step 3: Verify GREEN and commit**

Run: `ctest --test-dir Build -R 'ProcessorQuality|ProcessorSmoke' --output-on-failure`

Expected: phase-domain and deterministic-seed tests pass; other deliberately RED A1 tests may still fail and must remain listed.

Commit: `git add Source/DSP Tests/AudioQuality && git commit -m "fix: make oscillator phase randomization deterministic and normalized"`

### Task 7: Initialize and advance every active smoother

**Files:**
- Modify: `Source/DSP/SynthVoice.h`
- Modify: `Source/DSP/SynthVoice.cpp`
- Modify: `Tests/AudioQuality/ProcessorQualityTests.cpp`

- [x] **Step 1: Initialize without a startup ramp from zero**

After each smoother `reset`, call `setCurrentAndTargetValue()` with the current parameter value. Do this for cutoff, resonance, three oscillator levels, saturation, filter drive, three wavetable positions, and filter envelope amount.

- [x] **Step 2: Advance and consume one value per rendered sample**

At the top of the sample loop, capture each value exactly once:

```cpp
const float cutoffHz = smoothedCutoff.getNextValue();
const float resonance = smoothedResonance.getNextValue();
const float wavetable1 = smoothedWavetable1.getNextValue();
const float wavetable2 = smoothedWavetable2.getNextValue();
const float wavetable3 = smoothedWavetable3.getNextValue();
const float filterEnvAmount = smoothedFilterEnvAmount.getNextValue();
```

Use these locals for the entire sample. Never combine `getNextValue()` and `getCurrentValue()` for the same smoother in one sample.

- [x] **Step 3: Make wavetable position update every sample**

Set all base and unison oscillator positions from the smoothed local plus clamped modulation on every sample, even when modulation depth is zero. This prevents a parameter ramp from being ignored in the no-LFO path.

- [x] **Step 4: Verify GREEN and commit**

Run: `ctest --test-dir Build -R 'ProcessorQuality|ProcessorSmoke|WavetableDSP' --output-on-failure`

Expected: wavetable and filter-envelope automation tests pass without regressing the existing three suites.

Commit: `git add Source/DSP Tests/AudioQuality && git commit -m "fix: advance voice parameter smoothers at audio rate"`

### Task 8: Implement symmetric fixed-capacity unison for all three oscillators

**Files:**
- Create: `Source/DSP/UnisonBank.h`
- Create: `Source/DSP/UnisonBank.cpp`
- Create: `Tests/AudioQuality/UnisonBankTests.cpp`
- Modify: `Source/DSP/SynthVoice.h`
- Modify: `Source/DSP/SynthVoice.cpp`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Define and test the pure unison layout**

```cpp
struct UnisonLane
{
    float cents = 0.0f;
    float pan = 0.0f;
    float gain = 1.0f;
};

std::array<UnisonLane, 8> makeUnisonLayout (int count,
                                            float detuneCents,
                                            float stereoSpread) noexcept;
```

For `count > 1`, lane position is `2*i/(count-1)-1`; cents and pan equal that position times their requested range. For `count == 1`, cents and pan are zero. Gain is `1/sqrt(count)` for every active lane. Write the Task 5 assertions as a standalone `UnisonBankTests` target.

- [ ] **Step 2: Add detune controls without changing old IDs**

Add `osc1Detune`, `osc2Detune`, and `osc3Detune` float parameters with range `0..50 cents` and default `12 cents`, plus one global `unisonKeyTrack` parameter with range `-1..1` and default `0`. Keep every existing parameter ID and range intact. Quantize the existing float-valued unison counts with `roundToInt()` and clamp them to `1..8`. Add the detune and key-track controls compactly in the oscillator panel; do not change the main information hierarchy or remove existing controls.

- [ ] **Step 3: Replace dynamic unison storage**

Create an `OscillatorUnisonBank` that owns `std::array<WavetableOscillator, 8>`. Add `prepare`, `resetPhases`, `setRandomPhases`, and `processStereo` methods. Instantiate three banks in `SynthVoice`. Remove `std::vector<WavetableOscillator> unisonOscs` and the base-plus-extra double-rendering path.

Each oscillator's contribution is exactly its active bank output; count 1 must not render a hidden duplicate. Derive frequency as:

```cpp
const float ratio = std::exp2 ((coarseSemitones + modulationSemitones
                                + lane.cents * 0.01f) / 12.0f);
```

When key tracking is non-zero, scale the requested detune before layout generation with `exp2(unisonKeyTrack * (midiNote - 60) / 48)`, clamped to `0.5..2.0`. Add tests at MIDI 24/60/108 for negative, zero, and positive tracking; zero tracking must remain exactly the requested cents at every key.

- [ ] **Step 4: Verify all three banks and commit**

Run:

```bash
cmake --build Build --target UnisonBankTests ProcessorQualityTests -j 4
ctest --test-dir Build -R 'UnisonBank|ProcessorQuality|ProcessorSmoke' --output-on-failure
```

Expected: Osc 1/2/3 count changes are audible/measurable, mean detune is centred, count 1 has no gain jump, and mono fold-down remains finite without note loss.

Commit: `git add CMakeLists.txt Source Tests/AudioQuality && git commit -m "feat: add symmetric unison to all oscillators"`

### Task 9: Move filter-envelope modulation to an octave domain

**Files:**
- Modify: `Source/DSP/SynthVoice.cpp`
- Modify: `Tests/AudioQuality/ProcessorQualityTests.cpp`

- [ ] **Step 1: Replace the linear cutoff multiplier**

Use bipolar envelope movement in octaves:

```cpp
constexpr float maximumEnvelopeOctaves = 4.0f;
const float modulationOctaves = filterEnvAmount * maximumEnvelopeOctaves
                              * filterEnvelopeSample + lfoCutoffOctaves;
const float modulatedCutoff = baseCutoffHz * std::exp2 (modulationOctaves);
const float safeCutoff = juce::jlimit (20.0f,
                                       0.45f * static_cast<float> (sampleRate),
                                       modulatedCutoff);
```

Change the LFO cutoff accumulator from an arbitrary linear amount to octaves. At full LFO depth, cap it at `+/- 4 octaves`.

- [ ] **Step 2: Verify across sample rates and commit**

Run the filter-envelope fixture at 44.1, 48, 96, and 192 kHz. Expected: movement is ratio-consistent until the sample-rate-specific safe clamp and no cutoff becomes non-finite.

Commit: `git add Source/DSP Tests/AudioQuality && git commit -m "fix: map filter modulation in octaves"`

### Task 10: Implement true smoothed equal-power delay dry/wet

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Tests/AudioQuality/ProcessorQualityTests.cpp`

- [ ] **Step 1: Add prepared master-effect smoothers**

Add linear smoothers for delay time, feedback, delay mix, reverb mix, and master width. Reset them to 50 ms in `prepareToPlay()` and initialize each with `setCurrentAndTargetValue()`.

- [ ] **Step 2: Use an equal-power wet/dry law**

For normalized mix `m`:

```cpp
const float dryGain = std::cos (0.5f * juce::MathConstants<float>::pi * m);
const float wetGain = std::sin (0.5f * juce::MathConstants<float>::pi * m);
const float output = dry * dryGain + delayed * wetGain;
```

The feedback write remains `dry + delayed*feedback`; only the audible output uses dry/wet gains. At mix 1, the direct sample must be absent. Advance each smoother once per sample and set the reverb parameters once per block from smoothed block-end values.

- [ ] **Step 3: Verify automation, tails, and commit**

Run:

```bash
ctest --test-dir Build -R 'ProcessorQuality|ProcessorSmoke' --output-on-failure
Build/AudioQualityRunner_artefacts/Debug/AudioQualityRunner \
  --manifest Tests/AudioQuality/fixture-manifest.json \
  --fixture automation --output-dir Build/quality-a1
```

Expected: delay 0/0.5/1 semantics pass, automation contains no non-finite sample or discontinuity over `0.25` full scale, and declared effect tail remains at least the measured tail.

Commit: `git add Source/PluginProcessor.* Tests/AudioQuality && git commit -m "fix: use smoothed equal power effect mixing"`

### Task 11: Version state semantics and verify migration

**Files:**
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Tests/ProcessorSmokeTests.cpp`
- Modify: `Tests/AudioQuality/ProcessorQualityTests.cpp`
- Create: `Tests/AudioQuality/state-v1-fixture.b64`

- [ ] **Step 1: Add a parameter-state schema attribute**

Write `stateSchemaVersion=2` on the root XML. Continue reading wavetable payload version 1. When loading a root with no state version, treat it as version 1, preserve every old parameter value, supply `12 cents` for the three new detune parameters and `0` for `unisonKeyTrack`, rebuild mips, and restore MIDI mappings.

- [ ] **Step 2: Add old-state and round-trip tests**

Store one deterministic version-1 fixture as Base64, load it, verify old parameter IDs and wavetable samples, render, save as version 2, reload, and require sample-identical output with the same seed.

- [ ] **Step 3: Verify and commit**

Run: `ctest --test-dir Build -R 'ProcessorSmoke|ProcessorQuality|OfflineRenderer' --output-on-failure`

Expected: old-state migration and version-2 round trip pass with no JUCE assertion.

Commit: `git add Source Tests/AudioQuality Tests/ProcessorSmokeTests.cpp && git commit -m "feat: version sound quality state semantics"`

### Task 12: Run the complete A0+A1 gate and freeze the foundation evidence

**Files:**
- Create: `docs/quality/a0-a1-acceptance.md`
- Create after review: `Tests/AudioQuality/golden/manifest.json`
- Create after review: `Tests/AudioQuality/golden/*.wav`
- Verify: `Build/quality-a1/`

- [ ] **Step 1: Build and run all tests**

Run:

```bash
cmake -S . -B Build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build Build -j 4
ctest --test-dir Build --output-on-failure
```

Expected: `WavetableDSP`, `ProcessorSmoke`, `AudioQualityMetrics`, `OfflineRenderer`, `GoldenComparator`, `UnisonBank`, and `ProcessorQuality` all pass.

- [ ] **Step 2: Run the DSP-change matrix**

Run the quality runner over sample rates `44100,48000,88200,96000,176400,192000`, block sizes `16,32,64,128,256,512,1024,2048`, and mono/stereo layouts. Use an explicit matrix option added to the runner:

```bash
Build/AudioQualityRunner_artefacts/Debug/AudioQualityRunner \
  --manifest Tests/AudioQuality/fixture-manifest.json \
  --matrix full --output-dir Build/quality-a1 --write-audio
```

Expected hard gates: all samples finite; pitch drift-off error `<= 1 cent`; unison mean `<= 0.2 cent`; released voices reach digital silence by amp release plus 250 ms; deterministic state restore meets Golden tolerances.

Expected signal gates: technical silence `<= -120 dBFS RMS`; DC `<= -80 dBFS`; default/factory true peak `<= -1 dBTP`; rich-table aliasing below `-60 dBc` for at least 95% of notes and never above `-45 dBc`.

- [ ] **Step 3: Review and promote the first approved Goldens**

Compare `Build/quality-baseline` with `Build/quality-a1`, listen to the five critical excerpts, and write the decision to `docs/quality/a0-a1-acceptance.md`. Only after the report contains before/after metrics and no new critical artifact, copy the accepted WAVs into `Tests/AudioQuality/golden` and update their SHA-256 hashes in the Golden manifest.

- [ ] **Step 4: Commit the accepted foundation**

Run:

```bash
git add Source Tests CMakeLists.txt docs/quality
git commit -m "test: accept Plan A0 and A1 quality foundation"
git status --short
```

Expected: final status is clean. Record the commit hash and `Build/quality-a1/report.json` path in `docs/quality/a0-a1-acceptance.md`.

## A0+A1 completion boundary

A0+A1 is complete only when the entire Task 12 matrix passes, each currently registered synthesis parameter has a verified DSP effect, the version-1 state fixture migrates, and approved Goldens are backed by both objective metrics and the recorded listening decision. Do not begin Plan A2 on a partially RED foundation.
