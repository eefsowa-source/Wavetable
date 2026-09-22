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

## 6. 이 문서가 보증하지 않는 것

* 실청취(A/B). 기울기 수치와 소리 인상은 별개다.
* 호스트 로드, pluginval/auval. 별도 게이트다.
* 기존 세션의 톤 보존. index 0은 이전의 "12 dB/oct"에서 6 dB/oct로, index 2는
  "24 dB/oct"에서 18 dB/oct로 의미가 바뀐다. 기본값 index 3(24 dB/oct)의 기울기는
  이전과 같지만 섹션 구현이 JUCE SVF -> eon TPT로 교체되어 샘플 값은 달라진다.
* resonance 범위 확장(4절).
