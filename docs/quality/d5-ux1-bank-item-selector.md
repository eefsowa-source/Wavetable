# UX-1: two-step UI type selector (bank -> item)

Date: 2026-10-15 (KST)
Task: Plan C UX-1. Replaces the flat 90-entry combo with a bank combo
(A // Skins, B // Layouts, C // Curated) plus a 30-item combo. The persisted
`uiType` stays a single 0..89 value; no parameter or state schema change.

## Changes

- `Source/PluginEditor.{h,cpp}`: new `uiBankMenu` combo, `populateUiTypeItems(bank)`,
  `applyUiType` now syncs bank + item selection and writes
  `processor.setUiType()`.
- Bug fixed in passing: editor selection previously never reached
  `processor.uiType`, so the chosen type was not saved with the session.
- Bank switch keeps the same item index within the bank (A-17 <-> B-17),
  which makes comparing banks at the same row a two-click move.
- Dead `layoutTypeSelector` declaration removed; `uiBankMenu`/`uiTypeMenu`
  get component names for test/accessibility lookup.

## Evidence

- Build: Build-Release (CMAKE_BUILD_TYPE=Release), target ProcessorSmokeTests
  + SeoulDSP_UISnapshot (universal x86_64+arm64).
- `ctest -R ProcessorSmoke`: PASS (11.3 s). New assertions: editor creation,
  bank combo index for type 47/5, item text == layouts()[17] "Stage Compact",
  skins()[5] "Concrete Mono", and `processor.getUiType()==47` after
  `applyUiType` (persistence path).
- Snapshot: `SeoulDSP_UISnapshot --all Build/quality-ui-ux1 1120 760` ->
  90 PNGs, **90 unique md5** (regression-free).
- Single-mode dump type 47: `combo "B // Layouts" index=1`,
  `combo "Stage Compact" index=17` — two-step sync confirmed.
- type-65 single vs --all consistency was a prior gate; unchanged rendering
  path (only the top-row selector split), unique-md5 gate covers it.

## Limits / next

- Host/DAW evidence and listening stay open (SQ-5/SQ-6); this is editor-only,
  no DSP touched.
- Item names elide at narrow widths (min item combo ~80 px in the densest
  compact layout); full names remain visible in the dropdown.

