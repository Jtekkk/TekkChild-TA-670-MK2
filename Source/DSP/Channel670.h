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
        cvNetwork.prepare (sampleRate);
        inputTransformer.prepare (sampleRate);
        outputTransformer.prepare (sampleRate);
        tube.prepare (sampleRate);
        scFilter.prepare (sampleRate);

        inputTransformer.setDrive (0.45f);  // UTC A26: line level, modest flux
        outputTransformer.setDrive (0.9f);  // Sowter: driven by the power stage
        tube.setDrive (0.7f);

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
    }

    void setParams (const ChannelParams& p)
    {
        params = p;
        cvNetwork.setPosition (p.timeConstant);
        scFilter.enabled = p.scHpfHz > 0.0f;
        if (scFilter.enabled)
            scFilter.setCutoff (p.scHpfHz);
    }

    // Phase 1: condition the input and report this channel's control voltage.
    float computeControlVoltage (float input) noexcept
    {
        conditioned = inputTransformer.process (input);

        const float sc      = scFilter.process (feedbackSample);
        const float levelDb = 20.0f * std::log10 (std::abs (sc) + 1.0e-9f);
        const float overDb  = softOvershoot (levelDb - params.thresholdDb - params.biasDb,
                                             params.kneeDb);

        return cvNetwork.process (overDb * (1.0f / 24.0f)); // 24 dB over -> CV 1.0
    }

    // Phase 2: apply the (possibly link-combined) control voltage.
    float renderWithControlVoltage (float cv) noexcept
    {
        // Variable-mu law: the CV drives the remote-cutoff grids towards
        // cutoff, so transconductance -- and with it the ratio -- falls
        // progressively rather than at a fixed slope.
        const float grDb = std::min (kMaxGainReductionDb, kCvToDb * cv * (1.0f + cv));
        lastGrDb = params.compIn ? grDb : 0.0f;

        const float gain       = params.compIn ? dbToLin (-grDb) : 1.0f;
        const float compressed = tube.process (gain * conditioned);

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

    // calibration of the CV-to-gain-reduction law
    static constexpr float kCvToDb              = 36.0f;
    static constexpr float kMaxGainReductionDb  = 30.0f;

    ChannelParams params;

    ControlVoltageNetwork cvNetwork;
    TransformerModel      inputTransformer;
    TransformerModel      outputTransformer;
    TubeStage             tube;
    SidechainFilter       scFilter;

    float feedbackSample = 0.0f;
    float conditioned    = 0.0f;
    float lastGrDb       = 0.0f;
};

} // namespace tekk
