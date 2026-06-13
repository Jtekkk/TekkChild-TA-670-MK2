#pragma once

#include <algorithm>
#include <cmath>

namespace tekk
{

/*
    Static nonlinearity for the push-pull variable-mu (6386) gain stage.

    A matched push-pull pair cancels even harmonics, so the tube contribution
    is dominated by smooth odd-order (3rd) saturation -- the tanh law of the
    differential pair -- with only a small even (2nd) term from the unavoidable
    mismatch between the two halves. Because tanh(d*x)/d -> x for small x, the
    small-signal gain is ~unity regardless of drive: 'drive' sets how much
    harmonic content a given level produces, not the level itself.

    True to operation, the curvature is pushed harder as the stage is driven
    toward cutoff (more gain reduction), so the unit thickens as it works --
    the per-sample driveMul carries that from the control loop.
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

    float process (float x, float driveMul = 1.0f) noexcept
    {
        const float d = drive * driveMul;

        // the shaper is only meaningful within the tubes' swing; clamping
        // here also keeps the asymmetry term monotonic
        const float xd = std::clamp (d * x, -4.0f, 4.0f);
        const float shaped = std::tanh (xd + kAsymmetry * xd * xd) / d;

        dcState += dcCoeff * (shaped - dcState);
        return shaped - dcState;
    }

private:
    static constexpr float kAsymmetry = 0.035f; // small: push-pull cancels most even order

    float drive = 1.0f;
    float dcCoeff = 0.0f;
    float dcState = 0.0f;
};

} // namespace tekk
