cmake_minimum_required(VERSION 3.20)

project(SideDucker VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE via FetchContent
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SideDucker
    COMPANY_NAME "AudioInnovations"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE AuIn
    PLUGIN_CODE Sdkr
    FORMATS VST3 Standalone
    PRODUCT_NAME "SideDucker"
    DESCRIPTION "Sidechain Dynamic Ducking Audio Plugin"
)

target_sources(SideDucker
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/dsp/DuckingProcessor.h
        Source/dsp/DuckingProcessor.cpp
        Source/dsp/CustomEnvelope.h
        Source/dsp/CustomEnvelope.cpp
        Source/dsp/AudioVisualizerFifo.h
        Source/ui/CustomLookAndFeel.h
        Source/ui/CustomLookAndFeel.cpp
        Source/ui/DuckingVisualizer.h
        Source/ui/DuckingVisualizer.cpp
        Source/ui/EnvelopeEditor.h
        Source/ui/EnvelopeEditor.cpp
        Source/ui/GlowingKnob.h
        Source/ui/GlowingKnob.cpp
)

target_compile_definitions(SideDucker
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(SideDucker
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
