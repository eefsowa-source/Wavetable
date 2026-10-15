# SQ-5 host gates on the current binary (partial)

Date: 2026-10-15 (KST)
Binary: Build-Release of commits through 43cacb9 (all UX tasks landed).
- VST3 SHA-256: cffd6ac1f35a14535ebe3d0a68dbbb0ef9621553060bc653121923774427c7b4
- AU SHA-256: 7eb7142246e2e7daa066e33e7a9b9dd21d61c2a1385d2e1417e6f2c96a33aff8
- Installed copies verified byte-identical to the build artefacts (the stale-
  install trap from the notes).
- Host: macOS 15.7.9, pluginval 1.0.4, auval 1.10.0, REAPER 7.79 arm64.

## Results

- **VST3 pluginval s10: PASS** (33 s, fuzz parameter sweep through 55 params).
  Log: Build-Release/host-validation-20261015/pluginval-vst3.log
- **auval aumu/Hwbl/Eona: PASS** (render probes 11k-192k Hz, MIDI, ramped
  params). Log: Build-Release/host-validation-20261015/auval.log
- **AU pluginval: TIMEOUT (explicit limit, not pass)**. Reproduced the old
  stall; now localized: it dies inside pluginval's own AU harness at the
  channel-layout enumeration step, after parameter tests pass. auval itself
  covers the same contract cleanly. Recorded as harness limit, no plugin
  defect signal seen.
- **REAPER AU load: PASS (ReaScript)**. TrackFX_AddByName -> "AUi: SEOUL DSP
  (EON Audio)", 59 params, enabled. __startup.lua + profile-load via
  System Events; script removed after the run.
- **REAPER VST3 load: FAIL (explicit limit)**. TrackFX_AddByName returned -1
  for 15 name/path variants (VST3i:/VST3:/vst3: prefixes, vendor-qualified
  names, raw and full paths, file name). REAPER rescanned the plugin after
  the cache entry was removed (entry reappeared in
  reaper-vstplugins_arm64.ini, tagged !!!VSTi) but never maps the binary
  (lsof). Control: "VST3i: Serum (Xfer Records)" instantiates instantly, so
  REAPER's VST3i path works; the refusal is specific to this bundle before
  any code runs. Adhoc signature, no quarantine xattr. AU-in-REAPER and
  Ableton VST3 (b8) both load, so the plugin itself is loadable elsewhere.
  Unresolved; candidates if revisited: instrument-slot-specific insertion
  path, REAPER's VST3 child scanner process log.

## Still open

- Clean Ableton insertion, DAW automation/state-recall, dropout observation.
- SQ-6 expanded blind listening matrix; SQ-1/SQ-3 listening confirmations.
- Golden promotion remains blocked on the above.
