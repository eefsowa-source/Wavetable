# Plan C SQ-1 — output headroom and safety ceiling

측정일: 2026-09-29
상태: **구현 + 자동 게이트 통과** (호스트/청취 게이트는 미완료)

## 1. 문제

오실레이터 합산 이후 정규화나 리미터가 없어서, 합법 범위 안의 패치가 0 dBFS를
넘겼다. 문서화된 관측은 dense_chord_16의 +4.40 dBFS였고, 3개 오실레이터의 유니슨
뱅크까지 최대(level 1.0, unison 8, spread 1.0)로 올린 16음 코드는 훨씬 크다.

* RED (수정 전): 최악 케이스 코드 peak **+13.01 dBFS**.
  재현: ProcessorQualityTests의 renderWorstCaseChord() (16음, output 기본 -6 dB).

## 2. 수정

Source/DSP/OutputSafety.h / .cpp에 DC 블로커 뒤에 정적 소프트 실링을 추가했다.

* threshold 0.9 (약 -0.92 dBFS) 이하: 통과 신호를 변형하지 않는다.
* threshold 초과: T + k * tanh((|x| - T) / k), k = ceilingMaximum - T.
  점근값 ceilingMaximum = 0.999 (-0.009 dBFS)이라 어떤 입력에서도 full scale에
  도달하지 않는다. float에서 tanh가 1.0으로 포화되는 것을 감안해 1.0이 아니라
  0.999를 점근값으로 쓴다(초기 구현이 1.0에 정확히 도달해 테스트가 실패했다).
* 상태가 없다(샘플별 기억 없음). look-ahead나 attack/release가 없으므로 펌핑이
  없고, 임계 이하에서는 완전히 투명하다.
* clipActive() / clearClipFlag(): 실링이 실제로 제한한 적이 있는지 sticky
  플래그로 노출한다(오디오 스레드는 set만, 에디터가 폴링). UX-2 클립 표시등의 근거.

## 3. 측정

Release, Apple Silicon, macOS. 테스트는 ProcessorQualityTests와
OutputSafetyTests에 들어 있다.

* 최악 케이스 코드 peak: **+13.01 dBFS -> -0.009 dBFS** (true peak도 -0.009 dBTP).
* 실링 투명성: 진폭 0.5 톤에서 이득 오차 < 0.02 dB, 클립 플래그 미설정.
* 실링 상한: 진폭 8.0 톤에서 peak 0.999, 클립 플래그 설정, clear 후 해제.

### CPU A/B (같은 머신, Release, CpuBench, Normal, slope 3)

동일 워크트리에서 OutputSafety만 HEAD 버전으로 되돌려 두 번 측정했다.

* before (실링 없음): solo-unison8 **88.99 ms**, dense16-unison8 **1367.31 ms**
* after (실링 포함): solo-unison8 **88.96 ms**, dense16-unison8 **1380.34 ms**
* 차이: solo -0.03 %, dense16 +0.95 % - 실행 변동 범위. 게이트(<=5 %) 안.

dense16은 saturation 최대라 실링이 자주 걸리는데도 비용이 측정 노이즈 수준인
이유는 임계 초과 샘플에만 tanh가 들어가고, 초과 판정이 분기 하나이기 때문이다.

## 4. 회귀 확인

* ctest --test-dir Build-Release: **12/12 PASS**
  (AudioQualityMetrics, ProcessorQuality, QualityOrder, OfflineRenderer,
  GoldenComparator, UnisonBank, WavetableDSP, WavetableImporter, OutputSafety,
  ProcessorSmoke, EonDspContract, AnalogColorCandidate).
* 기존 alias(folded), DC(-80 dBFS), 유한성, 슬로프, 드라이브 THD, 리조넌스 유계
  게이트가 그대로 통과한다. QualityOrder의 folded-alias 상한 게이트도 유지된다.

## 5. 보증과 한계

보증:

* 합법 범위 안의 패치가 output 기본값 또는 그 이하에서 0 dBFS를 넘지 않는다.
* 임계(-0.92 dBFS) 이하 신호는 실링 도입 전과 동일한 알고리즘 경로를 지난다
  (실링 분기에 진입하지 않는다).

보증하지 않음:

* **true peak / inter-sample peak.** 샘플 peak만 0 dBFS 이하를 보장한다. 0.999
  상한이라 inter-sample overshoot 여지가 작지만, DAC 기준 true-peak 보장은
  별도 oversampled true-peak 미터가 필요하다.
* **청감.** 실링이 걸린 큰 코드의 왜곡 특성은 레벨 매칭 청취 게이트에서 확인해야
  한다. 샘플 peak가 -0.009 dBFS까지 붙으므로 큰 패치는 이전보다 조용하고 밀도가
  달라진다. 의도된 동작이지만 청취로 확정할 항목이다.
* **호스트/DAW.** pluginval, auval, REAPER/Ableton 삽입 및 드롭아웃은 별도 게이트다.
* 이 수치는 특정 커밋/바이너리에 묶인다. DSP가 바뀌면 다시 측정한다.

## 6. 다음

* UX-2: 에디터에 clipActive() 기반 클립 표시등을 붙인다.
* SQ-6: 큰 코드/유니슨 시나리오를 레벨 매칭 블라인드 청취에 추가한다.
* 대안 후보(보이스 수 비례 헤드룸)는 채택하지 않았다. 실링만으로 0 dBFS 보장과
  투명성을 동시에 얻었고, 보이스 수 정규화는 기존 패치의 레벨과 음색을 더 크게
  바꾸기 때문이다.
