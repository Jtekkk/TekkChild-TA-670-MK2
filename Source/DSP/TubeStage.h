#pragma once

#include <algorithm>
#include <cmath>

namespace tekk
{

/*
    Static nonlinearity for the push-pull tube stages.

    A matched push-pull pair cancels even harmonics, so the curve is
    dominated by smooth odd-order saturation (the tanh law of the
    differential pair). A small signal-dependent asymmetry models the
    real-world mismatch between the two halves and contributes the
    low-level 2nd harmonic measured on hardware units. The asymmetry
    rectifies slightly, so a DC blocker follows the shaper.
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

    // 1.0 = nominal operating level; higher values bias the stage hotter
    void setDrive (float newDrive) noexcept { drive = std::max (0.1f, newDrive); }

    float process (float x) noexcept
    {
        // the shaper is only meaningful within the tubes' swing; clamping
        // here also keeps the asymmetry term monotonic
        const float xd = std::clamp (drive * x, -4.0f, 4.0f);
        const float shaped = std::tanh (xd + kAsymmetry * xd * xd) / drive;

        dcState += dcCoeff * (shaped - dcState);
        return shaped - dcState;
    }

private:
    static constexpr float kAsymmetry = 0.05f;

    float drive = 1.0f;
    float dcCoeff = 0.0f;
    float dcState = 0.0f;
};

} // namespace tekk
