#include "PluginEditor.h"

#include <BinaryData.h>

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
const juce::Colour needle      { 0xff8c241a };
const juce::Colour knobTrack   { 0xff3a3a40 };

const juce::Colour metal       { 0xff55585f }; // gunmetal chassis base
const juce::Colour glow        { 0xffff7a16 }; // backlit lamp orange
} // namespace colours

//==============================================================================
// A hammered / brushed gunmetal texture, generated once and cached. Dappled
// light and dark dimples over a vertical brushed gradient read as struck metal.
inline juce::Image makeHammeredMetal (int w, int h, juce::Colour base, int seed)
{
    juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
    juce::Graphics g (img);

    g.setGradientFill (juce::ColourGradient (base.brighter (0.16f), 0.0f, 0.0f,
                                             base.darker (0.16f), 0.0f, (float) h, false));
    g.fillAll();

    juce::Random r (seed);

    // hammered dimples: each is a soft highlight offset against a soft shadow
    const int dimples = juce::jmax (60, (w * h) / 650);
    for (int i = 0; i < dimples; ++i)
    {
        const float x   = r.nextFloat() * w;
        const float y   = r.nextFloat() * h;
        const float rad = 6.0f + r.nextFloat() * 12.0f;

        g.setGradientFill (juce::ColourGradient (
            juce::Colours::white.withAlpha (0.09f), x - rad * 0.4f, y - rad * 0.4f,
            juce::Colours::transparentBlack,         x + rad,         y + rad, true));
        g.fillEllipse (x - rad, y - rad, rad * 2.0f, rad * 2.0f);

        g.setGradientFill (juce::ColourGradient (
            juce::Colours::black.withAlpha (0.13f), x + rad * 0.45f, y + rad * 0.45f,
            juce::Colours::transparentBlack,        x - rad,        y - rad, true));
        g.fillEllipse (x - rad, y - rad, rad * 2.0f, rad * 2.0f);
    }

    // fine vertical brushing
    for (int i = 0; i < w; i += 2)
    {
        const float a = r.nextFloat() * 0.03f;
        g.setColour ((r.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (a));
        g.drawVerticalLine (i, 0.0f, (float) h);
    }

    // subtle edge vignette for depth
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, w * 0.5f, h * 0.5f,
                                             juce::Colours::black.withAlpha (0.22f), 0.0f, 0.0f, true));
    g.fillRect (0, 0, w, h);

    return img;
}

// A domed, slotted screw head with a recessed shadow ring.
inline void drawScrew (juce::Graphics& g, float cx, float cy, float radius, float slotDeg)
{
    const juce::Rectangle<float> bounds (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // recess shadow under the head
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (bounds.expanded (1.6f).translated (0.0f, 1.0f));

    // metal head, lit from top-left
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb9bcc2), cx - radius * 0.5f, cy - radius * 0.5f,
                                             juce::Colour (0xff4c4e54), cx + radius * 0.6f, cy + radius * 0.6f, true));
    g.fillEllipse (bounds);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (bounds, 1.0f);

    // slot (with a lit lower edge for depth)
    const float a  = juce::degreesToRadians (slotDeg);
    const juce::Point<float> c (cx, cy);
    const auto p1 = c.getPointOnCircumference (radius * 0.72f, a);
    const auto p2 = c.getPointOnCircumference (radius * 0.72f, a + juce::MathConstants<float>::pi);

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine ({ p1, p2 }, radius * 0.34f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawLine ({ p1.translated (0.0f, 0.8f), p2.translated (0.0f, 0.8f) }, radius * 0.14f);
}

// Recessed dark window (meter cut-out) with an inner bevel.
inline void drawRecess (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.expanded (2.0f), corner + 2.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colours::black, r.getX(), r.getY(),
                                             juce::Colour (0xff202024), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
}

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
    auto full = getLocalBounds().toFloat();
    auto r    = full.reduced (7.0f); // leave a margin for the glow halo

    // recessed metal window the meter is mounted in
    drawRecess (g, r.expanded (3.0f), 8.0f);

    // warm backlight bleeding out around the glass
    g.setGradientFill (juce::ColourGradient (colours::glow.withAlpha (0.55f), r.getCentreX(), r.getCentreY(),
                                             juce::Colours::transparentBlack, r.getCentreX(), r.getY() - 6.0f, true));
    g.fillRoundedRectangle (full, 9.0f);

    // backlit meter face: bright lamp behind an amber scale
    juce::ColourGradient face (juce::Colour (0xffffc879), r.getCentreX(), r.getBottom(),
                               juce::Colour (0xff6e2f0c), r.getCentreX(), r.getY(), false);
    face.addColour (0.45, juce::Colour (0xffe98a2c));
    g.setGradientFill (face);
    g.fillRoundedRectangle (r, 6.0f);

    const juce::Point<float> pivot (r.getCentreX(), r.getBottom() - 8.0f);
    const float needleLen = r.getHeight() * 0.72f;

    // 0 dB GR rests on the right, deeper reduction swings the needle left
    auto angleFor = [] (float grDb)
    {
        const float t = juce::jlimit (0.0f, 1.0f, grDb / 20.0f);
        return juce::jmap (t, 0.38f, -0.38f) * juce::MathConstants<float>::pi;
    };

    g.setColour (juce::Colour (0xff3a1c08).withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)).boldened());

    for (float tick : { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f })
    {
        const float a  = angleFor (tick);
        const auto p1  = pivot.getPointOnCircumference (needleLen, a);
        const auto p2  = pivot.getPointOnCircumference (needleLen - 6.0f, a);
        g.drawLine (juce::Line<float> (p1, p2), 1.4f);

        const auto tp = pivot.getPointOnCircumference (needleLen + 8.0f, a);
        g.drawText (juce::String ((int) tick),
                    juce::Rectangle<float> (18.0f, 10.0f).withCentre (tp),
                    juce::Justification::centred);
    }

    g.drawText ("GAIN REDUCTION  dB",
                juce::Rectangle<float> (r.getWidth(), 12.0f)
                    .withCentre ({ r.getCentreX(), r.getBottom() - 14.0f }),
                juce::Justification::centred);

    // glass reflection across the top of the face
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), r.getX(), r.getY(),
                                             juce::Colours::transparentBlack, r.getX(), r.getCentreY(), false));
    g.fillRoundedRectangle (r.withTrimmedBottom (r.getHeight() * 0.55f), 6.0f);

    const float a = angleFor (needleDb);
    const auto tip = pivot.getPointOnCircumference (needleLen, a);

    g.setColour (juce::Colours::black.withAlpha (0.35f)); // needle shadow
    g.drawLine ({ pivot.translated (1.2f, 1.2f), tip.translated (1.2f, 1.2f) }, 2.2f);
    g.setColour (colours::needle);
    g.drawLine ({ pivot, tip }, 2.2f);

    // hub
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff70757c), pivot.x - 3.0f, pivot.y - 3.0f,
                                             juce::Colour (0xff202024), pivot.x + 3.0f, pivot.y + 3.0f, true));
    g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (pivot));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (pivot), 1.0f);
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
    auto full = getLocalBounds().toFloat();
    auto caption = full.removeFromBottom (14.0f);
    auto r = full;

    drawRecess (g, r, 3.0f);

    constexpr float minDb = -60.0f, maxDb = 6.0f;
    auto yFor = [&] (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        return r.getBottom() - t * r.getHeight();
    };

    const float bx = r.getX() + 1.5f, bw = r.getWidth() - 3.0f;

    // unlit scale ghost, so the bar reads against a faint backlit column
    g.setColour (colours::glow.withAlpha (0.05f));
    g.fillRect (bx, r.getY() + 1.0f, bw, r.getHeight() - 2.0f);

    // filled segments, coloured by zone, with a soft glow under the lit part
    auto fillTo = [&] (float lo, float hi, juce::Colour c)
    {
        const float top = yFor (std::min (levelDb, hi));
        const float bot = yFor (lo);
        if (levelDb > lo && bot > top)
        {
            g.setColour (c);
            g.fillRect (bx, top, bw, bot - top);
        }
    };

    fillTo (minDb, -12.0f, juce::Colour (0xff58c47e)); // green
    fillTo (-12.0f, -3.0f, juce::Colour (0xfff0a92e)); // amber
    fillTo (-3.0f, maxDb, juce::Colour (0xffe2483a));  // red

    if (levelDb > minDb)
    {
        const float top = yFor (levelDb);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.5f), bx, top,
                                                 juce::Colours::transparentBlack, bx, top + 10.0f, false));
        g.fillRect (bx, top, bw, 10.0f); // bright lit tip
    }

    // 0 dBFS reference and peak marker
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawHorizontalLine ((int) yFor (0.0f), r.getX(), r.getRight());

    if (peakDb > minDb)
    {
        g.setColour (peakDb > -1.0f ? juce::Colour (0xffe2483a) : colours::cream);
        g.fillRect (bx, yFor (peakDb) - 1.0f, bw, 2.0f);
    }

    // glass sheen
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), r.getX(), r.getY(),
                                             juce::Colours::transparentBlack, r.getRight(), r.getY(), false));
    g.fillRoundedRectangle (r, 3.0f);

    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.5f)).boldened());
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

    // raised brushed-metal sub-plate, screwed onto the chassis
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 8.0f); // drop shadow

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3e4046), r.getCentreX(), r.getY(),
                                             juce::Colour (0xff2a2b2f), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 8.0f);

    // top bevel highlight / bottom shade for a raised plate
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawLine (r.getX() + 8.0f, r.getY() + 1.0f, r.getRight() - 8.0f, r.getY() + 1.0f, 1.2f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r, 8.0f, 1.2f);

    // engraved title
    auto titleArea = getLocalBounds().removeFromTop (30).reduced (28, 0);
    g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawText (title, titleArea.translated (0, 1), juce::Justification::centredLeft);
    g.setColour (colours::amber);
    g.drawText (title, titleArea, juce::Justification::centredLeft);

    // corner screws
    const float inset = 13.0f;
    drawScrew (g, r.getX() + inset,     r.getY() + inset,      4.5f, 25.0f);
    drawScrew (g, r.getRight() - inset, r.getY() + inset,      4.5f, -40.0f);
    drawScrew (g, r.getX() + inset,     r.getBottom() - inset, 4.5f, 70.0f);
    drawScrew (g, r.getRight() - inset, r.getBottom() - inset, 4.5f, 10.0f);
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

    // -- TUBE section: type selector above the mascot, DRIVE / BIAS / VOLTAGE
    //    knobs below it --
    auto setupTubeKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name,
                                 double dflt, const juce::String& tip)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
        s.setDoubleClickReturnValue (true, dflt);
        s.setTooltip (tip);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (juce::FontOptions (11.0f)));
        l.setColour (juce::Label::textColourId, tekk::colours::textDim);
        addAndMakeVisible (l);
    };

    setupTubeKnob (driveSlider, driveLb, "DRIVE", 50.0,
                   "Drive - tube & transformer saturation (50 = stock 670)");
    setupTubeKnob (biasSlider, biasLb, "BIAS", 0.0,
                   "Tube Bias - shifts the operating point toward cutoff; adds 2nd-harmonic warmth");
    setupTubeKnob (voltSlider, voltLb, "VOLTAGE", 250.0,
                   "Plate Voltage - headroom; lower = starved/grittier, higher = cleaner");

    driveAt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, tekk::pid::drive, driveSlider);
    biasAt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, tekk::pid::tubeBias, biasSlider);
    voltAt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, tekk::pid::tubeVolt, voltSlider);

    tubeLb.setText ("TUBE", juce::dontSendNotification);
    tubeLb.setJustificationType (juce::Justification::centred);
    tubeLb.setFont (juce::Font (juce::FontOptions (11.0f)));
    tubeLb.setColour (juce::Label::textColourId, tekk::colours::textDim);
    addAndMakeVisible (tubeLb);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            processor.apvts.getParameter (tekk::pid::tubeType)))
        tubeBox.addItemList (choice->choices, 1);
    tubeBox.setTooltip ("Tube Type - roll a different valve into the gain stage");
    addAndMakeVisible (tubeBox);
    tubeAt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, tekk::pid::tubeType, tubeBox);

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

    faceArt = juce::ImageCache::getFromMemory (BinaryData::tekkchild_face_png,
                                               BinaryData::tekkchild_face_pngSize);

    noseHotspot.onSecretCombo = [this] { processor.triggerEasterEgg(); };
    addAndMakeVisible (noseHotspot);

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
    // hammered-metal chassis
    if (chassis.isValid())
        g.drawImage (chassis, getLocalBounds().toFloat());
    else
        g.fillAll (tekk::colours::metal);

    auto header = getLocalBounds().removeFromTop (52);

    // darker brushed nameplate across the top
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff34363c), 0.0f, 0.0f,
                                             juce::Colour (0xff202024), 0.0f, (float) header.getBottom(), false));
    g.fillRect (header);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (1, 0.0f, (float) getWidth());

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (21.0f)).boldened());
    g.drawText ("TEKKCHILD TC-670", header.reduced (40, 0).translated (0, 1),
                juce::Justification::centredLeft);
    g.setColour (tekk::colours::cream);
    g.drawText ("TEKKCHILD TC-670", header.reduced (40, 0),
                juce::Justification::centredLeft);

    g.setColour (tekk::colours::amber);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("TEKK ENGINEERING AUDIO LABS  ·  VARI-MU COMPRESSOR",
                header.reduced (74, 0), juce::Justification::centredRight);

    g.setColour (juce::Colour (0xff141416));
    g.fillRect (0, header.getBottom() - 1, getWidth(), 2);
    g.setColour (tekk::colours::amber.withAlpha (0.4f));
    g.fillRect (0, header.getBottom(), getWidth(), 1);

    // chassis corner screws
    for (auto p : { juce::Point<float> (18.0f, 26.0f),
                    juce::Point<float> ((float) getWidth() - 18.0f, 26.0f),
                    juce::Point<float> (18.0f, (float) getHeight() - 18.0f),
                    juce::Point<float> ((float) getWidth() - 18.0f, (float) getHeight() - 18.0f) })
        tekk::drawScrew (g, p.x, p.y, 6.0f, 30.0f * (p.x + p.y));

    // central faceplate graphic, mounted between the two channel sections
    if (! faceArea.isEmpty())
    {
        auto fa = faceArea.toFloat();

        // raised metal frame plate
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (fa.translated (0.0f, 2.0f), 9.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3e4046), fa.getCentreX(), fa.getY(),
                                                 juce::Colour (0xff26272b), fa.getCentreX(), fa.getBottom(), false));
        g.fillRoundedRectangle (fa, 9.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawLine (fa.getX() + 8.0f, fa.getY() + 1.0f, fa.getRight() - 8.0f, fa.getY() + 1.0f, 1.2f);
        g.setColour (juce::Colour (0xff141416));
        g.drawRoundedRectangle (fa, 9.0f, 1.4f);

        auto win   = fa.reduced (8.0f);
        auto label = win.removeFromBottom (20.0f); // engraved nameplate strip
        win.removeFromBottom (3.0f);

        tekk::drawRecess (g, win, 6.0f);

        // warm backlight behind the art
        g.setGradientFill (juce::ColourGradient (tekk::colours::glow.withAlpha (0.35f),
                                                 win.getCentreX(), win.getCentreY(),
                                                 juce::Colours::transparentBlack, win.getCentreX(), win.getY(), true));
        g.fillRoundedRectangle (win, 6.0f);

        if (faceArt.isValid() && ! faceImageRect.isEmpty())
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (win.reduced (2.0f), 5.0f);
            g.reduceClipRegion (clip);

            // whole artwork, fit (not cropped), so the full face is shown
            g.drawImageWithin (faceArt, faceImageRect.getX(), faceImageRect.getY(),
                               faceImageRect.getWidth(), faceImageRect.getHeight(),
                               juce::RectanglePlacement::centred);
        }

        // glass sheen across the top
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), win.getX(), win.getY(),
                                                 juce::Colours::transparentBlack, win.getX(), win.getCentreY(), false));
        g.fillRoundedRectangle (win.withTrimmedBottom (win.getHeight() * 0.62f), 6.0f);

        g.setColour (juce::Colour (0xff141416));
        g.drawRoundedRectangle (win, 6.0f, 1.2f);

        // engraved nameplate
        g.setFont (juce::Font (juce::FontOptions (14.0f)).boldened());
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawText ("TEAL", label.translated (0.0f, 1.0f), juce::Justification::centred);
        g.setColour (tekk::colours::amber);
        g.drawText ("TEAL", label, juce::Justification::centred);

        // corner screws on the plate
        const float ins = 12.0f;
        tekk::drawScrew (g, fa.getX() + ins,     fa.getY() + ins,      4.5f, 20.0f);
        tekk::drawScrew (g, fa.getRight() - ins, fa.getY() + ins,      4.5f, -35.0f);
        tekk::drawScrew (g, fa.getX() + ins,     fa.getBottom() - ins, 4.5f, 55.0f);
        tekk::drawScrew (g, fa.getRight() - ins, fa.getBottom() - ins, 4.5f, 5.0f);
    }
}

void TekkChild670Editor::resized()
{
    if (chassis.getWidth() != getWidth() || chassis.getHeight() != getHeight())
        chassis = tekk::makeHammeredMetal (getWidth(), getHeight(), tekk::colours::metal, 7);

    auto r = getLocalBounds();
    r.removeFromTop (52); // header, painted

    auto presetBar = r.removeFromTop (36).reduced (16, 4);
    presetLb.setBounds (presetBar.removeFromLeft (52));
    prevPreset.setBounds (presetBar.removeFromLeft (30).reduced (1));
    presetBox.setBounds (presetBar.removeFromLeft (240).reduced (2, 1));
    nextPreset.setBounds (presetBar.removeFromLeft (30).reduced (1));

    auto footer = r.removeFromBottom (52).reduced (14, 10);

    auto body = r.reduced (12, 6);
    const int colW  = 280;
    const int sideW = (body.getWidth() - colW) / 2;
    stripA.setBounds (body.removeFromLeft (sideW).reduced (4, 0));
    auto column = body.removeFromLeft (colW);
    stripB.setBounds (body.reduced (4, 0));

    // tube bay: TYPE selector on top, mascot in the middle, knobs below
    auto tubeTypeArea = column.removeFromTop (40);
    auto knobRow      = column.removeFromBottom (94);

    const int plateH = juce::jmin (column.getHeight(), colW - 4);
    faceArea = column.withSizeKeepingCentre (colW, plateH).reduced (4, 2);

    // mirror the faceplate maths in paint() to locate the artwork, then the nose
    {
        auto win = faceArea.toFloat().reduced (8.0f);
        win.removeFromBottom (20.0f + 3.0f); // nameplate strip
        auto inner = win.reduced (2.0f);

        const float side = juce::jmin (inner.getWidth(), inner.getHeight());
        faceImageRect = juce::Rectangle<float> (side, side)
                            .withCentre (inner.getCentre()).toNearestInt();

        // the mascot's nose sits a little above centre
        const float ns = side * 0.26f;
        noseHotspot.setBounds (juce::Rectangle<float> (ns, ns)
            .withCentre ({ (float) faceImageRect.getCentreX(),
                           faceImageRect.getY() + side * 0.42f }).toNearestInt());
    }

    // tube type selector
    tubeLb.setBounds (tubeTypeArea.removeFromLeft (46).withSizeKeepingCentre (46, 22));
    tubeBox.setBounds (tubeTypeArea.reduced (2, 7));

    // DRIVE / BIAS / VOLTAGE knobs
    {
        auto rowR = knobRow.reduced (6, 2);
        const int cw = rowR.getWidth() / 3;
        auto place = [] (juce::Rectangle<int> cell, juce::Slider& s, juce::Label& l)
        {
            l.setBounds (cell.removeFromBottom (14));
            s.setBounds (cell);
        };
        place (rowR.removeFromLeft (cw), driveSlider, driveLb);
        place (rowR.removeFromLeft (cw), biasSlider,  biasLb);
        place (rowR,                     voltSlider,  voltLb);
    }

    modeLb.setBounds (footer.removeFromLeft (44));
    modeBox.setBounds (footer.removeFromLeft (130));
    footer.removeFromLeft (18);
    qualityLb.setBounds (footer.removeFromLeft (54));
    qualityBox.setBounds (footer.removeFromLeft (180));

    bypassBtn.setBounds (footer.removeFromRight (92));
    puristBtn.setBounds (footer.removeFromRight (92));
}
