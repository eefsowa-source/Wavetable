# SEOUL DSP Plan B — eon_dsp 기반 음질 업그레이드 계획

> **For agentic workers:** 각 태스크는 RED -> GREEN 순서로 진행하고, 태스크 단위로 커밋한다.
> 체크박스(`- [ ]`)는 실행 시점에 갱신한다.

**작성일:** 2026-09-22  
**상태:** IN PROGRESS (Task 1 완료, 2026-09-22)  
**대상 DSP 코어:** `eon_dsp` rev `c8e71f3` (canonical: `~/Desktop/EON LLM wiki/EON Audio Plugin/eon_dsp`)  
**대상 제품:** SEOUL DSP (JUCE 8.0.14 wavetable synth, `HybridWavetable`)

---

## 1. 목표

SEOUL DSP의 음질 상한을 올린다. 이미 닫힌 Plan A 계보(A0+A1 기반, A2 signal-quality)
위에서, 최근 업데이트된 1st-party DSP 코어 `eon_dsp`를 도입해 **비선형 스테이지의
aliasing과 필터 드라이브의 진실성**을 개선한다. GUI 재설계, 프리셋 체계 변경, 배포/서명은
범위 밖이다.

핵심 판단: 지금 음질을 실제로 제한하는 것은 웨이브테이블 오실레이터가 아니라
**후단 비선형 처리**다. 오실레이터는 이미 11단 band-limited mip + Hermite 보간 +
0.35 octave smoothstep 크로스페이드를 갖고 있어 alias 예산을 거의 다 썼다. 반면
`saturation`은 per-voice 2x IIR half-band oversampling + 비대칭 tanh이고,
`filterDrive`는 **선형 이득**이다. eon_dsp는 정확히 이 두 지점을 겨냥한 도구를
제공한다(ADAA, Kaiser-FIR polyphase oversampler, ZDF/ladder, Lambert-W solver, triode,
transformer).

## 2. 이번 턴에 실제로 확인한 기준선 (evidence)

| 항목 | 결과 | 출처 |
| --- | --- | --- |
| Wavetable worktree | clean, HEAD `d706c71` | `git status --short` |
| Build-Release CTest | **9/9 PASS** (AudioQualityMetrics, ProcessorQuality, OfflineRenderer, GoldenComparator, UnisonBank, WavetableDSP, WavetableImporter, OutputSafety, ProcessorSmoke) | `ctest --test-dir Build-Release` |
| eon_dsp 사본 | 4곳에 존재하나 `Dsp/` 내용 동일. canonical = `~/Desktop/EON LLM wiki/EON Audio Plugin/eon_dsp` | `diff -rq` |
| eon_dsp rev | `c8e71f3 test: harden eon dsp core contracts` (2026-09-20) | `git log` |
| eon_dsp 외부 의존 | `Dsp/*.h`는 표준 헤더 + 자체 헤더만 사용. `<immintrin.h>`는 x86 가드 내부. `third_party/`(simde, sst-*)는 Dsp에서 참조되지 않음 | `grep -n '#include' Dsp/*.h` |
| eon_dsp 컴파일 프로브 | `clang++ -std=c++20 -O2 -Wall -Wextra` 로 전 헤더 + ADAA2/Ladder4/SvfTPT/DCBlocker/InductorResonator/Rng 인스턴스화 -> `ok`, exit 0, warning 없음 | 이번 턴 실행 |
| eon_dsp 자체 테스트 | fresh Release 빌드에서 `eon_dsp_core` PASS (0.41 s) | `cmake && ctest` |
| eon_dsp 라이선스 | 리포지토리에 LICENSE 파일 없음. 자체 저작(동일 계정 커밋)으로 보이나 배포 전 명시 필요 | `ls -a` |
| 현재 신호 경로 | osc bank 3개(각 8 unison) -> 선형 `filterDrive` 이득 -> SVF TPT 1~2단 -> ADSR VCA -> per-voice 2x IIR oversampling + 비대칭 tanh -> 합산 -> DC blocker -> mono bass -> width -> delay -> reverb | `SynthVoice.cpp`, `PluginProcessor.cpp` |
| Plan A2 잔여 결함 | #6 `filterDrive`가 여전히 선형 이득. #1~#5, #7은 코드에 수정이 반영됨(random phase 정규화, osc2/3 unison 소비, smoother 전진, 대칭 detune, per-lane pan, constant-power delay mix) | `UnisonBank.cpp`, `PluginProcessor.cpp:168` |
| **신규 발견** | `filterSlope`는 4개 선택지인데 DSP는 `slope >= 2`에서만 2단 캐스케이드 -> **slope 0과 1은 bit-identical, slope 2와 3도 동일.** 게다가 제조자 라벨이 불일치: processor `{"12 dB","12 dB","24 dB","24 dB"}` vs editor `{"8 dB","12 dB","18 dB","24 dB"}` | `SynthVoice.cpp:246`, `PluginProcessor.cpp:105`, `PluginEditor.cpp:528` |
| Golden | 미승격 상태(직전 `quality-dsp-upgrade-20260921`은 "golden missing"으로 `passed:false`) | `Build-Release/quality-dsp-upgrade-20260921/report.json` |

## 3. 유지 규칙 (위반 시 태스크 반려)

- 파라미터 ID, state v1 마이그레이션, 프리셋 스키마를 깨지 않는다. 새 파라미터는 추가만 한다.
- 오디오 콜백에서 allocation, lock, file I/O, GUI 호출, 전역 non-RT 난수를 쓰지 않는다.
- Golden을 승격하지 않는다. 회귀 판정은 절대 게이트(측정 임계)로만 한다.
- 증거 등급을 섞지 않는다: CTest / pluginval / auval / DAW 삽입 / 실청취는 각각 별도다.
- `extra CPU alone is not a quality result`: 알고리즘은 objective metric 개선 또는
  실청취 비교 통과일 때만 유지한다. CPU 회귀는 항상 측정해 기록한다.
- 벤더 코드는 사본 + `REVISION` + provenance 노트로 고정한다. 원본을 참조 빌드하지 않는다.

## 4. 태스크

### Task 1 — eon_dsp 벤더링, provenance, CTest 등록

**Files**

- Create: `third_party/eon_dsp/Dsp/*.h` (11 헤더, 원본 그대로)
- Create: `third_party/eon_dsp/REVISION` (내용: `c8e71f3c826e43f574804ecf63edaea1f34120ad`)
- Create: `third_party/eon_dsp/PROVENANCE.md` (원본 경로, 커밋, 복사일, 라이선스 상태, 외부 의존 없음 근거)
- Modify: `CMakeLists.txt` (`add_library(eon_dsp INTERFACE)` + `add_library(eon::dsp ALIAS eon_dsp)` + include dir)
- Create: `Tests/EonDspContractTests.cpp` (사본이 스스로 컴파일/동작함을 고정하는 최소 계약 테스트)

- [x] **Step 1 (RED):** 벤더링 전 `clang++ -fsyntax-only -Ithird_party/eon_dsp` ->
      `fatal error: 'Dsp/Adaa.h' file not found` 확인.
- [x] **Step 2:** `Dsp/` 11 헤더 + `REVISION` + `PROVENANCE.md` 복사, CMake에 `eon::dsp`
      INTERFACE 타깃 추가. `diff -rq`로 원본과 byte-identical 확인. submodule
      (`third_party/simde`, `sst-*`)은 어떤 헤더도 참조하지 않아 복사하지 않았다.
- [x] **Step 3 (GREEN):** `ctest -R EonDspContract` PASS (Release, Debug 각각).
      실제 계약 범위: `ADAA2` 반복노드 해석해 + nextafter 근접 샘플 유한성/상한,
      `tanhF1` 대인수 안정성·우함수, `HbTaps71/47/35` 대칭 + `2*sum(hE)==1` + `p` 값,
      oversampler DC 이득과 block 분할 독립성(stages 1..3), `Ladder4` 무포화 DC 이득 1 /
      포화 시 tanh(1) 정착 / 고공진 유한·유계 / 1 decade 20 dB 이상 감쇠, `SvfTPT`
      lp·hp·bp DC 응답과 per-sample cutoff 변조 유한성, `DCBlocker` DC 제거 + 1 kHz
      0.2% 통과 + 무효 샘플레이트 fail-closed, `ftz`/`ScopedDenormalsOff`,
      `Rng` 시드 결정성, `lambertW0`/`newtonScalar`/`solveDense`(특이행렬 거부 포함),
      회로 프리미티브(InductorResonator, AnalogAir, ClassAStage, TriodeStage,
      JilesAtherton, WdfDiode/WdfDiodePair) 유한성·유계, `measure::binMag`/`thdPercent`.
- [x] **Step 4:** 전체 CTest **10/10 PASS** (기존 9 + EonDspContract), Release와 Debug 양쪽.
      플러그인/툴 타깃 전부 재빌드 성공(ODR·링크 회귀 없음).

**증거:** RED 컴파일 오류 로그, `diff -rq` byte-identical, `shasum -a 256` 11개 해시
(`PROVENANCE.md`에 기록), `ctest` 요약, `REVISION` = `c8e71f3c826e43f574804ecf63edaea1f34120ad`.

**실행 기록 (2026-09-22):** 첫 GREEN 시도에서 `measure::thdPercent` 순수 사인 판정이
실패했다. 원인은 측정 잔차가 아니라 측정 설계였다: 8192 샘플에서 1000 Hz는 정수 주기가
아니어서 사각 윈도우 누설이 하모닉 빈으로 들어왔고, "THD < 0.01%"가 신호 특성이 아닌
윈도우 특성을 재고 있었다. 기준 신호를 정확히 1초(1000 주기)로 맞춰 누설을 제거한 뒤
임계를 1e-6으로 조인 상태로 통과시켰다. 이 기준은 Task 2의 alias proxy 측정에도
그대로 적용해야 한다.

### Task 2 — 등급별 alias proxy 하네스 (선행 필수)

Task 3/4의 개선을 수치로 판정하려면 "wrapper 수준 quality-order gate"가 먼저 있어야 한다.
`audio-knowledge/validation/renderer-quality-order-gate.md` 패턴을 따른다.

**Files**

- Create: `Tests/AudioQuality/QualityOrderTests.cpp`
- Modify: `CMakeLists.txt` (콘솔 앱 + `add_test(NAME QualityOrder ...)`)
- Create: `docs/quality/b2-alias-baseline.md`

- [ ] **Step 1 (RED):** 품질 등급 개념이 아직 없으므로, 등급 열거형과 렌더 함수를 요구하는 테스트가 컴파일 실패.
- [ ] **Step 2:** 실제 `HybridWavetableAudioProcessor` 렌더러로 결정적 매트릭스를 만든다.
      48 kHz, 10 kHz 사인, 16384 샘플 + 4096 warm-up, block 패턴 64/127/256,
      odd/even/balanced 캐릭터, medium/hot 레벨. 각 렌더의 folded-harmonic alias proxy를
      `audioquality::measureInharmonicAliasDbc`로 계산하고 등급별 median을 낸다.
- [ ] **Step 3 (GREEN):** Normal이 Eco보다 최소 12 dB 개선, High는 Normal 대비 1 dB 이상 악화되지 않음
      (YinYeng 2026-09 후보에서 쓴 값. 제품별로 재조정 가능).
- [ ] **Step 4:** 현재 코드(=단일 등급)의 alias proxy 수치를 `docs/quality/b2-alias-baseline.md`에
      머신/빌드타입/바이너리 SHA256과 함께 기록. Golden은 승격하지 않는다. 커밋.

**증거:** `Build/quality-b2-before/` 렌더 + report, baseline 문서.

### Task 3 — 새터레이션 스테이지 교체 (ADAA2 + eon::Oversampling, 등급제)

현재 `SynthVoice::renderNextBlock`은 per-voice `juce::dsp::Oversampling`
(2x, `filterHalfBandPolyphaseIIR`)로 비대칭 tanh를 감싼다. eon_dsp의 Kaiser-windowed
half-band FIR polyphase(71/47/35 taps, stopband -113/-102/-90 dB)와 ADAA를 조합해
같은 CPU 예산에서 더 깨끗한 비선형을 만든다.

**Files**

- Modify: `Source/DSP/SynthVoice.h`, `Source/DSP/SynthVoice.cpp`
- Modify: `Source/PluginProcessor.cpp` (새 choice 파라미터 등록, latency 재계산)
- Modify: `Source/PluginEditor.cpp` (등급 노출 — §7 결정 1에 따름)
- Modify: `Tests/AudioQuality/ProcessorQualityTests.cpp`, `Tests/AudioQuality/fixture-manifest.json`

- [ ] **Step 1 (RED):** hot 10 kHz 단일 사인에서 현재 경로의 alias proxy와 THD를 기록하고,
      "등급 전환 시 alias proxy가 단조 개선" 테스트를 실패시킨다.
- [ ] **Step 2:** shaper를 **해석적 antiderivative가 존재하는 형태**로 고정하고
      `eon::ADAA2`(double state, 근접 샘플 시 midpoint fallback)로 감싼다.
      oversampling은 `eon::Oversampling`으로 교체: Eco=ADAA2 단독, Normal=2x FIR+ADAA2,
      High=4x FIR+ADAA2.
- [ ] **Step 3 (GREEN):** Task 2의 게이트 통과 + finite/peak/DC 게이트 유지
      (fixture peak <= -1 dBTP, 무음 RMS <= -120 dBFS, 후단 DC <= -80 dBFS).
- [ ] **Step 4:** latency를 **최대 등급 기준으로 prepare 시 확정**하고
      등급 변경 시 재계산한다. `setLatencySamples` 값 변화를 기록하고, 오프라인 렌더
      정렬 게이트(REAPER 3-sample 선행 같은 기존 정렬 검증)를 다시 확인한다.
      `CpuBench`로 등급별 per-voice 비용을 측정한다(128 voice stress 포함).
- [ ] **Step 5:** 신규 파라미터는 **추가만** 하고 기본값은 기존 청감과 가장 가까운 등급으로 둔다.
      state v1 fixture 마이그레이션 테스트가 그대로 통과해야 한다. 커밋.

**주의:** per-voice 4x FIR oversampling x 128 voices는 CPU가 급증할 수 있다.
등급제로 상한을 두고, 필요하면 High 등급에서만 polyphony를 제한한다. 버스 단위로
옮기는 선택은 pre-sum per-voice 포화라는 현재 톤 설계를 바꾸므로 이 태스크에서 하지 않는다.

### Task 4 — 필터 드라이브 비선형화 + slope 정직화

두 개의 실제 결함을 함께 닫는다. (a) `filterDrive`는 `Decibels::decibelsToGain` 선형
전치증폭이고 포화는 필터 **뒤**에 있다 -> "비선형 필터 드라이브"가 아니다. (b) `filterSlope`는
4개 라벨에 2개 동작만 있고 제조자 라벨과 에디터 라벨이 서로 다르다.

**Files**

- Modify: `Source/DSP/SynthVoice.h`, `Source/DSP/SynthVoice.cpp`
- Modify: `Source/PluginProcessor.cpp` (choice 라벨 정정)
- Modify: `Source/PluginEditor.cpp` (combo 아이템 리스트 정정)
- Modify: `Tests/AudioQuality/ProcessorQualityTests.cpp`, `Tests/WavetableTests.cpp`

- [ ] **Step 1 (RED):** `slope 0`과 `slope 1` 렌더가 bit-identical임을 고정하는 회귀 테스트가
      실패하도록 기대치를 "서로 다른 기울기"로 바꾼다. 동시에 `filterDrive = 0 dB`에서
      기존 클린 경로와 일치해야 한다는 회귀를 세운다.
- [ ] **Step 2:** `eon::Solvers`(`newtonScalar`, `lambertW0`) + `eon::Zdf`를 사용해
      포화를 **필터 피드백 루프 안**으로 옮긴다. tanh-in-loop는 Lambert W 해석해 또는
      Newton 반복으로 푼다.
- [ ] **Step 3:** slope를 실제로 구현한다. `OnePoleTPT`(6 dB) + `SvfTPT`(12 dB) 조합으로
      6/12/18/24 dB/oct를 만들고, processor/editor 라벨을 실제 DSP와 일치시킨다(§7 결정 2).
- [ ] **Step 4 (GREEN):** 각 slope의 실측 기울기가 목표값의 ±10% 이내,
      `filterDrive = 0 dB`에서 기존 렌더와 동일(클린), `filterDrive = +24 dB`에서 THD 상승,
      그리고 resonance 자기진동이 유한하게 유지됨.
- [ ] **Step 5:** cutoff 변조(엔벨로프/LFO, per-sample)와 `filterType` 3종을 새 구조에서
      재확인하고, A2의 cutoff 게이트를 그대로 통과시킨다. 커밋.

### Task 5 — 아날로그 컬러 단계 (조건부, Task 3/4 이후에만)

`eon::TriodeStage`(Koren), `eon::Transformer`(Jiles-Atherton), `eon::InductorResonator`는
이미 `Tube comp`/EON 계보에서 파라미터 세트가 검증된 재료다. 새 노브를 늘리지 않고
기존 `saturation`/`filterDrive` 뒤단에 "color" 등급으로 매핑하는 것을 기본안으로 한다.

- [ ] **Step 1:** `color` 등급(off / triode / transformer)을 내부적으로만 두고 렌더 비교.
- [ ] **Step 2:** objective metric 개선 또는 실청취 비교 통과일 때만 유지. 아니면 폐기하고
      그 판단을 기록한다(유지 규칙). 채택 시에만 파라미터 노출을 논의한다.

### Task 6 — 출력단/실시간 안전

- [ ] **Step 1:** `OutputSafety`의 float 10 Hz DC blocker를 `eon::DCBlocker`(double,
      `prepare(sr, 18 Hz)`)로 교체하고 DC 게이트(-80 dBFS)와 1 kHz gain error(<0.01 dB)
      회귀를 유지한다.
- [ ] **Step 2:** voice/render 경계에 `eon::ScopedDenormalsOff`(`ftz`)를 적용해
      denormal CPU 폭주를 막고, RT 경로가 allocation-free임을 기존 게이트로 재확인한다.
- [ ] **Step 3:** drift/random phase 난수원을 `eon::Rng`로 통일한다. Golden 결정성을 위해
      기존 `RealtimeRandom` 시드 -> `eon::Rng` 시드 매핑을 문서화하고, 결정적 렌더 회귀를 유지한다.
- [ ] **Step 4:** `tracktion_...` 없이 순수 계산인 `eon::Measure`(thdPercent, foldedMagAt)를
      테스트 전용 측정에 활용해 Metrics와 중복 계산을 줄인다(선택).

### Task 7 — 통합 증거 리포트와 종료 조건

**Files**

- Create: `docs/quality/b-plan-report.md`
- Create: `docs/quality/b-task-matrices.md`

- [ ] **Step 1:** 48 kHz / block 64·128·512 매트릭스와 DSP 변경 게이트(44.1, 48, 88.2, 96,
      176.4, 192 kHz x block 16..2048)를 실행하고 태스크별 before/after를 표로 남긴다.
- [ ] **Step 2:** 모든 출력 finite, 무음 RMS, DC, peak, pitch/cents, tail, state restore
      게이트를 재확인한다.
- [ ] **Step 3:** `CpuBench`(등급별, 1/8/32/128 voice)와 latency 변화를 기록한다.
- [ ] **Step 4:** 남은 게이트를 명시적으로 "미검증"으로 남긴다: Ableton/REAPER 로드, pluginval,
      auval, 실청취. 이들은 Plan A3 증거 체계로만 닫는다.

## 5. 완료 조건 (Plan B acceptance 후보)

- Build-Release CTest 전부 PASS (기존 9개 + EonDspContract + QualityOrder).
- Task 2의 alias proxy 단조 개선 게이트가 등급별로 PASS.
- `filterDrive = 0 dB`에서 기존 클린 경로 동등성 유지, slope 4종이 실제로 구분됨.
- finite / peak / DC / 무음 / tail / state 게이트 유지.
- CPU 회귀가 등급별로 측정·기록됨(임계는 baseline 대비 사전 합의).
- provenance(REVISION + 커밋)와 evidence 리포트가 같은 커밋에 묶임.

## 6. 리스크

| 리스크 | 영향 | 완화 |
| --- | --- | --- |
| per-voice 다단 FIR oversampling x 128 voice | CPU 폭증, 드롭아웃 | 등급제, High에서 polyphony 상한, CpuBench 측정 |
| 새 oversampling으로 latency 변경 | 호스트 지연 보상 불일치, 오프라인 정렬 | prepare 시 확정 + `setLatencySamples` 재계산 + 정렬 게이트 재검증 |
| ADAA double state 및 근접 샘플 처리 | 고음역 잡음/불안정 | 근접 샘플 midpoint fallback + 코어 계약 테스트로 고정 |
| filterDrive 비선형화로 기존 톤 변화 | 프리셋 청감 변화 | drive 0 dB 동등성 회귀 + 기본값 유지 + 실청취 게이트 분리 |
| slope 라벨/DSP 변경 | 기존 프리셋 의미 변화 | 라벨 정정은 별도 커밋, 문서화, state 값 의미 유지 |
| eon_dsp LICENSE 부재 | 배포 시 라이선스 불명확 | 자체 1st-party로 기록, 배포 전 라이선스 명시 |
| Golden 미승격 상태 | 회귀 판정 기준 부재 | 절대 게이트 수치 + Task 2 하네스로 대체 |

## 7. 결정이 필요한 지점 (기본안 포함)

1. `saturationQuality` 등급을 **UI에 노출**할지, 내부 고정(Normal)으로 둘지.
   기본안: 내부 고정 + 고급 설정에서만 노출.
2. slope 라벨을 `6/12/18/24 dB/oct`로 정정할지(실제 DSP와 일치), `8 dB` 근사 표기를
   유지할지. 기본안: `6/12/18/24`로 정정.
3. Task 5 아날로그 컬러 단계를 이번 Plan B 범위에 포함할지, 별도 Plan으로 미룰지.
   기본안: Task 3/4 완료 후 조건부 진행.
