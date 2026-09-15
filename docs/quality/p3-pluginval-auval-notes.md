# P3: pluginval / auval validation notes (SEOUL DSP)

> 이 문서는 2026-09-03 A0+A1 바이너리 기록이다. 2026-09-06 A2 작업 트리의
> 재검증과 현재 해시는 [`a2-progress.md`](a2-progress.md)에 기록한다.

## 2026-09-15 정정 및 현재 Release 결과

- 기존 auval FAIL은 등록/팩토리 결함이 아니었다. `auval` 인자 순서는
  type, subtype, manufacturer인데 기존 기록은 `aumu Eona Hwbl`로 subtype과
  manufacturer를 뒤집어 호출했다.
- Info.plist의 실제 식별자 `aumu/Hwbl/Eona`에 맞춰
  `auval -v aumu Hwbl Eona`를 실행하면 Release AU가
  `AU VALIDATION SUCCEEDED`로 통과한다. custom UI, 55 parameters, MIDI,
  mono/stereo render를 포함하며 exit 0이다.
- Release VST3 pluginval strictness 10은 `SUCCESS`, exit 0, JUCE assertion
  0이다.
- 현재 Release 설치 해시: VST3
  `e6a11a2f2081be36d65932070b138752139ee1586b1378599f1e1011ffa09c5b`,
  AU `8e1b6c3662c3c471fc0c7e790ba14aecd111d4cca8ca040716ccd193e2c64a36`.
- 현재 로그:
  `Build/quality-ui/host-validation/pluginval-ui90-release-strictness10.log`,
  `Build/quality-ui/host-validation/auval-ui90-release.log`.

아래 auval FAIL 분석은 잘못된 명령으로부터 나온 2026-09-03 역사 기록이며 현재
등급으로 사용하지 않는다.

Date: 2026-09-03, macOS 15.7.9 (arm64).

## 1. pluginval grade - VST3 PASS (strictness 10)

- Install: GitHub release download (not brew). ~/tools/pluginval/pluginval.app, pluginval 1.0.4. Pre-existing /Applications/pluginval.app (same 1.0.4) untouched.
- Command: ~/tools/pluginval/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --verbose --timeout-ms 300000 "$HOME/Library/Audio/Plug-Ins/VST3/SEOUL DSP.vst3"
- Result: SUCCESS, exit 0, ~20 s. Prior AppKit abort / exit 134 was NOT reproduced on this build.
- Target identity (current UI-fix rebuild): installed VST3 binary sha256
  `1c2121e56d5703ca140248c603d8a9b146730449f4451b29edc3249615ac690c`, equal to
  `Build/HybridWavetable_artefacts/Debug/VST3/SEOUL DSP.vst3`.
- FAILURE lines: none. 505 JUCE assertion log lines (Debug build):
  - juce_AudioProcessor.cpp:451 x 1 (cold open)
  - juce_audio_plugin_client_VST3.cpp:3747 x 4
  - juce_AudioProcessor.cpp:599 x 500 (during fuzzing)
  - Not pluginval failures, but re-verify on a Release build to confirm they vanish.
- Log: Build/quality-p3/pluginval_ui_fix_strictness10.log
- Current post-sandbox log: `Build/quality-p3/pluginval_after_sandbox.log`.
- Release cross-check: `Build/quality-p3/pluginval_release_strictness10.log`
  also exits 0/SUCCESS and contains 0 JUCE assertion lines (Release binary SHA
  `a8fc0ac00ea7eb97b2df6d190899980f3d5b75cf327181f40ff41011044fbf7f`).
- Final installed Debug AU after the UI-fix rebuild has SHA-256
  `9c6fb212a213e705f66caac48f66aba8e5f8e73bc75f7eed948b943cced89094`; its
  Info.plist intentionally uses JUCE's default `resourceUsage` metadata.

## 2. auval historical result - invalid identifier order

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

## 3. Historical root-cause hypothesis (superseded)

The initial sandbox hypothesis is **rejected by experiment**: adding `AU_SANDBOX_SAFE TRUE`
to SEOUL DSP changed its plist from `resourceUsage` to `sandboxSafe=1`, but
`auval -v aumu Eona Hwbl` still fails identically. Because SEOUL DSP also offers
GUI file import and preset save, the production source keeps the explicit
`AU_SANDBOX_SAFE FALSE` setting; the experiment is not retained as a workaround.

Current leading hypotheses (confidence: medium):

1. AudioComponentRegistrar registration/cache state or a component-load error
   specific to this AU implementation. `auval -a` does not list SEOUL DSP,
   while REAPER's direct AU scan does load the same binary.
2. A stale installed `Hybrid Wavetable.component` shares the same
   `aumu/Eona/Hwbl` identity. Temporarily moving that duplicate out and restarting
   the registrar did **not** change the auval result, so duplicate identity is not
   proven as the cause.

Recommended follow-ups:

1. Compare a minimal JUCE AU template using the same `aumu/Eona/Hwbl` identity
   against SEOUL DSP to isolate factory/registration behavior.
2. If permitted, perform a sudo-level AudioComponentRegistrar reset or reboot,
   then rerun `auval -a`/`-v`.
3. Keep `AU_SANDBOX_SAFE FALSE` until a sandbox-safe file-access design is proven;
   the unresolved auval issue should be fixed at the registration/factory layer.

## 4. Still untested

- The earlier AU loading/render evidence is tied to the pre-UI-fix AU hash
  `7113dc…`. A current-binary REAPER retry after the UI fix was blocked at the
  REAPER GUI/plugin-scan startup, so do not relabel the old host-render WAV as
  current evidence. See the acceptance document for the exact blocker.
- sudo-level AudioComponentRegistrar reset (needs password).
- pluginval Release strictness 10 is verified PASS with zero JUCE assertion lines.

## 5. Evidence files

- pluginval log: Build/quality-p3/pluginval_vst3_strictness10.log
- auval -a listing: Build/quality-p3/auval_a_listing.txt
- auval SEOUL repeats: Build/quality-p3/auval_v_seoul_repeats.txt
- auval Aoi YUME control: Build/quality-p3/auval_v_aoiyume_control.txt
- auval other EON AUs: Build/quality-p3/auval_v_other_eon.txt
