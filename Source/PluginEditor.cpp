#include "PluginEditor.h"

#include "Parameters.h"

namespace tekk
{

namespace colours
{
const juce::Colour background  { 0xff1d1d20 };
const juce::Colour panel       { 0xff26262b };
const juce::Colour outline     { 0xff141416 };
const juce::Colour cream       { 0xffe8e0c9 };
const juce::Colour amber       { 0xffd49a3a };
const juce::Colour textDim     { 0xffb9b3a4 };
const juce::Colour needle      { 0xffb03a2e };
const juce::Colour knobTrack   { 0xff3a3a40 };
} // namespace colours

//==============================================================================
TekkLookAndFeel::TekkLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::textDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::textDim);

    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2e2e34));
    setColour (juce::ComboBox::textColourId, colours::cream);
    setColour (juce::ComboBox::outlineColourId, colours::outline);
    setColour (juce::ComboBox::arrowColourId, colours::amber);

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff2e2e34));
    setColour (juce::PopupMenu::textColourId, colours::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::amber);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::outline);

    setColour (juce::ToggleButton::textColourId, colours::cream);
    setColour (juce::ToggleButton::tickColourId, colours::amber);
    setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff5a5a60));
}

void TekkLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (size, size);

    const auto centre  = bounds.getCentre();
    const float radius = size * 0.5f;
    const float angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (colours::knobTrack);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path active;
    active.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                          rotaryStartAngle, angle, true);
    g.setColour (colours::amber);
    g.strokePath (active, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    const float knobR = radius * 0.76f;
    const auto knobArea = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35353b),
                                             centre.x, centre.y - knobR,
                                             juce::Colour (0xff1b1b1f),
                                             centre.x, centre.y + knobR, false));
    g.fillEllipse (knobArea);
    g.setColour (colours::outline);
    g.drawEllipse (knobArea, 1.5f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-2.0f, -knobR + 3.0f, 4.0f, knobR * 0.45f, 2.0f);
    g.setColour (colours::cream);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

//==============================================================================
GainReductionMeter::GainReductionMeter (std::function<float()> grSource)
    : source (std::move (grSource))
{
    startTimerHz (30);
}

void GainReductionMeter::timerCallback()
{
    const float target = source != nullptr ? source() : 0.0f;

    // VU-style ballistics: snappy rise, lazy fall
    const float rate = target > needleDb ? 0.55f : 0.16f;
    needleDb += rate * (target - needleDb);

    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f);

    g.setColour (colours::cream);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r, 6.0f, 2.0f);

    const juce::Point<float> pivot (r.getCentreX(), r.getBottom() - 8.0f);
    const float needleLen = r.getHeight() * 0.72f;

    // 0 dB GR rests on the right, deeper reduction swings the needle left
    auto angleFor = [] (float grDb)
    {
        const float t = juce::jlimit (0.0f, 1.0f, grDb / 20.0f);
        return juce::jmap (t, 0.38f, -0.38f) * juce::MathConstants<float>::pi;
    };

    g.setColour (juce::Colour (0xff3c3326));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    for (float tick : { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f })
    {
        const float a  = angleFor (tick);
        const auto p1  = pivot.getPointOnCircumference (needleLen, a);
        const auto p2  = pivot.getPointOnCircumference (needleLen - 6.0f, a);
        g.drawLine (juce::Line<float> (p1, p2), 1.2f);

        const auto tp = pivot.getPointOnCircumference (needleLen + 8.0f, a);
        g.drawText (juce::String ((int) tick),
                    juce::Rectangle<float> (18.0f, 10.0f).withCentre (tp),
                    juce::Justification::centred);
    }

    g.drawText ("GAIN REDUCTION  dB",
                juce::Rectangle<float> (r.getWidth(), 12.0f)
                    .withCentre ({ r.getCentreX(), r.getBottom() - 14.0f }),
                juce::Justification::centred);

    const float a = angleFor (needleDb);
    g.setColour (colours::needle);
    g.drawLine (juce::Line<float> (pivot, pivot.getPointOnCircumference (needleLen, a)), 2.0f);

    g.setColour (colours::outline);
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (pivot));
}

//==============================================================================
LevelMeter::LevelMeter (std::function<float()> levelSource, juce::String caption)
    : source (std::move (levelSource)), captionText (std::move (caption))
{
    startTimerHz (30);
}

void LevelMeter::timerCallback()
{
    const float target = source != nullptr ? source() : -100.0f;

    levelDb = target > levelDb ? target : levelDb + 0.25f * (target - levelDb);

    if (target >= peakDb)      { peakDb = target; peakHold = 30; } // ~1 s hold at 30 Hz
    else if (peakHold > 0)     { --peakHold; }
    else                       { peakDb -= 0.6f; }                 // then fall ~18 dB/s

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto caption = r.removeFromBottom (14.0f);

    g.setColour (juce::Colour (0xff141416));
    g.fillRoundedRectangle (r, 3.0f);

    constexpr float minDb = -60.0f, maxDb = 6.0f;
    auto yFor = [&] (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        return r.getBottom() - t * r.getHeight();
    };

    // filled segments, coloured by zone
    auto fillTo = [&] (float lo, float hi, juce::Colour c)
    {
        const float top = yFor (std::min (levelDb, hi));
        const float bot = yFor (lo);
        if (levelDb > lo && bot > top)
        {
            g.setColour (c);
            g.fillRect (r.getX() + 1.5f, top, r.getWidth() - 3.0f, bot - top);
        }
    };

    fillTo (minDb, -12.0f, juce::Colour (0xff4a9e6a)); // green
    fillTo (-12.0f, -3.0f, colours::amber);            // amber
    fillTo (-3.0f, maxDb, colours::needle);            // red

    // 0 dBFS reference and peak marker
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawHorizontalLine ((int) yFor (0.0f), r.getX(), r.getRight());

    if (peakDb > minDb)
    {
        g.setColour (peakDb > -1.0f ? colours::needle : colours::cream);
        g.fillRect (r.getX() + 1.5f, yFor (peakDb) - 1.0f, r.getWidth() - 3.0f, 2.0f);
    }

    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.drawText (captionText, caption, juce::Justification::centred);
}

//==============================================================================
ChannelStrip::ChannelStrip (TekkChild670Processor& p, int channelIndex, const juce::String& t)
    : processor (p),
      channel (channelIndex),
      title (t),
      meter   ([&p, channelIndex] { return p.getGainReductionDb (channelIndex); }),
      inMeter ([&p, channelIndex] { return p.getInputLevelDb (channelIndex); }, "IN"),
      outMeter ([&p, channelIndex] { return p.getOutputLevelDb (channelIndex); }, "OUT")
{
    setupKnob (input,        inputLb,        "INPUT");
    setupKnob (threshold,    thresholdLb,    "THRESHOLD");
    setupKnob (timeConstant, timeConstantLb, "TIME CONST");
    setupKnob (dcThreshold,  dcThresholdLb,  "DC THRESH");
    setupKnob (mix,          mixLb,          "MIX");
    setupKnob (output,       outputLb,       "OUTPUT");

    auto& apvts = processor.apvts;

    inputAt        = std::make_unique<SliderAttachment> (apvts, pid::forChannel (pid::input, channel), input);
    thresholdAt    = std::make_unique<SliderAttachment> (apvts, pid::forChannel (pid::threshold, channel), threshold);
    timeConstantAt = std::make_unique<SliderAttachment> (apvts, pid::forChannel (pid::timeConstant, channel), timeConstant);
    dcThresholdAt  = std::make_unique<SliderAttachment> (apvts, pid::forChannel (pid::dcThreshold, channel), dcThreshold);
    mixAt          = std::make_unique<SliderAttachment> (apvts, pid::forChannel (pid::mix, channel), mix);
    outputAt       = std::make_unique<SliderAttachment> (apvts, pid::forChannel (pid::output, channel), output);

    hpfLb.setText ("SC FILTER", juce::dontSendNotification);
    hpfLb.setFont (juce::Font (juce::FontOptions (12.0f)));
    hpfLb.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (hpfLb);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            apvts.getParameter (pid::forChannel (pid::scHpf, channel))))
        hpfBox.addItemList (choice->choices, 1);

    addAndMakeVisible (hpfBox);
    hpfAt = std::make_unique<ComboBoxAttachment> (apvts, pid::forChannel (pid::scHpf, channel), hpfBox);

    addAndMakeVisible (compInBtn);
    compInAt = std::make_unique<ButtonAttachment> (apvts, pid::forChannel (pid::compIn, channel), compInBtn);

    addAndMakeVisible (meter);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);
}

void ChannelStrip::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 16);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::Font (juce::FontOptions (12.0f)));
    l.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (l);
}

void ChannelStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    g.setColour (colours::amber);
    g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
    g.drawText (title, getLocalBounds().removeFromTop (30).reduced (14, 0),
                juce::Justification::centredLeft);
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced (10);
    r.removeFromTop (24); // title band, painted

    auto meterRow = r.removeFromTop (116);
    inMeter.setBounds  (meterRow.removeFromLeft (30).reduced (3, 4));
    outMeter.setBounds (meterRow.removeFromRight (30).reduced (3, 4));
    meter.setBounds    (meterRow.reduced (10, 2));

    auto layoutRow = [] (juce::Rectangle<int> row,
                         std::initializer_list<juce::Slider*> sliders,
                         std::initializer_list<juce::Label*> labels)
    {
        const int cellW = row.getWidth() / (int) sliders.size();
        auto s = sliders.begin();
        auto l = labels.begin();

        for (size_t i = 0; i < sliders.size(); ++i, ++s, ++l)
        {
            auto cell = row.removeFromLeft (cellW);
            (*l)->setBounds (cell.removeFromBottom (16));
            (*s)->setBounds (cell.reduced (2, 0));
        }
    };

    layoutRow (r.removeFromTop (140), { &input, &threshold, &timeConstant },
                                      { &inputLb, &thresholdLb, &timeConstantLb });
    layoutRow (r.removeFromTop (140), { &dcThreshold, &mix, &output },
                                      { &dcThresholdLb, &mixLb, &outputLb });

    auto bottom = r.removeFromTop (30);
    hpfLb.setBounds (bottom.removeFromLeft (66));
    hpfBox.setBounds (bottom.removeFromLeft (104).reduced (0, 3));
    compInBtn.setBounds (bottom.removeFromRight (92));
}

} // namespace tekk

//==============================================================================
TekkChild670Editor::TekkChild670Editor (TekkChild670Processor& p)
    : AudioProcessorEditor (p),
      processor (p),
      stripA (p, 0, "CHANNEL A - LEFT / LAT"),
      stripB (p, 1, "CHANNEL B - RIGHT / VERT")
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (stripA);
    addAndMakeVisible (stripB);

    auto setupGlobalBox = [this] (juce::ComboBox& box, juce::Label& label,
                                  const juce::String& text, const char* paramId)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (label);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                processor.apvts.getParameter (paramId)))
            box.addItemList (choice->choices, 1);

        addAndMakeVisible (box);
    };

    setupGlobalBox (modeBox, modeLb, "MODE", tekk::pid::mode);
    setupGlobalBox (qualityBox, qualityLb, "ENGINE", tekk::pid::quality);

    modeAt    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, tekk::pid::mode, modeBox);
    qualityAt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, tekk::pid::quality, qualityBox);

    addAndMakeVisible (puristBtn);
    addAndMakeVisible (bypassBtn);

    puristAt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, tekk::pid::purist, puristBtn);
    bypassAt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, tekk::pid::bypass, bypassBtn);

    // -- preset bar --
    presetLb.setText ("PRESET", juce::dontSendNotification);
    presetLb.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (presetLb);

    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);

    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != processor.getCurrentProgram())
            loadProgram (idx);
    };
    addAndMakeVisible (presetBox);

    prevPreset.onClick = [this] { loadProgram (processor.getCurrentProgram() - 1); };
    nextPreset.onClick = [this] { loadProgram (processor.getCurrentProgram() + 1); };
    addAndMakeVisible (prevPreset);
    addAndMakeVisible (nextPreset);

    refreshPresetBox();
    startTimerHz (8); // keep the box in step with host-driven program changes

    setSize (960, 632);
}

void TekkChild670Editor::loadProgram (int index)
{
    if (juce::isPositiveAndBelow (index, processor.getNumPrograms()))
    {
        processor.setCurrentProgram (index);
        refreshPresetBox();
    }
}

void TekkChild670Editor::refreshPresetBox()
{
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
}

void TekkChild670Editor::timerCallback()
{
    if (presetBox.getSelectedId() - 1 != processor.getCurrentProgram())
        refreshPresetBox();
}

TekkChild670Editor::~TekkChild670Editor()
{
    setLookAndFeel (nullptr);
}

void TekkChild670Editor::paint (juce::Graphics& g)
{
    g.fillAll (tekk::colours::background);

    auto header = getLocalBounds().removeFromTop (52);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2c33),
                                             0.0f, 0.0f,
                                             tekk::colours::background,
                                             0.0f, (float) header.getBottom(), false));
    g.fillRect (header);

    g.setColour (tekk::colours::cream);
    g.setFont (juce::Font (juce::FontOptions (21.0f)).boldened());
    g.drawText ("TEKKCHILD TA-670 MK2", header.reduced (16, 0),
                juce::Justification::centredLeft);

    g.setColour (tekk::colours::amber);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("TEKK AUDIO ENGINEERING  ·  VARIABLE-MU TUBE COMPRESSOR",
                header.reduced (16, 0), juce::Justification::centredRight);

    g.setColour (tekk::colours::amber.withAlpha (0.6f));
    g.fillRect (0, header.getBottom() - 1, getWidth(), 1);
}

void TekkChild670Editor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (52); // header, painted

    auto presetBar = r.removeFromTop (36).reduced (16, 4);
    presetLb.setBounds (presetBar.removeFromLeft (52));
    prevPreset.setBounds (presetBar.removeFromLeft (30).reduced (1));
    presetBox.setBounds (presetBar.removeFromLeft (240).reduced (2, 1));
    nextPreset.setBounds (presetBar.removeFromLeft (30).reduced (1));

    auto footer = r.removeFromBottom (52).reduced (14, 10);

    auto body = r.reduced (12, 6);
    const int half = body.getWidth() / 2;
    stripA.setBounds (body.removeFromLeft (half).reduced (4, 0));
    stripB.setBounds (body.reduced (4, 0));

    modeLb.setBounds (footer.removeFromLeft (44));
    modeBox.setBounds (footer.removeFromLeft (130));
    footer.removeFromLeft (18);
    qualityLb.setBounds (footer.removeFromLeft (54));
    qualityBox.setBounds (footer.removeFromLeft (180));

    bypassBtn.setBounds (footer.removeFromRight (92));
    puristBtn.setBounds (footer.removeFromRight (92));
}
