# SEOUL DSP Plan C — 음질·UI/UX 업그레이드 계획

> **For agentic workers:** 각 태스크는 RED -> GREEN 순서로 진행하고, 태스크 단위로 커밋한다.

**작성일:** 2026-09-29
**상태:** IN PROGRESS (SQ-1, SQ-2, SQ-3 구현 + 자동 게이트 완료, 2026-09-29)
**전제:** Plan B 계보(음질 기반 + eon_dsp 통합) 완료 상태에서 시작. `main`은 origin 대비 ahead 25, 미커밋 변경으로 Debug용 melatonin_inspector 통합이 추가됨(SOURCE_DIR 수정 포함).

## 1. 목표

음질은 "이미 측정된 깨끗함"을 넘어 **남은 구조적 결함(출력 클리핑)과 미검증 주장(High 등급 가치)을 닫는다**. UI/UX는 90타입 프레젠테이션을 보유한 상태에서 **일상 사용 흐름(선택·편집·피드백·프리셋)의 마찰**을 줄인다. 톤 캐릭터 재설계나 신규 신스 기능(모듈 매트릭스, 추가 이펙트)은 범위 밖으로 두고, 필요 시 별도 계획으로 분리한다.

## 2. 기준선 (기존 증거에서 확인된 사실)

* 비선형 alias: folded 프록시 측정 바닥(-156.5 dBc) 도달. Eco/Normal/High 3등급 존재 (b2-alias-baseline.md).
* High 등급 비용: dense16 +24% CPU인데 10 kHz 접힘 proxy 개선 없음. 가치 미증명 (cpu-optimization.md).
* 출력 클리핑: dense_chord_16 peak **+4.40 dBFS** (0 dBFS 초과). 오실레이터 합산 후 정규화/리미터 없음, Safety gain -6 dB만 존재 (cpu-optimization.md).
* 파라미터 스무딩: cutoff/resonance/osc level/saturation/drive/wavetable pos/env amount는 50 ms 스무더. **filterType/slope/유니슨·엔벌로프 파라미터는 비스무딩** — 전환 클릭 가능성 미측정 (SynthVoice.cpp:38-59).
* 임포트: 임의 소스를 2048 포지션으로 리샘플. band-limit/DC/정규화 정책 문서 미비 (README, WavetableImporter.cpp).
* 호스트 게이트: pluginval VST3 s10 PASS, auval PASS, REAPER AU 로드 PASS. **미완료: AU pluginval(120 s stall), REAPER VST3, clean Ableton 삽입, DAW 오토메이션/상태복원, DAW 드롭아웃, 확장 시나리오 Golden 승격** (b7-host-validation.md).
* 청취: 3-way(Serum/Vital/SEOUL) 레벨매치 블라인드 팩 존재, 단일 clean 원노트. 확장 매트릭스 미측정 (b8-blind-listening.md).
* UI: 90 타입(A30 스킨 / B30 레이아웃 / C30 큐레이션) 단일 콤보, 1120×760 고정, 툴팁 존재. 스냅샷 90종 고유 md5 검증됨 (ui-90-types.md).
* UX 갭: 타입 선택기 평면 90항목 / 출력미터·피크표시 없음 / 프리셋 prev-next·검색 없음 / 웨이브테이블 편집 undo 없음 / 리사이즈 없음 / 명암비 감사 없음 (PluginEditor.cpp 육안 감사).
* 계측 자산: OfflineRenderer, QualityOrder(프록시), CpuBench, UiSnapshot, eonqc 헤드리스, GoldenComparator(미승격) (Tests/, Tools/).

## 3. 유지 규칙 (Plan B에서 계승)

* 파라미터 ID·state 마이그레이션·프리셋 스키마를 깨지 않는다. 새 파라미터는 추가만.
* 오디오 콜백에서 allocation/lock/file I/O/GUI 호출 금지.
* CTest / pluginval / auval / DAW 삽입 / 스냅샷 / 실청취는 별도 증거 등급. 섞지 않는다.
* Golden 승격은 현재 바이너리가 정상이라는 독립 증거 후에만.
* extra CPU alone is not a quality result. CPU 회귀는 항상 측정·기록한다.
* UI 변경은 UiSnapshot으로 결정적 PNG를 남기고, 청감 주장은 level-matched 청취로만.

## 4. 음질 태스크 (SQ)

### SQ-1 — 출력 헤드룸/클리핑 구조적 해결

**근거:** dense_chord_16 +4.40 dBFS는 알려진 초과. 유니슨×3오실레이터 합산이 0 dBFS를 넘는 것은 패치 문제가 아니라 게인 스테이징 구조 문제.

* [x] RED: ProcessorQualityTests에 `renderWorstCaseChord()`(16음, output 기본 -6 dB) 추가. 수정 전 peak **+13.01 dBFS**로 실패 고정.
* [x] GREEN: 정적 소프트 실링 채택(OutputSafety). threshold 0.9 이하 투명, 초과는 tanh knee로 0.999 수렴. 보이스 수 정규화 후보는 기존 패치 레벨/음색을 더 크게 바꿔 기각.
* [x] 게이트(자동): ctest 12/12 + worst-case peak -0.009 dBFS + 기존 alias/DC/유한성 유지 + dense16 CPU 회귀 +0.95 %.
* [ ] 게이트(청취): 큰 코드/유니슨 시나리오 level-matched 청취. SQ-6과 함께 수행.

**Files:** Source/DSP/OutputSafety.{h,cpp}, Tests/AudioQuality/, Docs/quality/c1-output-headroom.md

### SQ-2 — High 등급 가치 판정 (유지/개선/제거)

**근거:** High(4x+ADAA1)는 dense16에서 Normal 대비 +24% CPU이나 접힘 proxy 개선 0. 측정이 48 kHz·10 kHz 프로브·단일 지점이라 미측정 영역이 남아 있다.

* [x] 측정 확장: sr/6 위 프로브로 접힘 3차 고조파를 만들어 48/96 kHz 스윕. 48 kHz 5개 케이스에서 High가 최고 +0.33 dB(대부분 더 나쁨). 96 kHz는 cutoff 상한 20 kHz 때문에 밴드 격리 불가로 제외.
* [x] 게이트: High 제거 결정. 파라미터 세 번째 선택지는 유지하고 내부 Normal 매핑, 라벨 `High (deprecated)`. dense16 CPU 회수 -17.5 %(1694 -> 1397 ms), 렌더는 Normal과 비트 동일.
* [ ] 후속: cutoff 상한(20 kHz)을 sr 비례로 올려 96 kHz에서 필터를 완전히 열 수 있게 한다. 8.6~9.2 kHz 접힘 구간(-67 dBc)은 별도 DSP 항목.

**Files:** Tests/AudioQuality/QualityOrderTests.cpp, Source/PluginProcessor.cpp, Docs/quality/c2-high-tier-verdict.md

### SQ-3 — 비스무딩 파라미터 전환 클릭 감사

**근거:** filterType/slope, 유니슨 카운트/디튠, ADSR 타겟 등은 스무더 밖. 호스트 오토메이션/프리셋 스냅 시 스텝 불연속이 클릭으로 나타날 수 있다.

* [x] RED: `renderHeldNote()` + per-block `onBlock` 훅으로 스텝 감사. slope 10.5배, type 9.0배 클릭 확인 (unison 2.3배는 통과).
* [x] GREEN: SynthVoice에 5 ms 구조 전환 크로스페이드(이전 SlopeFilter 인스턴스 유지 + 블렌드). slope/type 모두 기준선과 동일 수준(1.0배)으로 개선.
* [x] 게이트(자동): 스텝 감사 통과 + Release/Debug 12/12 + CPU 변동 범위.
* [ ] 게이트(청취): 실제 클릭 유무를 SQ-6 청취에서 확인. unison 전환(2.3배) 잔여 항목은 별도.

**Files:** Source/DSP/SynthVoice.cpp, Tests/AudioQuality/, Docs/quality/c3-zipper-audit.md

### SQ-4 — 임포트 테이블 품질 정책

* [ ] 임포트 경로에 DC 제거·피크 정규화·band-limit(mip 빌드와 동일 정책) 여부를 코드 감사 → 결함 있으면 RED 테스트 후 수정.
* [ ] 게이트: 임포트 fixture(고고조파 소스)로 alias 프록시가 네이티브 테이블 대비 명시 임계 이내.

### SQ-5 — 미완료 호스트 게이트 완결

* [ ] AU pluginval stall 원인 분석(120 s에서 멈춤 — 어느 단계인지 로그 확보) → 재시도 또는 한계 문서화.
* [ ] REAPER VST3 로드/렌더, clean Ableton 삽입(빈 세트), DAW 오토메이션·세션 저장/복원, DAW 드롭아웃 관측.
* [ ] 확장 시나리오 Golden 승격: 현재 바이너리 정상 증거(위 게이트) 확인 후 승격.

### SQ-6 — 블라인드 청취 매트릭스 확장

* [ ] 기존 3-way 팩 포맷을 재사용해 필터 스윕/유니슨/트랜지언트/고음 시나리오 추가. 각 시나리오 LUFS 매칭 + SHA 매니페스트 + mapping 분리.
* [ ] 청취 결과가 SQ-1~SQ-3의 유지 결정을 뒷받침해야 한다(메트릭 통과 + 청취 악화 시 롤백).

## 5. UI/UX 태스크 (UX)

### UX-1 — 타입 선택기 재구성

**근거:** 90개 평면 콤보는 탐색 비용이 크다. 데이터는 이미 뱅크 구조.

* [ ] 뱅크 탭 또는 2단 콤보(A/B/C → 항목)로 재구성. 단일 파라미터 `uiType` 유지.
* [ ] 게이트: UiSnapshot 90종 md5 회귀 없음 + 선택 텍스트 동기화 단위테스트 갱신.

### UX-2 — 출력 피드백 (레벨 미터 + 피크 인디케이터)

* [ ] RT 안전(atomic/lock-free) 출력 미터, 클리핑 표시등. SQ-1과 연동해 clip 상태를 가시화.
* [ ] 게이트: 스냅샷 결정성 유지 + 오디오 스레드 무할당 검증(기존 규칙).

### UX-3 — 편집 마찰 제거

* [ ] 노브 더블클릭 기본값 리셋, Shift/Alt 미세조정 배율, 드래그 중이 아닐 때도 값 표시 토글.
* [ ] 웨이브테이블 그리기 undo/redo (편집 버퍼 스냅샷 스택, 최소 16단계).
* [ ] 게이트: 단위테스트(undo 스택) + 스냅샷 회귀.

### UX-4 — 프리셋 UX

* [ ] 팩토리 11종 + 사용자 프리셋 prev/next 버튼, 현재 프리셋명 표시.
* [ ] A/B compare (파라미터 스냅샷 토글). 프리셋 스키마 변경 없이 상태 메모리에만 존재.

### UX-5 — 사이징/밀도

* [ ] 고정 1120×760 → 최소/최대 리사이즈 + 정수 스케일(0.75/1/1.25/1.5) 옵션. 레이아웃 골격 6종이 이미 밀도 대응하므로 스케일만 추가.
* [ ] 게이트: 대표 타입 6종 스냅샷(각 골격 1종) + 리사이즈 경계 테스트.

### UX-6 — 접근성·가독성 감사

* [ ] 30개 스킨의 텍스트/배경 명암비를 UiSnapshot PNG에서 자동 측정(라벨 vs 패널 샘플링) → WCAG AA 4.5:1 미만 스킨 목록화 → 팔레트 수정.
* [ ] 키보드 내비게이션 순서 검증(EDITOR_WANTS_KEYBOARD_FOCUS와 상호작용 — Inspector Cmd+I와 공존 확인).

### UX-7 — 디버그/측정 UX (이미 착수)

* [x] melatonin_inspector Debug 통합 (SOURCE_DIR 수정 포함, 미커밋). 커밋 포함 여부 결정.

## 6. 실행 순서

1. **SQ-1** (구조적 결함, 가장 높은 청감 영향) → **SQ-3** (저비용 클릭 감사) → **SQ-2** (High 판정) → SQ-4.
2. UX는 SQ-1과 병렬 가능: **UX-2**(SQ-1과 연동) → **UX-1** → **UX-3** → UX-4 → UX-5 → UX-6.
3. SQ-5 호스트 게이트와 SQ-6 청취 확장은 DSP 변경이 안정된 뒤 한 번에 수행(바이너리당 1회 비용).

각 태스크 산출물은 `Docs/quality/c*.md`에 머신/바이너리 SHA256/샘플레이트/버퍼와 함께 기록한다.

## 7. 완료 정의

* worst-case 패치에서도 출력 ≤ 0 dBFS이고 클리핑이 UI에 가시화된다.
* High 등급의 존재 근거가 측정으로 확정되거나 제거 결정이 기록된다.
* 비스무딩 전환 클릭이 측정되고 확인된 것만 제거됐다.
* 타입 선택·프리셋 탐색·웨이브테이블 편집이 1~2 스텝으로 수렴한다.
* 미완료 호스트 게이트(AU pluginval, REAPER VST3, clean Ableton, 오토메이션/복원, 드롭아웃)가 각각 PASS 또는 명시적 한계로 닫힌다.
* 확장 청취 매트릭스 결과가 문서화되고, 최종 유지 결정과 연결된다.
