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
- [ ] Task 3 — 결정적 프로세서 렌더러와 fixture manifest
- [ ] Task 4 — Golden 비교와 감사 가능한 품질 보고서
- [ ] Task 5 — 알려진 A1 결함 RED 테스트 고정
- [ ] Task 6 — 정규화된 voice PRNG
- [ ] Task 7 — 모든 활성 smoother 초기화/advance
- [ ] Task 8 — 세 오실레이터 대칭 고정-capacity unison
- [ ] Task 9 — 필터 엔벨로프 octave-domain 변조
- [ ] Task 10 — smoothed equal-power delay dry/wet
- [ ] Task 11 — 상태 스키마 버전과 마이그레이션
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
