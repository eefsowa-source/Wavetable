# SEOUL DSP Plan B integration report

The Plan B DSP upgrade is implemented through the pinned `eon_dsp` copy and
ends this pass with filter slopes, filter drive, saturation quality tiers,
conditional analog-color evaluation, output DC safety, denormal handling, and
deterministic RNG state covered by source and offline gates.

Release and Debug CTest both pass all 12 tests. The Task 5 analog-color
candidates were rejected because Triode did not improve the already-clean
baseline and Transformer failed the folded-energy ceiling. The production
SynthVoice path therefore has no new color parameter or unapproved circuit
stage.

The full offline safety matrix covers 192 sample-rate/block/channel cases with
zero finite failures. CPU measurements for Eco, Normal, and High are recorded
in [b-task-matrices.md](b-task-matrices.md); they are capacity evidence, not a
quality claim.

The exact results and candidate decisions are split into:

- [b2-alias-baseline.md](b2-alias-baseline.md) for saturation quality;
- [b4-filter-slopes.md](b4-filter-slopes.md) for the four slopes and drive;
- [b5-analog-color.md](b5-analog-color.md) for the rejected circuit candidates;
- [b6-output-safety-rt.md](b6-output-safety-rt.md) for DC, denormals, and RNG;
- [b-task-matrices.md](b-task-matrices.md) for this integration run;
- [b7-host-validation.md](b7-host-validation.md) for plugin and host evidence.

Host evidence now includes a successful VST3 pluginval run at strictness 10,
standalone AU `auval` success, and direct REAPER AU editor loading. Ableton
browser discovery and track selection were observed, but actual insertion is
still provisional because the captured set already contained other EON
devices. AU pluginval completion, REAPER VST3 loading, clean Ableton insertion,
DAW automation and state recall, CPU dropout behavior in a DAW, and
level-matched blind listening remain open. The runner reports in the matrix
directory also do not constitute Golden acceptance because the expanded
scenarios have no promoted Golden files. See
[b7-host-validation.md](b7-host-validation.md) for the exact boundary.
