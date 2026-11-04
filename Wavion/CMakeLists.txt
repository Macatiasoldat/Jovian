cmake_minimum_required(VERSION 3.22)
# macOS : binaire universel (Apple Silicon + Intel), compatible macOS 11+
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(Wavion VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Runtime Visual C++ lié statiquement : rien à installer sur la machine de musique
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# Formats : VST3 (Windows + macOS), AU (macOS uniquement — le format n'existe pas sous Windows), Standalone.
# AAX : nécessite le SDK AAX d'Avid (compte développeur gratuit, licence non redistribuable). Fournir son chemin :
#   cmake -B build -DWAVION_AAX_SDK_PATH=/chemin/vers/AAX_SDK
set(WAVION_AAX_SDK_PATH "" CACHE PATH "Dossier du SDK AAX d'Avid (optionnel, active le format AAX)")

set(WAVION_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND WAVION_FORMATS AU)
endif()
if(WAVION_AAX_SDK_PATH AND EXISTS "${WAVION_AAX_SDK_PATH}")
    juce_set_aax_sdk_path("${WAVION_AAX_SDK_PATH}")
    list(APPEND WAVION_FORMATS AAX)
    message(STATUS "Wavion : format AAX activé (${WAVION_AAX_SDK_PATH})")
elseif(WAVION_AAX_SDK_PATH)
    message(WARNING "Wavion : WAVION_AAX_SDK_PATH n'existe pas : AAX ignoré")
endif()

juce_add_plugin(Wavion
    COMPANY_NAME "KoalaAudio"
    PLUGIN_MANUFACTURER_CODE Koal
    PLUGIN_CODE Wav1
    FORMATS ${WAVION_FORMATS}
    PRODUCT_NAME "Wavion"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Wavion PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Wavion PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Wavion
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
