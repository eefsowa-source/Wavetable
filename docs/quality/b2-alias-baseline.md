# Plan B Task 2 — alias-quality instrument and baseline

측정일: 2026-09-22  
기준 커밋: `890fe6e` (Task 1 = eon_dsp 벤더링) + Task 2 변경  
상태: **INSTRUMENT + BASELINE (제품 게이트 아님)**

이 문서는 Plan B에서 등급별 alias 순서를 판정할 **계측기**와 현재 코드의
**기준선 수치**를 기록한다. 등급(Eco/Normal/High) 순서 게이트는 Task 3에서
등급이 실제로 생긴 뒤 같은 계측기 위에 추가한다.

## 1. 계측기

실제 `HybridWavetableAudioProcessor`를 `OfflineRenderer`로 렌더한 뒤 측정한다.
rationale은 `audio-knowledge/validation/renderer-quality-order-gate.md`와 같다:
품질 선택자는 래퍼에 살기 때문에 DSP 코어 단위 테스트로는 순서를 확립할 수 없다.

| 항목 | 값 |
| --- | --- |
| 프로브 | MIDI note 123, 순수 사인 프레임(osc1 only, unison 1) |
| tune | 요청 0.07623 semitones -> **전송 0.08** (0.01 격자 스냅) |
| 실제 프로브 주파수 | 10002.1767 Hz |
| folded 3차 고조파 | 17993.4698 Hz |
| 분석 | 48 kHz, 16384-point Hann, warm-up 4096 samples |
| 기준선 블록 | 64, 127, 256 |
| 프록시 | `10*log10(alias band / tone band)`. tone band = f0 ± 300 Hz, alias band = fold ± 600 Hz |

**프록시 정의:** 3차 고조파(3*f0)는 48 kHz에서 Nyquist 위에 있으므로 접혀서
`|3*f0 - 48000|` Hz에 나타난다. 선형 경로는 이 위치에 에너지를 만들 수 없으므로,
여기 있는 에너지는 비선형에서 생성되어 접힌 것이다. 기준선(톤) 대비 더 음수일수록 깨끗하다.

**`measureInharmonicAliasDbc`를 쓰지 않은 이유:** 이 지표는 각 고조파 주변 ±1.5 bin만
legal로 표시하므로 프로브 톤이 정확히 FFT bin 위에 있어야 한다. 계측기를 만들 당시에는
불가능했다. **당시 모든 float 파라미터가 0.01 단위로 양자화되었기 때문이다**: JUCE 8의
`AudioParameterFloat(id, name, min, max, default)` 생성자가
`NormalisableRange(min, max, 0.01f)`로 전달한다. `osc1Tune`(±24 semitones)에서
이 격자는 **1 cent**이고, 10 kHz에서 1 cent는 5.78 Hz로 16384-point 분석의 bin 간격
2.93 Hz보다 크다. 톤이 0.45 cent 빗나가면 Hann 주엽이 legal bin 밖으로 새어
오디오가 아무리 깨끗해도 지표가 **-8.7 dBc** 고정값을 보고했다(Task 2에서 실측).

Task 2b가 이 격자를 제거했으므로 이제는 bin 정렬이 가능하다. 그럼에도 밴드 프록시를
유지한다: 정렬에 의존하지 않아 더 견고하고, 이미 기준선이 이 방식으로 기록되어 있기
때문이다. 아래 수치는 Task 2b 이후 재측정한 값이다.

## 2. 기준선 수치 (현재 단일 품질 경로)

Release, Apple M1 Ultra, macOS 15.7.9. 실행 파일 SHA-256:
`32227731f81f375a5555fc15f6e4a7ec50017adb3dddfbfb6016a918fe22b2b2`.

| 설정 | block 64 | block 127 | block 256 | median |
| --- | --- | --- | --- | --- |
| `saturation = 0.15` (제품 기본) | -133.47 dBc | -133.47 dBc | -133.47 dBc | **-133.47 dBc** |
| `saturation = 1.00` (최대 drive) | -108.92 dBc | -108.92 dBc | -108.92 dBc | **-108.92 dBc** |
| `saturation = 0.00` (비선형 우회) | — | -150.67 dBc | — | 계측기 바닥 |

파생 수치:

- drive 민감도: **+24.55 dB** (기본 -> 최대). 계측기가 비선형 깊이를 확실히 분해한다.
- 계측기 바닥 대비 여유: 기본 경로 **+17.20 dB**, 최대 drive **+41.75 dB**.
  즉 기본값의 -133.47 dBc는 수치 잡음이 아니라 실제 접힘 에너지다.
- 블록 크기 의존성: 세 블록 모두 0.01 dB 이내. 순서 비교에 충분히 안정적이다.
- 피치 정확도: 기대 10002.1767 Hz, 실측 10002.1625 Hz -> **-0.0025 cents**.
  (Debug/Release 동일, 0.1 cent 게이트 통과.)
- 파라미터 해상도: tune 0.070 -> 9996.4029 Hz, tune 0.075 -> 9999.2745 Hz
  = **0.4972 cents** 간격. Task 2b 이전에는 0.9972 cents(0.01 semitone 스냅)였다.

Task 2b 이전 기록(clean `-133.48`, floor `-151.42`)과의 차이는 최대 0.75 dB이며,
이는 파라미터 값의 마지막 ULP 차이가 float32 FFT의 수치 바닥을 움직인 결과다.
판정에 쓰는 수치(민감도 24.5 dB, 순서 비교)는 그대로다.

## 3. 이 계측기가 보증하는 것 / 보증하지 않는 것

보증:

- 렌더러가 파라미터 격자 위에서 요청 피치에 0.1 cent 이내로 도달한다.
- float 파라미터가 0.01 단위로 스냅되지 않는다(아래 Task 2b). tune 0.005 semitone
  변화가 0.5 cent로 그대로 오디오에 도달한다.
- 같은 시드/설정에서 렌더가 bit-identical로 재현된다.
- 비선형 drive가 커지면 folded 에너지가 계측 가능하게 증가한다(≥15 dB 게이트).

보증하지 않음:

- **등급 순서(Eco < Normal < High)가 아니다.** 제품에 아직 품질 등급이 없어
  모든 등급 라벨이 동일하게 렌더되므로, 지금 그 게이트를 걸면 항상 통과하거나
  항상 실패한다. Task 3에서 등급과 함께 추가한다.
- 주관적 톤, IMD, CPU, 지연, 상태 복원, AU/VST 유효성, DAW 로딩은 별도 게이트다.
- 이 수치는 특정 커밋/바이너리에 묶인 것이다. DSP가 바뀌면 다시 측정한다.

## 4. Task 3에 남기는 것

1. `saturation`은 유지하고 `saturationQuality`(Eco/Normal/High)를 추가한 뒤,
   이 계측기 위에 **Normal이 Eco보다 최소 12 dB 개선**, **High는 Normal 대비 1 dB 이상
   악화되지 않음** 게이트를 추가한다(값은 제품별 재조정 가능).
2. 계측기 바닥(-151.42 dBc)이 개선 폭의 상한을 정한다. 등급이 낮을수록 증명 가능한
   개선 폭이 줄어들므로, Eco를 계측기 바닥 근처에 두는 설계는 게이트를 불가능하게 만든다.
3. 등급 도입으로 **블록 크기별 지연/품질 선택이 prepare 시점에 확정**되어야 한다
   (파라미터를 prepare 이후에 바꾸는 현재 테스트 방식은 prepare-time 설정에는
   반영되지 않으므로, 등급용 `prepare()` 재호출 경로가 필요하다).
4. ~~부수 발견: JUCE `AudioParameterFloat` 5-인자 생성자의 0.01 격자~~ -> **Task 2b에서
   해결**. `makeLayout()`이 명시적인 연속 `NormalisableRange`를 넘긴다. 자세한 내용은
   아래 6절.

## 6. Task 2b — 파라미터 해상도 복원 (완료)

Task 2에서 발견한 0.01 격자를 Plan B 범위에 포함해 수정했다.

- 원인: JUCE 8의 `AudioParameterFloat(id, name, min, max, default)` 생성자가
  `NormalisableRange(min, max, 0.01f)`로 전달한다. 스냅 단위가 각 파라미터의
  **자연 단위** 0.01이라 파라미터마다 의미가 달랐다.
- 영향: `osc1Tune` 1 cent, ADSR 6개 실효 최소 0.01 s(범위 최소 0.001 s 도달 불가),
  `delayTime` 10 ms, `osc1Pos` 등 1% 스텝.
- 수정: `Source/PluginProcessor.cpp`의 `makeLayout()`에 연속 범위 팩토리
  (`NormalisableRange<float>(min, max, 0.0f)`)를 추가하고 float 파라미터 39개에 적용.
  선형·skew 1이므로 **정규화 <-> 실제값 매핑은 이전과 동일**하고 스냅만 사라진다.
  따라서 기존 세션/프리셋의 저장값은 그대로 복원된다.
- RED: `ProcessorQuality` 6개 파라미터 전부 "keeps its requested resolution" 실패,
  `QualityOrder`에서 tune 0.005 semitone 변화가 **0.9972 cents**로 측정.
- GREEN: 동일 테스트 통과, 변화량 **0.4972 cents**, Release 11/11 / Debug 통과.
- 회귀 고정: `Tests/AudioQuality/ProcessorQualityTests.cpp`가 6개 파라미터의 값 보존을,
  `Tests/AudioQuality/QualityOrderTests.cpp`가 미세 tune이 오디오에 도달함을 검증한다.

## 5. 재현

```sh
cmake -S . -B Build-Release
cmake --build Build-Release --target QualityOrderTests -j8
ctest --test-dir Build-Release -R QualityOrder --output-on-failure
# 또는 수치 표를 직접 보려면:
Build-Release/QualityOrderTests_artefacts/Release/QualityOrderTests
```

Golden은 승격하지 않는다. 이 계측기는 절대 게이트와 before/after 비교를 위한 것이다.

## 7. Task 3 — 새터레이션 스테이지 교체 (완료)

`Source/DSP/SaturationStage.h` 신설. 이전 경로(비대칭 tanh + `juce::dsp::Oversampling`
2x half-band polyphase IIR)를 **`eon::ADAA1` + `eon::Oversampler` 2x Kaiser-FIR**로
교체했다. per-voice 배치(합산 전 포화)는 유지했다.

### 7.1 셰이퍼와 톤

이전 커브 `tanh(x + 0.12x²) - 0.118·tanh(0.12x²)`는 초등 antiderivative가 없어 ADAA를
적용할 수 없다. 커브를 **고정 입력 바이어스를 준 tanh** `tanh(v + b) - tanh(b)`로 바꿨다.
정확한 antiderivative `F1(v) = ln(cosh(v+b)) - tanh(b)·v`가 존재해 ADAA1이 그대로 적용되고,
바이어스가 기존의 비대칭(짝수 고조파) 성격을 유지한다.

바이어스는 기본 드라이브에서 프로파일이 그대로 남도록 맞췄다.
측정 방법: 1 kHz 사인, 정수 100주기, 직접 DFT로 H1..H5 진폭 비.

| 입력 진폭 | 셰이퍼 | H1 | H2/H1 | H3/H1 | H4/H1 | H5/H1 |
| --- | --- | --- | --- | --- | --- | --- |
| 0.44 (기본 drive) | 이전 커브 | 0.41987 | -34.14 | -36.18 | -59.09 | -71.56 |
| 0.44 (기본 drive) | 새 (bias 0.10 + ADAA1) | 0.41552 | -33.87 | -36.57 | -63.84 | -71.93 |
| 1.80 (최대 drive) | 이전 커브 | 1.06936 | -43.28 | -16.17 | -36.99 | -29.96 |
| 1.80 (최대 drive) | 새 (bias 0.10 + ADAA1) | 1.07934 | -28.58 | -16.59 | -37.88 | -30.84 |

기본 드라이브에서는 기본파가 0.09 dB 이내, H2/H3가 0.4 dB 이내로 일치한다. 최대
드라이브에서는 2차 고조파가 15 dB 더 뜨거워진다(기존 커브의 비대칭 항이 포화되면서
2차가 오히려 줄어드는 성질이 사라짐). 이는 의도된 변화이며 **실청취 게이트** 항목이다.

### 7.2 folded alias

프록시는 3차 고조파 접힘(`|3·f0 - 48000|`) 밴드 비율로 동일하다. 다만 밴드 폭을
600 → 150 Hz로 좁혔고(아래 7.3), 이 때문에 수치를 그대로 비교할 수는 없다.

| 설정 | 이전 (600 Hz 밴드) | 이후 (150 Hz 밴드) |
| --- | --- | --- |
| `saturation = 0.15` (기본) | -133.47 dBc | **-156.99 dBc** |
| `saturation = 1.00` (최대) | -108.92 dBc | **-156.54 dBc** |
| 비선형 우회(계측기 바닥) | -151.42 dBc | -156.65 dBc |

밴드 폭 변경만으로 바닥이 -153.05 → -156.65 dBc로 3.6 dB 움직이므로, 같은 조건으로
보면 최대 드라이브 개선은 약 40 dB다. **이후 값들은 렌더의 float32 양자화 잡음과
구분되지 않는다**: 스테이지의 접힘 에너지가 계측 바닥 아래로 내려갔다.

추가 실험: oversampling을 끄고 ADAA만 쓰면 최대 드라이브에서 **-62.69 dBc**로
교체 전 경로(-108.92)보다 46 dB 나쁘다. 10 kHz 톤에서 1차 ADAA는 약하다. 즉 이 개선은
half-band FIR이 대부분을 담당하고 ADAA가 잔여분을 지우는 조합의 결과다. 향후 등급 설계는
이 사실 위에서 세워야 한다(7.5).

### 7.3 계측기 변경

1. **float FFT → double 직접 DFT.** `juce::dsp::FFT`는 float32라 바닥이 약 -150 dBc였다.
   스테이지가 그보다 깨끗해지자 계측기가 자기 신호를 볼 수 없게 되어(두 드라이브 설정이
   모두 바닥에 붙음) double 직접 DFT + 회전 recurrence로 바꿨다.
2. **밴드 축소.** 톤 밴드 ±300 Hz, alias 밴드 ±600 → ±150 Hz. 렌더는 float32이고 잡음은
   백색이므로 밴드가 넓으면 신호 없이 바닥만 올라간다.
3. **게이트 교체.** 이전의 "drive 민감도 ≥ 15 dB" 게이트는 옛 설계를 재던 것이다. ADAA
   이후에는 두 설정 모두 바닥에 붙어 이 게이트가 성립하지 않는다. 대신
   **주입 라인 캘리브레이션**(-40/-80/-120 dBc를 접힘 주파수에 넣고 1 dB 이내로 보고해야 함,
   실측 -40.00/-80.00/-120.00)과 **절대 상한 `≤ -150 dBc`** 게이트로 바꿨다.

### 7.4 지연과 CPU

- 지연: 0 → **35 samples**(2x, eon half-band). 임펄스 왕복으로 측정해 정수임을 확인했고
  `setLatencySamples`가 그대로 보고한다.
- CPU: `CpuBench` Release 중앙값 solo-unison8 **84.8 ms**, dense16-unison8 **1328.5 ms**
  (Phase 2 대비 +35%/+34%, 최초 baseline 1293.0 ms 대비 +2.7%). 상세는
  [cpu-optimization.md](cpu-optimization.md).

### 7.5 남은 것

- 등급(`saturationQuality`)과 순서 게이트는 아직 없다. 위 측정으로 래더 설계가 정해졌다:
  Eco = 2x half-band + 기존 tanh(교체 전 동작), Normal = 2x + ADAA(현재), High = 4x + ADAA.
  "ADAA 단독"은 Eco 후보가 될 수 없다(-62.69 dBc).
- 실청취(최대 드라이브의 2차 고조파 변화), 호스트 로드, pluginval/auval은 별도 게이트다.
