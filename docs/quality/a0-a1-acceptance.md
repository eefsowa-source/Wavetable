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
| full 매트릭스 전개 | PASS, 2,592개 | [`Build/quality-a1-20260902/report.json`](../../Build/quality-a1-20260902/report.json) |
| 유한성 | PASS, 0 failures | `report.json: finiteFailureCount = 0` |
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
- pluginval: 로컬 실행 파일이 없어 미실행.
- auval: 설치된 Component를 찾지 못해 실패했다(`aumu/Eona/Hwbl`, `didn't find the component`). 빌드 산출물은 시스템 Component 경로에 설치하지 않았다.
- 호스트 삽입: 미실행.
- UI 렌더링/스크린샷: 미실행.
- 실청취: 미실행. 특히 다음 다섯 critical excerpt를 사람 검토 후에만 Golden을 승격한다: clean note, rich saw, transient, delay impulse, combined effects.

## 승격 조건과 다음 단계

1. `Build/quality-baseline`과 `Build/quality-a1-20260902`의 대표 excerpt를 동일 음량으로 비교하고 청취 결정을 기록한다.
2. 승인된 WAV만 `Tests/AudioQuality/golden/`으로 복사하고 SHA-256·메타데이터를 `golden/manifest.json`에 등록한다.
3. full runner를 재실행해 `goldenPresent`와 모든 비교 threshold를 확인한다.
4. 그 결과가 PASS일 때에만 이 문서의 상태를 `ACCEPTED`로 바꾸고 Plan A2를 시작한다.
