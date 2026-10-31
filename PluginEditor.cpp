#include "PluginEditor.h"

//==============================================================================
SpLook::SpLook()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg());
    setColour (juce::Slider::thumbColourId, accent());
    setColour (juce::Slider::rotarySliderFillColourId, accent());
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a3550));
    setColour (juce::Slider::trackColourId, accent());
    setColour (juce::Slider::backgroundColourId, juce::Colour (0xff2a3550));
    setColour (juce::Slider::textBoxTextColourId, text());
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, text());
    setColour (juce::ComboBox::backgroundColourId, panel());
    setColour (juce::ComboBox::textColourId, text());
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2a3550));
    setColour (juce::ComboBox::arrowColourId, accent());
    setColour (juce::PopupMenu::backgroundColourId, panel());
    setColour (juce::PopupMenu::textColourId, text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent());
    setColour (juce::PopupMenu::highlightedTextColourId, bg());
    setColour (juce::TextButton::buttonColourId, panel());
    setColour (juce::TextButton::buttonOnColourId, accent());
    setColour (juce::TextButton::textColourOffId, dim());
    setColour (juce::TextButton::textColourOnId, bg());
}

void SpLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                               float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool horiz = style == juce::Slider::LinearHorizontal;
    const bool vert  = style == juce::Slider::LinearVertical;
    if (! horiz && ! vert)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
        return;
    }

    const float thick = 6.0f;
    const auto track = horiz ? juce::Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - thick * 0.5f, (float) w, thick)
                             : juce::Rectangle<float> ((float) x + (float) w * 0.5f - thick * 0.5f, (float) y, thick, (float) h);

    g.setColour (s.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (track, thick * 0.5f);

    // zero (or minimum) position along the track
    const double zeroProp = s.valueToProportionOfLength (juce::jlimit (s.getMinimum(), s.getMaximum(), 0.0));
    const float zeroPos = minPos + (float) zeroProp * (maxPos - minPos);

    g.setColour (s.findColour (juce::Slider::trackColourId));
    if (horiz)
        g.fillRoundedRectangle (juce::Rectangle<float> (std::min (zeroPos, sliderPos), track.getY(),
                                                         std::abs (sliderPos - zeroPos), thick), 2.0f);
    else
        g.fillRoundedRectangle (juce::Rectangle<float> (track.getX(), std::min (zeroPos, sliderPos),
                                                         thick, std::abs (sliderPos - zeroPos)), 2.0f);

    // centre tick
    g.setColour (SpLook::dim());
    if (horiz) g.fillRect (zeroPos - 1.0f, track.getY() - 6.0f, 2.0f, thick + 12.0f);
    else       g.fillRect (track.getX() - 6.0f, zeroPos - 1.0f, thick + 12.0f, 2.0f);

    const float tr = 9.0f;
    const auto c = horiz ? juce::Point<float> (sliderPos, track.getCentreY()) : juce::Point<float> (track.getCentreX(), sliderPos);
    g.setColour (s.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (c.x - tr, c.y - tr, tr * 2, tr * 2);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawEllipse (c.x - tr, c.y - tr, tr * 2, tr * 2, 1.5f);
}

//==============================================================================
RadarPad::RadarPad (juce::AudioProcessorValueTreeState& s)
    : azAtt (*s.getParameter (SpatialPanProcessor::kAzimuth),   [this] (float v) { azimuth = v;   repaint(); }),
      distAtt (*s.getParameter (SpatialPanProcessor::kDistance), [this] (float v) { distance = v;  repaint(); }),
      elAtt (*s.getParameter (SpatialPanProcessor::kElevation),  [this] (float v) { elevation = v; repaint(); })
{
    azAtt.sendInitialUpdate();
    distAtt.sendInitialUpdate();
    elAtt.sendInitialUpdate();
}

float RadarPad::maxRadius() const
{
    // leave room for FRONT / L / R / BACK labels and the readout line
    return std::min ((float) getWidth() * 0.5f - 18.0f, ((float) getHeight() - 52.0f) * 0.5f);
}

juce::Point<float> RadarPad::centre() const
{
    return { (float) getWidth() * 0.5f, 18.0f + maxRadius() };
}

void RadarPad::paint (juce::Graphics& g)
{
    const auto c = centre();
    const float R = maxRadius();

    g.setColour (SpLook::panel());
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    // rear half of the room is slightly darker
    {
        juce::Path back;
        back.addPieSegment (c.x - R, c.y - R, R * 2, R * 2,
                            juce::MathConstants<float>::halfPi,
                            juce::MathConstants<float>::halfPi * 3.0f, 0.0f);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillPath (back);
    }

    // distance rings with real-world metres
    g.setFont (10.5f);
    for (float t : { 0.0f, 0.33f, 0.66f, 1.0f })
    {
        const float r = R * (kInner + (1.0f - kInner) * t);
        g.setColour (juce::Colour (0xff2a3550));
        g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);

        g.setColour (SpLook::dim().withAlpha (0.8f));
        const float lx = c.x + r * 0.7071f + 2.0f, ly = c.y + r * 0.7071f - 6.0f;
        g.drawText (juce::String (RadarPad::distanceToMetres (t * 100.0f), 1) + " m",
                    juce::Rectangle<float> (lx, ly, 46, 12).toNearestInt(), juce::Justification::left, false);
    }

    // crosshair
    g.setColour (juce::Colour (0xff2a3550));
    g.drawLine (c.x - R, c.y, c.x + R, c.y);
    g.drawLine (c.x, c.y - R, c.x, c.y + R);

    g.setColour (SpLook::dim());
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText ("FRONT", (int) c.x - 30, (int) (c.y - R - 11), 60, 12, juce::Justification::centred);
    g.drawText ("BACK",  (int) c.x - 30, (int) (c.y + R - 1),  60, 12, juce::Justification::centred);
    g.drawText ("L", (int) (c.x - R - 11), (int) c.y - 7, 12, 14, juce::Justification::centred);
    g.drawText ("R", (int) (c.x + R - 1),  (int) c.y - 7, 12, 14, juce::Justification::centred);

    // listener's head: circle, nose, ears
    const float hr = R * 0.085f;
    g.setColour (juce::Colour (0xff3a4768));
    g.fillEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2);
    juce::Path nose;
    nose.addTriangle (c.x - hr * 0.35f, c.y - hr * 0.9f, c.x + hr * 0.35f, c.y - hr * 0.9f, c.x, c.y - hr * 1.5f);
    g.fillPath (nose);
    g.fillEllipse (c.x - hr - 3.5f, c.y - 4.0f, 5.0f, 8.0f);
    g.fillEllipse (c.x + hr - 1.5f, c.y - 4.0f, 5.0f, 8.0f);

    // source position
    const float az = juce::degreesToRadians (azimuth);
    const float r  = R * (kInner + (1.0f - kInner) * (distance / 100.0f));
    const juce::Point<float> ground (c.x + r * std::sin (az), c.y - r * std::cos (az));
    const float up = elevation / 90.0f;                      // -1..1
    const juce::Point<float> dot (ground.x, ground.y - up * 22.0f);

    // line to the head, ground shadow, height stem
    g.setColour (SpLook::accent().withAlpha (0.35f));
    g.drawLine (c.x, c.y, ground.x, ground.y, 1.5f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (ground.x - 8, ground.y - 4, 16, 8);

    g.setColour (SpLook::warm().withAlpha (0.55f));
    g.drawLine (ground.x, ground.y, dot.x, dot.y, 1.5f);

    const auto col = up < 0 ? juce::Colour (0xff4c8dff).interpolatedWith (SpLook::accent(), up + 1.0f)
                            : SpLook::accent().interpolatedWith (SpLook::warm(), up);
    const float dr = 9.0f + 4.0f * up;
    g.setColour (col.withAlpha (0.28f));
    g.fillEllipse (dot.x - dr - 6, dot.y - dr - 6, (dr + 6) * 2, (dr + 6) * 2);
    g.setColour (col);
    g.fillEllipse (dot.x - dr, dot.y - dr, dr * 2, dr * 2);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawEllipse (dot.x - dr, dot.y - dr, dr * 2, dr * 2, 1.5f);

    // readout
    juce::String side = std::abs (azimuth) < 1.0f ? "front" : std::abs (azimuth) > 179.0f ? "behind"
                      : azimuth > 0 ? "right" : "left";
    g.setColour (SpLook::text());
    g.setFont (12.5f);
    const auto deg = juce::String::fromUTF8 ("\xc2\xb0");
    g.drawText ("Azimuth " + juce::String::formatted ("%+.0f", azimuth) + deg + " (" + side + ")    Distance "
                    + juce::String (distanceToMetres (distance), 1) + " m    Height "
                    + juce::String::formatted ("%+.0f", elevation) + deg,
                getLocalBounds().removeFromBottom (20).translated (0, -4), juce::Justification::centred);
}

void RadarPad::setFromMouse (juce::Point<float> pos)
{
    const auto c = centre();
    const float dx = pos.x - c.x, dy = c.y - pos.y;

    float az = juce::radiansToDegrees (std::atan2 (dx, dy));          // 0 = up/front
    const float rn = std::sqrt (dx * dx + dy * dy) / maxRadius();
    const float dist = juce::jlimit (0.0f, 1.0f, (rn - kInner) / (1.0f - kInner)) * 100.0f;

    // snap to the centre line when close to it (easy to hit straight ahead / behind)
    if (std::abs (az) < 2.0f) az = 0.0f;
    if (std::abs (std::abs (az) - 180.0f) < 2.0f) az = 180.0f;

    azAtt.setValueAsPartOfGesture (az);
    distAtt.setValueAsPartOfGesture (dist);
}

void RadarPad::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    azAtt.beginGesture();
    distAtt.beginGesture();
    setFromMouse (e.position);
}

void RadarPad::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging) setFromMouse (e.position);
}

void RadarPad::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    azAtt.endGesture();
    distAtt.endGesture();
}

void RadarPad::mouseDoubleClick (const juce::MouseEvent&)
{
    azAtt.setValueAsPartOfGesture (0.0f);
    distAtt.setValueAsPartOfGesture (30.0f);
    elAtt.setValueAsCompleteGesture (0.0f);
}

void RadarPad::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    elAtt.setValueAsCompleteGesture (juce::jlimit (-90.0f, 90.0f, elevation + w.deltaY * 40.0f));
}

//==============================================================================
SpatialPanEditor::SpatialPanEditor (SpatialPanProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      modeAtt (*p.apvts.getParameter (SpatialPanProcessor::kMode),
               [this] (float v) { setMode (v > 0.5f); }),
      pad (p.apvts)
{
    setLookAndFeel (&look);
    setSize (660, 410);

    for (auto* b : { &stereoBtn, &spatialBtn })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    stereoBtn.onClick  = [this] { modeAtt.setValueAsCompleteGesture (0.0f); };
    spatialBtn.onClick = [this] { modeAtt.setValueAsCompleteGesture (1.0f); };
    stereoBtn.setConnectedEdges  (juce::Button::ConnectedOnRight);
    spatialBtn.setConnectedEdges (juce::Button::ConnectedOnLeft);

    // --- stereo pan controls
    panSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 26);
    panSlider.setDoubleClickReturnValue (true, 0.0);
    panA = std::make_unique<SliderAtt> (proc.apvts, SpatialPanProcessor::kPan, panSlider);
    panSlider.textFromValueFunction = [] (double v)
    {
        if (std::abs (v) < 0.5) return juce::String ("CENTER");
        return (v < 0 ? "L " : "R ") + juce::String (juce::roundToInt (std::abs (v)));
    };
    panSlider.valueFromTextFunction = [] (const juce::String& t)
    {
        const auto s = t.trim().toUpperCase();
        if (s.startsWithChar ('C')) return 0.0;
        const double n = s.retainCharacters ("0123456789.").getDoubleValue();
        return s.startsWithChar ('L') || s.startsWithChar ('-') ? -n : n;
    };
    panSlider.updateText();

    lawBox.addItemList (proc.apvts.getParameter (SpatialPanProcessor::kLaw)->getAllValueStrings(), 1);
    lawA = std::make_unique<ComboAtt> (proc.apvts, SpatialPanProcessor::kLaw, lawBox);

    // --- 3D controls
    elevSlider.setSliderStyle (juce::Slider::LinearVertical);
    elevSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 20);
    elevSlider.setDoubleClickReturnValue (true, 0.0);
    elevSlider.setColour (juce::Slider::trackColourId, SpLook::warm());
    elevSlider.setColour (juce::Slider::thumbColourId, SpLook::warm());
    elevA = std::make_unique<SliderAtt> (proc.apvts, SpatialPanProcessor::kElevation, elevSlider);
    elevSlider.setTextValueSuffix (juce::String::fromUTF8 ("\xc2\xb0"));
    elevSlider.updateText();

    styleKnob (roomKnob, " %");
    styleKnob (distKnob, " %");
    styleKnob (outKnob, " dB");
    roomA = std::make_unique<SliderAtt> (proc.apvts, SpatialPanProcessor::kRoom, roomKnob);
    distA = std::make_unique<SliderAtt> (proc.apvts, SpatialPanProcessor::kDistLevel, distKnob);
    outA  = std::make_unique<SliderAtt> (proc.apvts, SpatialPanProcessor::kOutGain, outKnob);
    outKnob.setDoubleClickReturnValue (true, 0.0);

    for (auto* c : std::initializer_list<juce::Component*> { &pad, &panSlider, &lawBox, &elevSlider, &roomKnob, &distKnob, &outKnob })
        addAndMakeVisible (*c);

    // make sure every text box picks up our colours (they are created before the look is applied)
    for (auto* sl : { &panSlider, &elevSlider, &roomKnob, &distKnob, &outKnob })
    {
        sl->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        sl->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }
    panSlider.setColour (juce::Slider::trackColourId, SpLook::accent());

    modeAtt.sendInitialUpdate();
    updateVisibility();
}

SpatialPanEditor::~SpatialPanEditor()
{
    setLookAndFeel (nullptr);
}

void SpatialPanEditor::styleKnob (juce::Slider& s, const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    s.setTextValueSuffix (suffix);
    s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
}

void SpatialPanEditor::setMode (bool spatial)
{
    spatialMode = spatial;
    stereoBtn.setToggleState (! spatial, juce::dontSendNotification);
    spatialBtn.setToggleState (spatial, juce::dontSendNotification);
    // the on-colour only applies when the button is a toggle
    stereoBtn.setClickingTogglesState (false);
    updateVisibility();
    repaint();
}

void SpatialPanEditor::updateVisibility()
{
    const bool s = spatialMode;
    panSlider.setVisible (! s);
    lawBox.setVisible (! s);
    pad.setVisible (s);
    elevSlider.setVisible (s);
    roomKnob.setVisible (s);
    distKnob.setVisible (s);
    resized();
}

void SpatialPanEditor::paint (juce::Graphics& g)
{
    g.fillAll (SpLook::bg());

    g.setColour (SpLook::text());
    g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    g.drawText ("SPATIAL PAN", 20, 12, 200, 28, juce::Justification::centredLeft);

    g.setColour (SpLook::dim());
    g.setFont (11.5f);
    g.setColour (SpLook::dim());

    auto label = [&] (juce::Rectangle<int> r, const juce::String& t)
    {
        g.setColour (SpLook::dim());
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.drawText (t, r, juce::Justification::centred);
    };

    label (outLabel, "OUTPUT");

    if (spatialMode)
    {
        label (roomLabel, "ROOM");
        label (distLabel, "DIST. DROP");
        label (elevLabel, "HEIGHT");
    }
    else
    {
        g.setColour (SpLook::panel());
        g.fillRoundedRectangle (juce::Rectangle<float> (20.0f, 60.0f, (float) getWidth() - 40.0f, (float) getHeight() - 80.0f), 10.0f);

        g.setColour (SpLook::text());
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText ("TRUE STEREO PAN", 40, 76, 300, 18, juce::Justification::centredLeft);

        g.setColour (SpLook::dim());
        g.setFont (12.5f);
        g.drawFittedText ("Moves the whole stereo image instead of just turning one side down: "
                          "the channel that would be lost is folded into the other, so nothing disappears.",
                          juce::Rectangle<int> (40, 96, getWidth() - 80, 36), juce::Justification::topLeft, 2);

        // scale under the pan slider
        const auto sb = panSlider.getBounds();
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText ("L", sb.getX() - 4, sb.getY() - 16, 20, 14, juce::Justification::left);
        g.drawText ("R", sb.getRight() - 16, sb.getY() - 16, 20, 14, juce::Justification::right);

        label (panLawLabelArea, "PAN LAW (LEVEL AT HARD PAN)");
    }
}

void SpatialPanEditor::resized()
{
    auto area = getLocalBounds();

    const int bw = 130, bh = 30;
    const int bx = getWidth() - 20 - bw * 2;
    stereoBtn.setBounds (bx, 12, bw, bh);
    spatialBtn.setBounds (bx + bw, 12, bw, bh);

    if (spatialMode)
    {
        pad.setBounds (20, 56, 330, 340);

        elevLabel = { 365, 60, 70, 16 };
        elevSlider.setBounds (365, 78, 70, 312);

        const int kx = 455, kw = 100, kh = 100;
        roomLabel = { kx, 70, kw, 16 };
        roomKnob.setBounds (kx, 86, kw, kh);

        distLabel = { kx + 100, 70, kw, 16 };
        distKnob.setBounds (kx + 100, 86, kw, kh);

        outLabel = { kx, 215, kw, 16 };
        outKnob.setBounds (kx, 231, kw, kh);
    }
    else
    {
        panSlider.setBounds (50, 170, getWidth() - 100, 70);

        panLawLabelArea = { 40, 275, 260, 16 };
        lawBox.setBounds (40, 297, 160, 30);

        outLabel = { getWidth() - 160, 270, 100, 16 };
        outKnob.setBounds (getWidth() - 160, 286, 100, 100);
    }
}
