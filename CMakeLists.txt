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
