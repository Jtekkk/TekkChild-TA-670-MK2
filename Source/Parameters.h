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
inline constexpr const char* dryGain      = "drygain"; // dB trim for the dry side of the blend
inline constexpr const char* compIn       = "compin";

// Global
inline constexpr const char* mode      = "mode";    // Left/Right, Lat/Vert, L/R Linked
inline constexpr const char* quality   = "quality"; // Eco, Zero-Latency, Studio
inline constexpr const char* drive     = "drive";   // tube saturation amount
inline constexpr const char* ironDrive = "irondrive"; // transformer (iron) saturation amount
inline constexpr const char* tubeType  = "tubetype";// rolled valve type
inline constexpr const char* tubeBias  = "tubebias";// grid bias / operating point
inline constexpr const char* tubeVolt  = "tubevolt";// plate voltage / headroom
inline constexpr const char* noiseFloor = "noisefloor"; // valve hiss on/off
inline constexpr const char* linkAmount = "linkamount"; // 0..100 % blend toward full stereo link
inline constexpr const char* autoMakeup = "automakeup"; // auto makeup gain
inline constexpr const char* safety     = "safety";     // output soft-clip ceiling
inline constexpr const char* purist    = "purist";  // takes the MK2 additions out of circuit
inline constexpr const char* bypass    = "bypass";  // true bypass

// Tape Brain section
inline constexpr const char* tapeOn       = "tapeon";    // engage the tape section
inline constexpr const char* tapeParallel = "tapepar";   // throw switch: false=series, true=parallel
inline constexpr const char* tapeInput    = "tapein";    // dB
inline constexpr const char* tapeOutput   = "tapeout";   // dB (also the parallel blend level)
inline constexpr const char* tapeDrive    = "tapedrive"; // 0..10
inline constexpr const char* tapeSat      = "tapesat";   // 0..10
inline constexpr const char* tapeBias     = "tapebias";  // 0..10 (5 = nominal)
inline constexpr const char* tapeWow      = "tapewow";   // 0..10 wow & flutter
inline constexpr const char* tapeHiss     = "tapehiss";  // 0..10
inline constexpr const char* tapeNoise    = "tapenoise"; // 0..10
inline constexpr const char* tapeXtalk    = "tapextalk"; // 0..10 crosstalk
inline constexpr const char* tapeDegrade  = "tapedeg";   // 0..10

inline juce::String forChannel (const char* base, int channel)
{
    return juce::String (base) + (channel == 0 ? "A" : "B");
}

} // namespace tekk::pid
