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

    juce::Slider dryGain;
    juce::Label  dryGainLb;

    juce::Label        hpfLb;
    juce::ComboBox     hpfBox;
    juce::ToggleButton compInBtn { "AGC IN" };

    std::unique_ptr<SliderAttachment> inputAt, thresholdAt, timeConstantAt,
                                      dcThresholdAt, mixAt, outputAt, dryGainAt;
    std::unique_ptr<ComboBoxAttachment> hpfAt;
    std::unique_ptr<ButtonAttachment>   compInAt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

// Invisible hotspot over the mascot's nose: six clicks fire the easter egg.
class NoseHotspot : public juce::Component
{
public:
    NoseHotspot() { setInterceptsMouseClicks (true, false); }

    std::function<void()> onSecretCombo;

    void mouseDown (const juce::MouseEvent&) override
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastClickMs > 2500)
            clicks = 0; // too slow -- start over
        lastClickMs = now;

        if (++clicks >= 6)
        {
            clicks = 0;
            if (onSecretCombo)
                onSecretCombo();
        }
    }

private:
    int clicks = 0;
    juce::uint32 lastClickMs = 0;
};

// A Frankenstein knife-throw switch: lever down = SERIES, up = PARALLEL.
class ThrowSwitch : public juce::Button
{
public:
    ThrowSwitch() : juce::Button ("route") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Brushed-metal tape-machine knob ringed with glowing cyan tick marks.
class TapeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

// The "Tape Brain" tape-effects section: a weathered brass "MAGNETIC MEMORY"
// control unit (analog VU, cyan-ticked knobs, amber lamps) beside a tall glowing
// jar whose brain is built from film reels and coiled tape, plus the throw switch.
class TapeBrainPanel : public juce::Component,
                       private juce::Timer
{
public:
    explicit TapeBrainPanel (TekkChild670Processor&);
    ~TapeBrainPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& name, const juce::String& pid);

    TekkChild670Processor& processor;
    TapeLookAndFeel tapeLnf;

    juce::Slider drive, sat, bias, wow, hiss, noise, xtalk, degrade, input, output;
    juce::Label  driveLb, satLb, biasLb, wowLb, hissLb, noiseLb, xtalkLb, degradeLb, inputLb, outputLb;

    juce::ToggleButton powerBtn { "POWER" };
    ThrowSwitch        routeSwitch;
    juce::Image        chassis;

    std::vector<std::unique_ptr<SliderAttachment>> knobAtts;
    std::unique_ptr<ButtonAttachment> powerAt, routeAt;

    juce::Rectangle<int> vuArea, jarArea, switchArea, nameArea;
    float vuValue = 0.0f, jarPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeBrainPanel)
};

// Chunky vintage-TV control knob (dark knurled barrel + cream pointer + dial
// scale), used for the CRT module's MONO MAKER and STEREO ENHANCE controls.
class CrtLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

// The "TEKKVISION" stereo-imaging module: an old CRT television whose screen is
// a live green-phosphor goniometer (mono collapses it to a vertical line, width
// spreads it horizontally with a red/cyan 3-D fringe). Two TV knobs underneath.
class StereoImagerPanel : public juce::Component,
                          private juce::Timer
{
public:
    explicit StereoImagerPanel (TekkChild670Processor&);
    ~StereoImagerPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void drawScreen (juce::Graphics&, juce::Rectangle<float>);

    TekkChild670Processor& processor;
    CrtLookAndFeel crtLnf;

    juce::Slider monoKnob, enhanceKnob;
    juce::Label  monoLb, enhanceLb;
    std::unique_ptr<SliderAttachment> monoAt, enhanceAt;

    juce::Rectangle<int> screenArea, knobArea, brandArea;

    // the mascot on the CRT: full-colour plus red / cyan channel copies so the
    // face can be split into a 3-D anaglyph that grows with Stereo Enhance
    juce::Image faceArt, faceRed, faceCyan;

    // audio snapshot for the mouth waveform, refreshed on the UI timer
    static constexpr int kScopeN = 512;
    float scopeMid [kScopeN] {};
    float scopeSide[kScopeN] {};
    int   scopeCount = 0;
    float scanPhase  = 0.0f;
    float litLevel   = 0.0f; // smoothed brightness from recent signal energy

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoImagerPanel)
};

// A metal gas pedal that tilts with the slider value (rotary slider).
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

// The "1176" brick-wall limiter: a SPEED LIMIT road sign whose number follows
// the throttle pedal, with a gain-reduction needle showing how hard it limits.
class LimiterPanel : public juce::Component,
                     private juce::Timer
{
public:
    explicit LimiterPanel (TekkChild670Processor&);
    ~LimiterPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;

    TekkChild670Processor& processor;
    PedalLookAndFeel pedalLnf;

    juce::Slider throttle;
    std::unique_ptr<SliderAttachment> throttleAt;
    GainReductionMeter grMeter;
    juce::Label pedalLb;

    juce::Rectangle<int> titleArea, signArea, meterArea, pedalArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimiterPanel)
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
    tekk::TapeBrainPanel tapePanel;
    tekk::StereoImagerPanel imagerPanel;
    tekk::LimiterPanel limiterPanel;

    juce::Label    presetLb;
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };

    juce::Label    modeLb, qualityLb;
    juce::ComboBox modeBox, qualityBox;
    juce::ToggleButton puristBtn { "PURIST" }, bypassBtn { "BYPASS" };
    juce::ToggleButton autoGainBtn { "AUTO GAIN" }, safetyBtn { "SAFETY" }, noiseBtn { "HISS" };

    juce::Slider driveSlider, ironSlider, biasSlider, voltSlider;
    juce::Label  driveLb, ironLb, biasLb, voltLb, tubeLb;
    juce::ComboBox tubeBox;

    juce::Slider linkAmtKnob;
    juce::Label  linkAmtLb;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAt, qualityAt, tubeAt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   puristAt, bypassAt,
                                                                           autoGainAt, safetyAt, noiseAt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   driveAt, ironAt, biasAt, voltAt,
                                                                           linkAmtAt;

    juce::TooltipWindow tooltips { this };

    juce::Image chassis;  // cached hammered-metal background
    juce::Image faceArt;  // central faceplate graphic
    juce::Rectangle<int> faceArea;       // the framed plate
    juce::Rectangle<int> faceImageRect;  // where the artwork is drawn

    tekk::NoseHotspot noseHotspot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TekkChild670Editor)
};
