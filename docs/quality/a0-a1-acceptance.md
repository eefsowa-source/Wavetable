# SEOUL DSP Plan A0+A1 acceptance

검증일: 2026-09-02  
작업 디렉터리: `/Users/sungha/Desktop/EON LLM wiki/EON Audio Plugin/Wavetable`

## 결론

A0+A1 구현과 자동화 기반은 GREEN이다. 다만 현재 acceptance는 `BLOCKED`다. 등록된 Golden이 없고, 실청취·호스트 삽입·UI 캡처·pluginval 증거를 아직 확보하지 않았으므로 Foundation을 최종 승격하지 않는다.

## 구현 증거

| 항목 | 결과 | 증거 |
| --- | --- | --- |
| Debug 전체 빌드 | PASS | `cmake --build Build -j 4` |
| CTest 전체 | PASS, 7/7 | `ctest --test-dir Build -C Debug --output-on-failure` |
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

- 빌드: Debug 플러그인·VST3·AU·Standalone 타깃이 컴파일됐다.
- CTest: 7개 자동 테스트가 통과했다.
- 실행 바이너리 동일성: 전수 실행본의 `executableSha256`는 `fc920175…`이고, 상태 마이그레이션 보강 이후 현재 러너는 `a787f275…`다. 따라서 2,592건 결과는 마이그레이션 수정 이전 바이너리의 기록이며, Golden 승격 시점에는 현재 바이너리로 전수 재실행해야 한다.
- pluginval: 로컬 실행 파일이 없어 미실행.
- auval: Debug AU를 `~/Library/Audio/Plug-Ins/Components/SEOUL DSP.component`에 설치한 뒤에도 `aumu/Eona/Hwbl` 조회가 실패했다(`Cannot get Component's Name strings`, `didn't find the component`). 캐시/호스트 재검색 후 별도 재검증이 필요하다.
  - 2026-09-03 재시도: AudioComponentRegistrar 재시작 후에도 동일 실패.
    Info.plist 계약(`aumu`/`Hwbl`/`Eona`, factory `SEOUL_DSPAUFactory`)과
    코드 서명은 정상이고 같은 바이너리를 REAPER가 실제 로드하므로, 이 실패는
    auval 자체의 컴포넌트 등록 조회 문제로 분류한다. auval 등급은 여전히
    RED로 유지한다.
- 호스트 삽입: REAPER 7.79에서 실행했다. 사용자 제작 EONQC ReaScript 프로브
  (`find-sound-quality-check-tool-2/reascripts/eonqc_probe_seoul.lua`)가 트랙을
  만들고 `SEOUL DSP`를 삽입했다. 결과:
  - 호스트가 `AUi: SEOUL DSP (EON Audio)` 인스트루먼트로 로드, enabled=1.
  - 파라미터 58개 = 프로덕트 55개(소스 레이아웃과 이름·정규화 기본값 전부 일치) +
    호스트 래퍼 3개(Bypass/Wet/Delta).
  - 설치된 AU 바이너리 SHA-256 `198637b8…`는 이 워크스페이스 Debug AU 산출물과
    동일하다. 즉, auval CLI 실패와 무관하게 실제 호스트는 같은 바이너리를
    정상 스캔·삽입했다.
  - 원본 증거: `find-sound-quality-check-tool-2/artifacts/seoul-dsp-probe.txt`
    (2026-09-03 생성).
  - 이 증거는 auval·UI·실청취와 별개 등급이며, MIDI 발음·오디오 출력은 여전히
    미검증이다.
- UI 렌더링/스크린샷: 미실행.
- 실청취: 미실행. 특히 다음 다섯 critical excerpt를 사람 검토 후에만 Golden을 승격한다: clean note, rich saw, transient, delay impulse, combined effects.

## 승격 조건과 다음 단계

1. `Build/quality-baseline`과 `Build/quality-a1-20260902`의 대표 excerpt를 동일 음량으로 비교하고 청취 결정을 기록한다.
2. 승인된 WAV만 `Tests/AudioQuality/golden/`으로 복사하고 SHA-256·메타데이터를 `golden/manifest.json`에 등록한다.
3. full runner를 재실행해 `goldenPresent`와 모든 비교 threshold를 확인한다.
4. 그 결과가 PASS일 때에만 이 문서의 상태를 `ACCEPTED`로 바꾸고 Plan A2를 시작한다.
