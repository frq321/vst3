cmake_minimum_required(VERSION 3.22)
project(SpatialPan VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE -------------------------------------------------------------------
# Use an existing JUCE checkout with  -DSPATIALPAN_JUCE_PATH=/path/to/JUCE
# otherwise JUCE 8.0.8 is downloaded from GitHub automatically.
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

# ---- Plugin -----------------------------------------------------------------
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
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SpatialEngine.h)

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

# ---- Offline DSP tests (optional) -------------------------------------------
option(SPATIALPAN_BUILD_TESTS "Build the offline DSP test program" OFF)
if(SPATIALPAN_BUILD_TESTS)
    juce_add_console_app(SpatialPanTests PRODUCT_NAME "SpatialPanTests")
    target_sources(SpatialPanTests PRIVATE Tests/DspTest.cpp Source/SpatialEngine.h)
    target_include_directories(SpatialPanTests PRIVATE Source)
    target_compile_definitions(SpatialPanTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SpatialPanTests PRIVATE juce::juce_audio_basics juce::juce_core
                          PUBLIC juce::juce_recommended_config_flags)

    enable_testing()
    add_test(NAME SpatialPanDsp COMMAND SpatialPanTests)

    # Headless UI snapshot tool (renders the editor to PNG)
    juce_add_console_app(SpatialPanSnapshot PRODUCT_NAME "SpatialPanSnapshot")
    target_sources(SpatialPanSnapshot PRIVATE Tests/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(SpatialPanSnapshot PRIVATE Source)
    target_compile_definitions(SpatialPanSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SpatialPanSnapshot PRIVATE juce::juce_audio_utils
                          PUBLIC juce::juce_recommended_config_flags)
endif()
