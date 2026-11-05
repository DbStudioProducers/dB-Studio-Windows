cmake_minimum_required(VERSION 3.22)
project(dBStudio VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if(BUILD_TESTING)
  add_executable(dBStudioCoreTests tests/test_aux_assignment.cpp)
  target_include_directories(dBStudioCoreTests PRIVATE Source)
  add_executable(dBStudioMixTests tests/test_mix_renderer.cpp)
  add_executable(dBStudioQueueTests tests/test_spsc_audio_queue.cpp)
  target_include_directories(dBStudioMixTests PRIVATE Source)
  target_include_directories(dBStudioQueueTests PRIVATE Source)

  add_test(NAME AuxAssignmentRegistry COMMAND dBStudioCoreTests)
  add_test(NAME MixRenderer COMMAND dBStudioMixTests)
  add_test(NAME SpscAudioQueue COMMAND dBStudioQueueTests)
endif()

include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(juce)

juce_add_gui_app(dBStudio
  PRODUCT_NAME "dB Studio"
  COMPANY_NAME "dB Studio"
  BUNDLE_ID "com.dbstudio.desktop"
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_SYNTH FALSE
)

target_sources(dBStudio PRIVATE
  Source/Main.cpp
  Source/AudioEngine.cpp
  Source/AudioEngine.h
  Source/AuxAssignmentRegistry.h
  Source/MixRenderer.h
  Source/SpscAudioBlockQueue.h
  Source/MainComponent.cpp
  Source/MainComponent.h
)

target_compile_definitions(dBStudio PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:dBStudio,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="0.5.0"
)

target_link_libraries(dBStudio PRIVATE
  juce::juce_audio_utils
  juce::juce_audio_devices
  juce::juce_dsp
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)
