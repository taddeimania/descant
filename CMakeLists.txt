cmake_minimum_required(VERSION 3.22)

# macOS 11+. Builds for the machine you're on by default; for a universal
# binary pass -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64". Set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Descant VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---------------------------------------------------------------------
# By default JUCE is downloaded on first configure. To use a local checkout:
#   cmake -B build -DDESCANT_JUCE_DIR=/path/to/JUCE
set(DESCANT_JUCE_DIR "" CACHE PATH "Optional local JUCE checkout")

if(DESCANT_JUCE_DIR)
    add_subdirectory(${DESCANT_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Signal path (no JUCE) -----------------------------------------------------
# Pitch tracking, the grain shifter, the carrier synth, the vocoder bank and
# the harmony rules. The tests build against this alone.
add_library(DescantDSP STATIC
    Source/Engine/PitchTracker.cpp
    Source/Engine/Shifter.cpp
    Source/Engine/Carrier.cpp
    Source/Engine/Vocoder.cpp
    Source/Engine/Music.cpp
    Source/Engine/Engine.cpp)
target_include_directories(DescantDSP PUBLIC Source)
set_target_properties(DescantDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(DescantDSP PRIVATE /utf-8)
endif()

# --- Plugin -------------------------------------------------------------------
# Copy the built plugin into the system VST3 folder after each build:
#   macOS   ~/Library/Audio/Plug-Ins/VST3
#   Linux   ~/.vst3
#   Windows C:\Program Files\Common Files\VST3  (needs an administrator shell,
#           so it is off by default there)
if(WIN32)
    set(_descant_copy_default OFF)
else()
    set(_descant_copy_default ON)
endif()
option(DESCANT_COPY_PLUGIN "Install the plugin into the system VST3 folder after building" ${_descant_copy_default})

# An audio effect that also takes MIDI in, so Live lets a MIDI track (a
# keyboard, a clip, or Posy) send notes to it.
juce_add_plugin(Descant
    COMPANY_NAME                "Taddei"
    BUNDLE_ID                   com.taddei.descant
    PLUGIN_MANUFACTURER_CODE    Tadd
    PLUGIN_CODE                 Dsct
    PRODUCT_NAME                "Descant"
    DESCRIPTION                 "Vocoder and vocal harmony"
    FORMATS                     VST3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Pitch Shift Modulation
    COPY_PLUGIN_AFTER_BUILD     ${DESCANT_COPY_PLUGIN})

set(DESCANT_PLUGIN_SOURCES
    Source/Settings.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/Widgets.cpp
    Source/UI/Views.cpp)

target_sources(Descant PRIVATE ${DESCANT_PLUGIN_SOURCES})
target_include_directories(Descant PRIVATE Source)
if(MSVC)
    target_compile_options(Descant PRIVATE /utf-8)
endif()
target_compile_definitions(Descant PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Descant
    PRIVATE
        DescantDSP
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Engine tests (no JUCE needed) --------------------------------------------
enable_testing()
add_executable(DescantEngineTests Tests/EngineTests.cpp)
target_include_directories(DescantEngineTests PRIVATE Tests)
target_link_libraries(DescantEngineTests PRIVATE DescantDSP)
add_test(NAME engine COMMAND DescantEngineTests)

# --- Optional dev tool --------------------------------------------------------
#   DescantSnapshot out.png [wavFolder] [voice.wav]   render the panel (and every preset) headless
option(DESCANT_BUILD_TOOLS "Build the snapshot tool" OFF)
if(DESCANT_BUILD_TOOLS)
    juce_add_console_app(DescantSnapshot PRODUCT_NAME "DescantSnapshot")
    target_sources(DescantSnapshot PRIVATE Tools/Snapshot.cpp ${DESCANT_PLUGIN_SOURCES})
    target_include_directories(DescantSnapshot PRIVATE Source Tests)
    if(MSVC)
        target_compile_options(DescantSnapshot PRIVATE /utf-8)
    endif()
    target_compile_definitions(DescantSnapshot PRIVATE
        JucePlugin_Name="Descant"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(DescantSnapshot PRIVATE
        DescantDSP
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
endif()
