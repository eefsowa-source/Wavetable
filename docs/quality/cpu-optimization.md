# SEOUL DSP CPU optimization evidence

목적: 16화음·유니슨 8/8/8 워스트 케이스에서의 오디오 스레드 과부하를 측정 가능한 단계로 줄인다. 첫 단계는 측정(Phase 0)과 음질 무손실 최적화(Phase 1) + 고정비 절감(Phase 2 일부)이다.

## 측정 도구

- `Tools/CpuBench.cpp`: 실제 `processBlock`을 48 kHz / 64-sample / 스테레오로 2초 렌더. 워밍업 1회 + 측정 3회, 중앙값 보고.
  - 시나리오: `solo-unison8` (보이스 1), `dense16-unison8` (보이스 16, saturation/drive/LFO/drift 최대)
- 실행: `cmake --build Build-Release --target CpuBench -j 8 && Build-Release/CpuBench [quality] [slope]`
  (quality `0=Eco`, `1=Normal`, `2=High`, 생략 시 Normal; slope `0..3` = 6/12/18/24 dB/oct, 생략 시 3)

## 결과 (Apple Silicon 로컬, Release, 동일 조건 3회 중앙값)

| 단계 | solo median | dense16 median | dense16 realtime factor | 비고 |
| --- | --- | --- | --- | --- |
| baseline | 81.5 ms | 1293.0 ms | 1.55x | [baseline.json](../../Build/quality-cpu/baseline.json) |
| Phase 1 (레이아웃/MIP 캐시, 호이스트) | 64.9 ms (-20%) | 981.0 ms (-24%) | 2.04x | [phase1.json](../../Build/quality-cpu/phase1.json) |
| Phase 2 (새추레이션 0 드라이브 스킵) | 62.9 ms (-23%) | 991.3 ms (-23%) | 2.02x | [phase2-satbypass.json](../../Build/quality-cpu/phase2-satbypass.json) |
| Plan B Task 3 (ADAA + eon half-band 2x) | 84.8 ms (+35% vs Phase 2) | 1328.5 ms (+34% vs Phase 2) | 1.51x | [b2-alias-baseline.md](b2-alias-baseline.md) 7절 |
| Plan B Task 3b Eco | 81.5 ms | 1268.9 ms | 1.58x | 2x + legacy curve |
| Plan B Task 3b Normal (default) | 85.8 ms | 1310.5 ms | 1.53x | 2x + ADAA1 |
| Plan B Task 3b High | 103.1 ms | 1629.1 ms | 1.23x | 4x + ADAA1 |
| Plan B Task 4 slope 0 (6 dB/oct) | 77.6 ms | 1236.8 ms | 1.62x | eon TPT 1-pole |
| Plan B Task 4 slope 1 (12 dB/oct) | 78.5 ms | 1214.5 ms | 1.65x | eon SvfTPT 1단 |
| Plan B Task 4 slope 3 (24 dB/oct, 기본) | 79.2 ms | 1231.2 ms | 1.62x | SvfTPT 2단 |
| Plan B Task 4 + drive 클리퍼 (drive +12 dB) slope 1 | 90.9 ms | 1441.6 ms | 1.39x | ADAA1 클리퍼 활성 |
| Plan B Task 4 + drive 클리퍼 (drive +12 dB) slope 3 | 92.5 ms | 1477.3 ms | 1.35x | ADAA1 클리퍼 활성 |

- dense 시나리오는 saturation 최대이므로 새추레이션 스킵의 이득이 없고 노이즈 범위 내다. 스킵 이득은 드라이브 0 패치에서 발생한다.
- Plan B Task 3은 Phase 1/2에서 얻은 절감분을 음질로 되돌려 썼다. dense16은 원래 baseline(1293.0 ms)보다 2.7% 느린 수준이고, 대신 folded alias가 -108.9 dBc에서 측정 바닥(-156.5 dBc)까지 내려갔다. 이 비용은 새추레이션 스테이지의 ADAA(샘플당 지수/로그) + half-band FIR에서 나온다.
- Task 3b 세 등급은 같은 빌드에서 `CpuBench 0/1/2`로 연속 측정했다. dense16 기준 Eco는 Normal보다 약 3% 빠르고 High는 약 24% 느리다. 세 등급 모두 10 kHz 접힘 proxy의 -150 dBc 상한을 통과하므로 High의 추가 CPU를 alias 수치 개선으로 해석하지 않는다.
- Task 4에서 필터를 JUCE SVF 2개에서 `SlopeFilter`(eon TPT, 샘플당 `tan` 1회 공유)로 교체한 뒤 슬로프별로 측정했다. 세 슬로프 모두 Task 3b Normal 기록(85.8 / 1310.5 ms)보다 낮다 — JUCE SVF가 두 인스턴스에서 각자 `tan`을 계산하던 비용이 사라진 결과다. 별도 실행 회차이므로 수 ms 차이는 실행 변동으로 취급한다(상세: [b4-filter-slopes.md](b4-filter-slopes.md)).
- Task 4의 드라이브 클리퍼는 `drive > 0 dB`일 때만 켜지고, 켜지면 샘플당 채널당 `exp`/`log1p` 1회씩(ADAA1)을 더한다. `CpuBench` 워스트 케이스 패치는 `filterDrive = +12 dB`이므로 위 마지막 두 행이 그 비용이다: 클리퍼 비활성 대비 solo 약 +15 %, dense16 약 +19 %. **0 dB 이하 드라이브 패치는 이 비용을 전혀 내지 않는다.**

## 변경 내용

1. [UnisonBank.cpp](../../Source/DSP/UnisonBank.cpp): 레인 레이아웃(cents/pan/gain/frequencyRatio/leftGain/rightGain)을 파라미터 변경 시에만 재계산. 매 샘플 `makeUnisonLayout`, `exp2`, `sqrt` 제거. 산술식은 기존과 동일.
2. [WavetableOscillator.cpp](../../Source/DSP/WavetableOscillator.cpp): `selectMipLevels`을 increment 변경 시에만 재계산(캐시).
3. [SynthVoice.cpp](../../Source/DSP/SynthVoice.cpp): 블록 불변값(driftIncrement, spread, detune×keyTrack)을 샘플 루프 밖으로 호이스트. saturation ≤ 0.005 블록은 오버샘플+`tanh` 스킵(오버샘플러 상태/지연은 유지).

## 검증

- Release ctest 9/9 통과 (AudioQualityMetrics, ProcessorQuality, OfflineRenderer, GoldenComparator, UnisonBank, WavetableDSP, WavetableImporter, OutputSafety, ProcessorSmoke).
- saturation 바이패스 경로 보강 테스트 추가: drive 0 vs 0.9 렌더가 달라짐을 확인 (`saturation drive changes the rendered signal`).
- 산술 동등성: Phase 1은 소스 수준에서 캐시/호이스트만 포함(floating point 재배열 없음). 다만 바이너리 레벨에서는 아래 A/B 결과처럼 미세한 출력 차이가 존재한다.

## 호스트 검증 게이트 (2026-09-15, CPU 최적화 Release 빌드)

- pluginval VST3 strictness 10: **SUCCESS**, assertion 0. 로그: [pluginval-cpuopt-release-strictness10.log](../../Build/quality-cpu/host-validation/pluginval-cpuopt-release-strictness10.log)
- auval AU (aumu Hwbl Eona): **PASS**, exit 0. 로그: [auval-cpuopt-release.log](../../Build/quality-cpu/host-validation/auval-cpuopt-release.log)
- eonqc 헤드리스 호스트 스모크: 6시나리오 모두 non_finite=0. 리포트: [eonqc-report-rel.json](../../Build/quality-cpu/host-validation/eonqc-report-rel.json)
- REAPER 7.79 ReaScript 자동 렌더: status=pass, 파라미터 58개 확인, 동일 바이너리 2회 렌더 비트 동일. 로그: [reaper-render-log-current.txt](../../Build/quality-cpu/host-validation/reaper-render-log-current.txt)

## A/B 비교 (구형 빌드 2026-09-06 fixture 대비)

같은 fixture 재렌더 결과 66개 중 silence/chord-32/stress-128 6개는 비트 동일, 나머지 48개는 RMS 정렬 오차 약 -48 ~ -55 dBFS로 상이. 호스트 경로(REAPER 렌더)에서도 동일 크기로 확인:

| 비교 | latency | aligned RMS error | aligned max error |
| --- | --- | --- | --- |
| 신형 REAPER 렌더 ↔ 신형 오프라인 fixture | -3 samples | -68.75 dBFS | -39.25 dBFS |
| 신형 REAPER 렌더 ↔ 구형(9/6) REAPER 렌더 | -4 samples | -59.00 dBFS | -40.02 dBFS |

해석: 같은 바이너리끼리는 -68.75 dBFS 수준으로 일관되므로 새 코드는 자체 결함이 아니라 "구형 바이너리 대비 미세하게 다른 새 정상 동작"이다. 위상 누적 + Hermite 보간의 인덱스 분기 특성상 컴파일러 최적화 차이(FP 재연관, 인라인 확장 변화)로 미세 비트 차이가 발생하고, 유니슨 파형이 decorrelation되어 RMS 차이로 관측된 것으로 추정한다. 가청 유의미성은 별도 청취 게이트의 영역이다(오프라인 메트릭만으로 판정하지 않는다).

기존 관찰(회귀 아님): dense_chord_16 peak +3.07 dBFS(0 dBFS 초과)는 Debug(최적화 전)에서도 +3.05 dB로 존재하던 동작이다. 별도 과제로 추적.

## 미완료 게이트

- Ableton Live 스모크 미실행 (REAPER는 완료).
- Phase 2 잔여 항목: maxVoices 파라미터, fast-tanh 옵션, LFO 블록레이트 옵션(음질 메트릭 + 청취 필요).
- xctrace 프로파일 캡처는 보류(벤치 중앙값으로 우선 추적).

## 기본 프리셋 변경 (2026-09-17): 단음 기본값 + 5도 스택 보존

기존 기본 상태(Osc 1 근음 + Osc 2 +7반음 + Osc 3 −7반음)가 한 노트에 여러 음으로 들리는 원인이어서 변경:

1. 공장 기본값을 단음으로 변경: osc2Tune/osc3Tune 기본값 +7/−7 → 0 (레이아웃 [PluginProcessor.cpp](../../Source/PluginProcessor.cpp) makeLayout).
2. 기존 5도 스택 상태는 11번째 팩토리 프리셋 "STACK 11 // FIFTHS"로 보존 (첫 10개 랜덤 뱅크는 종전 캐릭터 유지, 인덱스 10만 오버라이드).
3. applyFactoryPreset이 osc1~3 Level/Tune을 명시 세팅하도록 확장 (기존엔 position/filter/env만 세팅).

검증 (새 기본값 빌드, 설치 해시 da579bff… 일치):

- ctest 9/9 통과 (프리셋 11개, 단음 기본 검증 추가).
- pluginval VST3 strictness 10: SUCCESS. 로그: [pluginval-unisontune-default-strictness10.log](../../Build/quality-cpu/host-validation/pluginval-unisontune-default-strictness10.log)
- auval (aumu Hwbl Eona): PASS.
- EONQC 헤드리스 스모크: 5/6 통과, non_finite 전부 0. dense_chord_16 peak +4.40 dBFS(0 dBFS 초과) — 유니슨 피치로 세 오실레이터가 동위상 합산되며 기존 +3.05/+3.07 dB에서 1.3 dB 상승. 이것은 오실레이터 레벨 합산 구조상의 기존 동작 연속이며, 이번 변경으로 생긴 결함이 아니라 "기본 프리셋이 더 두꺼워진" 결과다. 리포트: [eonqc-report-unisontune-default.json](../../Build/quality-cpu/host-validation/eonqc-report-unisontune-default.json)

참고: dense_chord_16의 0 dBFS 초과는 오실레이터 합산 이후 정규화/리미터가 없는 구조적 특성이다(출력 Safety 게인은 −6 dB 기본). OutputSafety 경로와 별도로 추적 중인 기존 관찰이다.
