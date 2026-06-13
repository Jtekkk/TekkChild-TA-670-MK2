#pragma once

#include <cmath>
#include "TimeConstants.h"

namespace tekk
{

/*
    The capacitor network on the 670's control-voltage line.

    Each branch is a one-pole integrator that charges towards the rectified
    sidechain drive with its attack time constant and discharges with its
    release time constant. The voltage seen by the variable-mu stage is the
    weighted sum of the branches, so on positions 5 and 6 short peaks ride
    on the fast branch while sustained material accumulates on the slow
    branches and holds the unit in gentle gain reduction.
*/
class ControlVoltageNetwork
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        updateCoefficients();
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : state)
            s = 0.0f;
    }

    void setPosition (int newPosition) // 0..5
    {
        newPosition = newPosition < 0 ? 0 : (newPosition > 5 ? 5 : newPosition);

        if (newPosition != position)
        {
            position = newPosition;
            updateCoefficients();
        }
    }

    float process (float rectifiedDrive) noexcept
    {
        const auto& tc = kTimeConstants[position];
        float cv = 0.0f;

        for (int i = 0; i < tc.numBranches; ++i)
        {
            const float coeff = rectifiedDrive > state[i] ? attackCoeff[i] : releaseCoeff[i];
            state[i] += coeff * (rectifiedDrive - state[i]);
            cv += tc.branch[i].weight * state[i];
        }

        return cv;
    }

private:
    void updateCoefficients()
    {
        const auto& tc = kTimeConstants[position];

        for (int i = 0; i < 3; ++i)
        {
            if (i < tc.numBranches)
            {
                attackCoeff[i]  = onePoleCoeff (tc.branch[i].attackMs * 0.001);
                releaseCoeff[i] = onePoleCoeff (tc.branch[i].releaseSec);
            }
            else
            {
                // branch not present at this position: drain any stale charge
                attackCoeff[i] = releaseCoeff[i] = 0.0f;
                state[i] = 0.0f;
            }
        }
    }

    float onePoleCoeff (double seconds) const
    {
        return (float) (1.0 - std::exp (-1.0 / (seconds * fs)));
    }

    double fs = 44100.0;
    int position = 1;
    float state[3] {};
    float attackCoeff[3] {};
    float releaseCoeff[3] {};
};

} // namespace tekk
