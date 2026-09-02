cmake_minimum_required(VERSION 3.22)
project(HybridWavetable VERSION 0.1.0 LANGUAGES C CXX)

if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal macOS build" FORCE)
endif()

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
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "SEOUL DSP")

juce_generate_juce_header(HybridWavetable)

target_sources(HybridWavetable PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/DSP/WavetableOscillator.cpp
  Source/DSP/SynthVoice.cpp)

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
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/DSP/WavetableOscillator.cpp Source/DSP/SynthVoice.cpp)
  target_compile_features(ProcessorQualityTests PRIVATE cxx_std_20)
  target_compile_definitions(ProcessorQualityTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(ProcessorQualityTests PRIVATE
    AudioQualitySupport juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_dsp juce::juce_gui_extra)
  set_target_properties(ProcessorQualityTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME ProcessorQuality COMMAND ProcessorQualityTests)

  juce_add_console_app(OfflineRendererTests PRODUCT_NAME "Offline Renderer Tests")
  juce_generate_juce_header(OfflineRendererTests)
  target_sources(OfflineRendererTests PRIVATE
    Tests/AudioQuality/OfflineRendererTests.cpp
    Tests/AudioQuality/OfflineRenderer.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/DSP/WavetableOscillator.cpp Source/DSP/SynthVoice.cpp)
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
    Source/DSP/WavetableOscillator.cpp Source/DSP/SynthVoice.cpp)
  target_compile_features(AudioQualityRunner PRIVATE cxx_std_20)
  target_compile_definitions(AudioQualityRunner PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(AudioQualityRunner PRIVATE
    AudioQualitySupport juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_audio_formats juce::juce_cryptography juce::juce_dsp juce::juce_gui_extra)
  set_target_properties(AudioQualityRunner PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")

  juce_add_console_app(WavetableTests PRODUCT_NAME "Wavetable Tests")
  juce_generate_juce_header(WavetableTests)
  target_sources(WavetableTests PRIVATE
    Tests/WavetableTests.cpp
    Source/DSP/WavetableOscillator.cpp)
  target_compile_features(WavetableTests PRIVATE cxx_std_20)
  target_compile_definitions(WavetableTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(WavetableTests PRIVATE juce::juce_audio_basics juce::juce_core juce::juce_dsp)
  set_target_properties(WavetableTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME WavetableDSP COMMAND WavetableTests)

  juce_add_console_app(ProcessorSmokeTests PRODUCT_NAME "Processor Smoke Tests")
  juce_generate_juce_header(ProcessorSmokeTests)
  target_sources(ProcessorSmokeTests PRIVATE
    Tests/ProcessorSmokeTests.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/DSP/WavetableOscillator.cpp Source/DSP/SynthVoice.cpp)
  target_compile_features(ProcessorSmokeTests PRIVATE cxx_std_20)
  target_compile_definitions(ProcessorSmokeTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(ProcessorSmokeTests PRIVATE
    juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp juce::juce_gui_extra)
  set_target_properties(ProcessorSmokeTests PROPERTIES MACOSX_DEPLOYMENT_TARGET "12.0")
  add_test(NAME ProcessorSmoke COMMAND ProcessorSmokeTests)
endif()
