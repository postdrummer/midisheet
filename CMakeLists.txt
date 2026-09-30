cmake_minimum_required(VERSION 3.22)

project(ArpExcel VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ARPEXCEL_BUILD_TESTS "Build engine unit tests" ON)

# Engine: plain C++ (no JUCE) so it can be unit-tested
add_library(arp_engine STATIC
    src/engine/ArpEngine.cpp
    src/engine/Formula.cpp
)
target_include_directories(arp_engine PUBLIC src)

# Fetch JUCE automatically
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Plugin target
juce_add_plugin(ArpExcel
    COMPANY_NAME "ArpExcel"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE ArpE
    PLUGIN_CODE ArpX
    FORMATS VST3 AU Standalone
    AU_MAIN_TYPE kAudioUnitType_MIDIProcessor
    VST3_CATEGORIES Fx Instrument
    PRODUCT_NAME "ArpExcel"
)

target_sources(ArpExcel
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/ArpPattern.cpp
        src/TrackerGrid.cpp
)

target_compile_definitions(ArpExcel
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ArpExcel
    PRIVATE
        arp_engine
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(ARPEXCEL_BUILD_TESTS)
    enable_testing()
    add_executable(engine_tests tests/EngineTests.cpp tests/FormulaTests.cpp tests/main.cpp)
    target_link_libraries(engine_tests PRIVATE arp_engine)
    add_test(NAME engine_tests COMMAND engine_tests)
endif()
