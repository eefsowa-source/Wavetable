# SEOUL DSP 품질 증거

오디오 품질 결과는 `AudioQualityRunner`가 생성한 JSON과 Markdown 요약으로 관리한다.

```sh
Build/AudioQualityRunner_artefacts/Debug/AudioQualityRunner \
  --manifest Tests/AudioQuality/fixture-manifest.json \
  --output-dir Build/quality-baseline \
  --fixture-group foundation --matrix per-build --write-audio
```

`golden/manifest.json`에 등록되지 않은 기준음은 실패로 기록된다. Golden 승격은 검토된 파일 작업으로만 수행하며 runner에는 승격 옵션을 두지 않는다.

증거 등급은 빌드, CTest, pluginval, auval, 호스트 삽입, UI 렌더링, 실청취를 각각 별도로 기록한다.
