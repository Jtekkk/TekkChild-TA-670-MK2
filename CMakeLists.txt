cmake_minimum_required(VERSION 3.22)

project(TekkChild670 VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

set(TEKK_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND TEKK_FORMATS AU)
endif()

juce_add_plugin(TekkChild670
    PRODUCT_NAME              "TekkChild TA-670 MK2"
    COMPANY_NAME              "Tekk Audio Engineering"
    BUNDLE_ID                 com.tekkaudio.tekkchild670
    PLUGIN_MANUFACTURER_CODE  Tekk
    PLUGIN_CODE               Tk67
    FORMATS                   ${TEKK_FORMATS}
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx Dynamics
    AU_MAIN_TYPE              kAudioUnitType_Effect
)

target_sources(TekkChild670 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(TekkChild670 PRIVATE Source)

target_compile_definitions(TekkChild670 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(TekkChild670
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Offline DSP smoke tests (headless console app, run in CI)
juce_add_console_app(TekkChild670Tests PRODUCT_NAME "TekkChild670Tests")

target_sources(TekkChild670Tests PRIVATE
    Tests/SmokeTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(TekkChild670Tests PRIVATE Source)

target_compile_definitions(TekkChild670Tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(TekkChild670Tests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
)
