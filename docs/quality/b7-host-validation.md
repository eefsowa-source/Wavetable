# Plan B host validation

This record covers the installed Release artifacts produced from the Plan B
source state. The documentation-only commit that records this report is
`2fae426`; the Release binaries and offline matrix were built from
`444261b60816a7edd424fb80f50eeb390110e5be`, the immediately prior source
commit for the implementation. The installed executable hashes are listed in
[b-task-matrices.md](b-task-matrices.md).

## VST3

`/Applications/pluginval.app/Contents/MacOS/pluginval` was run with
`--strictness-level 10` against the installed Release VST3 bundle. The run
completed with `SUCCESS` after exercising state, automation, parameter
thread-safety, fuzz, bus, and multiple sample-rate/block-size cases.

- pluginval SHA-256: `32ec60efb9f8161f0b840103f9bd98fa455cd86ec59eb2803133b4382fa4425c`
- log: `Build/quality-task7-pluginval-vst3.log`
- artifact: `Build-Release/HybridWavetable_artefacts/Release/VST3/SEOUL DSP.vst3`

## AU

Standalone Apple `auval` passed for `aumu/Hwbl/Eona` and ended with `AU
VALIDATION SUCCEEDED`. This validates the Audio Unit contract and render
probes, not DAW playback or listening.

- command: `auval -v aumu Hwbl Eona`
- log: `Build-Release/host-validation-20260921/auval-seoul-dsp-release.log`

The AU pluginval run is not accepted as a pass. The default 30-second run and
a retry with `--timeout-ms 120000` reached `pluginval / auval` and then
stalled before completion. Logs:

- `Build/quality-task7-pluginval-au.log`
- `Build/quality-task7-pluginval-au-retry.log`

## REAPER

REAPER `7.79.0_06dd787u` loaded the installed AU and displayed
`AUi: SEOUL DSP (EON Audio)` in the FX chain. The editor exposed the SEOUL DSP
controls, including the `24 dB/oct` filter slope value.

This is direct AU editor-load evidence. REAPER reported `[audio device
closed]` during capture, so this run does not prove realtime rendering,
dropout behavior, automation, state recall, or listening. REAPER VST3 loading
remains pending.

## Ableton Live

Ableton Live `12.4.5 (2026-08-19_225ce5e356)` was at `48.0 kHz`. With the
browser filtered to VST3, it exposed the installed `SEOUL DSP` entry and the
selected track was renamed `1-SEOUL DSP`. The observed CPU meter was roughly
21–25%; the block size was not available in the captured UI state.

The set already contained other EON devices, and the visible device title
remained `EON-Vari mu DSP`. Therefore this capture proves browser discovery
and selection only. A clean empty-set insertion test is still required before
calling Ableton VST3 loading passed. The current unsaved set was preserved.

## Remaining gates

The following remain open: AU pluginval completion, REAPER VST3 loading, a
clean Ableton insertion, DAW automation and state recall, DAW dropout testing,
and level-matched blind listening. The expanded offline scenarios still have
no promoted Golden files, so their runner reports remain safety and coverage
evidence rather than Golden acceptance.
