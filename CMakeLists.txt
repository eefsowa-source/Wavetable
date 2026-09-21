cmake_minimum_required(VERSION 3.22)
project(HybridWavetable VERSION 0.1.0 LANGUAGES C CXX)

if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal macOS build" FORCE)
endif()

# Vendored header-only DSP core (ADAA, half-band oversampling, ZDF/WDF filters,
# solvers, circuit stages). See third_party/eon_dsp/PROVENANCE.md for the pinned
# source revision and the external-dependency audit. Deliberately JUCE-free so
# the same headers can be unit tested and reused outside JUCE.
add_library(eon_dsp INTERFACE)
add_library(eon::dsp ALIAS eon_dsp)
target_include_directories(eon_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/eon_dsp)
target_compile_features(eon_dsp INTERFACE cxx_std_20)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.14
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HybridWavetable
  COMPANY_NAME "EON Audio"
  BUNDLE_ID com.eonaudio.hybridwavetable
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Eona
  PLUGIN_CODE Hwbl
  AU_SANDBOX_SAFE FALSE
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "SEOUL DSP")

juce_generate_juce_header(HybridWavetable)

target_sources(HybridWavetable PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/DSP/WavetableOscillator.cpp
  Source/DSP/WavetableImporter.cpp
  Source/DSP/OutputSafety.cpp
  Source/DSP/SynthVoice.cpp
  Source/DSP/UnisonBank.cpp)

target_compile_features(HybridWavetable PRIVATE cxx_std_20)
target_compile_definitions(HybridWavetable PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(HybridWavetable PRIVATE
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_dsp
  juce::juce_gui_extra)

set_target_properties(HybridWavetable PROPERTIES
  MACOSX_DEPLOYMENT_TARGET "12.0")

include(CTest)
if (BUILD_TESTING)
  # Headless UI snapshot used by the design/acceptance review. This renders
  # the actual editor component without opening a DAW or relying on a logged-in
  # desktop session, so the PNG remains deterministic and inspectable in CI.
  add_executable(SeoulDSP_UISnapshot Tools/UiSnapshot.cpp)
  target_compile_features(SeoulDSP_UISnapshot PRIVATE cxx_std_20)
  target_compile_definitions(SeoulDSP_UISnapshot PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_include_directories(SeoulDSP_UISnapshot PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/HybridWavetable_artefacts/JuceLibraryCode)
  target_link_libraries(SeoulDSP_UISnapshot PRIVATE
    HybridWavetable juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_dsp juce::juce_gui_extra)
  set_target_properties(SeoulDSP_UISnapshot PROPERTIES
    MACOSX_DEPLOYMENT_TARGET "12.0")

  add_executable(CpuBench Tools/CpuBench.cpp)
  target_compile_features(CpuBench PRIVATE cxx_std_20)
  target_compile_definitions(CpuBench PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_include_directories(CpuBench PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/HybridWavetable_artefacts/JuceLibraryCode)
  target_link_libraries(CpuBench PRIVATE
    HybridWavetable juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_dsp juce::juce_gui_extra)
  set_target_properties(CpuBench PROPERTIES
    MACOSX_DEPLOYMENT_TARGET "12.0")

  add_library(AudioQualitySupport STATIC
    Tests/AudioQuality/Metrics.cpp)
  target_compile_features(AudioQualitySupport PUBLIC cxx_std_20)
  target_compile_definitions(AudioQualitySupport PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(AudioQualitySupport PUBLIC
    juce::juce_audio_basics juce::juce_audio_formats juce::juce_core juce::juce_dsp)
  set_target_properties(AudioQualitySupport PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")

  juce_add_console_app(AudioQualityMetricsTests PRODUCT_NAME "Audio Quality Metrics Tests")
  juce_generate_juce_header(AudioQualityMetricsTests)
  target_sources(AudioQualityMetricsTests PRIVATE
    Tests/AudioQuality/MetricsTests.cpp)
  target_compile_features(AudioQualityMetricsTests PRIVATE cxx_std_20)
  target_compile_definitions(AudioQualityMetricsTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(AudioQualityMetricsTests PRIVATE AudioQualitySupport)
  set_target_properties(AudioQualityMetricsTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME AudioQualityMetrics COMMAND AudioQualityMetricsTests)

  juce_add_console_app(ProcessorQualityTests PRODUCT_NAME "Processor Quality Tests")
  juce_generate_juce_header(ProcessorQualityTests)
  target_sources(ProcessorQualityTests PRIVATE
    Tests/AudioQuality/ProcessorQualityTests.cpp
    Tests/AudioQuality/OfflineRenderer.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/DSP/WavetableOscillator.cpp Source/DSP/WavetableImporter.cpp Source/DSP/OutputSafety.cpp Source/DSP/SynthVoice.cpp
    Source/DSP/UnisonBank.cpp)
  target_compile_features(ProcessorQualityTests PRIVATE cxx_std_20)
  target_compile_definitions(ProcessorQualityTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(ProcessorQualityTests PRIVATE
    AudioQualitySupport juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_dsp juce::juce_gui_extra)
  set_target_properties(ProcessorQualityTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME ProcessorQuality COMMAND ProcessorQualityTests)

  # Renderer-backed alias-quality instrument and baseline (Plan B Task 2). Runs
  # the real processor so a future saturation quality tier can be gated on
  # measured folded-harmonic energy instead of an advertised label.
  juce_add_console_app(QualityOrderTests PRODUCT_NAME "Quality Order Tests")
  juce_generate_juce_header(QualityOrderTests)
  target_sources(QualityOrderTests PRIVATE
    Tests/AudioQuality/QualityOrderTests.cpp
    Tests/AudioQuality/OfflineRenderer.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/DSP/WavetableOscillator.cpp Source/DSP/WavetableImporter.cpp Source/DSP/OutputSafety.cpp Source/DSP/SynthVoice.cpp
    Source/DSP/UnisonBank.cpp)
  target_compile_features(QualityOrderTests PRIVATE cxx_std_20)
  target_compile_definitions(QualityOrderTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(QualityOrderTests PRIVATE
    AudioQualitySupport juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_dsp juce::juce_gui_extra)
  set_target_properties(QualityOrderTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME QualityOrder COMMAND QualityOrderTests)

  juce_add_console_app(OfflineRendererTests PRODUCT_NAME "Offline Renderer Tests")
  juce_generate_juce_header(OfflineRendererTests)
  target_sources(OfflineRendererTests PRIVATE
    Tests/AudioQuality/OfflineRendererTests.cpp
    Tests/AudioQuality/OfflineRenderer.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/DSP/WavetableOscillator.cpp Source/DSP/WavetableImporter.cpp Source/DSP/OutputSafety.cpp Source/DSP/SynthVoice.cpp
    Source/DSP/UnisonBank.cpp)
  target_compile_features(OfflineRendererTests PRIVATE cxx_std_20)
  target_compile_definitions(OfflineRendererTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(OfflineRendererTests PRIVATE
    AudioQualitySupport juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_dsp juce::juce_gui_extra)
  set_target_properties(OfflineRendererTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME OfflineRenderer COMMAND OfflineRendererTests)

  juce_add_console_app(GoldenComparatorTests PRODUCT_NAME "Golden Comparator Tests")
  juce_generate_juce_header(GoldenComparatorTests)
  target_sources(GoldenComparatorTests PRIVATE
    Tests/AudioQuality/GoldenComparatorTests.cpp
    Tests/AudioQuality/GoldenComparator.cpp)
  target_compile_features(GoldenComparatorTests PRIVATE cxx_std_20)
  target_compile_definitions(GoldenComparatorTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(GoldenComparatorTests PRIVATE AudioQualitySupport juce::juce_dsp)
  set_target_properties(GoldenComparatorTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME GoldenComparator COMMAND GoldenComparatorTests)

  juce_add_console_app(AudioQualityRunner PRODUCT_NAME "Audio Quality Runner")
  juce_generate_juce_header(AudioQualityRunner)
  target_sources(AudioQualityRunner PRIVATE
    Tests/AudioQuality/AudioQualityRunner.cpp
    Tests/AudioQuality/QualityReport.cpp
    Tests/AudioQuality/GoldenComparator.cpp
    Tests/AudioQuality/OfflineRenderer.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/DSP/WavetableOscillator.cpp Source/DSP/WavetableImporter.cpp Source/DSP/OutputSafety.cpp Source/DSP/SynthVoice.cpp
    Source/DSP/UnisonBank.cpp)
  target_compile_features(AudioQualityRunner PRIVATE cxx_std_20)
  target_compile_definitions(AudioQualityRunner PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(AudioQualityRunner PRIVATE
    AudioQualitySupport juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_audio_formats juce::juce_cryptography juce::juce_dsp juce::juce_gui_extra)
  set_target_properties(AudioQualityRunner PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")

  juce_add_console_app(UnisonBankTests PRODUCT_NAME "Unison Bank Tests")
  juce_generate_juce_header(UnisonBankTests)
  target_sources(UnisonBankTests PRIVATE
    Tests/AudioQuality/UnisonBankTests.cpp Source/DSP/UnisonBank.cpp
    Source/DSP/WavetableOscillator.cpp Source/DSP/WavetableImporter.cpp)
  target_compile_features(UnisonBankTests PRIVATE cxx_std_20)
  target_compile_definitions(UnisonBankTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(UnisonBankTests PRIVATE juce::juce_audio_basics juce::juce_core juce::juce_dsp)
  set_target_properties(UnisonBankTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME UnisonBank COMMAND UnisonBankTests)

  juce_add_console_app(WavetableTests PRODUCT_NAME "Wavetable Tests")
  juce_generate_juce_header(WavetableTests)
  target_sources(WavetableTests PRIVATE
    Tests/WavetableTests.cpp
    Source/DSP/WavetableOscillator.cpp Source/DSP/WavetableImporter.cpp)
  target_compile_features(WavetableTests PRIVATE cxx_std_20)
  target_compile_definitions(WavetableTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(WavetableTests PRIVATE juce::juce_audio_basics juce::juce_core juce::juce_dsp)
  set_target_properties(WavetableTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME WavetableDSP COMMAND WavetableTests)

  juce_add_console_app(WavetableImporterTests PRODUCT_NAME "Wavetable Importer Tests")
  juce_generate_juce_header(WavetableImporterTests)
  target_sources(WavetableImporterTests PRIVATE
    Tests/AudioQuality/WavetableImporterTests.cpp
    Source/DSP/WavetableOscillator.cpp Source/DSP/WavetableImporter.cpp)
  target_compile_features(WavetableImporterTests PRIVATE cxx_std_20)
  target_compile_definitions(WavetableImporterTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(WavetableImporterTests PRIVATE
    juce::juce_audio_basics juce::juce_core juce::juce_dsp)
  set_target_properties(WavetableImporterTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME WavetableImporter COMMAND WavetableImporterTests)

  juce_add_console_app(OutputSafetyTests PRODUCT_NAME "Output Safety Tests")
  juce_generate_juce_header(OutputSafetyTests)
  target_sources(OutputSafetyTests PRIVATE
    Tests/AudioQuality/OutputSafetyTests.cpp Source/DSP/OutputSafety.cpp)
  target_compile_features(OutputSafetyTests PRIVATE cxx_std_20)
  target_compile_definitions(OutputSafetyTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(OutputSafetyTests PRIVATE juce::juce_audio_basics juce::juce_core)
  set_target_properties(OutputSafetyTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME OutputSafety COMMAND OutputSafetyTests)

  juce_add_console_app(ProcessorSmokeTests PRODUCT_NAME "Processor Smoke Tests")
  juce_generate_juce_header(ProcessorSmokeTests)
  target_sources(ProcessorSmokeTests PRIVATE
    Tests/ProcessorSmokeTests.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/DSP/WavetableOscillator.cpp Source/DSP/WavetableImporter.cpp Source/DSP/OutputSafety.cpp Source/DSP/SynthVoice.cpp
    Source/DSP/UnisonBank.cpp)
  target_compile_features(ProcessorSmokeTests PRIVATE cxx_std_20)
  target_compile_definitions(ProcessorSmokeTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(ProcessorSmokeTests PRIVATE
    juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp juce::juce_gui_extra)
  set_target_properties(ProcessorSmokeTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME ProcessorSmoke COMMAND ProcessorSmokeTests)

  # Vendored-copy guard: proves the checked-in third_party/eon_dsp headers
  # compile in this toolchain and still honour the contracts the Plan B
  # sound-quality upgrade depends on. Needs no JUCE.
  add_executable(EonDspContractTests Tests/EonDspContractTests.cpp)
  target_link_libraries(EonDspContractTests PRIVATE eon::dsp)
  target_compile_options(EonDspContractTests PRIVATE -Wall -Wextra)
  set_target_properties(EonDspContractTests PROPERTIES
    MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME EonDspContract COMMAND EonDspContractTests)
endif()
