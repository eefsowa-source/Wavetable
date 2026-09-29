# Plan C UX-2 — output level meter and clip indicator

측정일: 2026-09-29
상태: **구현 + 렌더/단위 게이트 통과** (호스트/청취 게이트는 미완료)

## 1. 배경

SQ-1에서 출력 실링이 0 dBFS 초과를 막았지만, 사용자에게 그 사실이 보이지 않았다.
OutputSafety에는 이미 sticky `clipActive()`가 있었고 에디터는 그것을 쓰지 않았다.
UX-2는 레벨과 클립을 화면에 노출하고, 그 과정에서 클립 플래그를 실제로 소비한다.

## 2. 설계

오디오 스레드는 atomic만 게시하고, 에디터는 그것을 폴링한다.

* `OutputSafety`: `peakLevel()`을 추가했다. 매 버퍼 처리 후 마지막 버퍼의 최대
  |sample|(실링 적용 후)을 `std::atomic<float>`에 저장한다. `reset()`은 0으로 지운다.
  기존 sticky `clipActive()`/`clearClipFlag()`는 그대로다.
* `HybridWavetableAudioProcessor`: `outputPeakLevel()`, `outputClipActive()`,
  `clearOutputClip()` 공개 접근자를 추가했다.
* `OutputMeterComponent`([Source/PluginEditor.h](../../Source/PluginEditor.h)):
  `juce::Component` + `SettableTooltipClient` + `juce::Timer`. 30 Hz로 폴링해
  빠른 어택/느린 릴리스(초당 1.5 dB)로 막대를 그리고, 클립 시 LED를 점등한다.
  클릭하면 `clearOutputClip()`을 호출한다. 값이나 클립 상태가 바뀔 때만 repaint한다.
* 배치: 스켈레톤/밀도와 무관하게 상단 고정 위치(292, 30, 132×rowHeight)에 둔다.
  레이아웃 6종을 건드리지 않아 회귀 위험이 낮다.

## 3. RT 안전

* 오디오 콜백은 `peakValue.store` / `clipActiveFlag.store`만 한다. lock·할당 없음.
* 에디터 타이머는 atomic load와 repaint만 한다. 컴포넌트 생성 외 할당 없음.
* 실링/미터 모두 상태를 추가하지 않는다(피크는 per-buffer 값).

## 4. 검증

### 단위/스모크

* `OutputSafetyTests`: 아래 임계 신호에서 `peakLevel()`이 처리된 버퍼의
  `getMagnitude`와 일치(< 1e-6), 상한 신호에서 0.99 < peak < 1.0.
* `ProcessorSmokeTests`: 무음 블록에서 `outputPeakLevel() < 1e-6`이고
  `outputClipActive() == false`, `clearOutputClip()`이 안전.
* 전체: Release **12/12 PASS**, Debug **12/12 PASS**.

### 렌더 (UiSnapshot)

`SeoulDSP_UISnapshot --all Build/quality-ui/types-ux2 1120 760`: **90개 PNG, 90개
고유 md5**(스냅샷 결정성 유지). 픽셀 프로브로 컴포넌트 배치를 확인했다. 좌표는
미터 사각형(292,30,132,rowHeight) 기준이다.

| 위치 | 의미 | 측정 RGB | 설계값 |
| --- | --- | --- | --- |
| (300, 32) | 패널 배경 | (14,17,22) | 0x0e1116 |
| (350, 44) | 빈 레벨 바 | (27,33,41) | 0x1b2129 |
| (405, 44) | 꺼진 CLIP LED | (35,42,51) | 0x232a33 |

타입 0(스킨), 31(레이아웃), 65(큐레이션), 89에서 네 좌표가 모두 동일했다. 즉 미터는
프레젠테이션 타입과 무관하게 같은 자리에 같은 색으로 그려진다.

## 5. 보증과 한계

보증:

* 레벨 바와 클립 LED가 모든 UI 타입에서 동일하게 렌더된다.
* 클립 플래그가 이제 UI에서 소비되고 클릭으로 해제된다.

보증하지 않음:

* **true peak / LUFS.** 미터는 샘플 피크를 보여준다. inter-sample peak나 라우드니스는
  별도 계측이 필요하다(SQ-1 한계와 동일).
* **폴링 사이의 초단기 피크.** `peakLevel()`은 마지막 버퍼 값이라 30 Hz 폴링 사이의
  아주 짧은 피크는 놓칠 수 있다. 실링이 걸린 사실 자체는 sticky 클립 플래그가 남기므로
  클립 표시는 놓치지 않는다.
* **리사이즈 경계.** 고정 좌표라 창을 최소 폭(1040)까지 줄여도 겹치지 않지만, 향후
  레이아웃에 미터를 통합하려면 `resized()`의 스켈레톤 경로에 넣어야 한다.
* **호스트/청취.** 실제 DAW에서의 미터 동작과 가독성은 별도 게이트다.
