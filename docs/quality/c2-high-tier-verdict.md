# Plan C SQ-2 — saturation tier verdict (High removed)

측정일: 2026-09-29
상태: **측정 + 제거 구현 + 자동 게이트 통과** (청취 게이트는 미완료)

## 1. 질문

saturationQuality에는 Eco(2x, legacy curve), Normal(2x, ADAA1), High(4x, ADAA1) 세
등급이 있었다. 기존 계측은 48 kHz 10 kHz 프로브에서 세 등급 모두 계측 바닥에
있었고, 13 kHz 프로브는 2 dB 이내였다. 즉 High가 값을 하는지가 미결이었다.
dense16에서 High의 비용은 Normal 대비 약 +24 % CPU였다(cpu-optimization.md).

## 2. 방법

`QualityOrderTests`에 folded-third-harmonic 스윕을 추가했다. 프로브를 sr/6 위로
올려 3차 고조파가 Nyquist 바로 위에서 접히게 하고, 48 kHz와 96 kHz에서 등급별
접힘 에너지를 비교한다. 각 케이스마다 saturation을 우회한 floor도 측정해 밴드가
실제로 격리됐는지 확인한다(floor > -100 dBc면 케이스 제외).

## 3. 제거 전 측정 (48 kHz, max drive)

| probe | fold | floor | Eco | Normal | High | High-Normal |
| --- | --- | --- | --- | --- | --- | --- |
| 8600 Hz | 22200 Hz | -157.8 | -67.6 | -69.4 | -68.3 | **+1.10** |
| 9200 Hz | 20400 Hz | -155.3 | -98.2 | -100.1 | -98.9 | **+1.26** |
| 9800 Hz | 18600 Hz | -159.0 | -148.9 | -153.5 | -152.1 | **+1.41** |
| 10400 Hz | 16800 Hz | -154.4 | -152.3 | -153.6 | -154.0 | -0.33 |
| 11000 Hz | 15000 Hz | -151.0 | -146.8 | -148.2 | -147.9 | +0.31 |

단위 dBc. High-Normal이 양수면 High가 더 나쁘다는 뜻이다.

* 4x는 **어느 케이스에서도 Normal을 유의미하게 앞서지 못했다**. 최고 개선 +0.33 dB,
  최악 -1.41 dB. 다섯 케이스 중 넷에서 High가 더 나빴다.
* 8.6 kHz와 9.2 kHz 프로브에서는 접힘 에너지가 -67 ~ -100 dBc로 계측 바닥이 아니었다.
  즉 실제 접힘 구간이 존재하는데도 4x가 그것을 줄이지 못한다.
* 96 kHz 케이스는 전부 제외됐다(floor +2 ~ +26 dBc). 접힘 밴드가 cutoff 파라미터
  상한(20 kHz)보다 위에 있어 필터가 접힘 에너지를 감쇠하기 때문이다. 이 상한 자체가
  별도 항목이다(아래 6절).

## 4. 결정과 구현

High를 제거했다. 단, 기존 세션·프리셋이 index 2를 저장하고 있으므로 파라미터의
세 번째 선택지는 남기고 내부에서 Normal로 매핑한다.

* `Source/DSP/SynthVoice.cpp`: `requestedQuality >= 2 ? 1 : requestedQuality`.
* `Source/PluginProcessor.cpp`: 선택지 이름을 `High (deprecated)`로 변경.
* `SaturationStage`의 4x 구현과 `Quality::high` 열거형은 남겨 두었다. 파라미터에서는
  도달할 수 없지만, 계측 하네스가 직접 호출할 수 있고 제거 이력을 재측정할 수 있다.

## 5. CPU 회수 (같은 머신, Release, CpuBench, slope 3)

동일 워크트리에서 매핑 라인만 되돌려 4x 경로를 복원하고 측정했다.

* High 4x 경로 활성: solo-unison8 **109.72 ms**, dense16-unison8 **1694.09 ms**
* High가 Normal로 매핑된 뒤: solo-unison8 **90.17 ms**, dense16-unison8 **1396.85 ms**
* 회수: solo **-17.8 %**, dense16 **-17.5 %**. 음질은 동일하다(비트 동일 렌더).

## 6. 회귀 확인

* Release **12/12 PASS**, Debug **12/12 PASS**.
* `QualityOrderTests`의 등급 게이트는 이제 "세 번째 선택지가 Normal과 동일하게
  렌더된다"는 불변식을 검사한다. folded-alias 상한, 슬로프, DC, 유한성 게이트는 유지된다.

## 7. 보증과 한계

보증:

* 48 kHz에서 2x가 4x 이상이며, High 선택지는 이제 CPU만 Normal 수준으로 되돌린다.

보증하지 않음:

* **96 kHz 등급 판정.** cutoff 상한 20 kHz 때문에 96 kHz에서는 접힘 밴드를 격리할 수
  없었다. 이 상한을 올리면(예: 0.45·sr) 96 kHz 세션에서 필터를 완전히 열 수 있고
  측정도 가능해진다. 별도 후속 항목이다.
* **8.6~9.2 kHz 접힘 구간.** 세 등급 모두 -67 ~ -100 dBc의 접힘 에너지를 남긴다.
  등급 선택으로는 해결되지 않으며, half-band FIR 스톱밴드나 ADAA 차수를 손대야 하는
  별도 DSP 작업이다.
* **청감.** 등급 제거는 비트 동일 렌더로 확인했으므로 청감 변화가 없어야 하지만,
  최종 확인은 SQ-6 청취 게이트의 몫이다.
* 이 수치는 특정 커밋/바이너리에 묶인다.
