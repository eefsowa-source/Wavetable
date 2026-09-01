# SEOUL DSP Stereo and Arpeggiator Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make oscillator spread and master width produce a real stereo image, and make arpeggiator gate and disable transitions generate correct MIDI note-off events.

**Architecture:** `SynthVoice` becomes the stereo source, retaining separate filter state per channel and using the registered oscillator-spread parameters for deterministic panning. `HybridWavetableAudioProcessor` remains responsible for the master mid/side width transform. The arpeggiator maintains independent step and gate countdowns so every generated note-off is scheduled at a sample-accurate offset.

**Tech Stack:** C++20, JUCE 8.0.14, CMake/CTest, existing `ProcessorSmokeTests` console executable.

---

### Task 1: Add audio-output regression tests

**Files:**
- Modify: `Tests/ProcessorSmokeTests.cpp`
- Test: `Tests/ProcessorSmokeTests.cpp`

- [x] **Step 1: Write the failing stereo and width test**

After the first real MIDI render, set `osc1Spread` and `osc2Spread` to `1.0`, render another note, and record a maximum per-sample left/right difference. Then set `masterWidth` to `0.0`, render a fresh note, and assert every left/right sample pair differs by less than `1.0e-5f`.

```cpp
ok &= check (stereoDifference > 1.0e-4f,
             "oscillator spread produces a stereo voice image");
ok &= check (monoDifference < 1.0e-5f,
             "master width zero collapses the voice to mono");
```

- [x] **Step 2: Run the focused smoke test and verify RED**

Run: `Build/ProcessorSmokeTests_artefacts/Debug/ProcessorSmokeTests`

Expected: it exits non-zero because the current renderer copies channel 0 over channel 1, leaving no stereo difference.

- [x] **Step 3: Write the failing ARP gate and disable-event test**

Configure a 24 Hz arpeggiator with a 5% gate at 48 kHz, submit one held note, process a 2,048-sample block, and assert that its emitted MIDI contains a note-on at sample 0 and a note-off before the next step. Disable the arpeggiator with no incoming MIDI, process a block, and assert a note-off for the active generated note appears at sample 0.

```cpp
ok &= check (sawGatedNoteOff,
             "arpeggiator gate releases a note before its next step");
ok &= check (sawDisableNoteOff,
             "disabling arpeggiator releases its active generated note");
```

- [x] **Step 4: Run the focused smoke test and verify RED**

Run: `Build/ProcessorSmokeTests_artefacts/Debug/ProcessorSmokeTests`

Expected: it exits non-zero because `arpGate` is not read and the disabled path returns without a generated note-off.

### Task 2: Produce a genuine stereo voice image

**Files:**
- Modify: `Source/DSP/SynthVoice.h`
- Modify: `Source/DSP/SynthVoice.cpp`
- Modify: `Source/PluginProcessor.cpp`
- Test: `Tests/ProcessorSmokeTests.cpp`

- [x] **Step 1: Update voice rendering to write both output channels**

Prepare the two state-variable filters with two channels. In the render loop, calculate each oscillator contribution separately, pan oscillator 1 left and oscillator 2 right by their clamped spread values, alternate oscillator 3's direction by MIDI-note parity, process left and right samples through the corresponding filter channels, and accumulate both output channels. If the host buffer has only one channel, retain a safe mono write.

```cpp
const float osc1Pan = -juce::jlimit (0.0f, 1.0f, spread1->load());
const float osc2Pan =  juce::jlimit (0.0f, 1.0f, spread2->load());
const float left = oscillatorSample (osc1, osc1Pan) + oscillatorSample (osc2, osc2Pan)
                 + oscillatorSample (osc3, 0.0f);
const float right = oscillatorSample (osc1, -osc1Pan) + oscillatorSample (osc2, -osc2Pan)
                  + oscillatorSample (osc3, 0.0f);
```

- [x] **Step 2: Correct the master width transform**

Remove the `copyFrom` overwrite from `processBlock`. Transform the original left/right channels in place with mid/side math so width zero is mono and width one leaves the stereo image unchanged.

```cpp
const float mid = 0.5f * (left + right);
const float side = 0.5f * (left - right) * width;
left = mid + side;
right = mid - side;
```

- [x] **Step 3: Run the focused smoke test and verify GREEN**

Run: `Build/ProcessorSmokeTests_artefacts/Debug/ProcessorSmokeTests`

Expected: exit code `0`, including the two new stereo assertions.

### Task 3: Schedule ARP gates and reset disabled state

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Test: `Tests/ProcessorSmokeTests.cpp`

- [x] **Step 1: Add an ARP gate countdown**

Add `arpSamplesUntilGateOff = -1` next to `arpSamplesUntilStep`. On a generated note-on, calculate the gate duration as `round(stepSamples * arpGate)`, clamped to `[1, stepSamples]`.

- [x] **Step 2: Dispatch ARP events in chronological sample order**

Within `processArpeggiator`, consume the remaining block in increments bounded by the next step or pending gate-off. At a gate boundary, append a note-off and clear the active note. At a step boundary, first release a still-active generated note, choose the next held note according to the selected pattern, append its note-on, and reset both countdowns. Preserve pass-through of non-note MIDI events.

- [x] **Step 3: Release and clear on disable**

When `arpEnabled` is false, append a note-off at sample 0 when `arpActiveNote >= 0`, reset active note, held-note velocities, step index, and both countdowns, then return without replacing ordinary incoming MIDI.

- [x] **Step 4: Run the focused smoke test and verify GREEN**

Run: `Build/ProcessorSmokeTests_artefacts/Debug/ProcessorSmokeTests`

Expected: exit code `0`, including gate timing and disable-event assertions.

### Task 4: Full regression and plugin validation

**Files:**
- Verify only: `Build/`

- [x] **Step 1: Build all plug-in formats**

Run: `cmake --build Build --config Debug -j 4`

Expected: VST3, AU, Standalone, and both test executables build with exit code `0`.

- [x] **Step 2: Run the complete test suite**

Run: `ctest --test-dir Build --output-on-failure`

Expected: `WavetableDSP` and `ProcessorSmoke` both pass.

- [x] **Step 3: Validate the rebuilt VST3 bundle**

Run: `/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --timeout-ms 60000 --skip-gui-tests --validate "Build/HybridWavetable_artefacts/Debug/VST3/SEOUL DSP.vst3"`

Expected: report the exact result, keeping any JUCE assertions separate from the exit code.

- [x] **Step 4: Report AU and host limits separately**

Run `auval` only after confirming the current built AU is what the system discovers. Do not infer Logic, Ableton, or UI-rendering success from build/test/pluginval output.

Result: the installed AU bundle metadata is current (`SEOUL DSP`), but `auval` still discovers stale `Hybrid Wavetable` metadata and 18 parameters. This is a CoreAudio registration/cache mismatch, so current AU runtime and DAW UI behavior remain unproven.
