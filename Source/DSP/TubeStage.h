#pragma once

#include <algorithm>
#include <cmath>

namespace tekk
{

/*
    Tube types you can "roll" into the gain stage. Each is a small set of
    coefficients for the shared variable-mu shaper rather than a separate
    circuit:

        mu    - amplification factor: higher saturates sooner / harder
        asym  - even-harmonic (2nd) content from the curve's asymmetry
        head  - nominal headroom (plate voltage), scaled by the Voltage control

    These are voiced for musical variety, not measured from specific glass.
*/
struct TubeType
{
    const char* name;
    float mu;
    float asym;
    float head;
};

inline constexpr TubeType kTubeTypes[] =
{
    { "6386 (Stock)",      1.00f, 0.035f, 1.00f }, // the Fairchild remote-cutoff valve
    { "12AX7 (Hi-Gain)",   1.55f, 0.055f, 0.85f }, // hot, tight, 3rd-leaning
    { "12AU7 (Clean)",     0.72f, 0.030f, 1.35f }, // low-mu, lots of headroom
    { "6V6 (Warm)",        1.12f, 0.140f, 0.95f }, // 2nd-rich, fat
    { "EL84 (Chime)",      1.28f, 0.080f, 0.90f }, // bright, lively
    { "NOS Mullard (Smooth)", 0.92f, 0.060f, 1.18f }, // silky, gentle
};

inline constexpr int kNumTubeTypes = (int) (sizeof (kTubeTypes) / sizeof (kTubeTypes[0]));

/*
    Push-pull variable-mu gain-stage nonlinearity.

    The transconductance is set by the Drive (character) and the chosen tube's
    mu; the operating point is set by Bias; headroom by the plate Voltage. The
    bias offset is removed analytically (tanh(bias)) so moving it changes the
    curve's symmetry and gain -- as a real grid-bias adjustment does -- without
    a DC thump. Small-signal gain stays ~unity, so the controls change
    harmonic character and feel, not the clean level.
*/
class TubeStage
{
public:
    void prepare (double sampleRate)
    {
        constexpr double kPi = 3.14159265358979323846;
        dcCoeff = (float) (1.0 - std::exp (-2.0 * kPi * 5.0 / sampleRate));
        reset();
    }

    void reset() noexcept { dcState = 0.0f; }

    // base transconductance from the Drive/character control
    void setDrive (float newDrive) noexcept { drive = std::max (0.05f, newDrive); }

    // tube-roll coefficients + bias offset (set per block from the parameters)
    void setCoeffs (float newMu, float newAsym, float newHead, float newBias) noexcept
    {
        mu   = std::max (0.05f, newMu);
        asym = newAsym;
        head = std::max (0.1f, newHead);
        bias = newBias;
        tanhBias = std::tanh (bias);
    }

    float process (float x, float driveMul = 1.0f) noexcept
    {
        const float g  = std::max (1.0e-4f, drive * mu * driveMul);
        const float v  = head;
        const float xb = std::clamp (g * x / v, -6.0f, 6.0f);

        const float s      = std::tanh (xb + asym * xb * xb + bias) - tanhBias;
        const float shaped = v * s / g;

        dcState += dcCoeff * (shaped - dcState);
        return shaped - dcState;
    }

private:
    float drive = 1.0f;
    float mu = 1.0f, asym = 0.035f, head = 1.0f, bias = 0.0f, tanhBias = 0.0f;
    float dcCoeff = 0.0f;
    float dcState = 0.0f;
};

} // namespace tekk
