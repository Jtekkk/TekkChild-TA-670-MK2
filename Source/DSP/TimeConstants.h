#pragma once

namespace tekk
{

/*
    Fairchild 670 time-constant selector.

    The six positions of the TIME CONSTANT switch select different RC
    networks on the control-voltage line that re-biases the variable-mu
    gain stage. Positions 1-4 are single-capacitor networks; positions 5
    and 6 add slower branches that only charge on sustained programme
    material, producing the famous program-dependent ("auto") release.

    Attack / release values follow the published hardware specification:

        1:  0.2 ms / 0.3 s
        2:  0.2 ms / 0.8 s
        3:  0.4 ms / 2 s
        4:  0.8 ms / 5 s
        5:  0.4 ms / 2 s for individual peaks, 10 s for multiple peaks
        6:  0.2 ms / 0.3 s for peaks, 10 s and 25 s for sustained programme

    The slow branches use longer charge times so that brief transients
    never reach them -- only dense material accumulates voltage there.
*/

struct CapBranch
{
    float attackMs;   // charge time constant of this branch
    float releaseSec; // discharge time constant of this branch
    float weight;     // share of the summed control voltage
};

struct TimeConstantPosition
{
    int numBranches;
    CapBranch branch[3];
};

constexpr TimeConstantPosition kTimeConstants[6] =
{
    { 1, { { 0.2f, 0.3f, 1.0f } } },
    { 1, { { 0.2f, 0.8f, 1.0f } } },
    { 1, { { 0.4f, 2.0f, 1.0f } } },
    { 1, { { 0.8f, 5.0f, 1.0f } } },
    { 2, { { 0.4f, 2.0f, 0.65f }, { 40.0f, 10.0f, 0.35f } } },
    { 3, { { 0.2f, 0.3f, 0.5f }, { 20.0f, 10.0f, 0.3f }, { 200.0f, 25.0f, 0.2f } } },
};

} // namespace tekk
