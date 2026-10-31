// Renders the editor in each mode to PNG files (for visual checks without a DAW).
// Usage: SpatialPanSnapshot <output-dir>
#include "PluginProcessor.h"
#include "PluginEditor.h"

static void setParam (SpatialPanProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

static void snap (SpatialPanProcessor& p, const juce::File& file)
{
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setVisible (true);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::printf ("wrote %s (%dx%d)\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

// Exercise the actual plugin object like a host would: process audio, save/restore state.
static bool hostSmokeTest()
{
    SpatialPanProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 480);
    proc.prepareToPlay (48000.0, 480);

    bool ok = true;
    juce::AudioBuffer<float> buf (2, 480);
    juce::MidiBuffer midi;
    juce::Random rnd (3);

    for (int mode = 0; mode < 2; ++mode)
    {
        setParam (proc, "mode", (float) mode);
        setParam (proc, "pan", 80.0f);
        setParam (proc, "azimuth", 90.0f);
        for (int block = 0; block < 50; ++block)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 480; ++i) buf.setSample (c, i, rnd.nextFloat() - 0.5f);
            proc.processBlock (buf, midi);
        }
        const float rl = buf.getRMSLevel (0, 0, 480), rr = buf.getRMSLevel (1, 0, 480);
        std::printf ("host test, mode %d: rms L %.3f  R %.3f\n", mode, rl, rr);
        ok = ok && std::isfinite (rl) && std::isfinite (rr) && rr > rl;     // pan 80 / azimuth 90 -> right louder
    }

    juce::MemoryBlock state;
    proc.getStateInformation (state);
    setParam (proc, "pan", -50.0f);
    setParam (proc, "azimuth", -120.0f);
    proc.setStateInformation (state.getData(), (int) state.getSize());
    const float pan = proc.apvts.getRawParameterValue ("pan")->load();
    const float az  = proc.apvts.getRawParameterValue ("azimuth")->load();
    std::printf ("state restore: pan %.1f (expect 80), azimuth %.1f (expect 90)\n", pan, az);
    ok = ok && std::abs (pan - 80.0f) < 0.01f && std::abs (az - 90.0f) < 0.01f;

    std::printf ("host smoke test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (argc > 1 ? argv[1] : ".");
    dir.createDirectory();

    if (! hostSmokeTest())
        return 1;

    SpatialPanProcessor proc;

    setParam (proc, "mode", 0);
    setParam (proc, "pan", 35.0f);
    snap (proc, dir.getChildFile ("ui_stereo.png"));

    setParam (proc, "mode", 1);
    setParam (proc, "azimuth", 50.0f);
    setParam (proc, "distance", 65.0f);
    setParam (proc, "elevation", 35.0f);
    snap (proc, dir.getChildFile ("ui_3d.png"));

    setParam (proc, "azimuth", -150.0f);
    setParam (proc, "distance", 15.0f);
    setParam (proc, "elevation", -40.0f);
    snap (proc, dir.getChildFile ("ui_3d_behind.png"));
    return 0;
}
