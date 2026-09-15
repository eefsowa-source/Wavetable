# SEOUL DSP Plan A0+A1 acceptance

> 2026-09-06부터 A2 작업이 진행 중이다. 아래의 설치 바이너리 해시와 P3 증거는
> A0+A1 최종 스냅샷이며, 현재 작업 트리와 설치본은
> [`a2-progress.md`](a2-progress.md)를 따른다.

검증일: 2026-09-03
작업 디렉터리: `/Users/sungha/Desktop/EON LLM wiki/EON Audio Plugin/Wavetable`

## 결론

A0+A1 구현과 자동화 기반은 GREEN이다. 현재 acceptance는 `BLOCKED`다. 등록된 Golden과 실청취가 남아 있으므로 Foundation을 최종 승격하지 않는다.

## 구현 증거

| 항목 | 결과 | 증거 |
| --- | --- | --- |
| Debug 전체 빌드 | PASS | `cmake --build Build --target all --config Debug -j2` |
| CTest 전체 | PASS, 7/7 | `ctest --test-dir Build -C Debug --output-on-failure` (2026-09-03 재실행) |
| 상태 마이그레이션 | PASS | `ProcessorSmoke`: 체크인 v1 fixture → v2 저장 → 동일 seed sample-identical |
| full 매트릭스 전개 | PASS, 2,592개 | [`Build/quality-a1-20260902/report.json`](../../Build/quality-a1-20260902/report.json); migration 보강 후 smoke 재실행은 [`Build/quality-a1-postmigration/report.json`](../../Build/quality-a1-postmigration/report.json) |
| 유한성 | PASS, 0 failures | 2,592개 보고서 전수 스캔에서 `metrics.finite == false` 0건. 집계 필드 `finiteFailureCount`는 전수 실행 이후에 추가되었으므로 해당 `report.json`에는 없고, 재실행본 [`Build/quality-a1-postmigration/report.json`](../../Build/quality-a1-postmigration/report.json)에서 `finiteFailureCount = 0`으로 확인했다. |
| Golden 비교 | BLOCKED, 2,592 missing | [`Build/quality-a1-20260902/summary.md`](../../Build/quality-a1-20260902/summary.md) |

전체 매트릭스는 다음 조합을 실제 렌더링했다.

- 샘플레이트: 44.1/48/88.2/96/176.4/192 kHz
- 블록 크기: 16/32/64/128/256/512/1024/2048
- 출력: mono/stereo
- MIDI 경계: 0, block-1, block/3 위치를 조합별로 순환
- 총 샘플 수가 블록 크기로 나누어지는 경우 1 sample을 추가해 짧은 마지막 블록을 강제

## 대표 메트릭: 변경 전/후

동일한 48 kHz·128-sample·stereo 조건의 `single-note-60-clean`을 비교했다. A1 수정으로 음량·필터·unison 경로가 달라졌으므로 이 수치는 Golden 승인을 뜻하지 않고, 후속 청취 검토를 위한 before/after 기록이다.

| metric | baseline | A0+A1 candidate |
| --- | ---: | ---: |
| sample peak | -20.466 dBFS | -14.300 dBFS |
| true peak | -20.466 dBTP | -14.300 dBTP |
| RMS | -33.067 dBFS | -26.828 dBFS |
| DC | -130.617 dBFS | -109.869 dBFS |
| integrated loudness | -32.352 LUFS | -26.235 LUFS |

원본 기준선은 [`Build/quality-baseline/single-note-60-clean.json`](../../Build/quality-baseline/single-note-60-clean.json), 후보는 [`Build/quality-a1-20260902/single-note-60-clean__sr48000_bs128_ch2_b2.json`](../../Build/quality-a1-20260902/single-note-60-clean__sr48000_bs128_ch2_b2.json)이다. `silence` 보고서는 `-inf`를 문자열과 `metricState: silence`로 기록하며 JSON이 유효함을 확인했다.

## 별도 증거 등급

2026-09-15 현재 Release 재검증이 아래의 2026-09-03 역사 기록을 대체한다.
Release 유니버설 빌드와 CTest 9/9, VST3 pluginval strictness 10, AU
`auval -v aumu Hwbl Eona`가 모두 PASS다. 기존 auval RED는 subtype과
manufacturer 인자를 뒤집은 `aumu Eona Hwbl` 명령 오류였다. 현재 설치
Release 해시는 VST3
`e6a11a2f2081be36d65932070b138752139ee1586b1378599f1e1011ffa09c5b`,
AU `8e1b6c3662c3c471fc0c7e790ba14aecd111d4cca8ca040716ccd193e2c64a36`이다.
REAPER AU 렌더는 48 kHz/24-bit/stereo, non-finite 0이며 현재 Release
오프라인 fixture와 정렬 후 RMS/최대 오차가 모두 `-138.47 dBFS`다.
Ableton Live 12는 VST3 검색과 빈 MIDI 트랙 삽입/device active까지 PASS다.
Ableton 내부 UI 타입 전환과 세션 복원은 아직 미확인이다.

- 빌드: Debug 플러그인·VST3·AU·Standalone 타깃이 컴파일됐다.
- CTest: 7개 자동 테스트가 통과했다.
- 실행 바이너리 동일성: 전수 실행본의 `executableSha256`는 `fc920175…`이고, 상태 마이그레이션 보강 이후 현재 러너는 `a787f275…`다. 따라서 2,592건 결과는 마이그레이션 수정 이전 바이너리의 기록이며, Golden 승격 시점에는 현재 바이너리로 전수 재실행해야 한다.
- pluginval: PASS. pluginval 1.0.4 strictness 10, 설치 VST3 대상, exit=0/SUCCESS.
  UI 콤보박스 수정 후 현재 설치 바이너리 SHA-256은
  `1c2121e56d5703ca140248c603d8a9b146730449f4451b29edc3249615ac690c`이며,
  현재 로그는 `Build/quality-p3/pluginval_ui_fix_strictness10.log`다.
- auval 역사 기록(2026-09-03, 잘못된 식별자 순서): Debug AU에서
  `aumu/Eona/Hwbl` 조회가 실패했다. 현재 등급은 위의 2026-09-15
  `aumu/Hwbl/Eona` Release PASS를 사용한다.
  - 2026-09-03 재시도: `AU_SANDBOX_SAFE TRUE` 실험으로 Info.plist를
    `sandboxSafe=1`로 바꿔도 동일 실패했다. SEOUL DSP는 GUI 파일 불러오기와
    프리셋 저장을 지원하므로 최종 소스는 명시적으로 `AU_SANDBOX_SAFE FALSE`
    (JUCE 기본 `resourceUsage`)로 유지했다. `aumu`/`Hwbl`/`Eona`, factory
    `SEOUL_DSPAUFactory`, 코드 서명은 정상이며 REAPER는 같은 AU를 로드한다.
    원인은 아직 확정하지 못했으므로 auval 등급은 RED로 유지한다.
    상세 기록: [`docs/quality/p3-pluginval-auval-notes.md`](p3-pluginval-auval-notes.md).
- 호스트 삽입: REAPER 7.79에서 실행했다. 사용자 제작 EONQC ReaScript 프로브
  (`find-sound-quality-check-tool-2/reascripts/eonqc_probe_seoul.lua`)가 트랙을
  만들고 `SEOUL DSP`를 삽입했다. 결과:
  - 호스트가 `AUi: SEOUL DSP (EON Audio)` 인스트루먼트로 로드, enabled=1.
  - 파라미터 58개 = 프로덕트 55개(소스 레이아웃과 이름·정규화 기본값 전부 일치) +
    호스트 래퍼 3개(Bypass/Wet/Delta).
  - UI 콤보박스 수정 후 설치 AU SHA-256은 `9c6fb212a213e705f66caac48f66aba8e5f8e73bc75f7eed948b943cced89094`다.
    아래 REAPER 렌더 증거는 수정 전 AU(`7113dc…`) 기준이므로, 현재 AU에 대한 host-render 등급은 재실행 전까지 BLOCKED로 둔다.
  - 원본 증거: `find-sound-quality-check-tool-2/artifacts/seoul-dsp-probe.txt`
    (2026-09-03 생성).
  - 같은 툴의 SEOUL 렌더 드라이버로 C4 MIDI 1초를 48 kHz/2.5초 WAV로 렌더했다.
    출력은 2채널·24-bit·120,000 frames, non-finite=0.
  - 오프라인 러너의 동일 fixture WAV와 비교 시 REAPER가 3 samples 선행했고,
    정렬 후 RMS 오차 `-138.47 dBFS`, 최대 오차 `-138.47 dBFS`(1 LSB)였다.
    메트릭도 peak `-14.6131 dBFS`, RMS `-26.8721 dBFS`로 일치한다.
  - 원본 렌더 증거: `find-sound-quality-check-tool-2/artifacts/seoul-reaper-render.txt`,
    `find-sound-quality-check-tool-2/artifacts/seoul-reaper-smoke.wav`.
    비교 요약은 [`Build/quality-p3/reaper_seoul_compare.json`](../../Build/quality-p3/reaper_seoul_compare.json)에 보관했다.
  - 2026-09-15 현재 Release AU로 동일 프로브와 렌더를 재실행했다.
    프로젝트 sample rate를 플러그인 생성 전에 48 kHz로 고정하도록 ReaScript를
    수정했으며, 정렬 후 RMS/최대 오차는 모두 `-138.47 dBFS`다.
  - 이 증거는 auval·UI·실청취와 별개 등급이다. 현재 Release의
    MIDI·오디오 출력 경로만 GREEN이며 음악적 품질이나 Golden 승격을 뜻하지 않는다.
- UI 렌더링/스크린샷: PASS (headless component snapshot). 실제 `HybridWavetableAudioProcessorEditor`를
  `SeoulDSP_UISnapshot`으로 1040x760(최소), 1120x760(기본), 1800x1100(최대)에서 렌더했다.
  세 이미지 모두 `SEOUL DSP` 제목, cursive `aoi yume`, wavetable editor, oscillator/filter
  hierarchy, envelope controls, factory preset/MIDI learn/save-load controls가 보이며 essential
  label·knob·menu clipping/overlap이 없다. 로터리 값은 rest snapshot에서 숨겨지고, 활성 드래그
  표시 규칙은 `CyberpunkLookAndFeel::drawRotarySlider()` 소스에서 확인된다.
  - 실행: `cmake --build Build --target SeoulDSP_UISnapshot --config Debug -j2`
  - 렌더: `Build/SeoulDSP_UISnapshot "$PWD/Build/quality-ui/seoul-dsp-default.png" 1120 760`
    (동일하게 `seoul-dsp-min.png` 1040x760, `seoul-dsp-max.png` 1800x1100)
  - PNG SHA-256: default `cef4377a…`, min `57b78f85…`, max `138a518b…`.
  - 원본: [`Build/quality-ui/seoul-dsp-default.png`](../../Build/quality-ui/seoul-dsp-default.png),
    [`Build/quality-ui/seoul-dsp-min.png`](../../Build/quality-ui/seoul-dsp-min.png),
    [`Build/quality-ui/seoul-dsp-max.png`](../../Build/quality-ui/seoul-dsp-max.png).
  이 등급은 실제 editor component 시각 증거이며, 화면 세션에서의 마우스·키보드 상호작용이나
  실청취를 대신하지 않는다.
- 실청취: 미실행. 현재 Debug 러너(`executableSha256=e744cf62027b21b37f63b5610cc665763a19fb79ebe8d6ee77a0dccff246f15a`)로
  청취용 WAV를 준비했다. 대표 파일은 `Build/quality-listening-current/`의
  `single-note-60-clean__sr48000_bs128_ch2.wav`, `rich-saw-chromatic__sr48000_bs128_ch2.wav`,
  `transient-100ms__sr48000_bs128_ch2.wav`, `delay-impulse__sr48000_bs128_ch2.wav`,
  `combined-effects__sr48000_bs128_ch2.wav`다. 이 파일들을 사람이 직접 검토한 뒤에만 Golden을 승격한다.

## 승격 조건과 다음 단계

1. `Build/quality-baseline`과 `Build/quality-a1-20260902`의 대표 excerpt를 동일 음량으로 비교하고 청취 결정을 기록한다.
2. 승인된 WAV만 `Tests/AudioQuality/golden/`으로 복사하고 SHA-256·메타데이터를 `golden/manifest.json`에 등록한다.
3. full runner를 재실행해 `goldenPresent`와 모든 비교 threshold를 확인한다.
4. 그 결과가 PASS일 때에만 이 문서의 상태를 `ACCEPTED`로 바꾸고 Plan A2를 시작한다.
