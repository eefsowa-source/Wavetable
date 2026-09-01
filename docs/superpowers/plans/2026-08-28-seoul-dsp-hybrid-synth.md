# SEOUL DSP Hybrid Wavetable Synth Implementation Plan

**Goal:** Evolve the existing JUCE wavetable prototype into a usable SEOUL DSP workstation with a responsive tabbed UI, expanded synthesis controls, safe voice allocation, and host-validation evidence.

**Architecture:** Keep the existing `AudioProcessorValueTreeState` as the parameter authority and extend it with stable IDs. Keep all real-time processing allocation-free by preparing a bounded voice pool, while GUI-only editors own file dialogs, undo history, and status presentation. Use progressive disclosure: a compact SYNTH tab for immediate work and dedicated Wavetable/Mod/FX/ARP/PRESETS tabs.

**Tech Stack:** JUCE 8.0.14, C++20, CMake, VST3/AU/Standalone, existing DSP and test targets.

---

### Task 1: Parameter and identity foundation

**Files:** `Source/PluginProcessor.cpp`, `Source/PluginProcessor.h`, `Tests/ProcessorSmokeTests.cpp`

- [ ] Add SEOUL DSP product identity and stable parameter IDs for oscillator level/tune/unison/spread, filter envelope amount, voice mode/glide, LFOs, macros, FX, arp, and output meter state.
- [ ] Add failing state round-trip tests for every new parameter and legacy-state loading.
- [ ] Implement defaults, ranges, and migration-safe state restore.
- [ ] Run the focused smoke test and confirm it fails before implementation and passes after.

### Task 2: Real-time synthesis expansion

**Files:** `Source/DSP/SynthVoice.h`, `Source/DSP/SynthVoice.cpp`, `Source/PluginProcessor.cpp`, `Tests/WavetableTests.cpp`

- [ ] Add oscillator gain/tuning/unison controls and bounded 128-voice preparation without callback allocation.
- [ ] Implement filter envelope amount, true 8/12/18/24 dB slope behavior, resonance and drive protection, and output metering.
- [ ] Add focused DSP tests for finite output, slope ordering, drive bounds, and voice-pool limits.

### Task 3: Responsive tabbed editor

**Files:** `Source/PluginEditor.h`, `Source/PluginEditor.cpp`

- [ ] Replace the crowded one-page layout with SYNTH/WAVETABLE/MOD/FX/ARP/PRESETS navigation while preserving the cyberpunk Seoul visual language.
- [ ] Make 1040×760 the minimum, 1280×820 the default, and redistribute controls using proportional bounds.
- [ ] Add visible control labels, transient value HUDs, output meter/clip state, status messages, keyboard/fine/reset affordances, and filter labels.

### Task 4: Wavetable workflow and audio import

**Files:** `Source/DSP/WavetableOscillator.h`, `Source/DSP/WavetableOscillator.cpp`, `Source/PluginProcessor.cpp`, `Source/PluginEditor.cpp`, tests

- [ ] Add frame selection, multi-frame slicing, normalize/smooth/reset, undo/redo, and bounded import for JUCE/CoreAudio formats including WAV/AIFF/M4A where available.
- [ ] Preserve immutable audio-thread snapshots and test import/state persistence.

### Task 5: Modulation, effects, arpeggiator, presets

**Files:** processor/DSP/editor sources and tests as needed

- [ ] Add two LFOs, drag-target modulation matrix, four macros, distortion/chorus/delay/reverb, 16-step arp, and internal preset browser with search/tags/favorites.
- [ ] Keep the ten deterministic factory presets and migrate old user states.

### Task 6: Verification and reusable skill

**Files:** `docs/superpowers/specs/2026-08-28-seoul-dsp-ux-design.md`, `/Users/sungha/.codex/skills/eonsa-synth-ux/SKILL.md`, optional `agents/openai.yaml`

- [ ] Record the approved UX specification and implementation decisions.
- [ ] Create a reusable `eonsa-synth-ux` skill using RED/GREEN/REFACTOR pressure scenarios, then run `quick_validate.py`.
- [ ] Run CMake build, CTest, pluginval, auval, and host checks in Ableton Live and Logic Pro where installed; report unavailable gates explicitly.
