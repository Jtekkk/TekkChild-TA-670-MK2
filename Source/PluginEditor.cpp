#include "PluginEditor.h"

#include <cmath>

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

    // Dry Gain -- thin horizontal bar (trims the dry side of the Mix blend)
    dryGain.setSliderStyle (juce::Slider::LinearBar);
    dryGain.setTextValueSuffix (" dB");
    dryGain.setDoubleClickReturnValue (true, 0.0);
    dryGain.setColour (juce::Slider::trackColourId, colours::amber.withAlpha (0.5f));
    dryGain.setColour (juce::Slider::textBoxTextColourId, colours::textDim);
    dryGain.setTooltip ("Dry Gain - trims the dry side of the Mix blend");
    addAndMakeVisible (dryGain);

    dryGainLb.setText ("DRY GAIN", juce::dontSendNotification);
    dryGainLb.setJustificationType (juce::Justification::centredLeft);
    dryGainLb.setFont (juce::Font (juce::FontOptions (11.0f)));
    dryGainLb.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (dryGainLb);

    dryGainAt = std::make_unique<SliderAttachment> (apvts, pid::forChannel (pid::dryGain, channel), dryGain);

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

    layoutRow (r.removeFromTop (132), { &input, &threshold, &timeConstant },
                                      { &inputLb, &thresholdLb, &timeConstantLb });
    layoutRow (r.removeFromTop (132), { &dcThreshold, &mix, &output },
                                      { &dcThresholdLb, &mixLb, &outputLb });

    auto dryRow = r.removeFromTop (26);
    dryGainLb.setBounds (dryRow.removeFromLeft (72));
    dryGain.setBounds (dryRow.reduced (2, 4));

    auto bottom = r.removeFromTop (30);
    hpfLb.setBounds (bottom.removeFromLeft (66));
    hpfBox.setBounds (bottom.removeFromLeft (104).reduced (0, 3));
    compInBtn.setBounds (bottom.removeFromRight (92));
}

//==============================================================================
// --- Tape Brain emblem helpers (file-local procedural art) -------------------
namespace
{
const juce::Colour tapeTeal   { 0xff2ce0d2 };
const juce::Colour tapeBrass  { 0xffd8b56f };
const juce::Colour tapeBrassD { 0xff6e571f };

// One supply / take-up reel: dark wound tape, brass flange, spokes, hub bolt.
void paintReel (juce::Graphics& g, juce::Rectangle<float> b, float angle)
{
    const auto c = b.getCentre();

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (b.expanded (1.5f).translated (0.0f, 1.0f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a40), c.x, b.getY(),
                                             juce::Colour (0xff131316), c.x, b.getBottom(), false));
    g.fillEllipse (b);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (float rr = b.getWidth() * 0.46f; rr > b.getWidth() * 0.30f; rr -= 2.5f)
        g.drawEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), 0.6f);

    auto flange = b.reduced (b.getWidth() * 0.30f);
    g.setGradientFill (juce::ColourGradient (tapeBrass.brighter (0.2f), flange.getX(), flange.getY(),
                                             tapeBrassD, flange.getRight(), flange.getBottom(), false));
    g.fillEllipse (flange);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (flange, 1.0f);

    g.setColour (tapeBrassD.darker (0.3f));
    for (int i = 0; i < 3; ++i)
    {
        const float a = angle + (float) i * juce::MathConstants<float>::twoPi / 3.0f;
        const auto p1 = c.getPointOnCircumference (b.getWidth() * 0.15f, a);
        const auto p2 = c.getPointOnCircumference (b.getWidth() * 0.30f, a);
        g.drawLine ({ p1, p2 }, 2.2f);
    }

    g.setColour (juce::Colour (0xffe9dcb4));
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (c), 0.8f);
}

// The centrepiece: a glowing teal brain in a bell jar, on a two-reel deck.
void paintBrainJar (juce::Graphics& g, juce::Rectangle<float> area, float phase, bool powered)
{
    drawRecess (g, area, 10.0f);

    const float pulse = 0.5f + 0.5f * std::sin (phase);

    auto inner = area.reduced (10.0f);
    auto reelStrip = inner.removeFromBottom (inner.getHeight() * 0.30f);

    if (powered)
    {
        g.setGradientFill (juce::ColourGradient (
            tapeTeal.withAlpha (0.10f + 0.22f * pulse), inner.getCentreX(), inner.getCentreY(),
            juce::Colours::transparentBlack, inner.getCentreX(), inner.getY() - 12.0f, true));
        g.fillRect (area);
    }

    // jar base plate
    auto baseEll = juce::Rectangle<float> (inner.getWidth() * 0.62f, 13.0f)
                       .withCentre ({ inner.getCentreX(), inner.getBottom() - 3.0f });
    g.setColour (juce::Colour (0xff2a2a30));
    g.fillEllipse (baseEll);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (baseEll, 1.0f);

    // bell-jar glass envelope (rounded only at the dome)
    auto jar = inner.withSizeKeepingCentre (inner.getWidth() * 0.62f, inner.getHeight() * 0.94f);
    const float cR = jar.getWidth() * 0.5f;
    juce::Path glass;
    glass.addRoundedRectangle (jar.getX(), jar.getY(), jar.getWidth(), jar.getHeight(),
                               cR, cR, true, true, false, false);

    // brain, clipped inside the glass
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (glass);

        auto brain = jar.reduced (jar.getWidth() * 0.14f);
        brain = brain.withTrimmedTop (brain.getHeight() * 0.16f);
        const float lobeW = brain.getWidth() * 0.60f;
        const float lobeH = brain.getHeight() * 0.74f;

        g.setColour (tapeTeal.withAlpha (powered ? (0.30f + 0.25f * pulse) : 0.22f));
        g.fillEllipse (brain.expanded (6.0f)); // glow bloom

        auto drawLobe = [&] (float cx)
        {
            juce::Rectangle<float> lobe (lobeW, lobeH);
            lobe.setCentre (cx, brain.getCentreY());
            g.setGradientFill (juce::ColourGradient (tapeTeal.brighter (0.4f), lobe.getCentreX(), lobe.getY(),
                                                     tapeTeal.darker (0.5f), lobe.getCentreX(), lobe.getBottom(), false));
            g.fillEllipse (lobe);
        };
        drawLobe (brain.getCentreX() - lobeW * 0.26f);
        drawLobe (brain.getCentreX() + lobeW * 0.26f);

        g.setColour (tapeTeal.darker (0.7f).withAlpha (0.7f));
        for (int i = 0; i < 5; ++i)
        {
            const float y = brain.getY() + brain.getHeight() * (0.20f + 0.15f * (float) i);
            juce::Path w;
            w.startNewSubPath (brain.getX() + 4.0f, y);
            w.cubicTo (brain.getCentreX() - 8.0f, y - 6.0f,
                       brain.getCentreX() + 8.0f, y + 6.0f,
                       brain.getRight() - 4.0f, y);
            g.strokePath (w, juce::PathStrokeType (1.4f));
        }

        g.setColour (tapeTeal.darker (0.8f).withAlpha (0.8f));
        g.drawLine (brain.getCentreX(), brain.getY() + 2.0f, brain.getCentreX(), brain.getBottom() - 2.0f, 2.0f);

        if (powered)
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f + 0.45f * pulse));
            g.fillEllipse (brain.getCentreX() - 2.0f, brain.getY() + 1.0f, 4.0f, 4.0f);
        }
    }

    // glass sheen + rim
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), jar.getX(), jar.getY(),
                                             juce::Colours::transparentBlack, jar.getCentreX(), jar.getCentreY(), false));
    g.fillPath (glass);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.strokePath (glass, juce::PathStrokeType (1.4f));
    g.setColour (juce::Colour (0xff0c0c0e));
    g.strokePath (glass, juce::PathStrokeType (0.8f));

    // two reels on the deck, counter-rotating
    auto reelArea = reelStrip.reduced (6.0f, 2.0f);
    const float reelD = juce::jmin (reelArea.getHeight(), reelArea.getWidth() * 0.42f);
    juce::Rectangle<float> lr (reelD, reelD), rr (reelD, reelD);
    lr.setCentre (reelArea.getX() + reelD * 0.55f, reelArea.getCentreY());
    rr.setCentre (reelArea.getRight() - reelD * 0.55f, reelArea.getCentreY());

    g.setColour (juce::Colour (0xff1a1a1d));
    g.drawLine (lr.getCentreX(), lr.getY() + 2.0f, rr.getCentreX(), rr.getY() + 2.0f, 2.0f);

    paintReel (g, lr,  phase);
    paintReel (g, rr, -phase * 1.3f);
}

// Backlit vertical segment ladder for the TAPE SATURATION readout (value 0..1),
// suited to the tall narrow window between the jar and the throw switch.
void paintTapeVu (juce::Graphics& g, juce::Rectangle<float> r, float value)
{
    // engraved caption strip across the bottom
    auto caption = r.removeFromBottom (14.0f);
    drawRecess (g, r, 5.0f);

    auto col = r.reduced (6.0f, 6.0f);
    const int   segs    = 14;
    const float segH    = col.getHeight() / (float) segs;
    const float lit     = juce::jlimit (0.0f, 1.0f, value) * segs;

    for (int i = 0; i < segs; ++i)
    {
        auto seg = juce::Rectangle<float> (col.getX(), col.getBottom() - (float) (i + 1) * segH + 1.0f,
                                           col.getWidth(), segH - 2.0f);
        const float frac = (float) i / (float) (segs - 1);
        juce::Colour on = frac < 0.6f ? tapeTeal
                                      : (frac < 0.85f ? juce::Colour (0xfff0a92e)
                                                      : juce::Colour (0xffe2483a));
        if ((float) i < lit)
        {
            g.setColour (on);
            g.fillRoundedRectangle (seg, 1.5f);
            g.setColour (on.withAlpha (0.35f));
            g.fillRoundedRectangle (seg.expanded (1.5f, 0.0f), 1.5f); // bloom
        }
        else
        {
            g.setColour (on.withAlpha (0.10f)); // unlit ghost
            g.fillRoundedRectangle (seg, 1.5f);
        }
    }

    // glass sheen + bezel
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), r.getX(), r.getY(),
                                             juce::Colours::transparentBlack, r.getRight(), r.getY(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colour (0xff0c0c0e));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    g.setColour (colours::cream);
    g.setFont (juce::Font (juce::FontOptions (8.5f)).boldened());
    g.drawText ("TAPE SAT", caption, juce::Justification::centred);
}
} // anonymous namespace

//==============================================================================
// A Frankenstein knife-throw switch: blade down = SERIES, up = PARALLEL.
void ThrowSwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool parallel = getToggleState();

    // bakelite mounting base
    g.setColour (juce::Colour (0xff1c160e));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawLine (r.getX() + 6.0f, r.getY() + 1.5f, r.getRight() - 6.0f, r.getY() + 1.5f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    const float cx   = r.getCentreX();
    const float topY = r.getY() + 16.0f;
    const float botY = r.getBottom() - 16.0f;
    const float midY = (topY + botY) * 0.5f;

    // engraved position labels, the live one lit
    g.setFont (juce::Font (juce::FontOptions (9.0f)).boldened());
    g.setColour (parallel ? tapeTeal : juce::Colours::black.withAlpha (0.55f));
    g.drawText ("PARALLEL", juce::Rectangle<float> (r.getX(), r.getY() + 1.0f, r.getWidth(), 12.0f),
                juce::Justification::centred);
    g.setColour (! parallel ? juce::Colour (0xffff7a16) : juce::Colours::black.withAlpha (0.55f));
    g.drawText ("SERIES", juce::Rectangle<float> (r.getX(), r.getBottom() - 13.0f, r.getWidth(), 12.0f),
                juce::Justification::centred);

    // forked brass jaws top + bottom
    auto drawJaw = [&] (float y, bool live)
    {
        juce::Rectangle<float> jw (cx - 9.0f, y - 5.0f, 18.0f, 10.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (jw.expanded (1.2f), 3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8cd92), jw.getX(), jw.getY(),
                                                 juce::Colour (0xff715a22), jw.getX(), jw.getBottom(), false));
        g.fillRoundedRectangle (jw, 3.0f);
        if (live)
        {
            g.setColour ((y < midY ? tapeTeal : juce::Colour (0xffff7a16)).withAlpha (0.9f));
            g.fillRoundedRectangle (jw.reduced (4.0f, 3.0f), 2.0f);
        }
    };
    drawJaw (topY, parallel);
    drawJaw (botY, ! parallel);

    // blade swings about the centre pivot to the live jaw; handle just beyond
    const juce::Point<float> pivot (cx, midY);
    const float reach = midY - topY;
    const juce::Point<float> jawPt = parallel ? juce::Point<float> (cx, topY)
                                              : juce::Point<float> (cx, botY);
    const juce::Point<float> dir    = (jawPt - pivot) / reach;
    const juce::Point<float> handle = pivot + dir * (reach + 13.0f);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine (juce::Line<float> (pivot.translated (1.5f, 1.5f), jawPt.translated (1.5f, 1.5f)), 7.0f);
    g.setColour (juce::Colour (0xffcaa85f));
    g.drawLine (juce::Line<float> (pivot, jawPt), 7.0f);
    g.setColour (juce::Colour (0xfff3e0ab));
    g.drawLine (juce::Line<float> (pivot, jawPt), 2.4f);

    // insulated rod to the grip
    g.setColour (juce::Colour (0xff3a2c18));
    g.drawLine (juce::Line<float> (jawPt, handle), 5.0f);

    // red bakelite grip ball
    juce::Rectangle<float> ball (14.0f, 14.0f);
    ball.setCentre (handle);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (ball.expanded (1.5f).translated (0.0f, 1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (highlighted ? 0xffff6a5a : 0xffd2402f),
                                             ball.getX(), ball.getY(),
                                             juce::Colour (0xff5e160d), ball.getRight(), ball.getBottom(), false));
    g.fillEllipse (ball);
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.fillEllipse (ball.getX() + 3.5f, ball.getY() + 3.0f, 3.5f, 3.0f);

    // pivot bolt over the blade
    juce::Rectangle<float> hub (10.0f, 10.0f);
    hub.setCentre (pivot);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffbfc3c9), hub.getX(), hub.getY(),
                                             juce::Colour (0xff4a4c52), hub.getRight(), hub.getBottom(), false));
    g.fillEllipse (hub);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (hub, 1.0f);
}

//==============================================================================
TapeBrainPanel::TapeBrainPanel (TekkChild670Processor& p)
    : processor (p)
{
    setupKnob (input,   inputLb,   "INPUT",    pid::tapeInput);
    setupKnob (drive,   driveLb,   "DRIVE",    pid::tapeDrive);
    setupKnob (sat,     satLb,     "SAT",      pid::tapeSat);
    setupKnob (bias,    biasLb,    "BIAS",     pid::tapeBias);
    setupKnob (wow,     wowLb,     "WOW/FLUT", pid::tapeWow);
    setupKnob (hiss,    hissLb,    "HISS",     pid::tapeHiss);
    setupKnob (noise,   noiseLb,   "NOISE",    pid::tapeNoise);
    setupKnob (xtalk,   xtalkLb,   "X-TALK",   pid::tapeXtalk);
    setupKnob (degrade, degradeLb, "DEGRADE",  pid::tapeDegrade);
    setupKnob (output,  outputLb,  "OUTPUT",   pid::tapeOutput);

    powerBtn.setTooltip ("Tape Brain - engage the magnetic tape colour section");
    addAndMakeVisible (powerBtn);
    powerAt = std::make_unique<ButtonAttachment> (processor.apvts, pid::tapeOn, powerBtn);

    routeSwitch.setTooltip ("Throw down for SERIES (after the compressor), up for PARALLEL (blended alongside)");
    addAndMakeVisible (routeSwitch);
    routeAt = std::make_unique<ButtonAttachment> (processor.apvts, pid::tapeParallel, routeSwitch);

    startTimerHz (30);
}

void TapeBrainPanel::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name,
                                const juce::String& pidStr)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 13);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::Font (juce::FontOptions (10.5f)));
    l.setColour (juce::Label::textColourId, colours::cream);
    addAndMakeVisible (l);

    knobAtts.push_back (std::make_unique<SliderAttachment> (processor.apvts, pidStr, s));
}

void TapeBrainPanel::timerCallback()
{
    jarPhase += 0.05f;
    if (jarPhase > juce::MathConstants<float>::twoPi)
        jarPhase -= juce::MathConstants<float>::twoPi;

    const float target = processor.getTapeSaturation();
    vuValue += (target > vuValue ? 0.4f : 0.12f) * (target - vuValue);

    repaint();
}

void TapeBrainPanel::paint (juce::Graphics& g)
{
    auto rF = getLocalBounds().toFloat().reduced (2.0f);

    if (chassis.getWidth() != getWidth() || chassis.getHeight() != getHeight())
        chassis = makeHammeredMetal (getWidth(), getHeight(), juce::Colour (0xff6e5a34), 23);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (rF.translated (0.0f, 2.0f), 10.0f);

    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (rF, 10.0f);
        g.reduceClipRegion (clip);
        g.drawImage (chassis, rF);
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, rF.getCentreX(), rF.getCentreY(),
                                                 juce::Colours::black.withAlpha (0.35f), rF.getX(), rF.getY(), true));
        g.fillRoundedRectangle (rF, 10.0f);
    }

    // brass bevel frame + rivets
    g.setColour (juce::Colour (0xff2a2114));
    g.drawRoundedRectangle (rF, 10.0f, 2.0f);
    g.setColour (tapeBrass.withAlpha (0.5f));
    g.drawRoundedRectangle (rF.reduced (2.5f), 8.0f, 1.0f);

    const float ins = 14.0f;
    drawScrew (g, rF.getX() + ins,     rF.getY() + ins,      5.0f, 18.0f);
    drawScrew (g, rF.getRight() - ins, rF.getY() + ins,      5.0f, -30.0f);
    drawScrew (g, rF.getX() + ins,     rF.getBottom() - ins, 5.0f, 60.0f);
    drawScrew (g, rF.getRight() - ins, rF.getBottom() - ins, 5.0f, 8.0f);

    // title with a pilot lamp
    {
        auto t = nameArea.toFloat();
        const bool on = powerBtn.getToggleState();

        auto lamp = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ t.getX() + 9.0f, t.getCentreY() - 2.0f });
        if (on)
        {
            g.setColour (tapeTeal.withAlpha (0.35f * (0.5f + 0.5f * std::sin (jarPhase))));
            g.fillEllipse (lamp.expanded (6.0f));
        }
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (lamp.expanded (2.0f));
        g.setColour (on ? tapeTeal : juce::Colour (0xff0c3a37));
        g.fillEllipse (lamp);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillEllipse (lamp.getX() + 3.0f, lamp.getY() + 2.5f, 3.0f, 2.5f);

        auto textArea = t.withTrimmedLeft (26.0f);
        g.setFont (juce::Font (juce::FontOptions (19.0f)).boldened());
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawText ("TAPE BRAIN", textArea.translated (0.0f, 1.0f).withTrimmedBottom (9.0f),
                    juce::Justification::centredLeft);
        g.setColour (tapeTeal);
        g.drawText ("TAPE BRAIN", textArea.withTrimmedBottom (9.0f), juce::Justification::centredLeft);

        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.setColour (juce::Colour (0xffe7d6a8).withAlpha (0.8f));
        g.drawText ("MAGNETIC COLOUR LAB", textArea.withTrimmedTop (t.getHeight() * 0.58f),
                    juce::Justification::centredLeft);
    }

    // the brain-in-a-jar emblem and the saturation VU
    paintBrainJar (g, jarArea.toFloat(), jarPhase, powerBtn.getToggleState());
    paintTapeVu  (g, vuArea.toFloat(),  vuValue);

    // routing wires around the throw switch
    {
        auto sw = routeSwitch.getBounds().toFloat();
        const bool parallel = routeSwitch.getToggleState();
        const bool on       = powerBtn.getToggleState();

        const juce::Point<float> inGrommet  (sw.getX() - 13.0f, sw.getCentreY());
        const juce::Point<float> parJaw     (sw.getCentreX(),   sw.getY() + 16.0f);
        const juce::Point<float> serJaw     (sw.getCentreX(),   sw.getBottom() - 16.0f);
        const juce::Point<float> outGrommet (sw.getCentreX(),   sw.getBottom() + 8.0f);

        auto cable = [&] (juce::Point<float> p0, juce::Point<float> p1, bool live, juce::Colour col)
        {
            juce::Path path;
            path.startNewSubPath (p0);
            const float dx = (p1.x - p0.x) * 0.5f;
            path.cubicTo (p0.translated (dx, 0.0f), p1.translated (-dx, 0.0f), p1);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (live && on ? col : col.withAlpha (0.28f));
            g.strokePath (path, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };

        auto grommet = [&] (juce::Point<float> pt)
        {
            g.setColour (juce::Colour (0xff2a2a30));
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (pt));
            g.setColour (juce::Colour (0xffbfc3c9));
            g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (pt), 1.0f);
        };

        cable (inGrommet, serJaw, ! parallel, juce::Colour (0xffff7a16));
        cable (inGrommet, parJaw,   parallel, tapeTeal);
        cable (parallel ? parJaw : serJaw, outGrommet, true, tapeTeal);
        grommet (inGrommet);
        grommet (outGrommet);
    }
}

void TapeBrainPanel::resized()
{
    auto r = getLocalBounds().reduced (12);

    // title band with POWER at the right
    auto titleBand = r.removeFromTop (30);
    powerBtn.setBounds (titleBand.removeFromRight (88).reduced (2, 4));
    nameArea = titleBand;

    // feature row: brain jar (left), VU + wiring gutter + throw switch (right)
    auto feature = r.removeFromTop (188);
    jarArea = feature.removeFromLeft (juce::roundToInt (feature.getWidth() * 0.46f)).reduced (4);
    feature.removeFromLeft (6);

    auto switchCol = feature.removeFromRight (110);
    routeSwitch.setBounds (switchCol.reduced (6, 10));
    switchArea = switchCol;
    feature.removeFromRight (20); // wiring gutter
    vuArea = feature.reduced (2, 12);

    r.removeFromTop (10);

    // knob bank: 5 columns x 2 rows
    auto placeRow = [] (juce::Rectangle<int> row,
                        std::initializer_list<juce::Slider*> ss,
                        std::initializer_list<juce::Label*> ls)
    {
        const int cw = row.getWidth() / (int) ss.size();
        auto s = ss.begin();
        auto l = ls.begin();
        for (size_t i = 0; i < ss.size(); ++i, ++s, ++l)
        {
            auto cell = row.removeFromLeft (cw);
            (*l)->setBounds (cell.removeFromBottom (13));
            (*s)->setBounds (cell.reduced (1));
        }
    };

    const int rowH = juce::jmin (r.getHeight() / 2, 132);
    placeRow (r.removeFromTop (rowH), { &input, &drive, &sat, &bias, &wow },
                                      { &inputLb, &driveLb, &satLb, &biasLb, &wowLb });
    placeRow (r.removeFromTop (rowH), { &hiss, &noise, &xtalk, &degrade, &output },
                                      { &hissLb, &noiseLb, &xtalkLb, &degradeLb, &outputLb });
}

} // namespace tekk

//==============================================================================
TekkChild670Editor::TekkChild670Editor (TekkChild670Processor& p)
    : AudioProcessorEditor (p),
      processor (p),
      stripA (p, 0, "CHANNEL A - LEFT / LAT"),
      stripB (p, 1, "CHANNEL B - RIGHT / VERT"),
      tapePanel (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (stripA);
    addAndMakeVisible (stripB);
    addAndMakeVisible (tapePanel);

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

    autoGainBtn.setTooltip ("Auto Gain - adds the average gain reduction back at the output to hold level");
    safetyBtn.setTooltip ("Safety - gentle output ceiling that catches peaks from heavy Drive / saturation");
    addAndMakeVisible (autoGainBtn);
    addAndMakeVisible (safetyBtn);
    autoGainAt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, tekk::pid::autoMakeup, autoGainBtn);
    safetyAt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, tekk::pid::safety, safetyBtn);

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
                   "Drive - tube saturation amount (50 = stock 670 character)");
    setupTubeKnob (ironSlider, ironLb, "IRON", 100.0,
                   "Iron Drive - transformer saturation (100 = stock; lower cleaner, higher hotter)");
    setupTubeKnob (biasSlider, biasLb, "BIAS", 0.0,
                   "Tube Bias - shifts the operating point toward cutoff; adds 2nd-harmonic warmth");
    setupTubeKnob (voltSlider, voltLb, "VOLTAGE", 250.0,
                   "Plate Voltage - headroom; lower = starved/grittier, higher = cleaner");

    driveAt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, tekk::pid::drive, driveSlider);
    ironAt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, tekk::pid::ironDrive, ironSlider);
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

    // -- global Link Amount + Valve Hiss (footer) --
    linkAmtKnob.setSliderStyle (juce::Slider::LinearBar);
    linkAmtKnob.setTextValueSuffix (" %");
    linkAmtKnob.setDoubleClickReturnValue (true, 100.0);
    linkAmtKnob.setColour (juce::Slider::trackColourId, tekk::colours::amber.withAlpha (0.6f));
    linkAmtKnob.setColour (juce::Slider::textBoxTextColourId, tekk::colours::cream);
    linkAmtKnob.setTooltip ("Link Amount - 100% = hard stereo link, 0% = independent (L/R Linked mode)");
    addAndMakeVisible (linkAmtKnob);

    linkAmtLb.setText ("LINK", juce::dontSendNotification);
    linkAmtLb.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (linkAmtLb);

    linkAmtAt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, tekk::pid::linkAmount, linkAmtKnob);

    noiseBtn.setTooltip ("Valve Hiss - adds a gentle calibrated tube noise floor");
    addAndMakeVisible (noiseBtn);
    noiseAt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, tekk::pid::noiseFloor, noiseBtn);

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

    setSize (1440, 668);
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

    // patch cables crossing the seam from the compressor into the Tape Brain
    if (! tapePanel.getBounds().isEmpty())
    {
        auto sb = stripB.getBounds().toFloat();
        auto tb = tapePanel.getBounds().toFloat();

        auto patch = [&] (float yFrac, juce::Colour col)
        {
            const juce::Point<float> p0 (sb.getRight() - 6.0f, sb.getY() + sb.getHeight() * yFrac);
            const juce::Point<float> p1 (tb.getX() + 3.0f,     tb.getY() + tb.getHeight() * (yFrac + 0.02f));

            juce::Path path;
            path.startNewSubPath (p0);
            const float dx = (p1.x - p0.x) * 0.5f;
            path.cubicTo (p0.translated (dx, 14.0f), p1.translated (-dx, 14.0f), p1);

            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.strokePath (path, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col);
            g.strokePath (path, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // jack plugs at either end
            for (auto pt : { p0, p1 })
            {
                g.setColour (juce::Colour (0xff2a2a30));
                g.fillRoundedRectangle (juce::Rectangle<float> (11.0f, 8.0f).withCentre (pt), 2.0f);
                g.setColour (juce::Colour (0xffbfc3c9));
                g.drawRoundedRectangle (juce::Rectangle<float> (11.0f, 8.0f).withCentre (pt), 2.0f, 1.0f);
            }
        };

        patch (0.40f, juce::Colour (0xff2ce0d2));
        patch (0.56f, juce::Colour (0xffd49a3a));
    }
}

void TekkChild670Editor::resized()
{
    if (chassis.getWidth() != getWidth() || chassis.getHeight() != getHeight())
        chassis = tekk::makeHammeredMetal (getWidth(), getHeight(), tekk::colours::metal, 7);

    auto r = getLocalBounds();
    r.removeFromTop (52); // header, painted

    // Tape Brain: a tall module bolted to the right, below the header.
    auto tapeArea = r.removeFromRight (468);
    tapePanel.setBounds (tapeArea.reduced (10, 8));

    auto presetBar = r.removeFromTop (36).reduced (16, 4);
    presetLb.setBounds (presetBar.removeFromLeft (52));
    prevPreset.setBounds (presetBar.removeFromLeft (30).reduced (1));
    presetBox.setBounds (presetBar.removeFromLeft (240).reduced (2, 1));
    nextPreset.setBounds (presetBar.removeFromLeft (30).reduced (1));

    auto footerFull = r.removeFromBottom (84).reduced (14, 8);
    auto footerRow2 = footerFull.removeFromBottom (32);
    auto footer     = footerFull; // top row

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

    // DRIVE / IRON / BIAS / VOLTAGE knobs
    {
        auto rowR = knobRow.reduced (4, 2);
        const int cw = rowR.getWidth() / 4;
        auto place = [] (juce::Rectangle<int> cell, juce::Slider& s, juce::Label& l)
        {
            l.setBounds (cell.removeFromBottom (14));
            s.setBounds (cell);
        };
        place (rowR.removeFromLeft (cw), driveSlider, driveLb);
        place (rowR.removeFromLeft (cw), ironSlider,  ironLb);
        place (rowR.removeFromLeft (cw), biasSlider,  biasLb);
        place (rowR,                     voltSlider,  voltLb);
    }

    // footer row 1: mode / engine + output toggles
    modeLb.setBounds (footer.removeFromLeft (44));
    modeBox.setBounds (footer.removeFromLeft (130));
    footer.removeFromLeft (16);
    qualityLb.setBounds (footer.removeFromLeft (54));
    qualityBox.setBounds (footer.removeFromLeft (170));

    bypassBtn.setBounds (footer.removeFromRight (86));
    puristBtn.setBounds (footer.removeFromRight (82));
    footer.removeFromRight (12);
    safetyBtn.setBounds (footer.removeFromRight (84));
    autoGainBtn.setBounds (footer.removeFromRight (104));

    // footer row 2: stereo link amount + valve hiss
    linkAmtLb.setBounds (footerRow2.removeFromLeft (44));
    linkAmtKnob.setBounds (footerRow2.removeFromLeft (200).reduced (0, 4));
    noiseBtn.setBounds (footerRow2.removeFromRight (86));
}
