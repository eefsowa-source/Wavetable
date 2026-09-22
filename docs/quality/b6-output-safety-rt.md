# Plan B Task 6: output safety and real-time state

Task 6 keeps the public parameter set and state schema unchanged while making
the output safety and random state explicit.

`OutputSafety` now owns one pinned `eon::DCBlocker` per output channel. Each
blocker stores its state as double precision and is prepared at the host sample
rate with an 18 Hz pole. The existing DC removal and 1 kHz transparency tests
remain the acceptance gates.

`eon::ScopedDenormalsOff` is active at both real-time boundaries:

- `HybridWavetableAudioProcessor::processBlock`
- `SynthVoice::renderNextBlock`

The nested scope is intentional. A host callback may render no active voices,
while a voice render can also be called by a different JUCE render path. Both
boundaries are therefore protected independently.

The voice random source preserves the old 32-bit seed input but now expands it
to the eon stream as:

```text
nonZero = seed != 0 ? seed : 0x6d2b79f5
eonSeed  = (uint64(nonZero) << 32) | nonZero
```

This is used for random oscillator phase. The arpeggiator uses its own
`eon::Rng` stream seeded with `0x53454f55` and resets that stream with the
processor transport reset. No parameter, preset field, or state schema version
was changed.

The existing CTest suite covers the DC blocker, deterministic renderer, voice
render, and eon DSP contracts. These checks are source and offline-render
evidence; they do not prove DAW CPU headroom or host loading.
