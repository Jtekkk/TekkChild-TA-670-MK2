#include "PluginProcessor.h"

#include <limits>

#include <BinaryData.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Parameters.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
constexpr float kScHpfTable[] = { 0.0f, 40.0f, 60.0f, 100.0f, 150.0f, 200.0f, 300.0f, 400.0f };

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
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)),
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
        pDryGain[ch]      = apvts.getRawParameterValue (pid::forChannel (pid::dryGain, ch));
        pCompIn[ch]       = apvts.getRawParameterValue (pid::forChannel (pid::compIn, ch));
    }

    pMode       = apvts.getRawParameterValue (pid::mode);
    pQuality    = apvts.getRawParameterValue (pid::quality);
    pDrive      = apvts.getRawParameterValue (pid::drive);
    pIronDrive  = apvts.getRawParameterValue (pid::ironDrive);
    pTubeType   = apvts.getRawParameterValue (pid::tubeType);
    pTubeBias   = apvts.getRawParameterValue (pid::tubeBias);
    pTubeVolt   = apvts.getRawParameterValue (pid::tubeVolt);
    pNoiseFloor = apvts.getRawParameterValue (pid::noiseFloor);
    pLinkAmount = apvts.getRawParameterValue (pid::linkAmount);
    pAutoMk     = apvts.getRawParameterValue (pid::autoMakeup);
    pSafety     = apvts.getRawParameterValue (pid::safety);
    pPurist     = apvts.getRawParameterValue (pid::purist);

    pTapeOn    = apvts.getRawParameterValue (pid::tapeOn);
    pTapePar   = apvts.getRawParameterValue (pid::tapeParallel);
    pTapeIn    = apvts.getRawParameterValue (pid::tapeInput);
    pTapeOut   = apvts.getRawParameterValue (pid::tapeOutput);
    pTapeDrive = apvts.getRawParameterValue (pid::tapeDrive);
    pTapeSat   = apvts.getRawParameterValue (pid::tapeSat);
    pTapeBias  = apvts.getRawParameterValue (pid::tapeBias);
    pTapeWow   = apvts.getRawParameterValue (pid::tapeWow);
    pTapeHiss  = apvts.getRawParameterValue (pid::tapeHiss);
    pTapeNoise = apvts.getRawParameterValue (pid::tapeNoise);
    pTapeXtalk = apvts.getRawParameterValue (pid::tapeXtalk);
    pTapeDeg   = apvts.getRawParameterValue (pid::tapeDegrade);

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
    const juce::StringArray hpfChoices {
        "Off", "40 Hz", "60 Hz", "100 Hz", "150 Hz", "200 Hz", "300 Hz", "400 Hz" };

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

        group->addChild (std::make_unique<j::AudioParameterFloat> (
            j::ParameterID (pid::forChannel (pid::dryGain, ch), 1), "Dry Gain" + sfx,
            j::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
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

    layout.add (std::make_unique<j::AudioParameterFloat> (
        j::ParameterID (pid::drive, 1), "Drive",
        j::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        j::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<j::AudioParameterFloat> (
        j::ParameterID (pid::ironDrive, 1), "Iron Drive",
        j::NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
        j::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<j::AudioParameterFloat> (
        j::ParameterID (pid::linkAmount, 1), "Link Amount",
        j::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        j::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<j::AudioParameterBool> (
        j::ParameterID (pid::noiseFloor, 1), "Valve Hiss", false));

    juce::StringArray tubeChoices;
    for (int i = 0; i < tekk::kNumTubeTypes; ++i)
        tubeChoices.add (tekk::kTubeTypes[i].name);
    layout.add (std::make_unique<j::AudioParameterChoice> (
        j::ParameterID (pid::tubeType, 1), "Tube Type", tubeChoices, 0));

    layout.add (std::make_unique<j::AudioParameterFloat> (
        j::ParameterID (pid::tubeBias, 1), "Tube Bias",
        j::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<j::AudioParameterFloat> (
        j::ParameterID (pid::tubeVolt, 1), "Plate Voltage",
        j::NormalisableRange<float> (150.0f, 350.0f, 1.0f), 250.0f,
        j::AudioParameterFloatAttributes().withLabel ("V")));

    layout.add (std::make_unique<j::AudioParameterBool> (
        j::ParameterID (pid::autoMakeup, 1), "Auto Gain", false));

    layout.add (std::make_unique<j::AudioParameterBool> (
        j::ParameterID (pid::safety, 1), "Safety Clip", false));

    layout.add (std::make_unique<j::AudioParameterBool> (
        j::ParameterID (pid::purist, 1), "Purist Mode", false));

    layout.add (std::make_unique<j::AudioParameterBool> (
        j::ParameterID (pid::bypass, 1), "True Bypass", false));

    // -- Tape Brain --
    layout.add (std::make_unique<j::AudioParameterBool> (
        j::ParameterID (pid::tapeOn, 1), "Tape Brain", false));
    layout.add (std::make_unique<j::AudioParameterChoice> (
        j::ParameterID (pid::tapeParallel, 1), "Tape Routing",
        juce::StringArray { "Series", "Parallel" }, 0));

    layout.add (std::make_unique<j::AudioParameterFloat> (
        j::ParameterID (pid::tapeInput, 1), "Tape Input",
        j::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        j::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<j::AudioParameterFloat> (
        j::ParameterID (pid::tapeOutput, 1), "Tape Output",
        j::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        j::AudioParameterFloatAttributes().withLabel ("dB")));

    auto tapeKnob = [&layout] (const char* id, const char* name, float dflt)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (id, 1), name,
            juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), dflt));
    };
    tapeKnob (pid::tapeDrive,   "Tape Drive",      0.0f);
    tapeKnob (pid::tapeSat,     "Tape Saturation", 3.0f);
    tapeKnob (pid::tapeBias,    "Tape Bias",       5.0f);
    tapeKnob (pid::tapeWow,     "Wow & Flutter",   0.0f);
    tapeKnob (pid::tapeHiss,    "Tape Hiss",       0.0f);
    tapeKnob (pid::tapeNoise,   "Tape Noise",      0.0f);
    tapeKnob (pid::tapeXtalk,   "Crosstalk",       0.0f);
    tapeKnob (pid::tapeDegrade, "Degrade",         0.0f);

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
    tapeBuffer.setSize (2, samplesPerBlock);
    tapeBrain.prepare (sampleRate); // tape runs at base rate on the final output

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
        dryGainSm[ch].reset (sampleRate, 0.02);
        inputGainSm[ch].setCurrentAndTargetValue (inGain);
        outputGainSm[ch].setCurrentAndTargetValue (outGain);
        mixSm[ch].setCurrentAndTargetValue (pMix[ch]->load() * 0.01f);
        dryGainSm[ch].setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pDryGain[ch]->load()));
    }

    activeQuality = -1;
    applyQualityMode ((int) pQuality->load(), true);

    // the easter-egg clip is resampled to the host rate, so invalidate it here
    eggArm.store (false);
    eggActive = false;
    eggPos    = 0;
    eggLoaded = false;
    eggBuffer.setSize (2, 0);
}

void TekkChild670Processor::releaseResources()
{
}

//==============================================================================
void TekkChild670Processor::loadEasterEgg()
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    fm.registerFormat (new juce::OggVorbisAudioFormat(), false);

    auto stream = std::make_unique<juce::MemoryInputStream> (
        BinaryData::easter_egg_ogg, (size_t) BinaryData::easter_egg_oggSize, false);

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (std::move (stream)));
    eggLoaded = true; // even on failure, don't retry every click

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return;

    const int fileLen = (int) reader->lengthInSamples;
    const int fileCh  = juce::jmax (1, (int) reader->numChannels);

    juce::AudioBuffer<float> fileBuf (fileCh, fileLen + 16);
    fileBuf.clear();
    reader->read (&fileBuf, 0, fileLen, 0, true, fileCh > 1);

    const double ratio = reader->sampleRate / baseSampleRate; // in-samples per out-sample
    const int outLen = (int) ((double) fileLen / ratio);
    if (outLen <= 0)
        return;

    eggBuffer.setSize (2, outLen);
    eggBuffer.clear();

    for (int ch = 0; ch < 2; ++ch)
    {
        const float* src = fileBuf.getReadPointer (juce::jmin (ch, fileCh - 1));

        if (std::abs (ratio - 1.0) < 1.0e-9)
            eggBuffer.copyFrom (ch, 0, src, juce::jmin (outLen, fileLen));
        else
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, src, eggBuffer.getWritePointer (ch), outLen);
        }
    }
}

void TekkChild670Processor::triggerEasterEgg()
{
    if (! eggLoaded)
        loadEasterEgg();

    if (eggBuffer.getNumSamples() > 0)
        eggArm.store (true); // picked up at the next audio block
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
    tapeBrain.reset();
    tapeSatMeter.store (0.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        inputGainSm[ch].setCurrentAndTargetValue (inputGainSm[ch].getTargetValue());
        outputGainSm[ch].setCurrentAndTargetValue (outputGainSm[ch].getTargetValue());
        mixSm[ch].setCurrentAndTargetValue (mixSm[ch].getTargetValue());
        dryGainSm[ch].setCurrentAndTargetValue (dryGainSm[ch].getTargetValue());
        thresholdSm[ch].setCurrentAndTargetValue (thresholdSm[ch].getTargetValue());
        biasSm[ch].setCurrentAndTargetValue (biasSm[ch].getTargetValue());
        kneeSm[ch].setCurrentAndTargetValue (kneeSm[ch].getTargetValue());
        grMeterDb[ch].store (0.0f);
        inMeterDb[ch].store (-100.0f);
        outMeterDb[ch].store (-100.0f);
        makeupGrDb[ch] = 0.0f;
    }
    linkAmountSm.setCurrentAndTargetValue (linkAmountSm.getTargetValue());

    // stop any easter-egg playback (hosts call reset() on transport stop)
    eggArm.store (false);
    eggActive = false;
    eggPos    = 0;
}

bool TekkChild670Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    // Optional sidechain bus: must be disabled or stereo.
    const auto& sc = layouts.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::stereo();
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

    // link amount is consumed per OS sample, so smooth at the effective rate
    linkAmountSm.reset (effectiveFs, 0.02);
    linkAmountSm.setCurrentAndTargetValue (purist ? 1.0f : pLinkAmount->load() * 0.01f);

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

    const int hpfIdx = juce::jlimit (0, 7, (int) pScHpf[ch]->load());
    p.scHpfHz = purist ? 0.0f : kScHpfTable[hpfIdx];

    return p;
}

//==============================================================================
void TekkChild670Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // numCh is the main signal channel count (1 or 2). Sidechain channels live
    // at index numCh and above (when the SC bus is enabled) and must NOT be cleared.
    const int numCh = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    const int n     = buffer.getNumSamples();

    for (int ch = numCh; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
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

        for (int ch = 0; ch < 2; ++ch)
        {
            const float lvl = juce::Decibels::gainToDecibels (
                buffer.getMagnitude (juce::jmin (ch, numCh - 1), 0, n), -100.0f);
            grMeterDb[ch].store (0.0f);
            inMeterDb[ch].store (lvl);
            outMeterDb[ch].store (lvl);
        }
        return;
    }

    const int quality = juce::jlimit (0, 2, (int) pQuality->load());
    if (quality != activeQuality)
        applyQualityMode (quality, false);

    const bool purist = pPurist->load() > 0.5f;
    const int  mode   = juce::jlimit (0, 2, (int) pMode->load());
    const bool msMode = mode == 1 && numCh == 2;
    const bool linked = mode == 2 && numCh == 2;

    // Drive: 0..100 maps to a saturation scaler centred on 1.0 (== stock) at 50.
    const float driveKnob = juce::jlimit (0.0f, 100.0f, pDrive->load());
    const float driveTarget = driveKnob <= 50.0f ? juce::jmap (driveKnob, 0.0f, 50.0f, 0.5f, 1.0f)
                                                  : juce::jmap (driveKnob, 50.0f, 100.0f, 1.0f, 1.6f);
    characterCurrent += 0.25f * (driveTarget - characterCurrent); // per-block glide
    for (int ch = 0; ch < numCh; ++ch)
        channels[ch].setCharacter (characterCurrent);

    // Iron Drive scales the transformers (0..200% -> 0..2.0); Valve Hiss adds the
    // noise floor. Both are tonal additions, so purist forces stock / off.
    const float ironMul = purist ? 1.0f : pIronDrive->load() * 0.01f;
    const bool  noiseOn = ! purist && pNoiseFloor->load() > 0.5f;
    for (int ch = 0; ch < numCh; ++ch)
    {
        channels[ch].setIronDrive (ironMul);
        channels[ch].setNoiseEnabled (noiseOn);
    }

    // Link amount: in purist mode the link is always hard (100%).
    linkAmountSm.setTargetValue (purist ? 1.0f : pLinkAmount->load() * 0.01f);

    // Tube roll: glide toward the selected valve's coefficients, the grid bias
    // and the plate-voltage headroom, so swapping tubes warms up click-free.
    const int   tubeIdx   = juce::jlimit (0, tekk::kNumTubeTypes - 1, (int) pTubeType->load());
    const auto& tt        = tekk::kTubeTypes[tubeIdx];
    const float biasTarget = juce::jlimit (-100.0f, 100.0f, pTubeBias->load()) * 0.0045f; // -0.45..0.45
    const float voltScale  = juce::jmap (juce::jlimit (150.0f, 350.0f, pTubeVolt->load()),
                                         150.0f, 350.0f, 0.60f, 1.40f);

    tubeMuCur   += 0.25f * (tt.mu - tubeMuCur);
    tubeAsymCur += 0.25f * (tt.asym - tubeAsymCur);
    tubeHeadCur += 0.25f * (tt.head * voltScale - tubeHeadCur);
    tubeBiasCur += 0.25f * (biasTarget - tubeBiasCur);

    for (int ch = 0; ch < numCh; ++ch)
        channels[ch].setTube (tubeMuCur, tubeAsymCur, tubeHeadCur, tubeBiasCur);

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
        dryGainSm[ch].setTargetValue (purist ? 1.0f
                                             : juce::Decibels::decibelsToGain (pDryGain[ch]->load()));
    }

    // keep the true dry signal for the blend control
    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

    // input gain drives both colour and compression depth, as on the hardware
    for (int ch = 0; ch < numCh; ++ch)
        inputGainSm[ch].applyGain (buffer.getWritePointer (ch), n);

    // input meter reflects the post-gain drive into the unit (pre M/S encode)
    for (int ch = 0; ch < numCh; ++ch)
        inMeterDb[ch].store (juce::Decibels::gainToDecibels (buffer.getMagnitude (ch, 0, n), -100.0f));
    if (numCh == 1)
        inMeterDb[1].store (inMeterDb[0].load());

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

    // External sidechain: channels at index >= numCh belong to the SC bus and
    // are read at base rate, held across the OS sub-samples (the detector attack
    // is far slower than one OS period). NaN = key from the internal feedback.
    const bool  hasExtSC = getTotalNumInputChannels() > numCh
                        && buffer.getNumChannels() > numCh;
    const float* scPtr0 = hasExtSC ? buffer.getReadPointer (numCh) : nullptr;
    const float* scPtr1 = (hasExtSC && buffer.getNumChannels() > numCh + 1)
                        ? buffer.getReadPointer (numCh + 1) : scPtr0;
    const int osRate = (osN > 0 && n > 0) ? osN / n : 1;
    const float kNaN = std::numeric_limits<float>::quiet_NaN();

    float  maxGr[2] = { 0.0f, 0.0f };
    double sumGr[2] = { 0.0, 0.0 };

    for (int i = 0; i < osN; ++i)
    {
        const float linkAmt = linkAmountSm.getNextValue();
        const int   base    = juce::jmin (i / osRate, n - 1);

        const float thr0  = thresholdSm[0].getNextValue();
        const float bias0 = biasSm[0].getNextValue();
        const float knee0 = kneeSm[0].getNextValue();
        const float extSc0 = scPtr0 ? scPtr0[base] : kNaN;
        float cv0 = channels[0].computeControlVoltage (d0[i], thr0, bias0, knee0, extSc0);

        if (d1 != nullptr)
        {
            const float thr1  = thresholdSm[1].getNextValue();
            const float bias1 = biasSm[1].getNextValue();
            const float knee1 = kneeSm[1].getNextValue();
            const float extSc1 = scPtr1 ? scPtr1[base] : kNaN;
            float cv1 = channels[1].computeControlVoltage (d1[i], thr1, bias1, knee1, extSc1);

            if (linked)
            {
                // Blend each channel's own CV toward the harder of the two:
                // linkAmt 1.0 = classic hard stereo link, 0.0 = independent.
                const float linkedCv = juce::jmax (cv0, cv1);
                cv0 += linkAmt * (linkedCv - cv0);
                cv1 += linkAmt * (linkedCv - cv1);
            }

            d1[i]      = channels[1].renderWithControlVoltage (cv1);
            const float gr1 = channels[1].currentGainReductionDb();
            maxGr[1]   = juce::jmax (maxGr[1], gr1);
            sumGr[1]  += gr1;
        }

        d0[i]      = channels[0].renderWithControlVoltage (cv0);
        const float gr0 = channels[0].currentGainReductionDb();
        maxGr[0]   = juce::jmax (maxGr[0], gr0);
        sumGr[0]  += gr0;
    }

    // Auto makeup: slowly track the average gain reduction so it can be added
    // back at the output, holding level for sustained material while leaving
    // transient dynamics intact.
    const bool  autoMk    = pAutoMk->load() > 0.5f;
    const float grAvgCoef = 1.0f - std::exp (-(float) n / (1.0f * (float) baseSampleRate)); // ~1 s
    float makeupLin[2] = { 1.0f, 1.0f };
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float avgGr = (float) (sumGr[ch] / juce::jmax (1, osN));
        makeupGrDb[ch] += grAvgCoef * (avgGr - makeupGrDb[ch]);
        makeupLin[ch]   = autoMk ? juce::Decibels::decibelsToGain (makeupGrDb[ch]) : 1.0f;
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

    // blend with the latency-aligned dry signal, then the output trim + makeup
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* wet       = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        const float mk  = makeupLin[ch];

        for (int i = 0; i < n; ++i)
        {
            float drySample = dry[i];

            if (latencySamples > 0)
            {
                dryDelay.pushSample (ch, drySample);
                drySample = dryDelay.popSample (ch);
            }

            const float mixAmt  = mixSm[ch].getNextValue();
            const float dryMul  = dryGainSm[ch].getNextValue();
            const float outGain = outputGainSm[ch].getNextValue();

            wet[i] = mk * outGain * (mixAmt * wet[i] + (1.0f - mixAmt) * dryMul * drySample);
        }
    }

    grMeterDb[0].store (maxGr[0]);
    grMeterDb[1].store (numCh > 1 ? maxGr[1] : maxGr[0]);

    // Tape Brain: a tape-colour section after (series) or alongside (parallel)
    // the compressor. The throw switch picks the route; purist takes it out.
    if (! purist && pTapeOn->load() > 0.5f)
    {
        tekk::TapeBrain::Params tp;
        tp.inputGain  = juce::Decibels::decibelsToGain (pTapeIn->load());
        tp.outputGain = juce::Decibels::decibelsToGain (pTapeOut->load());
        tp.drive      = pTapeDrive->load() * 0.1f;
        tp.saturation = pTapeSat->load()   * 0.1f;
        tp.bias       = pTapeBias->load()  * 0.1f;
        tp.wowFlutter = pTapeWow->load()   * 0.1f;
        tp.hiss       = pTapeHiss->load()  * 0.1f;
        tp.noise      = pTapeNoise->load() * 0.1f;
        tp.crosstalk  = pTapeXtalk->load() * 0.1f;
        tp.degrade    = pTapeDeg->load()   * 0.1f;
        tapeBrain.setParams (tp);

        // stereo working copy (mono duplicates the single channel)
        for (int ch = 0; ch < 2; ++ch)
            tapeBuffer.copyFrom (ch, 0, buffer, juce::jmin (ch, numCh - 1), 0, n);

        tapeBrain.process (tapeBuffer.getWritePointer (0), tapeBuffer.getWritePointer (1), n);
        tapeSatMeter.store (tapeBrain.saturationMeter());

        const bool parallel = pTapePar->load() > 0.5f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            if (parallel)
                buffer.addFrom (ch, 0, tapeBuffer, ch, 0, n);   // tape blended on top
            else
                buffer.copyFrom (ch, 0, tapeBuffer, ch, 0, n);  // tape in line
        }
    }
    else
    {
        tapeSatMeter.store (0.0f);
    }

    // easter egg: mix the embedded clip on top of the output once armed
    if (eggArm.exchange (false))
    {
        eggActive = true;
        eggPos    = 0;
    }
    if (eggActive && eggBuffer.getNumSamples() > 0)
    {
        const int eggLen = eggBuffer.getNumSamples();
        const int count  = juce::jmin (n, eggLen - eggPos);

        for (int ch = 0; ch < numCh; ++ch)
            buffer.addFrom (ch, 0, eggBuffer, juce::jmin (ch, eggBuffer.getNumChannels() - 1),
                            eggPos, count, 0.9f);

        eggPos += count;
        if (eggPos >= eggLen)
        {
            eggActive = false;
            eggPos    = 0;
        }
    }

    // Safety: a gentle final ceiling so heavy Drive / saturation can't push overs
    if (pSafety->load() > 0.5f)
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                w[i] = safetyClip (w[i]);
        }

    for (int ch = 0; ch < numCh; ++ch)
        outMeterDb[ch].store (juce::Decibels::gainToDecibels (buffer.getMagnitude (ch, 0, n), -100.0f));
    if (numCh == 1)
        outMeterDb[1].store (outMeterDb[0].load());
}

//==============================================================================
int TekkChild670Processor::getNumPrograms()
{
    return (int) tekk::factoryPresets().size();
}

int TekkChild670Processor::getCurrentProgram()
{
    return currentProgram;
}

const juce::String TekkChild670Processor::getProgramName (int index)
{
    const auto& presets = tekk::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size())
               ? presets[(size_t) index].name : juce::String();
}

void TekkChild670Processor::setCurrentProgram (int index)
{
    const auto& presets = tekk::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;
    const auto& preset = presets[(size_t) index];

    // Reset every sound parameter to its default (bypass is left alone), so a
    // preset fully and deterministically defines the patch, then apply overrides.
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (ranged->getParameterID() != tekk::pid::bypass)
                ranged->setValueNotifyingHost (ranged->getDefaultValue());

    auto apply = [this] (const juce::String& id, float plain)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };

    for (const auto& entry : preset.channel)
        for (int ch = 0; ch < 2; ++ch)
            apply (tekk::pid::forChannel (entry.first.toRawUTF8(), ch), entry.second);

    for (const auto& entry : preset.global)
        apply (entry.first, entry.second);
}

//==============================================================================
juce::AudioProcessorEditor* TekkChild670Processor::createEditor()
{
    return new TekkChild670Editor (*this);
}

void TekkChild670Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentProgram", currentProgram, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TekkChild670Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    currentProgram = (int) tree.getProperty ("currentProgram", 0);
    apvts.replaceState (tree);

    // replaceState only re-syncs parameters whose (snapped) tree value changed.
    // A discrete parameter (bool / choice) stores an un-snapped raw value, so if
    // a prior value snapped to the same bin as the restored one, replaceState
    // leaves the raw value stale. Re-apply every stored value so each
    // parameter's normalised value matches the state exactly after a restore.
    std::function<void (const juce::ValueTree&)> reapply = [&] (const juce::ValueTree& node)
    {
        if (node.hasProperty ("id"))
            if (auto* p = apvts.getParameter (node["id"].toString()))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) node["value"]));

        for (auto child : node)
            reapply (child);
    };
    reapply (tree);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TekkChild670Processor();
}
