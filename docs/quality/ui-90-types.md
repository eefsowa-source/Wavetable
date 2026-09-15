# SEOUL DSP 90종 UI 타입 구현 기록

검증일: 2026-09-15
상태: **에디터/스냅샷/단위테스트 수준 검증 완료** (호스트·DAW 게이트 아님)

사용자 확정 요구사항 "abc 별로 30개 다 구현해"에 따라 에디터 UI 타입 총 90종을
단일 선택기로 제공한다. 오디오 파라미터와 컨트롤 세트는 모든 타입에서 동일하며,
프레젠테이션만 바뀐다. 이 문서는 UI 구현 범위의 증거만 담는다. pluginval, auval,
REAPER/Ableton 삽입, 실청취는 별도 게이트다.

## 구성

- **A 뱅크 (type 0-29) - 스킨 30종**: 팔레트(배경·패널·액센트 2색·텍스트),
  폰트, 노브 스타일 5종(arc/dot/bar/needle/split gauge), 배경 장식 5종
  (skyline/grid/scanlines/aurora/minimal), glow 토글 조합.
- **B 뱅크 (type 30-59) - 레이아웃 30종**: 골격 6종(stack/sidecar/twin/stage/rail/grid)
  × 밀도 5종(comfort~dense). B 뱅크는 기본 스킨(Neon Han)을 유지한다.
- **C 뱅크 (type 60-89) - 큐레이션 30종**: 스킨+레이아웃 쌍. 30개 스킨과 30개
  레이아웃이 각각 정확히 한 번씩 사용된다(테스트로 검증).

데이터는 `Source/EditorTypes.h`의 헤더 온리 테이블이다.

## 주요 스킨 (전체 30종은 소스 기준)

Neon Han, Han River Dawn, Terminal Mint, Ultraviolet, Sakura Neon, Concrete Mono,
Deep Ocean, Sunset VCT, Forest CRT, Lava Tube, Arctic Glass, Gold Leaf,
Hunter Green, Crimson Booth, Teal Circuit, Slate Blueprint, Sandstone,
Midnight Radio, Toxic Rave, Copper Wok, Pastel Yume, Noir Scarlet, Polar Night,
Plasma Flow, Sepia Film, Kyoto Maple, Bleached Cyan, Carbon Violet,
Haeundae Blue, Void Prism.

레이아웃 30종: {Studio, Sidecar, Twin, Stage, Rail, Grid} × {Comfort, Normal,
Compact, Tight, Dense}. 큐레이션 예: Neon Studio(0+1), Mono Twin(5+13),
Mint Rail(2+24), Prism Grid(29+25).

## 동작

- `uiTypeMenu` 콤보박스 90항목(뱅크별 separator, "A // 스킨명" 형식).
- `HybridWavetableAudioProcessor::uiType` 상태 저장/복원, 범위 밖 값 clamp.
  이전 세션 상태에 키가 없으면 0으로 기본 복원.
- `applyUiType()`이 스킨 -> LookAndFeel -> 웨이브 에디터 -> 라벨 색 -> resized ->
  repaint를 수행하고 콤보 선택 텍스트를 동기화한다. 상태 복원과 오프라인 도구에서도
  동일 경로를 쓰도록 public 메서드로 제공한다.
- 제약 유지: SEOUL DSP 타이틀 + cursive aoi yume 시그니처, 컨트롤 이름 항상
  표시, 노브 값은 드래그 중에만 표시, 오디오 콜백에서 UI/할당 없음.

## 이번 세션에서 수정한 결함

1. **--all 스냅샷이 실제 타입을 반영하지 않음**: 90개 PNG가 전부 동일 해시.
   원인은 `processor.setUiType()`만 호출하고 에디터 `applyUiType()`을 트리거하지
   않은 것. 루프에서 에디터에 타입을 푸시하도록 수정.
2. **콤보 텍스트 미갱신**: `applyUiType`이 콤보 선택을 갱신하지 않아 --all
   스냅샷의 선택 텍스트가 항상 type 0 표기였다. 수정 후 type-31(=60 복제 케이스)과
   type-60이 0픽셀 일치, 해시 기준 90종 모두 고유.
3. **스킨 전환 시 콤보 라벨 색 잔재**: 같은 LaF 인스턴스 `setLookAndFeel`은
   브로드캐스트가 없어 이전 스킨 텍스트 색이 남았다(`sendLookAndFeelChange()`
   추가). 단일 모드 vs --all type-65 픽셀 비교(495px 차이 -> 0)로 확인.
4. **--all 인자 파싱**: 디렉터리 문자열이 폭 인자로 파싱돼 1040폭으로 렌더됐다
   (`atoi("Build/...")==0`). --all <dir> [w] [h]로 파싱 분리, 기본 1120×760 유지.
5. **skinIndexForType B 뱅크 언더플로우**(직전 세션 수정): B 뱅크(30-59)가
   combined 테이블을 역인덱싱하는 버그를 기본 스킨 매핑으로 교정.

## 검증 증거

- ctest **9/9 PASS** (AudioQualityMetrics, ProcessorQuality, OfflineRenderer,
  GoldenComparator, UnisonBank, WavetableDSP, WavetableImporter, OutputSafety,
  ProcessorSmoke). ProcessorSmoke에 UI 타입 단언 포함: typeCount==90, clamp,
  B 뱅크 기본 스킨+자체 레이아웃, C 뱅크 각 스킨/레이아웃 정확히 1회 커버,
  상태 저장/복원(47), 범위 밖(500 -> 89 clamp), 키 없음(-> 0).
- 90종 스냅샷: `Build/quality-ui/types/type-00.png` ~ `type-89.png`,
  1120×760, **90개 고유 md5**, SHA-256 매니페스트
  `Build/quality-ui/types/manifest-sha256.txt` (90행).
- 단일 모드(`SeoulDSP_UISnapshot <path> 1120 760 65`)와 --all의 type-65 렌더가
  md5 일치 - 두 렌더 경로가 동일 프레젠테이션을 만든다.
- 픽셀 diff 스폿 체크: type 0 vs 1 (스킨) 99.99% 차이, type 30 vs 35 (골격+밀도)
  47.58%, type 0 vs 30 (밀도만) 0.10% - 콤보 폭·노브 높이 3-6px 경계 변화로 정상.
  type 0 vs 60 (큐레이션) 14.17%.

## 제한과 다음 게이트

- 이 변경은 아직 커밋되지 않았다(A2 음질 미커밋 변경과 함께 작업 트리 보존).
- Release pluginval strictness 10과 auval은 PASS다. REAPER AU 삽입/렌더는
  현재 Release에서 PASS했고 오프라인 fixture와 정렬 후 24-bit 1 LSB로 일치한다.
  Ableton Live 12 VST3 검색/삽입도 PASS지만 플러그인 내부 UI 타입 전환과
  세션 저장/복원 실호스트 확인은 아직 미수행이다.
- 90종 스킨의 폰트는 시스템 의존(Helvetica Neue, Menlo, Baskerville 등)이라
  타 호스트 환경에서 폰트 폴백 시 미세 렌더 차이가 있을 수 있다.
