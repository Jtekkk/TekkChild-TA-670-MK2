#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tekk
{

/*
    "Tape Brain" -- a stereo tape-machine colour section that can run after the
    compressor (series) or alongside it (parallel). It models the things that
    make tape tape:

        drive + saturation  magnetic S-curve compression and harmonics
        bias                operating point: under-bias dirtier + duller,
                            over-bias cleaner
        wow & flutter       slow + fast pitch modulation (a modulated delay)
        hiss                steady HF tape noise
        noise               broadband hum / grit
        crosstalk           adjacent-track bleed between L and R
        degrade             worn tape: HF loss + intermittent dropouts

    Everything is RT-safe (no allocation in process). Parameters arrive 0..1
    (the editor maps the 0..10 knobs); input/output are linear gains.
*/
class TapeBrain
{
public:
    struct Params
    {
        float inputGain  = 1.0f;
        float outputGain = 1.0f;
        float drive      = 0.0f; // 0..1
        float saturation = 0.0f; // 0..1
        float bias       = 0.5f; // 0..1 (0.5 = nominal)
        float wowFlutter = 0.0f; // 0..1
        float hiss       = 0.0f; // 0..1
        float noise      = 0.0f; // 0..1
        float crosstalk  = 0.0f; // 0..1
        float degrade    = 0.0f; // 0..1
    };

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;

        // modulated delay buffer: enough for the deepest wow plus headroom
        const int len = std::max (256, (int) (0.05 * sampleRate)); // 50 ms
        for (auto& d : delay)
        {
            d.assign ((size_t) len, 0.0f);
        }
        delayLen = len;

        constexpr float kPi = 3.14159265358979323846f;
        hissCoeff   = 1.0f - std::exp (-2.0f * kPi * 3000.0f / fs); // hiss tilt
        xtalkCoeff  = 1.0f - std::exp (-2.0f * kPi * 6000.0f / fs); // crosstalk HF roll
        dropoutRate = 1.0f - std::exp (-1.0f / (0.5f * fs));        // dropout LFO smoothing

        reset();
    }

    void reset() noexcept
    {
        for (auto& d : delay)
            std::fill (d.begin(), d.end(), 0.0f);

        writePos   = 0;
        wowPhase   = 0.0f;
        flutPhase  = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            lossState[c] = 0.0f;
            hissState[c] = 0.0f;
            xtalkState[c] = 0.0f;
            dropEnv[c]   = 1.0f;
            satEnv[c]    = 0.0f;
        }
        meterValue = 0.0f;
    }

    void setParams (const Params& p) noexcept { params = p; }

    float saturationMeter() const noexcept { return meterValue; } // 0..1 for the VU

    void process (float* left, float* right, int n) noexcept
    {
        const auto& p = params;

        // derived amounts
        const float drive  = 1.0f + p.drive * 7.0f;           // 1x..8x into the saturator
        const float satAmt = 0.15f + p.saturation * 2.2f;     // saturator curvature
        const float biasN  = p.bias;                          // 0..1 (0.5 nominal)
        const float asym   = (0.5f - biasN) * 0.6f;           // under-bias adds 2nd harmonic
        const float makeup = 1.0f / satAmt;                   // unity small-signal gain (tanh(g x)/g)

        // wow (slow) + flutter (fast) modulation depth, in samples
        const float wowDepth  = p.wowFlutter * 0.004f * fs;   // up to ~4 ms
        const float flutDepth = p.wowFlutter * 0.0006f * fs;  // up to ~0.6 ms
        const float wowInc    = 2.0f * 3.14159265f * 0.9f  / fs; // ~0.9 Hz
        const float flutInc   = 2.0f * 3.14159265f * 7.5f  / fs; // ~7.5 Hz
        const float baseDelay = wowDepth + flutDepth + 2.0f;     // keep read behind write

        // degrade: HF loss + dropout depth + a little extra grit
        const float lossCut   = 1.0f - p.degrade * 0.9f;      // 1 = open, ->0.1 dull
        const float lossCoeff = std::clamp (lossCut * lossCut, 0.02f, 1.0f);
        const float dropDepth = p.degrade * 0.6f;

        const float hissAmt  = p.hiss  * 0.02f;   // ~ -34 dB max
        const float noiseAmt = p.noise * 0.05f;   // broadband, a bit louder
        const float xtalkAmt = p.crosstalk * 0.5f;
        const float inG = p.inputGain, outG = p.outputGain;

        float meterPeak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            wowPhase  += wowInc;  if (wowPhase  > 6.2831853f) wowPhase  -= 6.2831853f;
            flutPhase += flutInc; if (flutPhase > 6.2831853f) flutPhase -= 6.2831853f;
            const float modL = baseDelay + wowDepth * std::sin (wowPhase)
                                         + flutDepth * std::sin (flutPhase);
            const float modR = baseDelay + wowDepth * std::sin (wowPhase + 0.6f)
                                         + flutDepth * std::sin (flutPhase + 1.1f);

            float s[2] = { left[i] * inG, right[i] * inG };
            float pre[2];

            for (int c = 0; c < 2; ++c)
            {
                // write to the modulated delay, read a fractional sample back
                delay[c][(size_t) writePos] = s[c];
                const float rd = modulatedRead (c, c == 0 ? modL : modR);

                // tape saturation (asymmetric S-curve), level-compensated
                const float driven = rd * drive;
                float sat = std::tanh (satAmt * (driven + asym * driven * driven)) * makeup / drive;

                // HF loss from wear (one-pole LP, opens as degrade -> 0)
                lossState[c] += lossCoeff * (sat - lossState[c]);
                sat = lossState[c];

                pre[c] = sat;
                meterPeak = std::max (meterPeak, std::abs (sat));
            }

            // dropouts: a slow random envelope that occasionally dips
            for (int c = 0; c < 2; ++c)
            {
                const float target = (whiteNoise (c) > 0.985f) ? (1.0f - dropDepth) : 1.0f;
                dropEnv[c] += dropoutRate * (target - dropEnv[c]);
                pre[c] *= dropEnv[c];
            }

            // crosstalk: HF-rolled bleed of the opposite channel
            xtalkState[0] += xtalkCoeff * (pre[1] - xtalkState[0]);
            xtalkState[1] += xtalkCoeff * (pre[0] - xtalkState[1]);
            float out0 = pre[0] + xtalkAmt * xtalkState[0];
            float out1 = pre[1] + xtalkAmt * xtalkState[1];

            // hiss (HF) + broadband noise
            hissState[0] += hissCoeff * (whiteNoise (0) * 2.0f - 1.0f - hissState[0]);
            hissState[1] += hissCoeff * (whiteNoise (1) * 2.0f - 1.0f - hissState[1]);
            out0 += hissAmt * (whiteNoise (0) * 2.0f - 1.0f - hissState[0]) + noiseAmt * (whiteNoise (0) * 2.0f - 1.0f);
            out1 += hissAmt * (whiteNoise (1) * 2.0f - 1.0f - hissState[1]) + noiseAmt * (whiteNoise (1) * 2.0f - 1.0f);

            left[i]  = out0 * outG;
            right[i] = out1 * outG;

            if (++writePos >= delayLen) writePos = 0;
        }

        // VU ballistics for the TAPE SATURATION meter
        const float target = std::min (1.0f, meterPeak);
        meterValue += (target > meterValue ? 0.5f : 0.05f) * (target - meterValue);
    }

private:
    float modulatedRead (int c, float delaySamples) noexcept
    {
        float readPos = (float) writePos - delaySamples;
        while (readPos < 0.0f) readPos += (float) delayLen;

        const int   i0 = (int) readPos;
        const float fr = readPos - (float) i0;
        const int   i1 = (i0 + 1) % delayLen;
        return delay[c][(size_t) i0] + fr * (delay[c][(size_t) i1] - delay[c][(size_t) i0]);
    }

    // xorshift32 white noise in [0,1)
    float whiteNoise (int c) noexcept
    {
        uint32_t& s = rng[c];
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) s * (1.0f / 4294967296.0f);
    }

    Params params;

    float fs = 44100.0f;
    int   delayLen = 0;
    int   writePos = 0;
    std::vector<float> delay[2];

    float wowPhase = 0.0f, flutPhase = 0.0f;
    float lossState[2] {}, hissState[2] {}, xtalkState[2] {}, dropEnv[2] { 1.0f, 1.0f }, satEnv[2] {};
    float hissCoeff = 0.0f, xtalkCoeff = 0.0f, dropoutRate = 0.0f;
    uint32_t rng[2] { 0x12345678u, 0x9E3779B9u };

    float meterValue = 0.0f;
};

} // namespace tekk
