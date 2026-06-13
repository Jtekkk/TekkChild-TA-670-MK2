#include "PluginProcessor.h"

#include "Parameters.h"
#include "PluginEditor.h"

namespace
{
constexpr float kScHpfTable[] = { 0.0f, 60.0f, 100.0f, 200.0f, 300.0f };

// THRESHOLD is a 0..10 control as on the hardware: 0 leaves the programme
// untouched at typical levels, 10 pulls the detector reference down to
// -32 dBFS for heavy limiting.
float thresholdKnobToDb (float knob)
{
    return -2.0f - 3.0f * knob;
}
} // namespace

//==============================================================================
TekkChild670Processor::TekkChild670Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    using namespace tekk;

    for (int ch = 0; ch < 2; ++ch)
    {
        pInput[ch]        = apvts.getRawParameterValue (pid::forChannel (pid::input, ch));
        pThreshold[ch]    = apvts.getRawParameterValue (pid::forChannel (pid::threshold, ch));
        pTimeConstant[ch] = apvts.getRawParameterValue (pid::forChannel (pid::timeConstant, ch));
        pDcThreshold[ch]  = apvts.getRawParameterValue (pid::forChannel (pid::dcThreshold, ch));
        pScHpf[ch]        = apvts.getRawParameterValue (pid::forChannel (pid::scHpf, ch));
        pMix[ch]          = apvts.getRawParameterValue (pid::forChannel (pid::mix, ch));
        pOutput[ch]       = apvts.getRawParameterValue (pid::forChannel (pid::output, ch));
        pCompIn[ch]       = apvts.getRawParameterValue (pid::forChannel (pid::compIn, ch));
    }

    pMode    = apvts.getRawParameterValue (pid::mode);
    pQuality = apvts.getRawParameterValue (pid::quality);
    pPurist  = apvts.getRawParameterValue (pid::purist);

    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (pid::bypass));
    jassert (bypassParam != nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout TekkChild670Processor::createParameterLayout()
{
    using namespace tekk;
    namespace j = juce;

    j::AudioProcessorValueTreeState::ParameterLayout layout;

    const juce::StringArray timeConstantChoices {
        "1 - 0.2 ms / 0.3 s", "2 - 0.2 ms / 0.8 s", "3 - 0.4 ms / 2 s",
        "4 - 0.8 ms / 5 s",   "5 - auto (2 s / 10 s)", "6 - auto (0.3 s / 10 s / 25 s)"
    };
    const juce::StringArray hpfChoices { "Off", "60 Hz", "100 Hz", "200 Hz", "300 Hz" };

    for (int ch = 0; ch < 2; ++ch)
    {
        const juce::String sfx = ch == 0 ? " A" : " B";
        auto group = std::make_unique<j::AudioProcessorParameterGroup> (
            ch == 0 ? "channelA" : "channelB",
            ch == 0 ? "Channel A (Left / Lat)" : "Channel B (Right / Vert)",
            "|");

        group->addChild (std::make_unique<j::AudioParameterFloat> (
            j::ParameterID (pid::forChannel (pid::input, ch), 1), "Input" + sfx,
            j::NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
            j::AudioParameterFloatAttributes().withLabel ("dB")));

        group->addChild (std::make_unique<j::AudioParameterFloat> (
            j::ParameterID (pid::forChannel (pid::threshold, ch), 1), "Threshold" + sfx,
            j::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f,
            j::AudioParameterFloatAttributes()));

        group->addChild (std::make_unique<j::AudioParameterChoice> (
            j::ParameterID (pid::forChannel (pid::timeConstant, ch), 1), "Time Constant" + sfx,
            timeConstantChoices, 1));

        group->addChild (std::make_unique<j::AudioParameterFloat> (
            j::ParameterID (pid::forChannel (pid::dcThreshold, ch), 1), "DC Threshold" + sfx,
            j::NormalisableRange<float> (-10.0f, 10.0f, 0.1f), 0.0f,
            j::AudioParameterFloatAttributes()));

        group->addChild (std::make_unique<j::AudioParameterChoice> (
            j::ParameterID (pid::forChannel (pid::scHpf, ch), 1), "SC High-Pass" + sfx,
            hpfChoices, 0));

        group->addChild (std::make_unique<j::AudioParameterFloat> (
            j::ParameterID (pid::forChannel (pid::mix, ch), 1), "Mix" + sfx,
            j::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
            j::AudioParameterFloatAttributes().withLabel ("%")));

        group->addChild (std::make_unique<j::AudioParameterFloat> (
            j::ParameterID (pid::forChannel (pid::output, ch), 1), "Output" + sfx,
            j::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
            j::AudioParameterFloatAttributes().withLabel ("dB")));

        group->addChild (std::make_unique<j::AudioParameterBool> (
            j::ParameterID (pid::forChannel (pid::compIn, ch), 1), "AGC In" + sfx, true));

        layout.add (std::move (group));
    }

    layout.add (std::make_unique<j::AudioParameterChoice> (
        j::ParameterID (pid::mode, 1), "Channel Mode",
        juce::StringArray { "Left / Right", "Lat / Vert", "L/R Linked" }, 2));

    layout.add (std::make_unique<j::AudioParameterChoice> (
        j::ParameterID (pid::quality, 1), "Engine",
        juce::StringArray { "Eco (No OS)", "Zero Latency (4x IIR)", "Studio (4x Linear Phase)" }, 1));

    layout.add (std::make_unique<j::AudioParameterBool> (
        j::ParameterID (pid::purist, 1), "Purist Mode", false));

    layout.add (std::make_unique<j::AudioParameterBool> (
        j::ParameterID (pid::bypass, 1), "True Bypass", false));

    return layout;
}

//==============================================================================
void TekkChild670Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseSampleRate = sampleRate;
    maxBlockSize   = samplesPerBlock;

    const auto numCh = (size_t) juce::jmax (1, juce::jmin (2, getTotalNumInputChannels()));

    using OS = juce::dsp::Oversampling<float>;
    oversamplerIIR = std::make_unique<OS> (numCh, 2, OS::filterHalfBandPolyphaseIIR, false, false);
    oversamplerFIR = std::make_unique<OS> (numCh, 2, OS::filterHalfBandFIREquiripple, true, true);
    oversamplerIIR->initProcessing ((size_t) samplesPerBlock);
    oversamplerFIR->initProcessing ((size_t) samplesPerBlock);

    dryBuffer.setSize (2, samplesPerBlock);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh };
    dryDelay.prepare (spec);
    dryDelay.setMaximumDelayInSamples (1 << 14);

    for (int ch = 0; ch < 2; ++ch)
    {
        const float inGain  = juce::Decibels::decibelsToGain (pInput[ch]->load());
        const float outGain = juce::Decibels::decibelsToGain (pOutput[ch]->load());

        inputGainSm[ch].reset (sampleRate, 0.02);
        outputGainSm[ch].reset (sampleRate, 0.02);
        mixSm[ch].reset (sampleRate, 0.02);
        inputGainSm[ch].setCurrentAndTargetValue (inGain);
        outputGainSm[ch].setCurrentAndTargetValue (outGain);
        mixSm[ch].setCurrentAndTargetValue (pMix[ch]->load() * 0.01f);
    }

    activeQuality = -1;
    applyQualityMode ((int) pQuality->load(), true);
}

void TekkChild670Processor::releaseResources()
{
}

void TekkChild670Processor::reset()
{
    // Clear all running state (filters, feedback detector, delay line) without
    // reallocating -- hosts call this on transport stop, and it gives the
    // offline measurement harness an uncontaminated starting point per point.
    for (auto& ch : channels)
        ch.reset();

    if (oversamplerIIR != nullptr) oversamplerIIR->reset();
    if (oversamplerFIR != nullptr) oversamplerFIR->reset();
    dryDelay.reset();

    for (int ch = 0; ch < 2; ++ch)
    {
        inputGainSm[ch].setCurrentAndTargetValue (inputGainSm[ch].getTargetValue());
        outputGainSm[ch].setCurrentAndTargetValue (outputGainSm[ch].getTargetValue());
        mixSm[ch].setCurrentAndTargetValue (mixSm[ch].getTargetValue());
        thresholdSm[ch].setCurrentAndTargetValue (thresholdSm[ch].getTargetValue());
        biasSm[ch].setCurrentAndTargetValue (biasSm[ch].getTargetValue());
        kneeSm[ch].setCurrentAndTargetValue (kneeSm[ch].getTargetValue());
        grMeterDb[ch].store (0.0f);
    }
}

bool TekkChild670Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    return in == out
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void TekkChild670Processor::applyQualityMode (int qualityIndex, bool reportLatencyNow)
{
    activeQuality = qualityIndex;

    const bool oversampled   = qualityIndex > 0;
    const double effectiveFs = baseSampleRate * (oversampled ? 4.0 : 1.0);

    // time constants, filter poles and DC blockers all live at the rate the
    // nonlinear core actually runs at
    for (auto& ch : channels)
        ch.prepare (effectiveFs);

    // the detector smoothers are consumed in the (oversampled) per-sample loop
    const bool purist = pPurist->load() > 0.5f;
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto p = paramsForChannel (ch, purist);
        thresholdSm[ch].reset (effectiveFs, 0.01);
        biasSm[ch].reset (effectiveFs, 0.01);
        kneeSm[ch].reset (effectiveFs, 0.01);
        thresholdSm[ch].setCurrentAndTargetValue (p.thresholdDb);
        biasSm[ch].setCurrentAndTargetValue (p.biasDb);
        kneeSm[ch].setCurrentAndTargetValue (p.kneeDb);
    }

    if (oversamplerIIR != nullptr) oversamplerIIR->reset();
    if (oversamplerFIR != nullptr) oversamplerFIR->reset();

    latencySamples = qualityIndex == 2 ? (int) oversamplerFIR->getLatencyInSamples()
                   : qualityIndex == 1 ? (int) std::round (oversamplerIIR->getLatencyInSamples())
                                       : 0;

    dryDelay.reset();
    dryDelay.setDelay ((float) latencySamples);

    // setLatencySamples can call back into the host, so only invoke it directly
    // from prepareToPlay; from the audio thread defer it to the message thread.
    if (reportLatencyNow)
    {
        setLatencySamples (latencySamples);
    }
    else
    {
        pendingLatency.store (latencySamples);
        triggerAsyncUpdate();
    }
}

void TekkChild670Processor::handleAsyncUpdate()
{
    setLatencySamples (pendingLatency.load());
}

tekk::ChannelParams TekkChild670Processor::paramsForChannel (int ch, bool purist) const
{
    tekk::ChannelParams p;

    p.thresholdDb  = thresholdKnobToDb (pThreshold[ch]->load());
    p.timeConstant = (int) pTimeConstant[ch]->load();
    p.compIn       = pCompIn[ch]->load() > 0.5f;

    // DC Threshold sweeps the rectifier bias: anticlockwise compression
    // starts earlier with a wide soft knee, clockwise it waits and bites
    const float dc = purist ? 0.0f : pDcThreshold[ch]->load();
    p.kneeDb = juce::jmap (dc, -10.0f, 10.0f, 9.0f, 0.5f);
    p.biasDb = dc * 0.6f;

    const int hpfIdx = juce::jlimit (0, 4, (int) pScHpf[ch]->load());
    p.scHpfHz = purist ? 0.0f : kScHpfTable[hpfIdx];

    return p;
}

//==============================================================================
void TekkChild670Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    const int n     = buffer.getNumSamples();

    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    if (n == 0 || numCh == 0)
        return;

    if (bypassParam->get())
    {
        // True bypass passes the signal unprocessed, but the host's bypass must
        // stay aligned with the latency we report -- so in an oversampled engine
        // the dry signal is delayed to match. In a zero-latency engine this is
        // a bit-exact passthrough.
        if (latencySamples > 0)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* x = buffer.getWritePointer (ch);

                for (int i = 0; i < n; ++i)
                {
                    dryDelay.pushSample (ch, x[i]);
                    x[i] = dryDelay.popSample (ch);
                }
            }
        }

        grMeterDb[0].store (0.0f);
        grMeterDb[1].store (0.0f);
        return;
    }

    const int quality = juce::jlimit (0, 2, (int) pQuality->load());
    if (quality != activeQuality)
        applyQualityMode (quality, false);

    const bool purist = pPurist->load() > 0.5f;
    const int  mode   = juce::jlimit (0, 2, (int) pMode->load());
    const bool msMode = mode == 1 && numCh == 2;
    const bool linked = mode == 2 && numCh == 2;

    for (int ch = 0; ch < numCh; ++ch)
    {
        const auto cp = paramsForChannel (ch, purist);
        channels[ch].setParams (cp);

        thresholdSm[ch].setTargetValue (cp.thresholdDb);
        biasSm[ch].setTargetValue (cp.biasDb);
        kneeSm[ch].setTargetValue (cp.kneeDb);

        inputGainSm[ch].setTargetValue (juce::Decibels::decibelsToGain (pInput[ch]->load()));
        outputGainSm[ch].setTargetValue (juce::Decibels::decibelsToGain (pOutput[ch]->load()));
        mixSm[ch].setTargetValue (purist ? 1.0f : pMix[ch]->load() * 0.01f);
    }

    // keep the true dry signal for the blend control
    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

    // input gain drives both colour and compression depth, as on the hardware
    for (int ch = 0; ch < numCh; ++ch)
        inputGainSm[ch].applyGain (buffer.getWritePointer (ch), n);

    if (msMode)
    {
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);

        for (int i = 0; i < n; ++i)
        {
            const float mid  = 0.5f * (l[i] + r[i]);
            const float side = 0.5f * (l[i] - r[i]);
            l[i] = mid;
            r[i] = side;
        }
    }

    auto* oversampler = quality == 1 ? oversamplerIIR.get()
                      : quality == 2 ? oversamplerFIR.get()
                                     : nullptr;

    juce::dsp::AudioBlock<float> baseBlock (buffer.getArrayOfWritePointers(),
                                            (size_t) numCh, (size_t) n);
    auto osBlock = oversampler != nullptr ? oversampler->processSamplesUp (baseBlock)
                                          : baseBlock;

    const int osN = (int) osBlock.getNumSamples();
    float* d0 = osBlock.getChannelPointer (0);
    float* d1 = numCh > 1 ? osBlock.getChannelPointer (1) : nullptr;

    float maxGr[2] = { 0.0f, 0.0f };

    for (int i = 0; i < osN; ++i)
    {
        const float thr0  = thresholdSm[0].getNextValue();
        const float bias0 = biasSm[0].getNextValue();
        const float knee0 = kneeSm[0].getNextValue();
        float cv0 = channels[0].computeControlVoltage (d0[i], thr0, bias0, knee0);

        if (d1 != nullptr)
        {
            const float thr1  = thresholdSm[1].getNextValue();
            const float bias1 = biasSm[1].getNextValue();
            const float knee1 = kneeSm[1].getNextValue();
            float cv1 = channels[1].computeControlVoltage (d1[i], thr1, bias1, knee1);

            if (linked)
                cv0 = cv1 = juce::jmax (cv0, cv1);

            d1[i]    = channels[1].renderWithControlVoltage (cv1);
            maxGr[1] = juce::jmax (maxGr[1], channels[1].currentGainReductionDb());
        }

        d0[i]    = channels[0].renderWithControlVoltage (cv0);
        maxGr[0] = juce::jmax (maxGr[0], channels[0].currentGainReductionDb());
    }

    if (oversampler != nullptr)
        oversampler->processSamplesDown (baseBlock);

    if (msMode)
    {
        auto* m = buffer.getWritePointer (0);
        auto* s = buffer.getWritePointer (1);

        for (int i = 0; i < n; ++i)
        {
            const float left  = m[i] + s[i];
            const float right = m[i] - s[i];
            m[i] = left;
            s[i] = right;
        }
    }

    // blend with the latency-aligned dry signal, then the output trim
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* wet       = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);

        for (int i = 0; i < n; ++i)
        {
            float drySample = dry[i];

            if (latencySamples > 0)
            {
                dryDelay.pushSample (ch, drySample);
                drySample = dryDelay.popSample (ch);
            }

            const float mixAmt  = mixSm[ch].getNextValue();
            const float outGain = outputGainSm[ch].getNextValue();

            wet[i] = outGain * (mixAmt * wet[i] + (1.0f - mixAmt) * drySample);
        }
    }

    grMeterDb[0].store (maxGr[0]);
    grMeterDb[1].store (numCh > 1 ? maxGr[1] : maxGr[0]);
}

//==============================================================================
juce::AudioProcessorEditor* TekkChild670Processor::createEditor()
{
    return new TekkChild670Editor (*this);
}

void TekkChild670Processor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TekkChild670Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TekkChild670Processor();
}
