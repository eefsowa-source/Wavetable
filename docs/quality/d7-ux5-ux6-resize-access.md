# UX-5 / UX-6: sizing, scale, and readability audit

Date: 2026-10-15 (KST)
Tasks: Plan C UX-5 (resize/scale), UX-6 (contrast + keyboard audit).
Editor-only changes; no DSP.

## UX-5 changes

- Resize limits are now 1120..1800 x 760..1140 (was 1040..1800 x 760..1100).
- Scale preset combo on the utility strip: 100% / 125% / 150% -> 1120x760,
  1400x950, 1680x1140.
- Defect found by the new boundary test: the top-row buttons (Load Audio,
  Save/Load Preset) ended past the right edge even at the default 1120 width
  (externalWidth went negative) and were unreachable. They moved to the
  utility strip next to Values/Undo/Redo/A-B/Scale. Minimum width raised to
  1120 because the knob and envelope rows genuinely cannot pack tighter.
- 75% was dropped: the layout cannot go below 1120 without overlapping rows.

## UX-6 results

- Contrast: computed WCAG contrast directly on the skin table (analytic, not
  PNG sampling). All 30 skins pass: text/panel worst 12.87 (Slate Blueprint),
  text/background worst 15.50, dim/background worst 5.50 - all above AA.
  No palette changes needed; the audit is now a ProcessorSmoke assertion set.
- Keyboard: editor had no focus setup at all; now setWantsKeyboardFocus(true).
  Focus traversal order is JUCE default (creation order). Inspector Cmd+I
  coexistence unverified in a live Debug editor - recorded as a limit.

## Evidence

- ctest -R ProcessorSmoke: PASS - resize-boundary containment at both limits,
  keyboard focus flag, per-skin contrast asserts (3 per skin, 90 checks).
- UiSnapshot --all Build/quality-ui-ux56 1120 760: 90 PNGs, 90 unique md5.
- Scaled render 1400x950 (125%): snapshot produced, controls within bounds.

## Limits

- Hosts that refuse editor resize may clamp sizes differently; the strip and
  controls stay inside at both declared extremes on this build.
- Keyboard navigation order is declaration order, not a curated tab order;
  no per-control focus outline audit was done.
