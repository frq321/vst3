#pragma once
#include "PluginProcessor.h"

// Dark theme shared by all widgets
struct SpLook : public juce::LookAndFeel_V4
{
    SpLook();
    static juce::Colour bg()      { return juce::Colour (0xff0e1320); }
    static juce::Colour panel()   { return juce::Colour (0xff171f31); }
    static juce::Colour accent()  { return juce::Colour (0xff35d0ba); }
    static juce::Colour warm()    { return juce::Colour (0xffffa94d); }
    static juce::Colour text()    { return juce::Colour (0xffe6ebf5); }
    static juce::Colour dim()     { return juce::Colour (0xff7f8aa3); }

    // Linear sliders fill from the centre (zero) outwards when the range is bipolar
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
};

//==============================================================================
// Top-down view of the listener's head. Drag the dot:
//   angle around the head = azimuth, distance from the head = distance.
// Mouse wheel = elevation. Double click = reset.
class RadarPad : public juce::Component
{
public:
    explicit RadarPad (juce::AudioProcessorValueTreeState&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static float distanceToMetres (float distPercent) { return 0.5f * std::pow (40.0f, distPercent / 100.0f); }

private:
    juce::Point<float> centre() const;
    float maxRadius() const;
    void setFromMouse (juce::Point<float>);

    juce::ParameterAttachment azAtt, distAtt, elAtt;
    float azimuth = 0.0f, distance = 30.0f, elevation = 0.0f;   // plain parameter values
    bool dragging = false;

    static constexpr float kInner = 0.14f;    // radius fraction where distance = 0 (close)
};

//==============================================================================
class SpatialPanEditor : public juce::AudioProcessorEditor
{
public:
    explicit SpatialPanEditor (SpatialPanProcessor&);
    ~SpatialPanEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void setMode (bool spatial);
    void updateVisibility();
    void styleKnob (juce::Slider&, const juce::String& suffix);

    SpatialPanProcessor& proc;
    SpLook look;

    juce::TextButton stereoBtn { "STEREO PAN" }, spatialBtn { "3D SPACE" };
    juce::ParameterAttachment modeAtt;
    bool spatialMode = false;

    RadarPad pad;
    juce::Slider panSlider, elevSlider, roomKnob, distKnob, outKnob;
    juce::ComboBox lawBox;

    std::unique_ptr<SliderAtt> panA, elevA, roomA, distA, outA;
    std::unique_ptr<ComboAtt> lawA;

    juce::Rectangle<int> panLawLabelArea, roomLabel, distLabel, outLabel, elevLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpatialPanEditor)
};
