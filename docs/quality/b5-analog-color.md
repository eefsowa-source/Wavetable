# Plan B Task 5: conditional analog color evaluation

This task evaluates the vendored `eon::TriodeStage` Koren/Newton stage and the
`eon::JilesAtherton` transformer model as internal candidates. They are not
connected to the shipping `SynthVoice` path and no parameter or preset schema
was added.

The candidate harness renders a 10 kHz sine at 48 kHz in irregular 127-sample
blocks. Each nonlinear candidate is processed at 4x through the pinned eon
half-band oversampler, then downsampled and DC blocked at 18 Hz. The measured
window is the final 96,000 samples after a 48,000-sample warmup.

The folded-third proxy measures the 18 kHz line produced by a 30 kHz third
harmonic folding through the 48 kHz sample rate. More-negative dBc is cleaner.
This is the same quality direction used by the Plan B saturation gate. THD is
reported as a colour signature, not as proof that more distortion is better.

The Release candidate executable (`AnalogColorCandidateTests`, SHA-256
`6644a4b02aa7bf48bd226625af349d28c5fa4333c9dd59e3cc18bbe4525973b3`) measured:

| path | RMS | DC | peak | folded third | THD |
| --- | ---: | ---: | ---: | ---: | ---: |
| linear baseline | 0.247487 | 7.46e-16 | 0.350000 | -249.72 dBc | 0.0000 |
| Triode | 0.150114 | 5.82e-10 | 0.222077 | -155.54 dBc | 0.0582 |
| Transformer | 0.176120 | 2.68e-12 | 0.248093 | -67.67 dBc | 0.0009 |

## Decision

The candidates are rejected for the production path in this plan:

- The existing linear path is already at the measurement floor, so the Triode
  candidate cannot demonstrate a three-dB alias improvement over it.
- The Transformer candidate adds a strong folded component at the high-frequency
  probe and does not clear the product's absolute alias ceiling.
- No level-matched human listening comparison was available in this task, so
  the alternative listening acceptance route remains open and unclaimed.

The candidate test remains as a reproducible rejection record. A future color
plan can revisit it only with a higher-rate/oversampled circuit formulation,
explicit level matching, a CPU budget, and a blind listening comparison.

## Reproduction

```sh
cmake --build Build-Release --target AnalogColorCandidateTests
ctest --test-dir Build-Release -R AnalogColorCandidate --output-on-failure
```

The test is DSP-only evidence. It does not prove pluginval, auval, REAPER or
Ableton loading, automation recall, CPU headroom in a DAW, or listening
preference.
