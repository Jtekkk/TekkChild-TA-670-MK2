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
      threshold   0..10            timeconst 0..5  (switch positions 1..6)
      schpf       0=Off 1..4 = 60/100/200/300 Hz   mix 0..100
      mode        0=L/R 1=Lat/Vert 2=Linked        quality 0=Eco 1=ZeroLat 2=Studio
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
        { "Init", {}, {} },

        { "Vocal Leveler",
          { { pid::input, 0.0f }, { pid::threshold, 6.0f }, { pid::timeConstant, 2.0f },
            { pid::scHpf, 2.0f }, { pid::mix, 100.0f }, { pid::output, 1.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Vocal Parallel",
          { { pid::threshold, 8.0f }, { pid::timeConstant, 0.0f }, { pid::scHpf, 1.0f },
            { pid::mix, 45.0f }, { pid::output, 2.0f }, { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Drum Bus Smash",
          { { pid::input, 2.0f }, { pid::threshold, 8.0f }, { pid::timeConstant, 0.0f },
            { pid::scHpf, 1.0f }, { pid::mix, 85.0f }, { pid::output, 2.0f },
            { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f }, { pid::drive, 64.0f },
            { pid::tubeType, 1.0f }, { pid::tubeBias, 18.0f } } },

        { "Drum Parallel Punch",
          { { pid::threshold, 9.0f }, { pid::timeConstant, 0.0f }, { pid::scHpf, 1.0f },
            { pid::mix, 35.0f }, { pid::output, 3.0f }, { pid::compIn, 1.0f } },
          { { pid::mode, 0.0f }, { pid::quality, 1.0f } } },

        { "Mix Glue",
          { { pid::threshold, 5.0f }, { pid::timeConstant, 4.0f }, { pid::scHpf, 1.0f },
            { pid::mix, 100.0f }, { pid::output, 1.0f }, { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::quality, 2.0f } } },

        { "Master Bus",
          { { pid::threshold, 4.0f }, { pid::timeConstant, 5.0f }, { pid::scHpf, 0.0f },
            { pid::mix, 100.0f }, { pid::output, 0.0f }, { pid::compIn, 1.0f } },
          { { pid::mode, 2.0f }, { pid::quality, 2.0f }, { pid::drive, 42.0f },
            { pid::tubeType, 5.0f }, { pid::tubeVolt, 305.0f } } },

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
          { { pid::mode, 0.0f }, { pid::quality, 2.0f }, { pid::drive, 80.0f },
            { pid::tubeType, 3.0f }, { pid::tubeBias, 28.0f } } },
    };

    return presets;
}

} // namespace tekk
