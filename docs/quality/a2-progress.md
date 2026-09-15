# SEOUL DSP Plan A2 signal-quality progress

검증일: 2026-09-06  
상태: **IN PROGRESS / NOT ACCEPTED**

이 기록은 A0+A1 이후의 현재 작업 트리를 대상으로 한다. 새 Golden과 사람의
실청취 승인은 아직 없으므로 A2 acceptance나 음질 최종 통과를 주장하지 않는다.

## 이번 개선

1. 웨이브테이블 mip 경계를 하드 스위치 대신 마지막 `0.35 octave` 구간의
   smoothstep 크로스페이드로 바꿨다. 프레임 보간과 mip 보간은 같은 샘플 경로에서
   수행되며 오디오 콜백에서 할당하지 않는다. 가장 안전한 mip는 fundamental만
   유지하고, fundamental 자체가 Nyquist 이상이면 해당 oscillator 출력을 0으로 만든다.
2. 오디오 임포트를 cyclic 16-lobe windowed-sinc resampling으로 바꿨다. 각 프레임은
   DC를 제거하고 첫 프레임과 2048-point FFT cyclic correlation으로 위상을 정렬한다. 기본값은
   프레임별 정규화를 하지 않고 전체 bank가 `0.98`을 넘을 때만 한 번 감쇠하므로
   프레임 사이의 의도된 레벨 변화가 유지된다.
3. 이펙트 뒤 최종 출력에 allocation-free 10 Hz DC blocker를 추가했다. 이 단계는
   리미터나 숨은 loudness 보정이 아니다. 호스트 transport reset에서도 필터 상태를
   명시적으로 초기화한다.
4. 기본 1120x760 UI에서 잘리던 Detune/Unison 표기를 짧은 패널 라벨과 전체 단위
   툴팁으로 나눴다.

## RED -> GREEN 증거

| 회귀 | RED | GREEN 기준 |
| --- | --- | --- |
| mip 경계 | 기존 하드 전환에서 `WavetableDSP` 실패 | 경계 양쪽 동일 위상 sample jump `< 0.01` |
| 임포트 | 기존 nearest sampling에서 DC/phase 테스트 실패 | DC `< 1e-5`, legal partial RMS error `< 1e-4`, 정규화된 phase error `< 0.01`, 프레임 레벨비 오차 `< 0.1 dB` |
| 출력 DC | 상수 테이블의 실제 processor render가 실패 | 후반 DC `< -80 dBFS`, 1 kHz gain error `< 0.01 dB` |

추가 회귀는 20 Hz부터 Nyquist 직전까지 선택되는 모든 mip의 harmonic cap이 안전한지,
Nyquist를 넘은 fundamental이 무음인지, 20~389차 partial이 섞인 프레임의 1018-sample
cyclic shift를 정확히 복구하는지 검증한다.

## 현재 검증

- Universal Release `all` build: PASS (`arm64;x86_64`).
- Release CTest: PASS, `9/9`.
- Release VST3 pluginval 1.0.4 strictness 10: `SUCCESS`, exit 0,
  JUCE assertion 0. 로그:
  `Build/quality-ui/host-validation/pluginval-ui90-release-strictness10.log`.
- VST3 binary SHA-256:
  `e6a11a2f2081be36d65932070b138752139ee1586b1378599f1e1011ffa09c5b`.
  Release 빌드와 설치본이 일치한다.
- AU binary SHA-256:
  `8e1b6c3662c3c471fc0c7e790ba14aecd111d4cca8ca040716ccd193e2c64a36`.
  Release 빌드와 설치본이 일치한다.
- AU `auval -v aumu Hwbl Eona`: `AU VALIDATION SUCCEEDED`, exit 0.
  이전 FAIL 기록은 subtype과 manufacturer를 뒤집어
  `aumu Eona Hwbl`로 호출한 검증 명령 오류였다.
- REAPER 7.79: 현재 Release AU를 instrument/enabled 상태로 삽입, host
  parameter 58개 확인. 48 kHz/2.5 s/24-bit stereo 렌더는 finite이며,
  현재 Release 오프라인 fixture보다 3 samples 선행한다. 정렬 후 RMS와 최대
  오차는 모두 `-138.47 dBFS`(24-bit 1 LSB)다.
- Ableton Live 12 Suite: 현재 Release VST3 검색 및 빈 MIDI 트랙 삽입 PASS
  (device active, 48 kHz). 플러그인 창은 Live에서 open 상태지만 다른 화면/창
  레이어에 있어 내부 UI 컨트롤과 세션 복원은 아직 직접 확인하지 못했다.
- UI snapshot: [`Build/quality-ui/seoul-dsp-a2-default.png`](../../Build/quality-ui/seoul-dsp-a2-default.png),
  SHA-256 `6e2152a9a576c74fc700e71070785c25bfc7618e7eef8e7d1337a489f32f1fec`.
- Release fixture per-build render: 66개, non-finite `0`.
  [`Build/quality-a2-final-current/report.json`](../../Build/quality-a2-final-current/report.json),
  SHA-256 `8076a93f2ef525a9abc36dcf8f568ff4a4c5eaf5c52d6d03cf35a0b82d413221`.

## 제한과 다음 게이트

- 위 66개 보고서는 모두 `golden missing`이므로 aggregate `passed=false`다.
- 현재 manifest runner는 fixture의 `preset` 이름이나 subsystem별 parameter state를
  실제 processor에 적용하지 않는다. 따라서 fixture 이름만으로 rich saw, automation,
  delay-only 등의 분리 검증이 되었다고 해석하면 안 된다. A2 isolation fixture contract와
  runner configuration 적용을 먼저 보강해야 한다.
- Ableton 내부 UI 타입 전환/세션 복원, REAPER VST3 경로, 다섯 critical
  excerpt 실청취는 아직 수행하지 않았다.
- Golden은 runner의 fixture 적용을 고친 뒤 objective before/after와 사람의 청취 결과를
  함께 검토해야만 승격한다.
