#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "DSP/Channel670.h"

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

    const juce::String getName() const override { return "TekkChild TA-670 MK2"; }

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

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

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

    juce::SmoothedValue<float> inputGainSm[2], outputGainSm[2], mixSm[2];

    // Detector controls are smoothed at the (possibly oversampled) detector
    // rate so automating Threshold / DC Threshold glides instead of zippering.
    juce::SmoothedValue<float> thresholdSm[2], biasSm[2], kneeSm[2];

    // Link amount (0..1) consumed per OS sample in the CV loop.
    juce::SmoothedValue<float> linkAmountSm;

    static int clampCh (int ch) noexcept { return ch < 0 ? 0 : (ch > 1 ? 1 : ch); }

    std::atomic<float> grMeterDb[2] { 0.0f, 0.0f };
    std::atomic<float> inMeterDb[2] { -100.0f, -100.0f };
    std::atomic<float> outMeterDb[2] { -100.0f, -100.0f };

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
    std::atomic<float>* pCompIn[2] {};
    std::atomic<float>* pMode {};
    std::atomic<float>* pLinkAmount {};
    std::atomic<float>* pQuality {};
    std::atomic<float>* pPurist {};

    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TekkChild670Processor)
};
