# UX-3 / UX-4: editing friction and preset UX

Date: 2026-10-15 (KST)
Tasks: Plan C UX-3 (editing friction), UX-4 (preset UX). Editor-only; no DSP
or parameter/state schema changes.

## UX-3 changes

- Double-click a knob -> parameter default, via setDoubleClickReturnValue with
  the APVTS default (covers 13 main knobs + 8 envelope sliders).
- Shift or Alt held during a rotary drag drops sensitivity to 1/4
  (FineSlider::mouseDrag; JUCE re-reads drag sensitivity per event, so the
  modifier can flip mid-drag without a value jump).
- VALUES toggle pins knob readouts on screen (LookAndFeel flag).
- Wavetable drawing undo/redo: one snapshot per stroke (mouseDown), 32-deep
  deque of frame 0, Undo/Redo buttons beside the Values toggle.

## UX-4 changes

- < / > buttons step through the 11 factory presets, wrapping; the combo keeps
  showing the current preset name (no separate label needed).
- A/B compare: ValueTree copyState snapshots in memory only; first toggle
  seeds B with A's state. Button text shows the live side.
- No user-preset directory exists in the codebase, so stepping covers the
  factory bank only - .preset files still go through Save/Load buttons.

## Evidence

- ctest -R ProcessorSmoke: PASS (12.5 s). New asserts: undo() restores frame 0
  exactly, redo() reapplies; A/B toggle round-trips a modified cutoff to its
  leaving value; earlier UX-1 bank/item sync asserts unchanged.
- UiSnapshot --all Build/quality-ui-ux34 1120 760: 90 PNGs, 90 unique md5.

## Limits

- Undo covers the wave editor's own mouseDown strokes. Direct wavetable writes
  that bypass the editor (e.g. audio import path) are not tracked - would need
  the snapshot to move into the processor's mutation API.
- Fine-tune factor is a fixed 4x for Shift and Alt alike; no separate finer
  Alt tier.
