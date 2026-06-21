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

# Embedded assets (central faceplate graphic + easter-egg audio)
juce_add_binary_data(TekkChildArt SOURCES
    Resources/tekkchild_face.png
    Resources/easter_egg.ogg)

set(TEKK_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND TEKK_FORMATS AU)
endif()

juce_add_plugin(TekkChild670
    PRODUCT_NAME              "TekkChild TC-670 Vari-Mu Compressor"
    COMPANY_NAME              "Tekk Engineering Audio Labs"
    BUNDLE_ID                 com.teal.tekkchildtc670
    PLUGIN_MANUFACTURER_CODE  Teal
    PLUGIN_CODE               Tc67
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
        TekkChildArt
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# --- Separate single-module plugins -----------------------------------------
# Each is the same source compiled with one TEKK_HAS_* module flag, producing a
# standalone VST3 for just that section of the TC-670.
function(tekk_add_module TGT PRODUCT CODE BUNDLE MODFLAG)
    juce_add_plugin(${TGT}
        PRODUCT_NAME              "${PRODUCT}"
        COMPANY_NAME              "Tekk Engineering Audio Labs"
        BUNDLE_ID                 "${BUNDLE}"
        PLUGIN_MANUFACTURER_CODE  Teal
        PLUGIN_CODE               ${CODE}
        FORMATS                   ${TEKK_FORMATS}
        IS_SYNTH                  FALSE
        NEEDS_MIDI_INPUT          FALSE
        NEEDS_MIDI_OUTPUT         FALSE
        IS_MIDI_EFFECT            FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD   FALSE
        VST3_CATEGORIES           Fx
        AU_MAIN_TYPE              kAudioUnitType_Effect
    )
    target_sources(${TGT} PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(${TGT} PRIVATE Source)
    target_compile_definitions(${TGT} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        TEKK_MODULE_SELECTED=1
        ${MODFLAG}=1
    )
    target_link_libraries(${TGT}
        PRIVATE
            TekkChildArt
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endfunction()

tekk_add_module(TekkComp   "TC-670 Compressor"   Tc6C com.teal.tc670comp TEKK_HAS_COMP)
tekk_add_module(TekkTape   "TC-670 Tape Brain"   Tc6T com.teal.tc670tape TEKK_HAS_TAPE)
tekk_add_module(TekkStereo "TC-670 Stereo Field" Tc6I com.teal.tc670img  TEKK_HAS_IMAGER)
tekk_add_module(TekkLimit  "TC-670 1176 Limiter" Tc6L com.teal.tc670lim  TEKK_HAS_LIMITER)

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
    # the test harness pumps the message loop to flush deferred latency updates
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(TekkChild670Tests
    PRIVATE
        TekkChildArt
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
)

# Offline characterisation harness: writes CSV + SVG measurement plots
juce_add_console_app(TekkChild670Measure PRODUCT_NAME "TekkChild670Measure")

target_sources(TekkChild670Measure PRIVATE
    Tools/Measure.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(TekkChild670Measure PRIVATE Source)

target_compile_definitions(TekkChild670Measure PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(TekkChild670Measure
    PRIVATE
        TekkChildArt
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
)
