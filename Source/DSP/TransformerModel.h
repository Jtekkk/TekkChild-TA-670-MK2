#pragma once

#include <algorithm>
#include <cmath>

namespace tekk
{

/*
    Transformer colour model (UTC A26 input / Sowter output).

    Core flux density rises as frequency falls, so low frequencies
    saturate first. The signal is split with a one-pole crossover and the
    low band passes through a soft saturator while the high band stays
    clean -- this reproduces the LF-weighted harmonic profile measured on
    iron-core transformers. A 6 Hz high-pass models the open-circuit
    inductance roll-off and doubles as a DC blocker for the saturation
    products.
*/
class TransformerModel
{
public:
    void prepare (double sampleRate)
    {
        constexpr double kPi = 3.14159265358979323846;
        lpCoeff = (float) (1.0 - std::exp (-2.0 * kPi * kCrossoverHz / sampleRate));
        hpCoeff = (float) (1.0 - std::exp (-2.0 * kPi * 6.0 / sampleRate));
        reset();
    }

    void reset() noexcept { lpState = hpState = 0.0f; }

    // how hard nominal level pushes the core (input transformers see less
    // flux than output transformers driven by the power stage)
    void setDrive (float newDrive) noexcept { drive = std::max (0.1f, newDrive); }

    float process (float x) noexcept
    {
        lpState += lpCoeff * (x - lpState);
        const float lo = lpState;
        const float hi = x - lo;

        const float loSat = std::tanh (drive * lo) / drive;
        const float y = loSat + hi;

        hpState += hpCoeff * (y - hpState);
        return y - hpState;
    }

private:
    static constexpr float kCrossoverHz = 160.0f;

    float drive = 0.8f;
    float lpCoeff = 0.0f, hpCoeff = 0.0f;
    float lpState = 0.0f, hpState = 0.0f;
};

} // namespace tekk
