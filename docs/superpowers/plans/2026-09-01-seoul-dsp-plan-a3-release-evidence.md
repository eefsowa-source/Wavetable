# SEOUL DSP Plan A3 Release Evidence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove that the accepted A2 source becomes identifiable universal Release artifacts that pass automated DSP, wrapper, installed-host, DAW, UI, state, bounce, CPU, and listening gates without conflating one evidence class with another.

**Architecture:** A reproducible Release build produces Standalone, AU, and VST3 bundles plus test executables. A release-audit script hashes source/build/installed identities and stores raw logs. The existing quality runner validates rendered DSP; pluginval and auval validate wrapper contracts; Logic and Ableton checklists validate installed artifacts in real hosts; a randomized listening-set builder records subjective evidence. A signed manifest links every result to one source commit and artifact set.

**Tech Stack:** C++20, JUCE 8.0.14, CMake/CTest, macOS `codesign`/`plutil`/`lipo`/`shasum`, Apple `auval`, Tracktion pluginval, Logic Pro, Ableton Live, Markdown/JSON evidence.

---

## Evidence policy

- Plan A3 begins only after `docs/quality/a2-acceptance.md` exists and the worktree is clean.
- Use a new `Build-Release` directory. Do not treat Debug artifacts or a previous installed bundle as release evidence.
- Built bundle, installed bundle, discovered AU identity, loaded DAW plug-in, UI screenshot, offline bounce, and listened clip must all be traceable to the same release manifest.
- A validator pass does not prove sound quality. A DAW launch does not prove insertion. A screenshot does not prove audio. A listening preference does not prove realtime safety.
- Ad-hoc signing can prove local integrity only. Developer ID signing, notarization, and distribution readiness remain explicitly `not-run` unless valid credentials and an authorized release workflow are available.
- Raw logs and generated reports live under `dist/SEOUL-DSP-0.1.0-evidence`; release bundles/archives live under `dist/SEOUL-DSP-0.1.0`. Both directories are ignored by Git, while templates/scripts and the final concise acceptance record are committed.

### Task 1: Add reproducible release-audit tooling and evidence templates

**Files:**
- Create: `Scripts/release-audit.sh`
- Create: `Scripts/run-quality-matrix.sh`
- Create: `Tests/AudioQuality/ListeningSetBuilder.cpp`
- Create: `Tests/AudioQuality/ListeningSetBuilderTests.cpp`
- Create: `Tests/AudioQuality/listening-player.html`
- Create: `docs/quality/templates/host-audit-template.md`
- Create: `docs/quality/templates/listening-report-template.md`
- Create: `docs/quality/templates/release-manifest-template.json`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write a shell syntax and missing-input RED check**

`release-audit.sh` must use `set -euo pipefail`, accept exactly:

```text
Scripts/release-audit.sh <source-root> <build-root> <artifact-root> <evidence-root>
```

It exits non-zero if the source commit is dirty/missing, any Standalone/AU/VST3 bundle is absent, an executable is not universal, or a requested tool is missing. Add a CTest entry that invokes `bash -n Scripts/release-audit.sh` and a second test with CMake property `WILL_FAIL TRUE` that passes a missing artifact root.

Run: `bash -n Scripts/release-audit.sh`

Expected before implementation: RED because the script does not exist.

- [ ] **Step 2: Implement stable bundle hashing and identity capture**

For each bundle, store:

- bundle path and recursive SHA-256 list sorted by relative path;
- main executable SHA-256;
- `file` and `lipo -archs` output;
- `codesign -dv --verbose=4` and `codesign --verify --deep --strict --verbose=4` output;
- `plutil -convert json -o - Contents/Info.plist` output;
- source commit and `git status --porcelain=v1`;
- CMake cache values for build type, macOS architectures, deployment target, and JUCE version.

Resolve exact bundles as:

```bash
standalone="$artifact_root/Standalone/SEOUL DSP.app"
au="$artifact_root/AU/SEOUL DSP.component"
vst3="$artifact_root/VST3/SEOUL DSP.vst3"
```

Hash files relative to each bundle root so hashes remain comparable after installation at a different parent path.

- [ ] **Step 3: Implement the matrix wrapper**

`run-quality-matrix.sh` accepts the Release runner path, manifest, evidence directory, and `per-build|full`. It records start/end time, exit status, stdout/stderr, report SHA-256, and host hardware/OS metadata. It must propagate a runner failure rather than printing success.

- [ ] **Step 4: Create evidence templates with explicit statuses**

Every checklist row uses one of `pass`, `fail`, `blocked`, or `not-run`; no blank cell implies success. The release manifest template contains:

```json
{
  "schemaVersion": 1,
  "product": "SEOUL DSP",
  "version": "0.1.0",
  "sourceCommit": "",
  "buildIdentity": {},
  "artifacts": {},
  "qualityReport": {},
  "validators": {},
  "hosts": {},
  "listening": {},
  "distribution": {
    "developerIdSigning": "not-run",
    "notarization": "not-run"
  }
}
```

Empty identity fields are invalid in the finalized manifest; the empty strings are template values only.

- [ ] **Step 5: Test-first the hidden-reference builder before the release build**

Add `ListeningSetBuilderTests` and `ListeningSetBuilder` CMake targets. Tests generate short synthetic reference/candidate pairs and require 10-to-25-second output clips, integrated level difference `<= 0.1 dB`, opaque labels with no source/version clues, deterministic randomization from a non-zero seed, and one complete answer-key mapping per output. Implement the builder and static local player described in Task 9 until these tests pass. This keeps every source-controlled tool inside the source commit captured by the Release build.

- [ ] **Step 6: Verify and commit tooling**

Run:

```bash
bash -n Scripts/release-audit.sh
bash -n Scripts/run-quality-matrix.sh
ctest --test-dir Build -R 'ReleaseAuditScripts|ListeningSetBuilder' --output-on-failure
```

Expected: syntax tests pass and the deliberate missing-root test observes a non-zero exit.

Commit: `git add CMakeLists.txt Scripts Tests/AudioQuality docs/quality/templates && git commit -m "test: add release evidence tooling"`

### Task 2: Produce a fresh universal Release build

**Files:**
- Verify: `CMakeLists.txt`
- Create build output: `Build-Release/`
- Create release staging: `dist/SEOUL-DSP-0.1.0/`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/build/`

- [ ] **Step 1: Verify clean accepted source**

Run:

```bash
git status --short
test -f docs/quality/a2-acceptance.md
git rev-parse HEAD
```

Expected: clean output from `git status`, acceptance file exists, and the source commit is captured. Stop on failure.

- [ ] **Step 2: Configure from an empty Release directory**

Run:

```bash
test ! -e Build-Release
mkdir -p dist/SEOUL-DSP-0.1.0-evidence/build dist/SEOUL-DSP-0.1.0
cmake -S . -B Build-Release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64'
```

Expected: configure succeeds with JUCE 8.0.14 and no architecture downgrade. `Build-Release/CMakeCache.txt` contains `CMAKE_BUILD_TYPE:STRING=Release` and `CMAKE_OSX_ARCHITECTURES:STRING=arm64;x86_64`.

If `Build-Release` already exists, stop and preserve it; archive or choose a new fully propagated build-root name before continuing. Do not delete an unidentified prior build.

- [ ] **Step 3: Build every product and test target**

Run:

```bash
set -o pipefail
cmake --build Build-Release --config Release -j 4 \
  2>&1 | tee dist/SEOUL-DSP-0.1.0-evidence/build/build.log
```

Expected: exit `0`; Standalone, AU, VST3, all unit tests, and `AudioQualityRunner` exist under `Build-Release/*_artefacts/Release`.

- [ ] **Step 4: Verify universal executables**

Run:

```bash
lipo -archs 'Build-Release/HybridWavetable_artefacts/Release/Standalone/SEOUL DSP.app/Contents/MacOS/SEOUL DSP'
lipo -archs 'Build-Release/HybridWavetable_artefacts/Release/AU/SEOUL DSP.component/Contents/MacOS/SEOUL DSP'
lipo -archs 'Build-Release/HybridWavetable_artefacts/Release/VST3/SEOUL DSP.vst3/Contents/MacOS/SEOUL DSP'
```

Expected for each: both `arm64` and `x86_64` are listed.

- [ ] **Step 5: Copy bundles into an immutable staging tree**

Create `Standalone`, `AU`, and `VST3` staging subdirectories, then use `ditto` to copy the three bundles from `Build-Release/HybridWavetable_artefacts/Release` into the matching subdirectory under `dist/SEOUL-DSP-0.1.0`. Do not rebuild or edit inside staging after Task 3 hashes it.

### Task 3: Audit built artifact identity, signing, and hashes

**Files:**
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/artifacts/`
- Create: `dist/SEOUL-DSP-0.1.0-evidence/release-manifest.json`
- Verify: `dist/SEOUL-DSP-0.1.0/`

- [ ] **Step 1: Run the release audit against staged bundles**

Run:

```bash
Scripts/release-audit.sh "$PWD" "$PWD/Build-Release" \
  "$PWD/dist/SEOUL-DSP-0.1.0" \
  "$PWD/dist/SEOUL-DSP-0.1.0-evidence/artifacts"
```

Expected: identity/hashes are written for all formats and the command exits `0`.

- [ ] **Step 2: Verify expected metadata**

Require:

- display/product name `SEOUL DSP`;
- bundle ID `com.eonaudio.hybridwavetable` for the plug-in target metadata;
- AU type/subtype/manufacturer `aumu/Hwbl/Eona`;
- VST3 and AU executable name `SEOUL DSP`;
- macOS deployment target 12.0;
- version `0.1.0`.

Any mismatch is a build failure; do not patch `Info.plist` inside the bundle.

- [ ] **Step 3: Verify local signatures without overstating distribution status**

Run `codesign --verify --deep --strict --verbose=4` on all three staged bundles. Record the signing authority and TeamIdentifier. Ad-hoc signatures may pass local verification but keep Developer ID and notarization as `not-run`.

- [ ] **Step 4: Populate the release manifest**

Copy the template and fill every build/artifact field with captured values and relative evidence paths. Validate JSON:

Run: `plutil -lint dist/SEOUL-DSP-0.1.0-evidence/release-manifest.json`

Expected: `OK`; source commit and all three executable hashes are non-empty.

### Task 4: Run Release CTest, the full quality matrix, and CPU gates

**Files:**
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/tests/`
- Modify evidence: `dist/SEOUL-DSP-0.1.0-evidence/release-manifest.json`

- [ ] **Step 1: Run all Release tests**

Run:

```bash
set -o pipefail
ctest --test-dir Build-Release -C Release --output-on-failure \
  2>&1 | tee dist/SEOUL-DSP-0.1.0-evidence/tests/ctest-release.log
```

Expected: every test passes. Record the test count and log SHA-256; do not summarize only the last line.

- [ ] **Step 2: Run the complete release matrix**

Run:

```bash
Scripts/run-quality-matrix.sh \
  "$PWD/Build-Release/AudioQualityRunner_artefacts/Release/AudioQualityRunner" \
  "$PWD/Tests/AudioQuality/fixture-manifest.json" \
  "$PWD/dist/SEOUL-DSP-0.1.0-evidence/tests/quality-full" full
```

Expected: all sample rates `44.1/48/88.2/96/176.4/192 kHz`, block sizes `16..2048`, mono/stereo, MIDI boundary offsets, and irregular final blocks pass the A0+A2 hard, signal, and Golden gates.

- [ ] **Step 3: Run identified CPU measurements**

At 48 kHz/block 128, render the 32-note musical fixture at Eco and High. Require p95 below 25% and maximum below 80% of the 2.667 ms deadline. Run the 128-note stress fixture and require finite/allocation-free output without presenting it as a production CPU promise.

Record hardware model, OS, architecture, build configuration, sample format, mode latency, source commit, and runner hash. Add Eco/High CPU ratio and status to the manifest.

- [ ] **Step 4: Stop on any Release-only divergence**

If Debug was green but Release fails or produces a different deterministic render, retain both reports, identify the first divergent fixture/block, and return to the responsible A0-A2 task. Do not weaken tolerances inside A3.

### Task 5: Validate the staged VST3 with pluginval

**Files:**
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/validators/pluginval-5.log`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/validators/pluginval-10.log`
- Modify evidence: `dist/SEOUL-DSP-0.1.0-evidence/release-manifest.json`

- [ ] **Step 1: Verify the validator binary identity**

Run:

```bash
test -x /Applications/pluginval.app/Contents/MacOS/pluginval
/Applications/pluginval.app/Contents/MacOS/pluginval --version
shasum -a 256 /Applications/pluginval.app/Contents/MacOS/pluginval
```

Expected: executable exists; version and hash are recorded before results.

- [ ] **Step 2: Run regular strictness 5**

Run:

```bash
set -o pipefail
/Applications/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 5 --timeout-ms 60000 \
  --validate "$PWD/dist/SEOUL-DSP-0.1.0/VST3/SEOUL DSP.vst3" \
  2>&1 | tee dist/SEOUL-DSP-0.1.0-evidence/validators/pluginval-5.log
```

Expected: pluginval returns `0` and reports success for the exact staged path.

- [ ] **Step 3: Run release strictness 10 including GUI tests**

Run the same command with `--strictness-level 10`, saving `pluginval-10.log`. Do not add `--skip-gui-tests` for the release gate.

Expected: exit `0`. Any crash, timeout, JUCE assertion, or skipped required test is recorded separately and blocks A3 even when another pluginval summary line says pass.

- [ ] **Step 4: Bind results to the VST3 hash**

Write pluginval version/hash, bundle executable hash, command, exit status, log hash, and timestamp into the release manifest.

### Task 6: Install exact AU/VST3 copies and validate AU discovery

**Files:**
- Install: `~/Library/Audio/Plug-Ins/Components/SEOUL DSP.component`
- Install: `~/Library/Audio/Plug-Ins/VST3/SEOUL DSP.vst3`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/install/`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/validators/auval.log`
- Modify evidence: `dist/SEOUL-DSP-0.1.0-evidence/release-manifest.json`

- [ ] **Step 1: Back up conflicting installed bundles without deleting them**

If a destination exists and its recursive hashes differ, move it into `dist/SEOUL-DSP-0.1.0-evidence/install/previous/<timestamp>/`. Record the old bundle hash and path. Never silently overwrite an unidentified installed artifact.

- [ ] **Step 2: Install with resource-preserving copies**

Run:

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components" "$HOME/Library/Audio/Plug-Ins/VST3"
ditto "$PWD/dist/SEOUL-DSP-0.1.0/AU/SEOUL DSP.component" \
  "$HOME/Library/Audio/Plug-Ins/Components/SEOUL DSP.component"
ditto "$PWD/dist/SEOUL-DSP-0.1.0/VST3/SEOUL DSP.vst3" \
  "$HOME/Library/Audio/Plug-Ins/VST3/SEOUL DSP.vst3"
```

Expected: `diff -qr` between staged and installed bundles prints nothing and both executable hashes match the release manifest.

- [ ] **Step 3: Refresh and prove the current AU identity**

Terminate only the per-user AudioComponentRegistrar process so macOS can relaunch it, then run:

```bash
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -a | rg 'Eona|Hwbl|SEOUL DSP'
set -o pipefail
auval -v aumu Hwbl Eona \
  2>&1 | tee dist/SEOUL-DSP-0.1.0-evidence/validators/auval.log
```

Expected: discovery lists `aumu/Hwbl/Eona`; targeted auval returns `0` for `SEOUL DSP`. If stale `Hybrid Wavetable` metadata appears, mark AU validation failed and diagnose cache/registration identity before continuing.

- [ ] **Step 4: Preserve the auval boundary**

Record the command, installed AU hash, discovered identity, exit code, and log hash. State explicitly that auval does not test audible DSP quality or Logic UI behavior.

### Task 7: Validate real-time and offline behavior in Logic Pro AU

**Files:**
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/hosts/logic-audit.md`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/hosts/logic-ui.png`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/hosts/logic-realtime.wav`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/hosts/logic-bounce.wav`
- Modify evidence: `dist/SEOUL-DSP-0.1.0-evidence/release-manifest.json`

- [ ] **Step 1: Confirm the loaded component identity**

Open a new empty Logic project, create one software-instrument track, insert `SEOUL DSP` from manufacturer EON Audio, and record Logic/macOS version plus the installed AU hash. A previously saved project or merely seeing the plug-in in the menu is insufficient.

- [ ] **Step 2: Exercise audio and automation**

Play MIDI notes 24/60/96, a 32-note chord, and release tails at 44.1/48/96 kHz with buffer sizes 32/128/1024 where Logic permits. Automate cutoff, resonance, three wavetable positions, filter envelope amount, Filter Drive, saturation, delay time/feedback/mix, reverb mix, master width, and each unison/detune control.

Pass criteria: audible response for every control, no click/dropout/non-finite mute, correct mono/stereo behavior, and no stuck note after transport stop.

- [ ] **Step 3: Verify save/close/reopen state**

Edit the wavetable, assign one MIDI CC, choose a factory preset, change quality mode, save, close Logic completely, reopen, and verify parameter state, table, mapping, pending/active quality behavior, latency, and audible render.

- [ ] **Step 4: Verify UI rendering**

Open the editor at minimum/default/maximum supported sizes. Capture the actual loaded UI with the plug-in title visible. Pass criteria: no blank/black region, clipped essential control, broken attachment, or resize crash. The screenshot is UI evidence only.

- [ ] **Step 5: Compare real-time capture and offline bounce**

Capture the same deterministic one-note technical MIDI region in real time and via offline bounce with random phase/drift disabled. Align within 32 samples and require RMS/loudness/true-peak/spectral deltas within the Golden tolerances. Record any host gain/pan/latency compensation used.

### Task 8: Validate real-time and offline behavior in Ableton Live VST3

**Files:**
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/hosts/ableton-audit.md`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/hosts/ableton-ui.png`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/hosts/ableton-realtime.wav`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/hosts/ableton-export.wav`
- Modify evidence: `dist/SEOUL-DSP-0.1.0-evidence/release-manifest.json`

- [ ] **Step 1: Confirm the loaded VST3 identity**

Start a new empty Live Set, rescan if needed, insert the installed VST3 from `~/Library/Audio/Plug-Ins/VST3`, and record Live/macOS version plus the installed executable hash.

- [ ] **Step 2: Repeat the required control, state, UI, and transport checks**

Use the same MIDI, parameter list, quality-mode, state-reopen, resize, stuck-note, and sample-rate/buffer checklist as Logic. Do not copy Logic results into the Ableton report.

- [ ] **Step 3: Compare real-time capture and offline export**

Render the same deterministic technical region, align, and enforce the same Golden tolerances. Record Live warp, track gain, pan, sample-rate conversion, normalization, and plugin-delay-compensation settings; all must be neutral or explicitly accounted for.

- [ ] **Step 4: Record host independence**

Logic and Ableton each require their own pass. One host cannot substitute for a blocked or failed result in the other.

### Task 9: Build and run a randomized hidden-reference listening set

**Files:**
- Verify: `Tests/AudioQuality/ListeningSetBuilder.cpp`
- Verify: `Tests/AudioQuality/ListeningSetBuilderTests.cpp`
- Verify: `Tests/AudioQuality/listening-player.html`
- Verify: `CMakeLists.txt`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/listening/set/`
- Create evidence: `dist/SEOUL-DSP-0.1.0-evidence/listening/listening-report.md`
- Create evidence after scoring: `dist/SEOUL-DSP-0.1.0-evidence/listening/answer-key.json`

- [ ] **Step 1: Re-run level-match and blinding tests from the Release build**

Run the tests created in Task 1 against the Release executable. Confirm output clips are 10-to-25 seconds, integrated level difference is `<= 0.1 dB`, file labels contain no `reference`, `candidate`, algorithm, commit, or version clue, and the answer key maps every opaque label exactly once.

- [ ] **Step 2: Generate the listening set with the already-tested builder**

Accept:

```text
ListeningSetBuilder --reference-dir <path> --candidate-dir <path>
  --manifest <path> --output-dir <path> --seed <non-zero integer>
```

For each excerpt, create A/B/X files where X is a randomized duplicate of A or B. Apply one constant gain per complete clip to achieve `<=0.1 dB` level match; do not compress, limit, EQ, or normalize individual sections. Write the answer key outside the player-visible metadata.

- [ ] **Step 3: Use the local switchable player**

Verify that the static player loads only opaque A/B/X filenames, supports instant position-matched switching, loop selection, one score per excerpt, notes, and JSON export. It must not load or display the answer key before the listener submits scores.

- [ ] **Step 4: Build the five critical excerpts**

Use high-note aliasing, unison bass clarity, filter resonance sweep, transient/release, and dense-chord clarity. Reference is the accepted A0+A1 or pre-A2 Golden as declared per excerpt; candidate is the exact A2 Release render or host capture. Store source hashes in the hidden key.

- [ ] **Step 5: Conduct and score the session**

The candidate must have no newly introduced critical artifact and be preferred or judged equivalent on all five excerpts. Record ABX correctness, preference/equivalence, artifact notes, monitoring chain, listening level, room/headphones, listener count, and date. With one listener, state that the result is evidence, not statistical significance.

- [ ] **Step 6: Hash listening evidence**

Run: `ctest --test-dir Build-Release -C Release -R ListeningSetBuilder --output-on-failure`

Expected: blinding, duration, level matching, key completeness, and deterministic-seed tests pass. Hash the exported scores, listening report, and answer key into the release manifest; do not modify source after the artifact commit was built.

### Task 10: Finalize the release manifest, package, and acceptance decision

**Files:**
- Create: `docs/quality/plan-a-acceptance.md`
- Finalize: `dist/SEOUL-DSP-0.1.0-evidence/release-manifest.json`
- Create: `dist/SEOUL-DSP-0.1.0-macOS-universal.zip`
- Create: `dist/SEOUL-DSP-0.1.0-macOS-universal.zip.sha256`
- Verify: all evidence from Tasks 2-9

- [ ] **Step 1: Reject incomplete evidence explicitly**

Validate that the manifest contains no empty required field and no `not-run` for Release build, CTest, quality matrix, CPU gate, pluginval 5/10, auval, Logic AU, Ableton VST3, UI render, state restore, real-time audio, offline bounce, or listening. `blocked` and `fail` prevent Plan A acceptance.

Developer ID signing and notarization may remain `not-run` only if the package is labeled local-validation/non-distribution.

- [ ] **Step 2: Recheck identity after host validation**

Re-hash the staged and installed AU/VST3 bundles. Expected: staged hashes match Task 3; installed hashes match staged hashes; Logic and Ableton audit records name those same hashes. If any differs, repeat the affected validator/host evidence with the final artifact.

- [ ] **Step 3: Create the archive without changing staged bundles**

Run:

```bash
ditto -c -k --sequesterRsrc --keepParent \
  dist/SEOUL-DSP-0.1.0 dist/SEOUL-DSP-0.1.0-macOS-universal.zip
shasum -a 256 dist/SEOUL-DSP-0.1.0-macOS-universal.zip \
  > dist/SEOUL-DSP-0.1.0-macOS-universal.zip.sha256
```

Expected: archive exists and its SHA-256 is inserted into the release manifest.

- [ ] **Step 4: Write the concise Plan A acceptance record**

`docs/quality/plan-a-acceptance.md` must list:

- source commit and release/archive hashes;
- built and installed AU/VST3 executable hashes;
- Release CTest count/result;
- quality report path/hash and matrix result;
- Eco/High CPU/latency results;
- pluginval 5/10 and auval status/log hashes;
- Logic and Ableton status with screenshot/audio/bounce paths;
- listening result and report/key hashes;
- signing/notarization status;
- remaining limitations and the explicit `Plan A accepted` or `Plan A rejected` decision.

- [ ] **Step 5: Validate manifests and commit only source-controlled evidence**

Run:

```bash
plutil -lint dist/SEOUL-DSP-0.1.0-evidence/release-manifest.json
shasum -a 256 -c dist/SEOUL-DSP-0.1.0-macOS-universal.zip.sha256
git add CMakeLists.txt Scripts Tests/AudioQuality docs/quality
git commit -m "test: record Plan A release acceptance"
git status --short
```

Expected: JSON/archive checks pass and the Git worktree is clean. Generated `dist` artifacts remain ignored but their exact hashes and paths are in the committed acceptance record.

## A3 completion boundary

Plan A is accepted only when the same universal Release artifacts pass the full DSP matrix, CPU gates, pluginval 5/10, targeted auval, Logic AU and Ableton VST3 real-time/offline/state/UI checks, and the five-excerpt listening gate. Any blocked or failed required evidence means `Plan A rejected/incomplete`; it is never converted into a pass by another evidence class. Plan B implementation may start only after this acceptance record says `Plan A accepted`.
