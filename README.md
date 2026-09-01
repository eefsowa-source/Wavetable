# Hybrid Wavetable

JUCE 8.0.14 기반 macOS 하이브리드 웨이브테이블 신시사이저 프로토타입입니다.

## Targets

- VST3
- Audio Unit (AU)
- Standalone
- Universal Binary (`arm64` + `x86_64`)
- Minimum macOS 12.0

## Build

```sh
cmake -B Build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64'
cmake --build Build --config Debug -j2
```

The first configure downloads JUCE 8.0.14. With `COPY_PLUGIN_AFTER_BUILD` enabled,
the AU and VST3 products are copied into `~/Library/Audio/Plug-Ins/`.

## Tests

```sh
ctest --test-dir Build -C Debug --output-on-failure
auval -v aumu Hwbl Eona
```

The DSP test covers default table generation, position morphing, audio resampling,
empty-input handling, and finite oscillator output. The processor smoke test covers
MIDI Learn/preset round-trips and a full 128-voice render/release stress block.
`auval` validates the AU lifecycle, parameters, MIDI, formats, and render paths.

## Controls

The editor includes three independent wavetable position controls, filter type/slope, cutoff,
resonance, filter drive, saturation, amp/filter ADSR controls, audio import,
preset save/load, and a visible factory-preset menu with ten deterministic
random-generated cyberpunk sounds. Selecting one immediately applies its oscillator,
filter, drive, saturation, output, and envelope settings. Drag in the waveform area to draw;
Shift-drag edits harmonic content.

## Current prototype boundaries

- Audio import uses JUCE's platform formats; on macOS this includes WAV, AIFF,
  M4A/CoreAudio and other formats supported by CoreAudio. The chooser accepts
  any file and the decoder determines whether it is readable. Import samples are
  read at the table's 2048 resampling positions, so source-file size does not
  create a full-file memory allocation.
- Preset files serialize APVTS parameters, the edited wavetable payload, and MIDI
  CC assignments.
- The 8/12/18/24 dB/oct choices map to one or two JUCE state-variable filter stages;
  they are practical slope presets rather than a calibrated analogue model.
- Parameters are host-automatable and MIDI input/Learn are enabled. An installed-host
  Ableton/Logic smoke test still requires a DAW.
