# P3: pluginval / auval validation notes (SEOUL DSP)

Date: 2026-09-03, macOS 15.7.9 (arm64). REAPER was in use by another agent; REAPER was not touched.

## 1. pluginval grade - VST3 PASS (strictness 10)

- Install: GitHub release download (not brew). ~/tools/pluginval/pluginval.app, pluginval 1.0.4. Pre-existing /Applications/pluginval.app (same 1.0.4) untouched.
- Command: ~/tools/pluginval/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --verbose --timeout-ms 300000 "$HOME/Library/Audio/Plug-Ins/VST3/SEOUL DSP.vst3"
- Result: SUCCESS, exit 0, ~20 s. Prior AppKit abort / exit 134 was NOT reproduced on this build.
- Target identity: installed VST3 binary sha256 b983493b...67f88ca8a equals workspace Debug artifact (same build).
- FAILURE lines: none. 505 JUCE assertion log lines (Debug build):
  - juce_AudioProcessor.cpp:451 x 1 (cold open)
  - juce_audio_plugin_client_VST3.cpp:3747 x 4
  - juce_AudioProcessor.cpp:599 x 500 (during fuzzing)
  - Not pluginval failures, but re-verify on a Release build to confirm they vanish.
- Log: Build/quality-p3/pluginval_vst3_strictness10.log

## 2. auval grade - FAIL (registration layer; not runtime quality)

Checks (raw evidence in Build/quality-p3/):

| check | result |
|---|---|
| (a) auval -a listing | No Seoul entry. EON family shows only EON-Vari mu DSP, EE-1073, EE-1176, Tube Comp, YinYeng (auval_a_listing.txt). |
| (b) quarantine xattr | Only com.apple.provenance present; no quarantine. Not the cause. |
| (c) auval -f vs -v | -f is not a valid option (Invalid Arguments). -v repeated 3x: "Cannot get Component's Name strings" then "didn't find the component". Probes like "aumu Eona Eona" also not found. |
| (d) version=256 | Normal encoding (1.0). Aoi YUME uses the same value and PASSES. Not the cause. |
| (e) known-working control | auval -v aumu AoYu EonL (Aoi YUME) PASSES 3/3 on this machine, so the CLI works. YinYeng / EEF-JP8000 / EE-1073 (user-scope EON AUs) ALL fail identically. |

Additional observations:

- kill -9 of both AudioComponentRegistrar processes then retry: unchanged. Full registry rebuild likely needs sudo (SIP); not attempted (no passwordless sudo in this session).
- auvaltool Cache.db contains only a plugivery URL entry; no per-component cache rows found.

## 3. Root-cause hypothesis

Primary (confidence: high, ~85%): among user-scope (~/Library/...) AUs, ONLY Aoi YUME - the sole one with sandboxSafe=true in its AudioComponents plist - registers with auval; all AUs declaring resourceUsage instead (SEOUL DSP, YinYeng, EEF-JP8000, EE-1073) are missing. The single substantive Info.plist difference between SEOUL DSP and the passing control is exactly sandboxSafe:true vs resourceUsage{...}; build-wise this maps to AU_SANDBOX_SAFE TRUE present in Aoi YUME's CMake (plugins/rompler/CMakeLists.txt:16) and absent from SEOUL DSP's CMakeLists.txt (no AU_SANDBOX_SAFE anywhere; juce_add_plugin at CMakeLists.txt:27).

Secondary (confidence: medium, ~15%): AudioComponentRegistrar cache staleness with many user-scope AUs plus a very large /Library component set. A sudo-level registrar kickstart could partially clear this, but the AoYu-vs-everything pattern fits the sandboxSafe correlation far better than random cache corruption.

Recommended follow-ups (source edits, outside this verification scope):

1. Add AU_SANDBOX_SAFE TRUE to HybridWavetable's juce_add_plugin, rebuild, reinstall, rerun auval (primary test).
2. Cross-check with sudo kickstart of AudioComponentRegistrar before/after to separate cache effects.
3. Re-run pluginval on a Release build to confirm the 505 Debug assertions disappear.

## 4. Still untested

- AU loading in a live host this session (REAPER was in use; prior report says REAPER 7.79 loads this AU fine - host evidence is separate from auval).
- sudo-level AudioComponentRegistrar reset (needs password).
- pluginval on a Release build (Debug assertion clearance unconfirmed).

## 5. Evidence files

- pluginval log: Build/quality-p3/pluginval_vst3_strictness10.log
- auval -a listing: Build/quality-p3/auval_a_listing.txt
- auval SEOUL repeats: Build/quality-p3/auval_v_seoul_repeats.txt
- auval Aoi YUME control: Build/quality-p3/auval_v_aoiyume_control.txt
- auval other EON AUs: Build/quality-p3/auval_v_other_eon.txt
