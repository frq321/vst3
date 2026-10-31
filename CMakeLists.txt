cmake_minimum_required(VERSION 3.22)
project(SpatialPan VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SPATIALPAN_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(SPATIALPAN_JUCE_PATH)
    add_subdirectory(${SPATIALPAN_JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(SpatialPan
    COMPANY_NAME "SpatialPan"
    PLUGIN_MANUFACTURER_CODE Spnx
    PLUGIN_CODE SpPn
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx Spatial
    PRODUCT_NAME "Spatial Pan")

target_sources(SpatialPan PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    SpatialEngine.h)

target_include_directories(SpatialPan PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(SpatialPan PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SpatialPan
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

option(SPATIALPAN_BUILD_TESTS "Build the offline DSP test program" OFF)
if(SPATIALPAN_BUILD_TESTS)
    juce_add_console_app(SpatialPanTests PRODUCT_NAME "SpatialPanTests")
    target_sources(SpatialPanTests PRIVATE DspTest.cpp SpatialEngine.h)
    target_include_directories(SpatialPanTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(SpatialPanTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SpatialPanTests PRIVATE juce::juce_audio_basics juce::juce_core
                          PUBLIC juce::juce_recommended_config_flags)

    enable_testing()
    add_test(NAME SpatialPanDsp COMMAND SpatialPanTests)
endif()
