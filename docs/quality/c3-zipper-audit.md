# Plan C SQ-3 — parameter-step click audit

측정일: 2026-09-29
상태: **구현 + 자동 게이트 통과** (청취/호스트 게이트는 미완료)

## 1. 배경

cutoff, resonance, 오실레이터 레벨, saturation, filterDrive, wavetable 위치,
filterEnvAmount는 50 ms 스무더를 지난다. 반면 filterSlope와 filterType은 필터의
*구조*를 바꾸는 값이라 스무딩 대상이 아니고, 블록당 한 번 읽힌다. 프리셋 적용이나
호스트 오토메이션이 이 값을 스텝하면 파형에 불연속이 생길 수 있다.

## 2. 계측기

`ProcessorQualityTests`의 `renderHeldNote()`가 1초 홀드 노트를 렌더하고,
전환 시점(0.5 s)에 파라미터를 스텝한다. 판정은 전환 창(±256/+2048 샘플)에서
연속 샘플 간 최대 점프다. 기준선은 같은 창의 스텝 없는 렌더다.

`OfflineRenderer::render()`에 per-block 콜백(`onBlock`)을 추가했다. 블록 렌더
직전에 호출되므로 블록 사이에서 파라미터를 바꿀 수 있다.

## 3. RED (수정 전)

128 샘플 블록, 전환 블록 187.

* 기준선(스텝 없음): **0.00359**
* filterSlope 0 -> 3 스텝: **0.03785** (기준선의 **10.5배**) — 클릭
* filterType low-pass -> band-pass 스텝: **0.03239** (기준선의 **9.0배**) — 클릭
* osc1Unison 1 -> 8 스텝: **0.00828** (기준선의 **2.3배**) — 게이트 안

게이트는 `스텝 점프 < 4 x 기준선`으로 두었다. 슬로프와 타입이 실패했다.

## 4. 수정

`Source/DSP/SynthVoice.h/.cpp`에 구조 전환 크로스페이드를 추가했다.

* 슬로프나 타입이 바뀌면 실행 중인 `SlopeFilter`를 `previousFilterLeft/Right`로
  복사하고(상태 포함), 새 설정으로 계속 진행한다.
* 5 ms(샘플레이트 비례) 동안 `previous` 필터를 이전 슬로프/타입으로 계속 돌리며
  두 출력을 선형 블렌드한다. `t = 1 - remaining / length`.
* 크로스페이드 중에도 두 필터 모두 매 샘플 setParams를 받아 cutoff/resonance/drive
  변조를 공유한다.
* `startNote`에서 페이드와 activeSlope/activeType을 현재 파라미터로 동기화해, 이전
  노트의 잔여 페이드가 새 노트로 새지 않게 한다.

SlopeFilter는 POD 상태(svf, ADAA 노드, one-pole 배열, 스칼라)만 가진 값 타입이라
복사가 안전하다.

## 5. GREEN (수정 후)

* 기준선: **0.00359**
* filterSlope 스텝: **0.00359** — 기준선과 동일(10.5배 -> 1.0배)
* filterType 스텝: **0.00359** — 기준선과 동일(9.0배 -> 1.0배)
* osc1Unison 스텝: **0.00828** — 변화 없음(2.3배, 게이트 안)

## 6. CPU

CpuBench(Normal, slope 3), 같은 머신. 크로스페이드 분기는 정상 상태에서
not-taken이고, 페이드는 5 ms 동안만 돈다.

* solo-unison8: 88.96 -> **89.98 ms** (덮어쓴 ceiling 커밋 대비)
* dense16-unison8: 1380.34 -> **1371.46 ms**

두 수치 모두 실행 변동 범위이며 회귀가 아니다.

## 7. 회귀 확인

* Release: **12/12 PASS**, Debug: **12/12 PASS**.
* 슬로프 6/12/18/24 dB/oct 게이트, 0 dB 드라이브 선형성, 드라이브 THD, 리조넌스
  유계, alias, DC, 유한성 게이트가 그대로 통과한다(`renderSlopeTone`은 렌더 전에
  슬로프를 설정하므로 페이드를 타지 않는다).

## 8. 보증과 한계

보증:

* filterSlope/filterType 스텝이 정상 파형 기울기와 같은 수준으로 매끄러워진다.

보증하지 않음:

* **unison 카운트 전환은 여전히 2.3배 스텝**이다(=음성 수가 1 -> 8로 바뀌며 레벨이
  계단으로 오른다). 게이트 안이라 이번 범위에서 고치지 않았다. 유니슨 크로스페이드는
  뱅크 레이아웃 자체를 블렌드해야 하므로 별도 항목이다.
* **청감.** 4배 기준은 계기적 판정이다. 실제 클릭 유무는 레벨 매칭 청취(SQ-6)에서
  확인해야 한다.
* **호스트 오토메이션.** 블록 경계에서의 스텝을 측정했다. DAW 오토메이션 지연과
  버퍼 크기 변화는 별도 호스트 게이트다.
* 이 수치는 특정 커밋/바이너리에 묶인다.
