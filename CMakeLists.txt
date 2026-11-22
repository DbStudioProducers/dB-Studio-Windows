cmake_minimum_required(VERSION 3.22)
project(dBStudio VERSION 0.5.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE 8.0.6 is the supported build baseline for Windows 10/11 and macOS.
set(DBSTUDIO_JUCE_VERSION "8.0.6" CACHE STRING "JUCE version used for this build")
option(DBSTUDIO_ENABLE_ASIO "Enable ASIO host support (requires licensed Steinberg ASIO SDK headers)" OFF)
set(DBSTUDIO_ASIO_SDK_COMMON_DIR "" CACHE PATH "Path to the Steinberg ASIO SDK common directory (must contain iasiodrv.h)")

# This proprietary distribution must not compile or redistribute Steinberg ASIO
# components until the applicable licensing terms have been confirmed.
if(DBSTUDIO_ENABLE_ASIO)
  message(FATAL_ERROR "ASIO distribution is disabled until the applicable Steinberg license is confirmed")
endif()


include(CTest)

if(BUILD_TESTING)
  find_package(Threads REQUIRED)

  add_executable(dBStudioCoreTests tests/test_aux_assignment.cpp)
  add_executable(dBStudioMixTests tests/test_mix_renderer.cpp)
  add_executable(dBStudioQueueTests tests/test_spsc_audio_queue.cpp)
  add_executable(dBStudioAuthorizationTests tests/test_session_authorization.cpp)
  add_executable(dBStudioPacketTests tests/test_audio_packet_codec.cpp)
  target_link_libraries(dBStudioQueueTests PRIVATE Threads::Threads)

  add_test(NAME AuxAssignmentRegistry COMMAND dBStudioCoreTests)
  add_test(NAME MixRenderer COMMAND dBStudioMixTests)
  add_test(NAME SpscAudioQueue COMMAND dBStudioQueueTests)
  add_test(NAME SessionAuthorization COMMAND dBStudioAuthorizationTests)
  add_test(NAME AudioPacketCodec COMMAND dBStudioPacketTests)
endif()

include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG ${DBSTUDIO_JUCE_VERSION}
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
  Source/AudioPacketCodec.h
  Source/MainComponent.cpp
  Source/MainComponent.h
)

target_compile_definitions(dBStudio PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:dBStudio,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="0.5.1"
  JUCE_ASIO=$<BOOL:${DBSTUDIO_ENABLE_ASIO}>
  JUCE_ASIO_USE_EXTERNAL_SDK=0
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

add_custom_command(TARGET dBStudio POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/scripts/update-dbstudio.ps1"
          "$<TARGET_FILE_DIR:dBStudio>/update-dbstudio.ps1"
  COMMENT "Copying in-place update helper beside the application"
)
