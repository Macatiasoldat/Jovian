cmake_minimum_required(VERSION 3.22)
# macOS : binaire universel (Apple Silicon M1 + Intel), compatible macOS 11+
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(Stage90 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Runtime Visual C++ lié statiquement : pas besoin de redistribuable sur la machine de musique
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(STAGE90_FORMATS VST3 AU Standalone)   # AU = format lu par GarageBand / Logic
else()
    set(STAGE90_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Stage90
    COMPANY_NAME "KoalaAudio"
    PLUGIN_MANUFACTURER_CODE Koal
    PLUGIN_CODE Stg9
    FORMATS ${STAGE90_FORMATS}
    PRODUCT_NAME "Stage90"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Stage90 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Stage90 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(Stage90
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Programme de test du moteur (sans JUCE) : cmake --build . --target s90_render_test
add_executable(s90_render_test EXCLUDE_FROM_ALL tests/render_test.cpp)

# Test du plugin sans interface (MIDI, modes, programmes, sauvegarde d'état) : cmake --build . --target s90_proc_test
add_executable(s90_proc_test EXCLUDE_FROM_ALL tests/proc_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(s90_proc_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1
                           JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_include_directories(s90_proc_test PRIVATE Source)
target_link_libraries(s90_proc_test PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

# Capture d'écran hors écran de l'interface : cmake --build . --target s90_snapshot
add_executable(s90_snapshot EXCLUDE_FROM_ALL tests/snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(s90_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1
                           JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_include_directories(s90_snapshot PRIVATE Source)
target_link_libraries(s90_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
