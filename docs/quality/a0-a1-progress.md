# SEOUL DSP Plan A0+A1 진행 기록

기준일: 2026-09-02  
기준 커밋: `161b5db` (`chore: establish SEOUL DSP quality baseline`)

## 증거 경계

- 빌드 성공, CTest, pluginval, auval, 호스트 삽입, UI 렌더링, 실청취는 서로 다른 증거 등급으로 기록한다.
- Golden 파일은 전후 측정값과 청취 판단 없이 갱신하지 않는다.
- 오디오 콜백 경로에는 할당, 락, 파일 I/O, 전역 난수, GUI 호출을 추가하지 않는다.

## 실행 체크리스트

- [x] Task 1 — 복구 가능한 기준점 생성
- [x] Task 2 — 오디오 품질 지원 라이브러리와 테스트 타깃
- [x] Task 3 — 결정적 프로세서 렌더러와 fixture manifest
- [x] Task 4 — Golden 비교와 감사 가능한 품질 보고서
- [x] Task 5 — 알려진 A1 결함 RED 테스트 고정
- [x] Task 6 — 정규화된 voice PRNG
- [x] Task 7 — 모든 활성 smoother 초기화/advance
- [x] Task 8 — 세 오실레이터 대칭 고정-capacity unison
- [x] Task 9 — 필터 엔벨로프 octave-domain 변조
- [x] Task 10 — smoothed equal-power delay dry/wet
- [x] Task 11 — 상태 스키마 버전과 마이그레이션
- [ ] Task 12 — A0+A1 전체 게이트와 acceptance 문서

## 기준선 확인

명령:

```sh
ctest --test-dir Build -C Debug --output-on-failure
```

결과: 기존 테스트 2/2 통과 (`WavetableDSP`, `ProcessorSmoke`).

참고: `.superpowers/`는 기존 로컬 작업 메타데이터이며 기준 커밋에는 포함하지 않았다.

## Task 2 검증

```sh
cmake -S . -B Build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64'
cmake --build Build --target AudioQualityMetricsTests ProcessorQualityTests -j 4
ctest --test-dir Build -R 'AudioQualityMetrics|ProcessorQuality' --output-on-failure
```

결과: 2/2 통과. 메트릭 실행 파일과 프로세서 생성 smoke가 GREEN이다.

## Task 3 검증

```sh
cmake --build Build --target OfflineRendererTests ProcessorQualityTests -j 4
ctest --test-dir Build -R 'OfflineRenderer|ProcessorQuality' --output-on-failure
```

결과: 2/2 통과. 동일 seed 렌더는 sample-identical, 다른 seed + randomPhase는 onset이 달라지며, MIDI offset과 irregular final block을 확인했다.

## Task 4 검증

```sh
ctest --test-dir Build -R 'GoldenComparator|AudioQualityMetrics|OfflineRenderer' --output-on-failure
Build/AudioQualityRunner_artefacts/Debug/AudioQualityRunner \
  --manifest Tests/AudioQuality/fixture-manifest.json \
  --output-dir Build/quality-baseline-recheck --fixture silence --matrix per-build
```

결과: CTest 3/3 통과. Runner는 등록된 Golden이 없음을 `FAIL (golden missing)`으로 보고하고, `-inf` 메트릭을 문자열과 `metricState: silence`로 유효한 JSON에 기록했다. 진단 오디오는 `Build/quality-baseline` 아래에만 보존했다.

## Task 5 RED 검증

```sh
Build/ProcessorQualityTests_artefacts/Debug/ProcessorQualityTests \
  2>&1 | tee Build/quality-baseline/a1-red.txt
```

결과: 의도된 non-zero. 현재 결함으로 필터 엔벨로프 +/−4 octave와 delay mix 1/0.5 equal-power 검사가 실패하며, 이 로그를 수리 전 증거로 보존했다.

## Task 6 검증

`RealtimeRandom`을 voice별 고정 상태로 도입하고 phase 입력을 `[0, 1)` 정규화 도메인으로 유지했다. ProcessorSmoke는 스택 안전성 보강 후 통과했고, phase RNG 단위 테스트는 정상이며, 미수리 엔벨로프/delay RED는 계속 남겨 두었다.

## Task 7 검증

모든 활성 voice smoother를 host 값으로 초기화하고 sample loop에서 한 번씩 소비했다. wavetable 위치도 LFO가 꺼진 경로를 포함해 샘플별 smoother 값을 사용한다. `WavetableDSP`와 `ProcessorSmoke`는 통과했으며, Task 5의 엔벨로프/delay RED만 의도적으로 남아 있다.

## Task 8–10 검증

`OscillatorUnisonBank`를 세 오실레이터에 적용하고, 1/2/4/8 lane 대칭 detune·팬·`1/sqrt(count)` 정규화를 고정했다. `osc1Detune`/`osc2Detune`/`osc3Detune`/`unisonKeyTrack` 파라미터와 UI attachment를 추가했다. 필터 엔벨로프는 ±4 octave 지수 곡선과 Nyquist-safe clamp를 사용하고, master effect는 준비된 smoother와 equal-power delay 법칙을 사용한다. `UnisonBank`, `ProcessorQuality`, `ProcessorSmoke`, `OfflineRenderer`가 모두 통과했다.

## Task 11 검증

루트 XML에 `stateSchemaVersion=2`를 기록하고, 버전 1 또는 속성 누락 상태를 읽을 때 새 detune/key-track 기본값을 채운다. wavetable payload v1과 MIDI mapping은 기존 방식으로 보존한다. ProcessorSmoke에서 schema 2, legacy migration, wavetable/parameter/MIDI round-trip을 모두 통과했다.
