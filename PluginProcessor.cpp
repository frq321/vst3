#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout SpatialPanProcessor::createLayout()
{
    using namespace juce;
    using PID = ParameterID;
    using Attr = AudioParameterFloatAttributes;

    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterChoice> (PID { kMode, 1 }, "Mode",
                                                          StringArray { "Stereo Pan", "3D Space" }, 0));

    p.push_back (std::make_unique<AudioParameterFloat> (PID { kPan, 1 }, "Pan",
                                                         NormalisableRange<float> (-100.0f, 100.0f, 0.01f), 0.0f));

    p.push_back (std::make_unique<AudioParameterChoice> (PID { kLaw, 1 }, "Pan Law",
                                                          StringArray { "0 dB", "-3 dB", "-4.5 dB", "-6 dB" }, 1));

    p.push_back (std::make_unique<AudioParameterFloat> (PID { kAzimuth, 1 }, "Azimuth",
                                                         NormalisableRange<float> (-180.0f, 180.0f, 0.1f), 0.0f,
                                                         Attr().withLabel (juce::String::fromUTF8 ("\xc2\xb0"))));

    p.push_back (std::make_unique<AudioParameterFloat> (PID { kDistance, 1 }, "Distance",
                                                         NormalisableRange<float> (0.0f, 100.0f, 0.01f), 30.0f,
                                                         Attr().withLabel ("%")));

    p.push_back (std::make_unique<AudioParameterFloat> (PID { kElevation, 1 }, "Elevation",
                                                         NormalisableRange<float> (-90.0f, 90.0f, 0.1f), 0.0f,
                                                         Attr().withLabel (juce::String::fromUTF8 ("\xc2\xb0"))));

    p.push_back (std::make_unique<AudioParameterFloat> (PID { kRoom, 1 }, "Room",
                                                         NormalisableRange<float> (0.0f, 100.0f, 0.01f), 35.0f,
                                                         Attr().withLabel ("%")));

    p.push_back (std::make_unique<AudioParameterFloat> (PID { kDistLevel, 1 }, "Distance Level Drop",
                                                         NormalisableRange<float> (0.0f, 100.0f, 0.01f), 50.0f,
                                                         Attr().withLabel ("%")));

    p.push_back (std::make_unique<AudioParameterFloat> (PID { kOutGain, 1 }, "Output Gain",
                                                         NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
                                                         Attr().withLabel ("dB")));

    return { p.begin(), p.end() };
}

SpatialPanProcessor::SpatialPanProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pMode      = apvts.getRawParameterValue (kMode);
    pPan       = apvts.getRawParameterValue (kPan);
    pLaw       = apvts.getRawParameterValue (kLaw);
    pAz        = apvts.getRawParameterValue (kAzimuth);
    pDist      = apvts.getRawParameterValue (kDistance);
    pEl        = apvts.getRawParameterValue (kElevation);
    pRoom      = apvts.getRawParameterValue (kRoom);
    pDistLevel = apvts.getRawParameterValue (kDistLevel);
    pOut       = apvts.getRawParameterValue (kOutGain);
}

bool SpatialPanProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SpatialPanProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, std::max (samplesPerBlock, 512));
}

void SpatialPanProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    sp::Params p;
    p.spatial      = pMode->load() > 0.5f;
    p.pan          = pPan->load() / 100.0f;
    p.law          = (int) std::lround (pLaw->load());
    p.azimuthDeg   = pAz->load();
    p.distance     = pDist->load() / 100.0f;
    p.elevationDeg = pEl->load();
    p.room         = pRoom->load() / 100.0f;
    p.distLevel    = pDistLevel->load() / 100.0f;
    p.outGainDb    = pOut->load();

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples(), p);
}

juce::AudioProcessorEditor* SpatialPanProcessor::createEditor()
{
    return new SpatialPanEditor (*this);
}

void SpatialPanProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SpatialPanProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpatialPanProcessor();
}
