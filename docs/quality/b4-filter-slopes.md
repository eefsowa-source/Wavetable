# Plan B Task 4 (part 1) — four real filter slopes

측정일: 2026-09-22  
대상: SEOUL DSP (`HybridWavetable`), Release `Build-Release`  
코드: `Source/DSP/SlopeFilter.h` (신설), `Source/DSP/SynthVoice.{h,cpp}`, `Tests/AudioQuality/ProcessorQualityTests.cpp`

## 1. 닫은 결함

`filterSlope`는 4개 선택지였지만 DSP에는 기울기가 두 개뿐이었다. `SynthVoice`가
`slope >= 2`일 때만 두 번째 12 dB 섹션을 켰기 때문이다. 그래서

| 항목 | 이전 상태 | 근거 |
| --- | --- | --- |
| `slope 0` vs `slope 1` | 렌더가 **bit-identical** (차이 0.00000000) | `ProcessorQuality` 실측 |
| `slope 2` vs `slope 3` | 렌더가 bit-identical | 같은 코드 경로 |
| 파라미터 라벨 | `12 / 12 / 24 / 24 dB/oct` | `PluginProcessor.cpp` |
| 에디터 라벨 | `8 / 12 / 18 / 24 dB/oct` | `PluginEditor.cpp` |
| 어느 쪽이 맞는가 | **둘 다 아님.** 실제 DSP는 12 또는 24 dB/oct 두 가지뿐 | 위 두 표 |

## 2. 새 구조

`SlopeFilter`는 eon_dsp의 TPT 프리미티브로 네 기울기를 만든다.

| index | 라벨 | 구조 |
| --- | --- | --- |
| 0 | 6 dB/oct | TPT 1-pole |
| 1 | 12 dB/oct | `eon::SvfTPT` 1단 |
| 2 | 18 dB/oct | `eon::SvfTPT` -> TPT 1-pole |
| 3 | 24 dB/oct | `eon::SvfTPT` -> `eon::SvfTPT` (Q = 1/sqrt2) |

resonance는 **첫 섹션만** 감쇠한다. 기울기를 올릴 때 피킹이 같이 커지지 않게 하려는
것이고, 두 번째 섹션을 Butterworth로 두는 이유도 같다. TPT 계수 `tan(pi*fc/fs)`는
샘플당 한 번만 계산해 모든 섹션이 공유한다(4-pole에서도 `tan` 1회).

밴드패스는 라벨의 폴 수를 유지하되 위·아래 스커트를 같은 수의 1-pole로 나눈다.
`bandpass`는 resonance를 쓰지 않는다: 감쇠할 "첫 섹션"이 없는 구조이기 때문이다.
LP/HP와 달리 BP는 이번 개정에서 레조넌스가 걸리지 않는다는 점을 명시해 둔다.

## 3. 실측 기울기 (Release, 렌더 경로)

방법: 60 Hz 코너의 로우패스에 두 톤(523.25 Hz, 1046.50 Hz)을 통과시켜 한 옥타브
감쇠량을 측정. 두 톤 모두 코너의 8.7배 이상 위라 점근값이 형성됐고, resonance는 최대
1.0으로 두어 감쇠항이 기울기를 휘게 하지 못하게 했다.

| index | 라벨 | 실측 | 오차 |
| --- | ---: | ---: | ---: |
| 0 | 6 dB/oct | 5.99 dB/oct | -0.01 |
| 1 | 12 dB/oct | 12.10 dB/oct | +0.10 |
| 2 | 18 dB/oct | 18.10 dB/oct | +0.10 |
| 3 | 24 dB/oct | 24.22 dB/oct | +0.22 |

게이트는 4개 인덱스가 서로 다른 오디오를 렌더하는지(이전에는 0/1, 2/3이 같았다),
그리고 각 실측값이 라벨의 ±1.5 dB 안인지를 함께 검사한다. 전 슬로프 x 전 타입
조합은 최대 resonance에서 유한하고 +18 dBFS 아래로 유계임을 검사한다.

## 4. 부수 발견 — resonance 범위가 사실상 무의미하다 (미수정)

이전 필터는 JUCE `StateVariableTPTFilter`였고, 그 `resonance` 인자는 내부적으로
`R2 = 1/resonance`로 들어가 damping이 된다. 즉 **호스트 파라미터가 곧 Q**이고, 범위가
0.1~1.0이라 최대 resonance에서도 Q = 1.0(= +1.2 dB 정도의 완만한 bump)에 그친다.
자기발진은 구조적으로 불가능하다.

이번 개정은 슬로프만 다룬다. 새 `SlopeFilter`도 같은 의미(첫 섹션 damping)를 유지해
기존 프리셋의 톤을 바꾸지 않았다. resonance를 "실제로 걸리는" 범위로 넓히는 일은
모든 프리셋의 소리를 바꾸므로 별도 결정으로 남긴다.

## 5. CPU (Release, `CpuBench 1 <slope>`, 3회 중앙값)

| slope | solo-unison8 | dense16-unison8 | realtime factor |
| ---: | ---: | ---: | ---: |
| 0 (6 dB/oct) | 77.6 ms | 1236.8 ms | 1.62x |
| 1 (12 dB/oct) | 78.5 ms | 1214.5 ms | 1.65x |
| 3 (24 dB/oct) | 79.2 ms | 1231.2 ms | 1.62x |

같은 조건의 직전 기록(Task 3b Normal: solo 85.8 ms, dense16 1310.5 ms)보다 **낮다**.
JUCE SVF 두 개가 각자 `tan`을 계산하던 경로를, 계수 1회 + eon 섹션들로 바꾼 결과다.
다만 별도 실행 회차이므로 수 ms 수준 차이는 실행 변동으로 취급한다.

## 6. Task 4 (part 2) — filterDrive 비선형화

이전 `filterDrive`는 `Decibels::decibelsToGain` 선형 전치증폭이었고 포화는 필터 **뒤**
(`saturation` 스테이지)에 있었다. 즉 노브는 레벨 컨트롤이었고, "드라이브가 걸린 필터"가
아니었다. 이제 `SlopeFilter`가 드라이브를 소유하고, 캐스케이드에 신호가 들어가는 노드를
소프트클립한다.

| 항목 | 값 |
| --- | --- |
| 클리퍼 | `tanh(k v) / k`, k = 0에서 정확히 항등 |
| k 매핑 | `k = 7 * clamp(driveDb / 24, 0, 1)` |
| 메이크업 | `(k*0.6) / tanh(k*0.6)` — 0.6 진폭에서 unity |
| 0 dB 이하 | 클리퍼 off, 순수 게인(감쇠 구간은 예전과 동일) |
| 클리퍼 off 임계 | depth < 1e-3 (아래 버그 항목) |
| alias 대책 | `eon::ADAA1` + `tanhF1` (Task 3과 같은 구성) |

실측 (`ProcessorQuality`):

| 설정 | THD | peak |
| --- | ---: | ---: |
| `filterDrive = 0 dB` | **0.0035 %** (float 잔차) | — |
| `filterDrive = +24 dB` | **13.37 %** | 0.422 |

0 dB 렌더는 `eon::SvfTPT` 선형 섹션과 최대 차이 `5.96e-8`(해당 진폭에서 float 1 ulp)로
일치한다. 즉 "아주 약한 웨이브셰이퍼"가 아니라 **정확히 선형 필터**다.

### 6.1 작업 중 발견한 버그 — float 잔차가 클리퍼를 켜둔 채 두었다

`filterDrive` 파라미터를 0 dB로 두어도 `juce::SmoothedValue<float>`가 **3.576e-7 dB**를
반환했다. 수학적으로는 무시할 값이지만 클리퍼는 켜졌고, 결과는 광대역 잡음이었다:
드라이브 노드에서는 `w = k*v ~ 1e-7`이고 tanh의 `F1 = ln(cosh w) = -ln2 + w²/2`이므로
ADAA의 차분이 상수 `ln2`에 대해 상쇄되어 반올림 잔차만 남는데, `1/k` 메이크업이 그것을
1e7배 증폭했다. 관측 증상은 슬로프 게이트가 12.10 → **-3.63 dB/oct**로 무너지는 것이었고,
이는 별개의 회귀처럼 보였다.

수정: `minimumActiveDriveDepth = 1e-3` 미만은 **정확히 off**. 1e-3 depth는 클리핑 효과
기준 약 -60 dB라 "꺼짐"으로 부르는 것이 정직하다. 게이트는 이제 0 dB 드라이브가 선형
필터와 일치함을 직접 검사한다.

### 6.2 실행 중 변경 — 클리퍼 위치

계획은 포화를 "필터 피드백 루프 안"으로 옮기는 것이었다. 이번 개정은 캐스케이드 입력
노드(아날로그 필터의 입력단이 포화하는 지점)에 두었다. 이유는 측정이 아니라 회로 구조다:
로우패스에서 SVF의 high-pass 노드(두 적분기를 먹이는 지점)는 컷오프 아래 성분만 나르므로,
컷오프 20 kHz에서는 입력의 극히 일부만 통과한다. 그 노드에 드라이브를 걸면 노브가
사실상 0 dB 효과가 되고 컷오프가 신호보다 낮을 때만 작동한다. 그것은 회로의 실제
거동이지 드라이브 컨트롤이 아니므로, 공진 노드 자체의 포화(비선형 SVF 해)는 다음
단계로 남긴다.

## 7. 이 문서가 보증하지 않는 것

* 실청취(A/B). 기울기 수치와 소리 인상은 별개다.
* 호스트 로드, pluginval/auval. 별도 게이트다.
* 기존 세션의 톤 보존. index 0은 이전의 "12 dB/oct"에서 6 dB/oct로, index 2는
  "24 dB/oct"에서 18 dB/oct로 의미가 바뀐다. 기본값 index 3(24 dB/oct)의 기울기는
  이전과 같지만 섹션 구현이 JUCE SVF -> eon TPT로 교체되어 샘플 값은 달라진다.
* resonance 범위 확장(4절).
* 드라이브 클리퍼의 alias 단독 측정. 구성(ADAA1 + `tanhF1`)은 Task 3에서 측정된 것과
  같지만 이 노드 자체의 folded-alias 수치는 재지 않았다.
* IMD, 실청취, 호스트 로드. 별도 게이트다.
