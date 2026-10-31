#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SpatialEngine.h"

class SpatialPanProcessor : public juce::AudioProcessor
{
public:
    SpatialPanProcessor();
    ~SpatialPanProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Spatial Pan"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }   // room reverb tail

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Parameter IDs
    static constexpr const char* kMode      = "mode";
    static constexpr const char* kPan       = "pan";
    static constexpr const char* kLaw       = "panLaw";
    static constexpr const char* kAzimuth   = "azimuth";
    static constexpr const char* kDistance  = "distance";
    static constexpr const char* kElevation = "elevation";
    static constexpr const char* kRoom      = "room";
    static constexpr const char* kDistLevel = "distLevel";
    static constexpr const char* kOutGain   = "outGain";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    sp::Engine engine;
    std::atomic<float> *pMode, *pPan, *pLaw, *pAz, *pDist, *pEl, *pRoom, *pDistLevel, *pOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpatialPanProcessor)
};
