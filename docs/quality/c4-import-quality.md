# Plan C SQ-4 — import table quality audit

측정일: 2026-09-29
상태: **감사 + 결함 1건 수정 + 자동 게이트 통과**

## 1. 감사 범위

`WavetableImporter::import`와 `HybridWavetableAudioProcessor::loadAudioFile`을
DC 제거, 밴드리밋, 정규화/피크 정책, 채널 처리 관점에서 확인했다.

## 2. 감사 결과 (이미 갖춰진 것)

* **DC 제거**: `removeMean`이 프레임마다 실행되고 기본값이 켜짐(`removeDc = true`).
  테스트가 DC 바이어스가 있는 소스에서 평균 < 1e-5를 확인한다.
* **밴드리밋**: `resampleCyclic`이 16-lobe 윈도우 sinc로 리샘플하고, 다운샘플 시
  `cutoff = min(1, 1/speedRatio)`로 목표 나이퀴스트 위를 먼저 저역통과한다.
  업샘플(speedRatio < 1)에서는 cutoff = 1로 저역통과하지 않는다(올바름).
* **피크 정책**: 프레임별 정규화는 기본 꺼짐(상대 레벨 보존), 은행 전체 피크가
  `bankPeakCeiling = 0.98`을 넘으면 전체 게인으로 클램프한다. 테스트가 프레임 간
  상대 레벨을 0.1 dB 이내로 확인한다.
* **위상 정렬**: `alignCyclicPhase`가 FFT 상호상관으로 프레임 간 순환 위상을 맞춘다.
* **RT 안전**: 임포트는 메시지 스레드에서만 실행되므로 버퍼 할당이 허용된다.

## 3. 발견한 결함 (수정)

**다중 채널 소스가 채널 0으로 축소됐다.** 두 곳 모두였다.

1. `WavetableImporter::import`가 `resampleCyclic(source.getReadPointer(0, ...))`로
   채널 0만 읽었다.
2. `HybridWavetableAudioProcessor::loadAudioFile`가
   `r->read(..., /*useLeftChan*/ true, /*useRightChan*/ false)`로 파일에서 좌채널만
   읽었다.

결과적으로 스테레오 파일은 좌채널만 테이블이 되었고, 오른쪽으로 패닝된 소재는
임포트에서 조용히 사라졌다. 웨이브테이블은 한 주기를 담는 모노 데이터이므로
정답은 채널 평균이다.

수정:

* 임포터가 소스 채널 전체를 `1/numChannels` 게인으로 합산해 모노 버퍼를 만들고,
  그 버퍼에서 리샘플한다.
* 파일 로더가 우채널이 있으면 함께 읽어 두 채널을 0.5씩 평균한다.

## 4. 검증

RED -> GREEN 순서로 고정했다.

* RED: `WavetableImporterTests`에 L = 0.6·sin, R = 0.2·sin 소스를 넣고 결과가
  0.4·sin이어야 한다는 단언을 추가. 수정 전 실패(채널 0 = 0.6 기록).
* GREEN: 수정 후 통과.
* 파일 경로: `ProcessorSmokeTests`가 스테레오 WAV를 임시 파일로 써서
  `loadAudioFile`을 호출하고, 테이블 피크가 0.4(평균)임을 확인한다. 좌채널만 읽으면
  0.6이 되어 0.2 차이로 실패한다.
* 전체: Release **12/12 PASS**, Debug **12/12 PASS**.

## 5. 보증과 한계

보증:

* 다중 채널 소스가 채널 평균으로 임포트된다(임포터 직접 호출과 파일 로더 양쪽).

보증하지 않음:

* **임포트 alias 프록시.** 계획의 원래 게이트는 임포트 결과의 접힘 에너지를 네이티브
  테이블과 비교하는 것이었다. 이번에는 기존 테스트(합법 파셜 보존 정확도, 고조파
  위상 정렬)와 코드 감사로 밴드리밋 존재를 확인하는 데 그쳤다. 고고조파 소스를
  넣는 정량 프록시는 별도 후속 항목이다.
* **조용한 임포트의 레벨.** `normaliseEachFrame`이 기본 꺼짐이라 작은 소스는 작게
  남는다. 이는 상대 레벨 보존을 위한 의도된 동작이며, 사용자가 프레임별 정규화를
  켤 수 있다(옵션 노출 여부는 UX 항목).
* **청감/호스트.** 임포트 결과의 청감 확인과 실제 DAW 임포트는 별도 게이트다.
