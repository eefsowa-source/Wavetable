# Hybrid Wavetable

- `README.md`의 현재 프로토타입 범위와 빌드·검사 명령을 따른다. 구현은 `Source/`, DSP·프로세서 검사는 `Tests/`, 측정 도구는 `Tools/`에 있다.
- 변경한 DSP/상태 복원 경로는 CTest로 확인한다. AU 검사는 README의 `auval` 명령으로 별도 수행한다.
- 빌드는 VST3/AU를 사용자 설치 위치에 복사할 수 있다. 설치 바이너리 해시, 실제 DAW 로드, UI 및 청취 결과를 구분한다.
