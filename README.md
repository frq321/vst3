# Spatial Pan

A VST3 effect with two modes:

**Stereo Pan** - a *real* stereo pan. The usual pan knob on a stereo track only turns one channel down (a balance
control), so the content of that channel is lost. Here the channel that would be turned down is folded into the
other one, so the whole stereo image moves and nothing disappears:

```
pan to the right:  L' = L * (1 - p)     R' = R + L * p
pan to the left:   L' = L + R * |p|     R' = R * (1 - |p|)
```

Hard right = everything on the right (L + R). A pan law (0 / -3 / -4.5 / -6 dB) keeps the level from jumping up as the
signal is folded together. -3 dB gives exactly constant power for a mono source over the whole sweep.

**3D Space** - place the sound anywhere around your head:

| Control        | What it does |
|----------------|--------------|
| Radar pad      | Drag the dot. Angle = azimuth (front, right, behind, left), distance from the head = near/far. Mouse wheel = height. Double click resets. |
| Height         | Below / above the head (7.5 kHz brightness cue, plus it reduces left/right cues straight above or below). |
| Room           | Amount of room reflections. Reflections stay constant while the direct sound fades with distance, which is what makes things sound *far*. |
| Dist. drop     | How much the level falls as the source moves away (0 % = distance only changes tone and room, 100 % = -32 dB at the far edge). |

How it works (all in `Source/SpatialEngine.h`):
interaural time difference up to 0.66 ms (Woodworth model), interaural level difference, head-shadow low-pass on the
far ear, air-absorption low-pass with distance, darker sound behind the head, and a reverb send that grows with
distance. The input is summed to mono, so in 3D mode the track behaves like one point source. It works best on
headphones; on speakers you still get a strong left/right and near/far impression. It is a model, not measured HRTFs,
so front/back and height are subtle cues compared with a full binaural renderer.

## Get the plugin

### Easiest (no compiler needed): let GitHub build it
1. Create a GitHub repository and upload this folder.
2. Open the **Actions** tab, run **Build plugin**, then download the `SpatialPan-VST3-Windows` artifact.

### Windows
1. Install Visual Studio 2022 (Community, "Desktop development with C++") and CMake.
2. In a terminal in this folder:
   ```
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release --target SpatialPan_VST3
   ```
   (the first run downloads JUCE, needs internet and a few minutes)
3. Copy `build\SpatialPan_artefacts\Release\VST3\Spatial Pan.vst3` to `C:\Program Files\Common Files\VST3`.

### FL Studio
Options -> Manage plugins -> *Find installed plugins* (or *Verify plugins*). Add it from the mixer's effect slot menu.
FL Studio will ask which instance to use if you also have other formats installed; pick the VST3.

### macOS / Linux
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target SpatialPan_VST3
```
macOS needs Xcode command line tools; Linux needs `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev
libxcursor-dev libxext-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev`.

## Tests

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSPATIALPAN_BUILD_TESTS=ON
cmake --build build --target SpatialPanTests
./build/SpatialPanTests_artefacts/Release/SpatialPanTests
```
Checks the pan maths (nothing lost, constant power), interaural delay/level, head shadow, front/back, distance, room,
elevation, and that automation + constant mode switching never produces NaN or runaway levels.

## Layout
```
Source/SpatialEngine.h   all DSP (pan + 3D)
Source/PluginProcessor.* parameters, host glue
Source/PluginEditor.*    UI with the radar pad
Tests/                   offline DSP tests and a headless UI snapshot tool
```

Automatable parameters: Mode, Pan, Pan Law, Azimuth, Distance, Elevation, Room, Distance Level Drop, Output Gain.
Azimuth wraps smoothly through "behind" (the left/right cues are continuous at +-180 degrees), so you can automate a
full circle around the head.
