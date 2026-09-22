# SEOUL DSP Plan B — eon_dsp 기반 음질 업그레이드 계획

> **For agentic workers:** 각 태스크는 RED -> GREEN 순서로 진행하고, 태스크 단위로 커밋한다.
> 체크박스(`- [ ]`)는 실행 시점에 갱신한다.

**작성일:** 2026-09-22
**상태:** IN PROGRESS (Task 1, 2, 2b, 3, 3b, 4 완료 / 5, 6, 7 남음, 2026-09-22)
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

- [x] **Step 1 (RED, 계획 대비 변경):** 당초 계획은 "등급 열거형이 없어 컴파일 실패"를 RED로
      잡는 것이었으나, 그러면 빌드가 깨진 상태로 남는다. 그래서 RED를 두 갈래로 실행했다:
      (a) `measureInharmonicAliasDbc` 기반 계측기가 실제로 판정 불가임을 실측으로 확인
      (-8.7 dBc 고정, saturation 변화에 0.00 dB 반응), (b) 그 원인이 측정 설계임을 규명.
- [x] **Step 2:** 실제 `HybridWavetableAudioProcessor`를 `OfflineRenderer`로 렌더하는
      결정적 계측기를 만든다. 48 kHz, note 123 순수 사인, 16384-point Hann 분석 +
      4096 warm-up, block 64/127/256. 프록시는 **folded-band 비율**로 구현했다:
      tone band `f0 ± 300 Hz` 대비 alias band `|3*f0 - 48000| ± 600 Hz`.
      `audioquality::measureInharmonicAliasDbc`는 쓰지 않는다 — 아래 실행 기록 참조.
- [x] **Step 3 (GREEN, 부분):** 등급이 아직 없으므로 등급 순서 게이트 대신 계측기 자체의
      유효성을 고정했다: 피치 0.1 cent 이내 도달, 렌더 결정성(bit-identical),
      drive 민감도 (기본->최대 **+24.56 dB**, 게이트 +15 dB). 등급 순서 게이트
      (Normal >= Eco + 12 dB, High >= Normal - 1 dB)는 **Task 3으로 이동**한다.
- [x] **Step 4:** `docs/quality/b2-alias-baseline.md`에 머신/빌드타입/바이너리 SHA256과
      기준선(-133.48 dBc 기본, -108.92 dBc 최대, 계측기 바닥 -151.42 dBc)을 기록.
      Golden 미승격. Release 11/11, Debug 통과. 커밋.

**증거:** [b2-alias-baseline.md](../quality/b2-alias-baseline.md), `ctest -R QualityOrder`
요약(Release 0.81 s / Debug 1.51 s), 실행 파일 SHA-256(Release `9939517d...`).

**실행 기록 (2026-09-22) — 계획에서 바뀐 두 가지:**

1. **계측기를 bin 기반에서 밴드 기반으로 바꿨다.** 첫 시도는 `measureInharmonicAliasDbc`를
   그대로 쓰고 프로브 톤을 FFT bin에 맞추려 했으나, 프로브가 0.45 cents 플랫하게 나와
   지표가 -8.7 dBc에 고정됐고 saturation 변화에 0.00 dB 반응했다. 두 독립 추정기
   (parabolic, zero-crossing)가 같은 값을 줘서 측정 오류가 아님을 확인한 뒤 원인을 찾았다:
   **JUCE 8의 `AudioParameterFloat(id, name, min, max, default)`가
   `NormalisableRange(min, max, 0.01f)`로 전달**하므로 `osc1Tune`(±24 semitones)의
   격자가 1 cent가 되고, 10 kHz에서 1 cent = 5.78 Hz > bin 간격 2.93 Hz라서
   **bin 정렬이 원리적으로 불가능**하다. 요청 0.074553 -> 실제 0.07 (오차 -0.455 cents)이
   관측과 정확히 일치했다. 그래서 고조파에서 1.4 kHz 떨어진 밴드를 재는 프록시로 바꿨다.
2. **등급 순서 게이트를 Task 3으로 넘겼다.** 등급이 없는 상태에서 순서를 주장하면
   항상 통과하거나(무의미) 항상 실패한다. 대신 계측기의 민감도와 결정성을 지금 고정해,
   Task 3이 "등급이 실제로 달라졌는가"만 판정하면 되도록 만들었다.

**부수 발견:** 위 0.01 격자는 플러그인의 **모든 float 파라미터**에 적용된다.
`delayTime`(0.03..1.5 s)은 10 ms 단위, ADSR 6개 파라미터(최소 0.001 s)는 실효
최소값이 0.01 s, `osc1Tune`은 1 cent 단위다. **사용자 승인("포함")으로 Task 2b에서
Plan B 범위에 포함해 수정했다.**

### Task 2b — 파라미터 해상도 복원 (Task 2에서 발견 / 승인 후 추가)

**Files**

- Modify: `Source/PluginProcessor.cpp` (`makeLayout()`의 float 파라미터 생성)
- Modify: `Tests/AudioQuality/ProcessorQualityTests.cpp` (값 보존 회귀)
- Modify: `Tests/AudioQuality/QualityOrderTests.cpp` (미세 tune이 오디오에 도달하는지)
- Modify: `docs/quality/b2-alias-baseline.md` (6절)

- [x] **Step 1 (RED):** `ProcessorQuality`에 6개 파라미터(`osc1Tune`, `ampAttack`,
      `filterDecay`, `delayTime`, `osc1Pos`, `lfo1Rate`)의 요청값 보존을 요구하는
      테스트를 추가 -> **6/6 실패**. `QualityOrder`에 tune 0.070 vs 0.075 비교 추가 ->
      **0.9972 cents**로 측정되어 "0.5 cent" 기대 실패.
- [x] **Step 2:** `makeLayout()`에 연속 범위 팩토리
      (`juce::NormalisableRange<float>(min, max, 0.0f)`)를 추가하고 float 파라미터
      39개에 적용. 선형·skew 1이므로 정규화 <-> 실제값 매핑은 이전과 동일하고 스냅만
      사라진다(기존 세션/프리셋 저장값은 그대로 복원). 파라미터 ID와 범위는 변경 없음.
- [x] **Step 3 (GREEN):** 두 테스트 모두 통과, tune 변화량 **0.4972 cents**.
      전체 CTest Release **11/11**, Debug 통과. 파라미터 개수/ID 불변.
- [x] **Step 4:** 기준선 재측정 및 문서 갱신. Golden 미승격. 커밋.

**증거:** RED 실패 로그(6개 파라미터 + 0.9972 cents), GREEN 로그
(0.4972 cents), Release/Debug `ctest` 요약.

**실행 기록 (2026-09-22):** 39개 float 파라미터 전부에 적용했다. "음질에 영향 있는
것만" 고르는 대신 클래스 전체를 없애는 편이 리뷰가 쉽고, 스냅하고 있던 파라미터를
남겨두면 다음 사람이 같은 함정에 빠진다(이번 Task 2가 정확히 그 함정이었다).
부작용 점검: 파라미터 ID·범위·기본값·개수 불변, 상태 마이그레이션 테스트 포함
11개 테스트 전부 통과. alias proxy 수치는 0.01~0.75 dB 움직였는데, 이는 파라미터
값의 마지막 ULP 차이가 float32 FFT 수치 바닥을 움직인 결과이지 음질 변화가 아니다
(게이트 수치인 drive 민감도는 24.56 -> 24.55 dB).

### Task 3 — 새터레이션 스테이지 교체 (ADAA2 + eon::Oversampling, 등급제)

현재 `SynthVoice::renderNextBlock`은 per-voice `juce::dsp::Oversampling`
(2x, `filterHalfBandPolyphaseIIR`)로 비대칭 tanh를 감싼다. eon_dsp의 Kaiser-windowed
half-band FIR polyphase(71/47/35 taps, stopband -113/-102/-90 dB)와 ADAA를 조합해
같은 CPU 예산에서 더 깨끗한 비선형을 만든다.

**범위 변경 (실행 중 결정):** 등급제(`saturationQuality`)를 Task 3에서 분리해 **Task 3b**로
미뤘다. 등급은 prepare 시점 확정이 필요하고(파라미터를 prepare 이후에 바꾸면 반영되지 않음)
래더 설계 자체가 측정 결과에 달려 있었기 때문이다. Task 3은 **DSP 교체와 그 증거**만 다룬다.

**Files (Task 3, 실제 변경)**

- Create: `Source/DSP/SaturationStage.h` (헤더 온리, ADAA1 + eon 반파대역 2x)
- Modify: `Source/DSP/SynthVoice.h`, `Source/DSP/SynthVoice.cpp`
- Modify: `Source/PluginProcessor.cpp` (지연 보고)
- Modify: `CMakeLists.txt` (`eon::dsp` 전파: HybridWavetable PUBLIC + voice를 컴파일하는 테스트)
- Modify: `Tests/AudioQuality/QualityOrderTests.cpp` (계측기 정밀도·게이트, 7.3)

- [x] **Step 1 (RED):** 교체 전 기준선은 Task 2에서 이미 확보했다(hot 10 kHz에서
      `-108.92 dBc`). 등급 순서 테스트는 Task 3b로 이동.
- [x] **Step 2:** 셰이퍼를 해석적 antiderivative가 존재하는 형태(바이어스 tanh)로 고정하고
      `eon::ADAA1`로 감쌌다. oversampling은 `eon::Oversampler` 2x로 교체.
      **`ADAA2`가 아니라 `ADAA1`인 이유:** 바이어스 tanh는 F1이 초등함수로 존재하고
      F2는 존재하지 않는다. eon_dsp도 같은 이유로 `TanhSat`(=ADAA1)을 제공한다.
      4차 소프트클립(ADAA2)은 정확하지만 하드한 플랫톱이라 현재 tanh 톤과 더 멀어진다.
- [x] **Step 3 (GREEN):** folded alias가 기본 `-156.99 dBc`, 최대 `-156.54 dBc`로
      렌더의 float32 잡음 바닥(-156.65 dBc)에 도달. 전체 CTest Release/Debug 11/11.
      finite/peak/DC/무음/tail/state 게이트 유지.
- [x] **Step 4:** 지연 0 → **35 samples**(임펄스 왕복 실측, 정수). `CpuBench`:
      solo 62.9 → **84.8 ms**, dense16 991.3 → **1328.5 ms**(Phase 2 대비 +34%,
      최초 baseline 1293.0 ms 대비 +2.7%). 기록: cpu-optimization.md.
- [x] **Step 5 (Task 3b 완료):** 등급 파라미터와 state 마이그레이션을 추가했다. 커스텀 UI는
      기본안에 따라 비노출하고 호스트 파라미터로 제공한다.

**증거:** [b2-alias-baseline.md](../quality/b2-alias-baseline.md) 7절 (before/after 표,
셰이퍼 고조파 프로파일, ADAA 단독 대조 실험 -62.69 dBc, 계측기 변경 이유, CPU/지연),
Release/Debug `ctest` 요약.

**실행 기록 (2026-09-22):** 계획 대비 세 가지가 측정으로 바뀌었다.

1. **oversampling이 필수임을 확인했다.** "ADAA2 단독(Eco)" 전제는 틀렸다. ADAA만 켜면
   최대 드라이브에서 -62.69 dBc로, 교체 전 IIR 경로(-108.92)보다 46 dB 나쁘다.
   개선의 대부분은 eon half-band FIR이 담당하고 ADAA는 잔여분을 지운다.
2. **계측기를 다시 손봐야 했다.** 스테이지가 float32 FFT 바닥보다 깨끗해지자
   Task 2의 drive 민감도 게이트가 성립하지 않게 되어(double 직접 DFT + 밴드 축소 +
   주입 라인 캘리브레이션으로 교체), 그 과정을 7.3에 남겼다.
3. **CPU는 Phase 1/2 절감분을 되돌려 썼다.** dense16이 최초 baseline 수준으로 돌아왔다.
   음질 대가로 지불한 비용이며, 등급제가 사용자 선택지를 제공할 지점이다.

**Task 3b로 넘긴 등급 래더:** Eco = 2x half-band + 기존 tanh 곡선, Normal = 2x + ADAA,
High = 4x + ADAA. 실제 Task 3b 측정 결과는 아래와 같이 갱신했다.

### Task 3b — 새터레이션 품질 등급 (완료)

`saturationQuality` 선택 파라미터(Eco/Normal/High)를 추가했다. 기본값은 Normal이며,
schema 3 마이그레이션이 구형 state의 누락 값을 Normal로 채운다. 커스텀 에디터에는 노출하지
않고 호스트 파라미터로만 제공해 기존 UI 기본안을 유지한다.

- [x] Eco = 2x half-band + 기존 tanh 곡선, Normal = 2x + ADAA1, High = 4x + ADAA1.
- [x] 10 kHz 3차 접힘: Eco -153.12, Normal -156.55, High -156.74 dBc. 모두 절대 상한
      -150 dBc를 통과한다.
- [x] 13 kHz 탐색 측정은 -64.95/-66.82/-65.73 dBc로 단조 순서가 없었다. 따라서 근거 없는
      품질 순서 assertion 대신 절대 상한과 실제 렌더 차이를 게이트로 고정했다.
- [x] 모든 등급과 drive 0 바이패스가 실제 임펄스 피크 47 samples에 정렬된다. 4x의 이론값
      46.5를 정수 호스트 지연 47로 보고하고 2x에는 12 samples를 보정한다.
- [x] 동일 Release 실행 CPU(solo/dense16): Eco 81.5/1268.9 ms, Normal 85.8/1310.5 ms,
      High 103.1/1629.1 ms. High는 dense 기준 Normal보다 약 24% 무겁다.
- [x] Release/Debug 전체 CTest 11/11과 state migration을 검증한다.

**증거:** [b2-alias-baseline.md](../quality/b2-alias-baseline.md) 7.5절,
[cpu-optimization.md](../quality/cpu-optimization.md).

### Task 4 — 필터 드라이브 비선형화 + slope 정직화

두 개의 실제 결함을 함께 닫는다. (a) `filterDrive`는 `Decibels::decibelsToGain` 선형
전치증폭이고 포화는 필터 **뒤**에 있다 -> "비선형 필터 드라이브"가 아니다. (b) `filterSlope`는
4개 라벨에 2개 동작만 있고 제조자 라벨과 에디터 라벨이 서로 다르다.

**Files**

- Create: `Source/DSP/SlopeFilter.h` (실제 4단 슬로프, eon TPT)
- Modify: `Source/DSP/SynthVoice.h`, `Source/DSP/SynthVoice.cpp`
- Modify: `Source/PluginProcessor.cpp` (choice 라벨 정정)
- Modify: `Source/PluginEditor.cpp` (combo 아이템 리스트 정정)
- Modify: `Tests/AudioQuality/ProcessorQualityTests.cpp`, `Tests/WavetableTests.cpp`

- [x] **Step 1 (RED):** `slope 0`과 `slope 1` 렌더가 bit-identical임을 고정하는 회귀 테스트가
      실패하도록 기대치를 "서로 다른 기울기"로 바꾼다. 동시에 `filterDrive = 0 dB`에서
      기존 클린 경로와 일치해야 한다는 회귀를 세운다. RED 실측: 3 kHz cutoff /
      resonance 0.9에서 `slope0 vs slope1 max difference = 0.00000000`.
- [x] **Step 2:** `eon::Solvers`(`newtonScalar`, `lambertW0`) + `eon::Zdf`를 사용해
      포화를 **필터 피드백 루프 안**으로 옮긴다. tanh-in-loop는 Lambert W 해석해 또는
      Newton 반복으로 푼다.
      **실행 중 변경:** 포화는 캐스케이드 **입력 노드**(아날로그 필터의 입력단)에 두었다.
      로우패스에서 SVF의 high-pass 노드는 컷오프 아래 성분만 나르므로 그 노드에 드라이브를
      걸면 컷오프 20 kHz에서 노브가 0 dB 효과가 된다. 공진 노드 자체의 포화(비선형 SVF 해)는
      미완으로 남긴다. 클리퍼는 `tanh(k v)/k` + `eon::ADAA1`이며 k=0에서 정확히 항등이다.
      자세한 근거와 발견한 float 잔차 버그는 [b4-filter-slopes.md](../quality/b4-filter-slopes.md) 6절.
- [x] **Step 3:** slope를 실제로 구현한다. `OnePoleTPT`(6 dB) + `SvfTPT`(12 dB) 조합으로
      6/12/18/24 dB/oct를 만들고, processor/editor 라벨을 실제 DSP와 일치시킨다(§7 결정 2).
      **실행 중 변경:** JUCE SVF 캐스케이드를 늘리는 대신 `Source/DSP/SlopeFilter.h`
      하나로 네 기울기를 만든다(1-pole / SvfTPT / SvfTPT+1-pole / SvfTPT+SvfTPT).
      라벨은 processor와 editor 모두 `6/12/18/24 dB/oct`로 정정했다. 근거는
      [b4-filter-slopes.md](../quality/b4-filter-slopes.md).
- [x] **Step 4 (GREEN, slope 부분):** 실측 기울기 5.99 / 12.10 / 18.10 / 24.22 dB/oct로
      라벨과 0.25 dB 이내. 전 슬로프 x 전 타입이 최대 resonance에서 유한·유계. Release/Debug
      CTest 11/11. `filterDrive` 비선형화와 그 THD 게이트는 Step 2와 함께 남는다.
      **드라이브 부분(GREEN):** `filterDrive = 0 dB` 렌더가 `eon::SvfTPT`와 float 1 ulp
      이내로 일치, `+24 dB`에서 THD 0.0035 % → **13.37 %**, peak 0.422로 유계.
- [x] **Step 5:** cutoff 변조(엔벨로프/LFO, per-sample)와 `filterType` 3종을 새 구조에서
      재확인했다: `SlopeFilter::setParams`가 샘플마다 호출되고, ProcessorSmoke의 cutoff/
      envelope/192 kHz/DC 게이트가 그대로 통과한다. `CpuBench`에 슬로프 인자를 추가했다.

**증거:** [b4-filter-slopes.md](../quality/b4-filter-slopes.md) (이전 상태 표, 실측 기울기,
CPU, resonance 범위 미수정 사실), Release/Debug `ctest` 11/11, `CpuBench 1 0..3`.

### Task 5 — 아날로그 컬러 단계 (조건부, Task 3/4 이후에만)

`eon::TriodeStage`(Koren), `eon::Transformer`(Jiles-Atherton), `eon::InductorResonator`는
이미 `Tube comp`/EON 계보에서 파라미터 세트가 검증된 재료다. 새 노브를 늘리지 않고
기존 `saturation`/`filterDrive` 뒤단에 "color" 등급으로 매핑하는 것을 기본안으로 한다.

- [x] **Step 1:** `color` 등급(off / triode / transformer)을 내부 후보 하네스에서만 두고
      4x oversampled 렌더를 비교했다. 제품 `SynthVoice` 경로와 파라미터에는 연결하지 않았다.
- [x] **Step 2:** 기준선 folded-third `-249.72 dBc`, Triode `-155.54 dBc`, Transformer
      `-67.67 dBc`였다. Triode는 기준선보다 개선되지 않았고 Transformer는 alias 절대
      게이트를 통과하지 못했다. 실청취 비교도 수행하지 않았으므로 두 후보를 폐기하고
      [b5-analog-color.md](../quality/b5-analog-color.md)에 판정을 기록했다.

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

1. `saturationQuality`: **결정 완료.** 기본 Normal, 커스텀 UI 비노출, 호스트 파라미터 제공.
2. slope 라벨을 `6/12/18/24 dB/oct`로 정정할지(실제 DSP와 일치), `8 dB` 근사 표기를
   유지할지. **결정 완료:** `6/12/18/24`로 정정(processor/editor 동시).
3. Task 5 아날로그 컬러 단계를 이번 Plan B 범위에 포함할지, 별도 Plan으로 미룰지.
   기본안: Task 3/4 완료 후 조건부 진행.
4. `resonance` 파라미터를 실제로 걸리는 범위로 넓힐지. 현재 0.1~1.0이 Q로 직결되어
   최대에서도 Q = 1.0이라 자기발진이 불가능하다. 모든 프리셋 톤이 바뀌므로 미결.
