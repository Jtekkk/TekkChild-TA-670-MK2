#pragma once

#include <algorithm>
#include <cmath>

namespace tekk
{

/*
    2nd-order TPT (Zavalishin) state-variable high-pass used on the
    detector sidechain, so low frequencies can be kept from pumping the
    compressor without touching the audio path.
*/
class SidechainFilter
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        setCutoff (cutoffHz);
        reset();
    }

    void reset() noexcept { s1 = s2 = 0.0f; }

    void setCutoff (float hz)
    {
        cutoffHz = hz;
        constexpr double kPi = 3.14159265358979323846;
        const double limited = std::min ((double) hz, 0.45 * fs);

        g  = (float) std::tan (kPi * limited / fs);
        a1 = 1.0f / (1.0f + g * (g + kDamping));
        a2 = g * a1;
        a3 = g * a2;
    }

    bool enabled = false;

    float process (float x) noexcept
    {
        if (! enabled)
            return x;

        const float v3 = x - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;

        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;

        return x - kDamping * v1 - v2;
    }

private:
    static constexpr float kDamping = 1.41421356f; // Butterworth response

    double fs = 44100.0;
    float cutoffHz = 100.0f;
    float g = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;
};

} // namespace tekk
