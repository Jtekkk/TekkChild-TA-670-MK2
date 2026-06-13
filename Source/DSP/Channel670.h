#pragma once

#include <algorithm>
#include <cmath>

#include "ControlVoltageNetwork.h"
#include "SidechainFilter.h"
#include "TimeConstants.h"
#include "TransformerModel.h"
#include "TubeStage.h"

namespace tekk
{

struct ChannelParams
{
    float thresholdDb  = -17.0f; // detector reference level
    int   timeConstant = 1;      // 0..5 (switch positions 1..6)
    float kneeDb       = 4.75f;  // from the DC Threshold control
    float biasDb       = 0.0f;   // from the DC Threshold control
    float scHpfHz      = 0.0f;   // 0 = filter out of circuit
    bool  compIn       = true;   // "AGC in" -- gain stage still colours when out
};

/*
    One channel of the TA-670:

        input transformer -> variable-mu gain stage -> push-pull output
        tubes -> output transformer

    The detector follows the hardware's feedback topology: it rectifies
    the *output* of the gain stage rather than the input. Digitally this
    is realised with a one-sample delay, which at the oversampled rate
    sits far below the fastest (0.2 ms) attack time constant. Feedback
    detection is what gives the 670 its soft knee and its progressive
    ratio -- light material is barely touched, dense material is held.

    Processing is split into two phases per sample so a host processor
    can combine control voltages across channels (stereo link) before the
    gain is applied:

        1. computeControlVoltage()  -- condition input, rectify feedback
        2. renderWithControlVoltage() -- apply CV, run the audio path
*/
class Channel670
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;

        cvNetwork.prepare (sampleRate);
        inputTransformer.prepare (sampleRate);
        outputTransformer.prepare (sampleRate);
        tube.prepare (sampleRate);
        scFilter.prepare (sampleRate);

        inputTransformer.setDrive (0.45f);   // UTC A26: line level, modest flux
        outputTransformer.setDrive (0.8f);   // Sowter: driven by the power stage
        inputTransformer.setHfRolloff (60000.0f);  // essentially flat in band
        outputTransformer.setHfRolloff (30000.0f); // output iron sets the bandwidth
        tube.setDrive (0.7f);

        // ~15 ms linear ramp when AGC In is toggled, so the gain reduction
        // fades in/out instead of switching as a step
        engageInc = (float) (1.0 / (0.015 * fs));

        reset();
    }

    void reset() noexcept
    {
        cvNetwork.reset();
        inputTransformer.reset();
        outputTransformer.reset();
        tube.reset();
        scFilter.reset();
        feedbackSample = 0.0f;
        conditioned    = 0.0f;
        lastGrDb       = 0.0f;
        engage         = engageTarget;
    }

    void setParams (const ChannelParams& p)
    {
        params = p;
        cvNetwork.setPosition (p.timeConstant);
        scFilter.enabled = p.scHpfHz > 0.0f;
        if (scFilter.enabled)
            scFilter.setCutoff (p.scHpfHz);

        engageTarget = p.compIn ? 1.0f : 0.0f;
    }

    // Phase 1: condition the input and report this channel's control voltage.
    // Detector threshold / bias / knee arrive already smoothed, so automating
    // them glides the reference rather than stepping it once per block.
    float computeControlVoltage (float input, float thresholdDb, float biasDb,
                                 float kneeDb) noexcept
    {
        conditioned = inputTransformer.process (input);

        const float sc      = scFilter.process (feedbackSample);
        const float levelDb = 20.0f * std::log10 (std::abs (sc) + 1.0e-9f);
        const float overDb  = softOvershoot (levelDb - thresholdDb - biasDb, kneeDb);

        return cvNetwork.process (overDb); // smoothed dB over threshold
    }

    // Phase 2: apply the (possibly link-combined) control voltage (dB over thr).
    float renderWithControlVoltage (float cv) noexcept
    {
        engage += std::clamp (engageTarget - engage, -engageInc, engageInc);

        // Variable-mu law: as the rectified control voltage pulls the remote-
        // cutoff grids toward cutoff, transconductance falls and gain reduction
        // builds progressively, saturating toward a ceiling rather than holding
        // a fixed ratio -- a few dB come easily, the last few dB are hard, which
        // is what makes the 670 behave gently then limit.
        const float grDb = engage * kMaxGainReductionDb
                         * (1.0f - std::exp (-cv * kGrSlope));
        lastGrDb = grDb;

        // true to operation, the tube distorts more the harder it is biased
        // toward cutoff, so the unit thickens as it compresses
        const float driveMul   = 1.0f + kTubeGrDrive * (grDb * (1.0f / kMaxGainReductionDb));
        const float compressed = tube.process (dbToLin (-grDb) * conditioned, driveMul);

        feedbackSample = compressed;
        return outputTransformer.process (compressed);
    }

    float currentGainReductionDb() const noexcept { return lastGrDb; }

private:
    static float softOvershoot (float xDb, float kneeDb) noexcept
    {
        if (kneeDb < 0.1f)
            return std::max (0.0f, xDb);

        const float t = xDb / kneeDb;
        if (t > 20.0f)  return xDb;
        if (t < -20.0f) return 0.0f;

        return kneeDb * std::log1p (std::exp (t));
    }

    static float dbToLin (float db) noexcept
    {
        return std::pow (10.0f, db * (1.0f / 20.0f));
    }

    // calibration of the variable-mu gain-reduction law
    static constexpr float kMaxGainReductionDb = 22.0f; // ceiling the loop approaches
    static constexpr float kGrSlope            = 1.0f / 13.0f; // GR build per dB over threshold
    static constexpr float kTubeGrDrive        = 1.2f;  // extra tube curvature at full GR

    ChannelParams params;

    ControlVoltageNetwork cvNetwork;
    TransformerModel      inputTransformer;
    TransformerModel      outputTransformer;
    TubeStage             tube;
    SidechainFilter       scFilter;

    double fs            = 44100.0;
    float feedbackSample = 0.0f;
    float conditioned    = 0.0f;
    float lastGrDb       = 0.0f;

    float engage       = 1.0f; // current AGC-in factor (0..1)
    float engageTarget = 1.0f;
    float engageInc    = 1.0f;
};

} // namespace tekk
