cmake_minimum_required(VERSION 3.22)

project(ArpExcel VERSION 1.0.0)

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
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE ArpE
    PLUGIN_CODE ArpX
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ArpExcel"
)

target_sources(ArpExcel
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/Arpeggiator.cpp
        src/FormulaEngine.cpp
        src/ArpPattern.cpp
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
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
