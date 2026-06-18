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
inline constexpr const char* compIn       = "compin";

// Global
inline constexpr const char* mode     = "mode";    // Left/Right, Lat/Vert, L/R Linked
inline constexpr const char* quality  = "quality"; // Eco, Zero-Latency, Studio
inline constexpr const char* drive    = "drive";   // tube + transformer saturation amount
inline constexpr const char* tubeType = "tubetype";// rolled valve type
inline constexpr const char* tubeBias = "tubebias";// grid bias / operating point
inline constexpr const char* tubeVolt = "tubevolt";// plate voltage / headroom
inline constexpr const char* purist   = "purist";  // takes the MK2 additions out of circuit
inline constexpr const char* bypass   = "bypass";  // true bypass

inline juce::String forChannel (const char* base, int channel)
{
    return juce::String (base) + (channel == 0 ? "A" : "B");
}

} // namespace tekk::pid
