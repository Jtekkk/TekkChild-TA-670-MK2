#pragma once

#include <juce_core/juce_core.h>

namespace tekk::pid
{

// Per-channel parameters get an 'A' (Left / Lateral) or 'B'
// (Right / Vertical) suffix appended with forChannel().
inline constexpr const char* input        = "input";
inline constexpr const char* threshold    = "threshold";
inline constexpr const char* timeConstant = "timeconst";
inline constexpr const char* dcThreshold  = "dcthresh";
inline constexpr const char* scHpf        = "schpf";
inline constexpr const char* mix          = "mix";
inline constexpr const char* output       = "output";
inline constexpr const char* dryGain      = "drygain";  // dB trim for the dry side of the blend
inline constexpr const char* compIn       = "compin";

// Global
inline constexpr const char* mode       = "mode";       // Left/Right, Lat/Vert, L/R Linked
inline constexpr const char* linkAmount = "linkamount"; // 0..100 % blend toward full stereo link
inline constexpr const char* ironDrive  = "irondrive";  // 0..200 % transformer + tube saturation
inline constexpr const char* noiseFloor = "noisefloor"; // valve hiss on/off
inline constexpr const char* quality    = "quality";    // Eco, Zero-Latency, Studio
inline constexpr const char* purist     = "purist";     // takes the MK2 additions out of circuit
inline constexpr const char* bypass     = "bypass";     // true bypass

inline juce::String forChannel (const char* base, int channel)
{
    return juce::String (base) + (channel == 0 ? "A" : "B");
}

} // namespace tekk::pid
