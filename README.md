# SEOUL DSP

JUCE 8.0.14 기반 macOS 하이브리드 웨이브테이블 신시사이저입니다. 제품 이름은 **SEOUL DSP**, CMake 타깃은 `HybridWavetable`, 제조사 코드는 `Eona`, 플러그인 코드는 `Hwbl`입니다.

프로토타입입니다. 설치 패키지는 없습니다.

## 형식

- VST3
- Audio Unit (AU) — macOS 전용
- Standalone
- Universal Binary (`arm64` + `x86_64`)
- 최소 macOS 12.0

## 빌드

```sh
cmake -B Build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64'
cmake --build Build --config Debug -j2
```

첫 configure가 JUCE 8.0.14를 받습니다. `COPY_PLUGIN_AFTER_BUILD`가 켜져 있으면 AU와 VST3가 `~/Library/Audio/Plug-Ins/`에 복사됩니다.

macOS가 아닌 환경에서는 AU를 빼고 VST3와 Standalone만 만듭니다. 그 경로는 CI용이고, 배포 바이너리는 macOS 빌드입니다.

## 테스트

```sh
ctest --test-dir Build -C Debug --output-on-failure
auval -v aumu Hwbl Eona
```

DSP 테스트는 기본 테이블 생성, 포지션 모핑, 오디오 리샘플, 빈 입력, 유한한 오실레이터 출력을 봅니다. 프로세서 스모크 테스트는 MIDI Learn과 프리셋 왕복, 128보이스 렌더/릴리스를 봅니다. `auval`은 AU 수명, 파라미터, MIDI, 포맷, 렌더 경로를 검사하며 CI가 아니라 로컬 macOS에서 돌립니다.

품질 측정 러너와 golden 규칙은 [docs/quality/README.md](docs/quality/README.md)에 있습니다.

## 조작

에디터에는 독립된 웨이브테이블 포지션 세 개, 필터 타입/슬로프, 컷오프, 레조넌스, 필터 드라이브, 새촬레이션, 앰프/필터 ADSR, 오디오 임포트, 프리셋 저장/불러오기, 팩토리 프리셋 메뉴가 있습니다. 팩토리 프리셋은 열 개의 결정적 생성 사운드이며, 고르면 오실레이터, 필터, 드라이브, 새촬레이션, 출력, 엔벨로프가 바로 적용됩니다. 파형 영역을 드래그하면 그리고, Shift-드래그는 하모닉을 고칩니다.

## 프로토타입 한계

- 오디오 임포트는 JUCE 플랫폼 포맷을 씁니다. macOS에서는 WAV, AIFF, M4A/CoreAudio 등 CoreAudio가 읽는 포맷입니다. 선택기는 모든 파일을 받고, 디코더가 읽을 수 있는지 결정합니다. 임포트 샘플은 테이블의 2048 리샘플 위치에서 읽으므로, 원본 파일 크기 전체가 메모리에 올라가지 않습니다.
- 프리셋 파일은 APVTS 파라미터, 편집된 웨이브테이블, MIDI CC 할당을 저장합니다.
- 8/12/18/24 dB/oct는 JUCE 상태변수 필터 스테이지 하나 또는 둘에 대응합니다. 보정된 아날로그 모델이 아닙니다.
- 파라미터는 호스트 오토메이션이 되고 MIDI 입력과 Learn이 켜져 있습니다. Ableton/Logic에 넣어 보는 검사는 아직 DAW가 필요합니다.

## 저장소 지도

- `Source/` 플러그인과 DSP
- `Tests/` CTest
- `Tools/` CPU 벤치와 UI 스냅샷
- `third_party/eon_dsp/` 벤더된 헤더. 출처는 [PROVENANCE.md](third_party/eon_dsp/PROVENANCE.md)

## 라이선스

SPDX 라이선스가 없습니다. `third_party/eon_dsp`도 LICENSE 파일이 없고, 배포 전에 명시해야 한다고 [PROVENANCE.md](third_party/eon_dsp/PROVENANCE.md)에 적혀 있습니다. 라이선스를 정하기 전에는 이 저장소를 오픈소스 재배포로 보지 마세요. configure 때 받는 JUCE 8은 JUCE 자신의 라이선스를 따릅니다.
