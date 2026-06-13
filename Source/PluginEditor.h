#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

namespace tekk
{

class TekkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TekkLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

// Vintage VU-style needle meter showing gain reduction, 0 dB at rest on
// the right, swinging left as the unit pulls down.
class GainReductionMeter : public juce::Component,
                           private juce::Timer
{
public:
    explicit GainReductionMeter (std::function<float()> grSource);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::function<float()> source;
    float needleDb = 0.0f;
};

// Vertical peak level meter (dBFS) with fast attack, slow release and a
// short peak hold. Used for the per-channel input and output bars.
class LevelMeter : public juce::Component,
                   private juce::Timer
{
public:
    LevelMeter (std::function<float()> levelSource, juce::String caption);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::function<float()> source;
    juce::String captionText;
    float levelDb   = -100.0f;
    float peakDb    = -100.0f;
    int   peakHold  = 0;
};

class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (TekkChild670Processor&, int channelIndex, const juce::String& title);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void setupKnob (juce::Slider&, juce::Label&, const juce::String& name);

    TekkChild670Processor& processor;
    int channel;
    juce::String title;

    GainReductionMeter meter;
    LevelMeter inMeter, outMeter;

    juce::Slider input, threshold, timeConstant, dcThreshold, mix, output;
    juce::Label  inputLb, thresholdLb, timeConstantLb, dcThresholdLb, mixLb, outputLb;

    juce::Label        hpfLb;
    juce::ComboBox     hpfBox;
    juce::ToggleButton compInBtn { "AGC IN" };

    std::unique_ptr<SliderAttachment> inputAt, thresholdAt, timeConstantAt,
                                      dcThresholdAt, mixAt, outputAt;
    std::unique_ptr<ComboBoxAttachment> hpfAt;
    std::unique_ptr<ButtonAttachment>   compInAt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

} // namespace tekk

//==============================================================================
class TekkChild670Editor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit TekkChild670Editor (TekkChild670Processor&);
    ~TekkChild670Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;          // keeps the preset box in sync
    void loadProgram (int index);
    void refreshPresetBox();

    TekkChild670Processor& processor;

    tekk::TekkLookAndFeel lookAndFeel;

    tekk::ChannelStrip stripA, stripB;

    juce::Label    presetLb;
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };

    juce::Label    modeLb, qualityLb;
    juce::ComboBox modeBox, qualityBox;
    juce::ToggleButton puristBtn { "PURIST" }, bypassBtn { "BYPASS" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAt, qualityAt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   puristAt, bypassAt;

    juce::Image chassis;  // cached hammered-metal background
    juce::Image faceArt;  // central faceplate graphic
    juce::Rectangle<int> faceArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TekkChild670Editor)
};
