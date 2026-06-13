#pragma once

#include <vector>

#include <juce_core/juce_core.h>

#include "Parameters.h"

namespace tekk
{

/*
    Factory presets.

    A preset resets every sound parameter to its default and then applies its
    own overrides, so loading one is deterministic regardless of the current
    state. "Channel" entries are applied to both A and B; "global" entries use
    their full id. Values are plain (knob / index / percent), matching the
    parameter ranges.

    Plain-value reminders:
      threshold   0..10             timeconst   0..5  (switch positions 1..6)
      dcthresh   -10..10            drygain   -12..12 dB
      schpf: 0=Off 1=40Hz 2=60Hz 3=100Hz 4=150Hz 5=200Hz 6=300Hz 7=400Hz
      mix         0..100 %          irondrive   0..200 % (100=calibrated)
      mode: 0=L/R  1=Lat/Vert  2=Linked
      quality: 0=Eco  1=ZeroLat  2=Studio
*/
struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> channel; // base id -> value (both channels)
    std::vector<std::pair<juce::String, float>> global;   // full id -> value
};

inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets = {

        //------------------------------------------------------------------
        // 1. Blank slate
        //------------------------------------------------------------------
        { "Init", {}, {} },

        //------------------------------------------------------------------
        // 2–10. Original classics
        //------------------------------------------------------------------
        { "Vocal Leveler",
          { { pid::input, 0.0f }, { pid::threshold, 6.0f }, { pid::timeConstant, 2.0f },
            { pid::scHpf, 3.0f }, { pid::mix, 100.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Vocal Parallel",
          { { pid::threshold, 8.0f }, { pid::timeConstant, 0.0f }, { pid::scHpf, 2.0f },
            { pid::mix, 45.0f }, { pid::output, 2.0f }, { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Drum Bus Smash",
          { { pid::input, 2.0f }, { pid::threshold, 8.0f }, { pid::timeConstant, 0.0f },
            { pid::scHpf, 2.0f }, { pid::mix, 85.0f }, { pid::output, 2.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Drum Parallel Punch",
          { { pid::threshold, 9.0f }, { pid::timeConstant, 0.0f }, { pid::scHpf, 2.0f },
            { pid::mix, 35.0f }, { pid::output, 3.0f }, { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Mix Glue",
          { { pid::threshold, 5.0f }, { pid::timeConstant, 4.0f }, { pid::scHpf, 2.0f },
            { pid::mix, 100.0f }, { pid::output, 1.0f }, { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::quality, 2.0f } } },

        { "Master Bus",
          { { pid::threshold, 4.0f }, { pid::timeConstant, 5.0f }, { pid::scHpf, 0.0f },
            { pid::mix, 100.0f }, { pid::output, 0.0f }, { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::quality, 2.0f } } },

        { "Bass DI",
          { { pid::input, 1.0f }, { pid::threshold, 7.0f }, { pid::timeConstant, 1.0f },
            { pid::scHpf, 0.0f }, { pid::mix, 100.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Brick Limiter",
          { { pid::threshold, 10.0f }, { pid::timeConstant, 0.0f }, { pid::scHpf, 0.0f },
            { pid::mix, 100.0f }, { pid::output, 0.0f }, { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::quality, 2.0f } } },

        { "Clean Color",
          { { pid::input, 3.0f }, { pid::threshold, 5.0f }, { pid::mix, 100.0f },
            { pid::output, -2.0f }, { pid::compIn, 0.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 2.0f } } },

        //------------------------------------------------------------------
        // 11–15. First expansion (added in v2.1)
        //------------------------------------------------------------------
        { "Acoustic Guitar",
          { { pid::input, 1.0f }, { pid::threshold, 7.0f }, { pid::timeConstant, 2.0f },
            { pid::scHpf, 2.0f }, { pid::mix, 90.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Overhead Bus",
          { { pid::threshold, 8.5f }, { pid::timeConstant, 4.0f },
            { pid::scHpf, 3.0f }, { pid::mix, 100.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::linkAmount, 100.0f }, { pid::quality, 1.0f } } },

        { "Dialogue",
          { { pid::threshold, 7.0f }, { pid::timeConstant, 1.0f },
            { pid::scHpf, 3.0f }, { pid::mix, 100.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Room Mics",
          { { pid::threshold, 8.0f }, { pid::timeConstant, 5.0f },
            { pid::scHpf, 2.0f }, { pid::mix, 100.0f }, { pid::output, 2.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::linkAmount, 100.0f }, { pid::quality, 2.0f } } },

        { "Punch Bus",
          { { pid::input, 2.0f }, { pid::threshold, 9.0f }, { pid::timeConstant, 0.0f },
            { pid::scHpf, 2.0f }, { pid::mix, 40.0f }, { pid::output, 3.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::linkAmount, 100.0f }, { pid::quality, 1.0f } } },

        //------------------------------------------------------------------
        // 16–30. Second expansion (added in v2.2)
        //------------------------------------------------------------------

        // Vocals
        { "Lead Vocal Heavy",
          { { pid::threshold, 7.0f }, { pid::timeConstant, 1.0f },
            { pid::scHpf, 3.0f }, { pid::mix, 100.0f }, { pid::output, 1.5f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "BG Vocals",
          { { pid::threshold, 7.5f }, { pid::timeConstant, 2.0f }, { pid::dcThreshold, -4.0f },
            { pid::scHpf, 3.0f }, { pid::mix, 100.0f }, { pid::output, 0.5f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Sibilance Control",
          { { pid::threshold, 6.0f }, { pid::timeConstant, 0.0f },
            { pid::scHpf, 4.0f }, { pid::mix, 100.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        // Drums
        { "Kick Channel",
          { { pid::input, 1.0f }, { pid::threshold, 8.0f }, { pid::timeConstant, 0.0f },
            { pid::scHpf, 0.0f }, { pid::mix, 100.0f }, { pid::output, 2.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Snare Bus",
          { { pid::threshold, 7.0f }, { pid::timeConstant, 1.0f },
            { pid::scHpf, 3.0f }, { pid::mix, 100.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Hip Hop Crush",
          { { pid::input, 3.0f }, { pid::threshold, 9.0f }, { pid::timeConstant, 0.0f },
            { pid::scHpf, 2.0f }, { pid::mix, 30.0f }, { pid::dryGain, 3.0f },
            { pid::output, 4.0f }, { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::linkAmount, 100.0f }, { pid::quality, 1.0f } } },

        { "Full Drum Mix",
          { { pid::threshold, 8.0f }, { pid::timeConstant, 4.0f },
            { pid::scHpf, 3.0f }, { pid::mix, 100.0f }, { pid::output, 1.5f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::linkAmount, 100.0f },
            { pid::ironDrive, 130.0f }, { pid::quality, 1.0f } } },

        // Bass
        { "Electric Bass",
          { { pid::input, 1.0f }, { pid::threshold, 7.0f }, { pid::timeConstant, 1.0f },
            { pid::scHpf, 2.0f }, { pid::mix, 100.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Bass Parallel",
          { { pid::input, 2.0f }, { pid::threshold, 9.0f }, { pid::timeConstant, 0.0f },
            { pid::scHpf, 0.0f }, { pid::mix, 50.0f }, { pid::dryGain, 2.0f },
            { pid::output, 2.0f }, { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        // Keys / Piano
        { "Piano Sustain",
          { { pid::threshold, 6.0f }, { pid::timeConstant, 3.0f }, { pid::dcThreshold, -3.0f },
            { pid::scHpf, 1.0f }, { pid::mix, 100.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::linkAmount, 100.0f }, { pid::quality, 1.0f } } },

        // Guitar
        { "Electric Guitar",
          { { pid::threshold, 7.0f }, { pid::timeConstant, 1.0f },
            { pid::scHpf, 3.0f }, { pid::mix, 85.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        // Synths / Strings
        { "Synth Bus",
          { { pid::threshold, 6.0f }, { pid::timeConstant, 4.0f },
            { pid::scHpf, 2.0f }, { pid::mix, 100.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::linkAmount, 80.0f }, { pid::quality, 1.0f } } },

        { "String Section",
          { { pid::threshold, 6.0f }, { pid::timeConstant, 5.0f }, { pid::dcThreshold, -5.0f },
            { pid::scHpf, 1.0f }, { pid::mix, 100.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::linkAmount, 100.0f }, { pid::quality, 2.0f } } },

        // Mix / Master
        { "Broadcast Comp",
          { { pid::threshold, 5.0f }, { pid::timeConstant, 4.0f },
            { pid::scHpf, 3.0f }, { pid::mix, 100.0f }, { pid::output, 0.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::linkAmount, 100.0f }, { pid::quality, 2.0f } } },

        // Character extremes
        { "Lo-Fi Tape",
          { { pid::input, 4.0f }, { pid::threshold, 8.0f }, { pid::timeConstant, 2.0f },
            { pid::scHpf, 0.0f }, { pid::mix, 100.0f }, { pid::output, -2.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::ironDrive, 180.0f },
            { pid::noiseFloor, 1.0f }, { pid::quality, 2.0f } } },

        { "Transparent",
          { { pid::threshold, 5.0f }, { pid::timeConstant, 3.0f },
            { pid::scHpf, 0.0f }, { pid::mix, 100.0f }, { pid::output, 0.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::linkAmount, 100.0f },
            { pid::ironDrive, 30.0f }, { pid::quality, 2.0f } } },
    };

    return presets;
}

} // namespace tekk
