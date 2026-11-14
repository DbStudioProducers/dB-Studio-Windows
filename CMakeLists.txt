cmake_minimum_required(VERSION 3.22)
project(dBStudio VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE 8.0.6 is the primary supported build. A separate JUCE 7.0.12
# compatibility build is available for Windows 8.1 users.
set(DBSTUDIO_JUCE_VERSION "8.0.6" CACHE STRING "JUCE version used for this build")
option(DBSTUDIO_WINDOWS_81_TARGET "Build the legacy Windows 8.1-compatible variant" OFF)
if(DBSTUDIO_WINDOWS_81_TARGET AND NOT DBSTUDIO_JUCE_VERSION STREQUAL "7.0.12")
  message(FATAL_ERROR "The Windows 8.1 compatibility target requires JUCE 7.0.12")
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
  JUCE_APPLICATION_VERSION_STRING="0.5.0"
)

if(DBSTUDIO_WINDOWS_81_TARGET)
  # Windows 8.1 is version 6.3. Keep this isolated to the JUCE 7 legacy build.
  target_compile_definitions(dBStudio PRIVATE
    WINVER=0x0603
    _WIN32_WINNT=0x0603
    # Windows SDK's rpcndr.h defines "small" as a macro. JUCE 7 uses
    # "small" as an enum member in PushNotifications; self-definition keeps
    # the identifier intact and prevents the SDK macro collision.
    small=small
  )
endif()

target_link_libraries(dBStudio PRIVATE
  juce::juce_audio_utils
  juce::juce_audio_devices
  juce::juce_dsp
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)
