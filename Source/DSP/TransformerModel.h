#pragma once

#include <algorithm>
#include <cmath>

namespace tekk
{

/*
    Transformer colour model (UTC A26 input / Sowter output).

    Core flux density rises as frequency falls, so low frequencies saturate
    first. The signal is split with a one-pole crossover and the low band
    passes through an *asymmetric* soft saturator while the high band stays
    clean -- this reproduces the LF-weighted, 2nd-harmonic-rich profile that
    gives the iron its warmth (unlike the push-pull tubes, a transformer is
    not balanced, so it makes 2nd as well as 3rd).

    The band edges follow the unit's real bandwidth: a high-pass models the
    open-circuit inductance roll-off (and blocks the saturation DC), and a
    gentle low-pass models the leakage-inductance / winding-capacitance
    roll-off at the top, so the model is not flat to Nyquist the way ideal
    maths would be.
*/
class TransformerModel
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        updateCoeffs();
        reset();
    }

    void reset() noexcept { lpState = hpState = hfState = 0.0f; }

    // how hard nominal level pushes the core (input transformers see less
    // flux than output transformers driven by the power stage)
    void setDrive (float newDrive) noexcept { drive = std::max (0.1f, newDrive); }

    // top-end bandwidth corner (Hz); the output iron sets the system bandwidth
    void setHfRolloff (float hz) noexcept { hfHz = hz; if (fs > 0.0) updateCoeffs(); }

    float process (float x) noexcept
    {
        // bandwidth limit (HF roll-off from leakage L / winding C)
        hfState += hfCoeff * (x - hfState);
        const float band = hfState;

        // LF-weighted core saturation, slightly asymmetric -> 2nd + 3rd
        lpState += lpCoeff * (band - lpState);
        const float lo = lpState;
        const float hi = band - lo;

        const float dl    = drive * lo;
        const float loSat = std::tanh (dl + kCoreAsym * dl * dl) / drive;
        const float y     = loSat + hi;

        // open-circuit inductance roll-off, doubles as DC blocker
        hpState += hpCoeff * (y - hpState);
        return y - hpState;
    }

private:
    void updateCoeffs() noexcept
    {
        constexpr double kPi = 3.14159265358979323846;
        lpCoeff = (float) (1.0 - std::exp (-2.0 * kPi * kCrossoverHz / fs));
        hpCoeff = (float) (1.0 - std::exp (-2.0 * kPi * kHpHz / fs));
        hfCoeff = (float) (1.0 - std::exp (-2.0 * kPi * std::min ((double) hfHz, 0.48 * fs) / fs));
    }

    static constexpr float kCrossoverHz = 160.0f; // below here the core saturates
    static constexpr float kHpHz        = 6.0f;   // open-circuit inductance roll-off
    static constexpr float kCoreAsym    = 0.13f;  // 2nd-harmonic share of the iron

    double fs = 44100.0;
    float drive = 0.8f;
    float hfHz  = 30000.0f;
    float lpCoeff = 0.0f, hpCoeff = 0.0f, hfCoeff = 0.0f;
    float lpState = 0.0f, hpState = 0.0f, hfState = 0.0f;
};

} // namespace tekk
