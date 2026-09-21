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

**`measureInharmonicAliasDbc`를 쓰지 않은 이유(중요):** 이 지표는 각 고조파 주변
±1.5 bin만 legal로 표시하므로 프로브 톤이 정확히 FFT bin 위에 있어야 한다.
이 제품에서는 불가능하다. **모든 float 파라미터가 0.01 단위로 양자화되기 때문이다**:
JUCE 8의 `AudioParameterFloat(id, name, min, max, default)` 생성자가
`NormalisableRange(min, max, 0.01f)`로 전달한다. `osc1Tune`(±24 semitones)에서
이 격자는 **1 cent**이고, 10 kHz에서 1 cent는 5.78 Hz로 16384-point 분석의 bin 간격
2.93 Hz보다 크다. 톤이 0.45 cent 빗나가면 Hann 주엽이 legal bin 밖으로 새어
오디오가 아무리 깨끗해도 지표가 **-8.7 dBc** 고정값을 보고한다(Task 2에서 실측).
고조파에서 1.4 kHz 떨어진 밴드를 재면 이 의존성이 사라진다.

## 2. 기준선 수치 (현재 단일 품질 경로)

Release, Apple M1 Ultra, macOS 15.7.9. 실행 파일 SHA-256:
`9939517d161c83c5b81eb3a0a1bf8f85af7e13ec01ec44f1eb46cd5d9e8488fa`.

| 설정 | block 64 | block 127 | block 256 | median |
| --- | --- | --- | --- | --- |
| `saturation = 0.15` (제품 기본) | -133.48 dBc | -133.48 dBc | -133.48 dBc | **-133.48 dBc** |
| `saturation = 1.00` (최대 drive) | -108.92 dBc | -108.91 dBc | -108.92 dBc | **-108.92 dBc** |
| `saturation = 0.00` (비선형 우회) | — | -151.42 dBc | — | 계측기 바닥 |

파생 수치:

- drive 민감도: **+24.56 dB** (기본 -> 최대). 계측기가 비선형 깊이를 확실히 분해한다.
- 계측기 바닥 대비 여유: 기본 경로 **+17.94 dB**, 최대 drive **+42.50 dB**.
  즉 기본값의 -133.48 dBc는 수치 잡음이 아니라 실제 접힘 에너지다.
- 블록 크기 의존성: 세 블록 모두 0.01 dB 이내. 순서 비교에 충분히 안정적이다.
- 피치 정확도: 기대 10002.1767 Hz, 실측 10002.1625 Hz -> **-0.0025 cents**.
  (Debug/Release 동일, 0.1 cent 게이트 통과.)

## 3. 이 계측기가 보증하는 것 / 보증하지 않는 것

보증:

- 렌더러가 파라미터 격자 위에서 요청 피치에 0.1 cent 이내로 도달한다.
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
4. 부수 발견: JUCE `AudioParameterFloat` 5-인자 생성자의 0.01 격자는 이 플러그인의
   모든 float 파라미터에 적용된다. `delayTime`(0.03..1.5 s)은 **10 ms 단위**,
   `ampAttack/ampDecay/filterAttack/filterDecay/ampRelease/filterRelease`(최소 0.001 s)는
   실효 최소값이 0.01 s, `osc1Tune`은 1 cent 단위가 된다. 음질 계획과 별개로
   판단이 필요한 항목으로 기록한다(범위를 바꾸지 않고 `NormalisableRange`를 명시하면 해결).

## 5. 재현

```sh
cmake -S . -B Build-Release
cmake --build Build-Release --target QualityOrderTests -j8
ctest --test-dir Build-Release -R QualityOrder --output-on-failure
# 또는 수치 표를 직접 보려면:
Build-Release/QualityOrderTests_artefacts/Release/QualityOrderTests
```

Golden은 승격하지 않는다. 이 계측기는 절대 게이트와 before/after 비교를 위한 것이다.
