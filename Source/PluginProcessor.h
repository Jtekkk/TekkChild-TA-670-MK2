#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "DSP/Channel670.h"
#include "DSP/TapeBrain.h"

class TekkChild670Processor : public juce::AudioProcessor,
                              private juce::AsyncUpdater
{
public:
    TekkChild670Processor();

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TekkChild TC-670 Vari-Mu Compressor"; }

    bool acceptsMidi() const override   { return false; }
    bool producesMidi() const override  { return false; }
    bool isMidiEffect() const override  { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    //==========================================================================
    // Per-block meter feeds for the editor (gain reduction needle + I/O bars).
    float getGainReductionDb (int channel) const noexcept
    {
        return grMeterDb[clampCh (channel)].load();
    }

    float getInputLevelDb (int channel) const noexcept
    {
        return inMeterDb[clampCh (channel)].load();
    }

    float getOutputLevelDb (int channel) const noexcept
    {
        return outMeterDb[clampCh (channel)].load();
    }

    // Tape Brain saturation level (0..1) for its VU meter.
    float getTapeSaturation() const noexcept { return tapeSatMeter.load(); }

    // 1176 limiter gain reduction (dB, >= 0) for its meter.
    float getLimiterGR() const noexcept { return limGrMeter.load(); }

    // Stereo Imager goniometer feed. Copies up to maxN of the most recent
    // mid/side sample pairs (newest last) into the caller's buffers and returns
    // the count. Lock-free best-effort: a benign race only flickers the scope.
    int getScopeData (float* midOut, float* sideOut, int maxN) const noexcept;

    // Hidden easter egg: the editor calls this (from the message thread) when
    // the mascot's nose is clicked six times; it plays the embedded clip once.
    void triggerEasterEgg();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void loadEasterEgg(); // decode + resample the embedded clip to the host rate

    // reportLatencyNow is true only when called from prepareToPlay (message
    // thread); from the audio thread we defer setLatencySamples to handleAsyncUpdate.
    void applyQualityMode (int qualityIndex, bool reportLatencyNow);
    void handleAsyncUpdate() override;
    tekk::ChannelParams paramsForChannel (int channel, bool purist) const;

    //==========================================================================
    tekk::Channel670 channels[2];

    // 4x oversamplers: minimum-phase IIR for the zero-latency mode,
    // linear-phase FIR for the studio mode
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplerIIR, oversamplerFIR;

    juce::AudioBuffer<float> dryBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay;

    // Tape Brain: a tape-colour section after / in parallel with the compressor.
    tekk::TapeBrain tapeBrain;
    juce::AudioBuffer<float> tapeBuffer; // parallel-path scratch
    std::atomic<float> tapeSatMeter { 0.0f };

    // Stereo Imager: Mono Maker + Stereo Enhance act on the side signal. The
    // side gain is smoothed so width moves glide instead of zippering.
    juce::SmoothedValue<float> sideGainSm;

    // Goniometer ring: the audio thread writes the post-imaging mid/side here;
    // the editor's CRT reads the most recent window. Plain arrays published via
    // an atomic write index (single producer / single consumer).
    static constexpr int kScopeSize = 512; // power of two for cheap masking
    float scopeMid  [kScopeSize] {};
    float scopeSide [kScopeSize] {};
    std::atomic<int> scopeWritePos { 0 };

    juce::SmoothedValue<float> inputGainSm[2], outputGainSm[2], mixSm[2], dryGainSm[2];

    // Detector controls are smoothed at the (possibly oversampled) detector
    // rate so automating Threshold / DC Threshold glides instead of zippering.
    juce::SmoothedValue<float> thresholdSm[2], biasSm[2], kneeSm[2];

    // Link amount (0..1) consumed per OS sample in the CV loop.
    juce::SmoothedValue<float> linkAmountSm;

    static int clampCh (int ch) noexcept { return ch < 0 ? 0 : (ch > 1 ? 1 : ch); }

    std::atomic<float> grMeterDb[2] { 0.0f, 0.0f };
    std::atomic<float> inMeterDb[2] { -100.0f, -100.0f };
    std::atomic<float> outMeterDb[2] { -100.0f, -100.0f };

    // easter-egg playback: clip decoded once (message thread), mixed into the
    // output (audio thread) when armed
    juce::AudioBuffer<float> eggBuffer;
    bool eggLoaded = false;
    std::atomic<bool> eggArm { false };
    bool eggActive = false;
    int  eggPos = 0;

    int currentProgram = 0;

    double baseSampleRate = 44100.0;
    int    maxBlockSize   = 512;
    int    activeQuality  = -1;
    int    latencySamples = 0;
    std::atomic<int> pendingLatency { 0 };

    // cached raw parameter values
    std::atomic<float>* pInput[2] {};
    std::atomic<float>* pThreshold[2] {};
    std::atomic<float>* pTimeConstant[2] {};
    std::atomic<float>* pDcThreshold[2] {};
    std::atomic<float>* pScHpf[2] {};
    std::atomic<float>* pMix[2] {};
    std::atomic<float>* pOutput[2] {};
    std::atomic<float>* pDryGain[2] {};
    std::atomic<float>* pCompIn[2] {};
    std::atomic<float>* pMode {};
    std::atomic<float>* pQuality {};
    std::atomic<float>* pDrive {};
    std::atomic<float>* pIronDrive {};
    std::atomic<float>* pTubeType {};
    std::atomic<float>* pTubeBias {};
    std::atomic<float>* pTubeVolt {};
    std::atomic<float>* pNoiseFloor {};
    std::atomic<float>* pLinkAmount {};
    std::atomic<float>* pAutoMk {};
    std::atomic<float>* pSafety {};
    std::atomic<float>* pPurist {};

    // tape brain
    std::atomic<float>* pTapeOn {};
    std::atomic<float>* pTapePar {};
    std::atomic<float>* pTapeIn {};
    std::atomic<float>* pTapeOut {};
    std::atomic<float>* pTapeDrive {};
    std::atomic<float>* pTapeSat {};
    std::atomic<float>* pTapeBias {};
    std::atomic<float>* pTapeWow {};
    std::atomic<float>* pTapeHiss {};
    std::atomic<float>* pTapeNoise {};
    std::atomic<float>* pTapeXtalk {};
    std::atomic<float>* pTapeDeg {};

    // stereo imager
    std::atomic<float>* pMonoMaker {};
    std::atomic<float>* pStereoEnh {};

    // 1176 limiter
    std::atomic<float>* pLimitThrottle {};
    float limiterGain = 1.0f;           // current brick-wall gain (instant attack)
    float limRelCoef  = 0.0f;           // per-sample release coefficient
    std::atomic<float> limGrMeter { 0.0f };

    float characterCurrent = 1.0f; // smoothed tube/transformer drive scaler

    // smoothed tube-roll coefficients (glide between valve types / bias / voltage)
    float tubeMuCur = 1.0f, tubeAsymCur = 0.035f, tubeHeadCur = 1.0f, tubeBiasCur = 0.0f;

    // auto-makeup: slow per-channel estimate of the gain reduction to add back
    float makeupGrDb[2] { 0.0f, 0.0f };

    // gentle output ceiling, linear below the knee then asymptotic to ~0 dBFS
    static float safetyClip (float x) noexcept
    {
        constexpr float t = 0.65f, c = 0.985f;
        const float a = std::abs (x);
        if (a <= t)
            return x;
        return (x < 0.0f ? -1.0f : 1.0f) * (t + (c - t) * std::tanh ((a - t) / (c - t)));
    }

    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TekkChild670Processor)
};
