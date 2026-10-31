#pragma once
// =============================================================================
//  SpatialEngine.h
//  All DSP for the plugin lives here (no GUI, no plugin-host code), so it can be
//  unit tested offline. Two processing paths:
//
//    1. truePan()      - width-preserving stereo pan ("fold-in" panning)
//    2. Spatializer    - 3D positioning: azimuth / distance / elevation
//
//  Engine crossfades between the two when the mode changes.
// =============================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace sp
{
constexpr float kPi = 3.14159265358979323846f;

struct Params
{
    bool  spatial      = false;  // false = stereo pan, true = 3D space
    float pan          = 0.0f;   // -1 (left) .. +1 (right)
    int   law          = 1;      // 0: 0 dB, 1: -3 dB, 2: -4.5 dB, 3: -6 dB
    float azimuthDeg   = 0.0f;   // 0 = front, +90 = right, -90 = left, +-180 = behind
    float distance     = 0.3f;   // 0 (close) .. 1 (far)
    float elevationDeg = 0.0f;   // -90 (below) .. +90 (above)
    float room         = 0.3f;   // 0..1 amount of room reflections
    float distLevel    = 0.5f;   // 0..1 how much level drops with distance
    float outGainDb    = 0.0f;
};

// -----------------------------------------------------------------------------
//  1. TRUE STEREO PAN
//
//  A normal "pan" knob on a stereo track is really a balance control: it just turns
//  the far channel down, so whatever lived in that channel is thrown away.
//  Here the channel that would be turned down is instead folded into the other
//  channel, so the whole stereo image is moved and nothing disappears:
//
//      pan > 0 (to the right):   L' = L * (1 - p)      R' = R + L * p
//      pan < 0 (to the left):    L' = L + R * |p|      R' = R * (1 - |p|)
//
//  Hard right gives L' = 0, R' = L + R (everything on the right).
//  The folding makes things louder at the edges, so a pan law brings the level
//  back down (0 dB = none, -3 dB = equal power, -6 dB = equal amplitude for mono).
// -----------------------------------------------------------------------------
inline float panLawGain (int law, float a /* |pan| 0..1 */)
{
    const float g3 = 1.0f / std::sqrt (1.0f + a * a);  // constant power for a mono source
    const float g6 = 1.0f / (1.0f + a);                // constant amplitude for a mono source
    switch (law)
    {
        case 0:  return 1.0f;
        case 1:  return g3;
        case 2:  return std::sqrt (g3 * g6);           // halfway (in dB) between -3 and -6
        default: return g6;
    }
}

inline void truePan (float inL, float inR, float p, int law, float& outL, float& outR)
{
    const float g = panLawGain (law, std::abs (p));

    if (p >= 0.0f)
    {
        outL = inL * (1.0f - p) * g;
        outR = (inR + inL * p) * g;
    }
    else
    {
        outL = (inL - inR * p) * g;          // p is negative here
        outR = inR * (1.0f + p) * g;
    }
}

// -----------------------------------------------------------------------------
//  Small RBJ peaking filter (used for the elevation cue)
// -----------------------------------------------------------------------------
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    void setPeak (double sampleRate, double f0, double q, double gainDb)
    {
        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = 2.0 * juce::MathConstants<double>::pi * f0 / sampleRate;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double c     = std::cos (w0);
        const double a0    = 1.0 + alpha / A;

        b0 = (float) ((1.0 + alpha * A) / a0);
        b1 = (float) ((-2.0 * c) / a0);
        b2 = (float) ((1.0 - alpha * A) / a0);
        a1 = b1;
        a2 = (float) ((1.0 - alpha / A) / a0);
    }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// -----------------------------------------------------------------------------
//  2. 3D SPATIALIZER
//
//  Models the main cues the brain uses to place a sound:
//    - ITD  interaural time difference: far ear hears it a fraction of a ms later
//    - ILD  interaural level difference + head shadow (far ear is quieter and duller)
//    - distance: level drops, air absorbs highs, more room reflections vs direct
//    - front/back: sounds behind you are darker (pinna shadow)
//    - elevation: a gentle 7.5 kHz shelf-like cue, up = brighter, down = duller
//
//  This is a lightweight model (no HRTF measurements), best on headphones,
//  but it also gives a strong left/right + near/far impression on speakers.
//  The input is summed to mono: the 3D mode treats the track as a single point
//  source in space.
// -----------------------------------------------------------------------------
class Spatializer
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        delayBuf.assign (kDelayLen, 0.0f);
        wetL.assign ((size_t) maxBlock, 0.0f);
        wetR.assign ((size_t) maxBlock, 0.0f);

        for (auto* s : { &gL, &gR, &dL, &dR, &aL, &aR, &aC, &wet })
            s->reset (sr, 0.03);

        juce::Reverb::Parameters rp;
        rp.roomSize   = 0.72f;
        rp.damping    = 0.55f;
        rp.wetLevel   = 1.0f;
        rp.dryLevel   = 0.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.setSampleRate (sr);

        clearState();
    }

    void clearState()
    {
        std::fill (delayBuf.begin(), delayBuf.end(), 0.0f);
        writePos = 0;
        lpC = lpL = lpR = 0.0f;
        elevBq.reset();
        reverb.reset();
    }

    // Compute the parameters of the model for this block. snap = jump straight there.
    void setTargets (const Params& p, bool snap)
    {
        const float az = juce::degreesToRadians (p.azimuthDeg);
        const float el = juce::degreesToRadians (p.elevationDeg);

        const float lat   = std::sin (az) * std::cos (el);   // -1 = left ear, +1 = right ear
        const float front = std::cos (az) * std::cos (el);   // +1 = front, -1 = behind
        const float up    = std::sin (el);                   // +1 = above, -1 = below
        const float back  = std::max (0.0f, -front);         // 0..1 how far behind
        const float dist  = juce::jlimit (0.0f, 1.0f, p.distance);
        const float absLat = std::abs (lat);

        // --- ITD (Woodworth model, head radius 8.75 cm) -----------------------
        const float theta = std::asin (juce::jlimit (-1.0f, 1.0f, absLat));
        const float itdSec = (0.0875f / 343.0f) * (theta + absLat);
        const float itdSamples = itdSec * (float) sr;
        const float delayL = lat > 0.0f ? itdSamples : 0.0f;   // source on the right -> left ear is late
        const float delayR = lat < 0.0f ? itdSamples : 0.0f;

        // --- ILD: far ear quieter. Closer sources have a stronger level difference
        const float ildDb   = juce::jmap (dist, 9.0f, 4.0f);
        const float farGain = juce::Decibels::decibelsToGain (-ildDb * absLat);
        float gainL = lat > 0.0f ? farGain : 1.0f;
        float gainR = lat < 0.0f ? farGain : 1.0f;

        // keep the overall loudness steady while moving around the head
        const float norm = 1.0f / std::sqrt (0.5f * (gainL * gainL + gainR * gainR));

        // --- distance level ---------------------------------------------------
        const float distGain = juce::Decibels::decibelsToGain (-32.0f * dist * p.distLevel);
        gainL *= norm * distGain;
        gainR *= norm * distGain;

        // --- head shadow on the far ear: one-pole low-pass --------------------
        const float nyq = 0.45f * (float) sr;
        const float fcFar = std::min (nyq, 20000.0f * std::pow (0.14f, absLat));
        const float fcNear = nyq;
        const float fcL = lat > 0.0f ? fcFar : fcNear;
        const float fcR = lat < 0.0f ? fcFar : fcNear;

        // --- common darkening: air absorption (distance) and pinna shadow (behind)
        const float fcCommon = std::max (1200.0f,
                                         std::min (nyq, 20000.0f * std::pow (0.30f, dist) * std::pow (0.35f, back)));

        // --- elevation cue ----------------------------------------------------
        const float elevDb = 6.0f * up;
        if (std::abs (elevDb - lastElevDb) > 0.01f || snap)
        {
            elevBq.setPeak (sr, std::min (7500.0, 0.45 * sr), 0.9, elevDb);
            lastElevDb = elevDb;
        }

        // --- reverb send: more room, relatively, as the source gets farther ---
        const float wetGain = p.room * (0.15f + 0.85f * dist) * kWetScale;

        auto set = [&] (juce::SmoothedValue<float>& s, float v)
        {
            if (snap) s.setCurrentAndTargetValue (v); else s.setTargetValue (v);
        };

        set (gL, gainL);   set (gR, gainR);
        set (dL, delayL);  set (dR, delayR);
        set (aL, onePoleCoef (fcL));
        set (aR, onePoleCoef (fcR));
        set (aC, onePoleCoef (fcCommon));
        set (wet, wetGain);

        if (snap) clearState();
    }

    // in: stereo (summed to mono), out: stereo. in and out may NOT alias.
    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (inL[i] + inR[i]);

            // shared pre-filters (air absorption, behind-the-head darkening, elevation)
            lpC += aC.getNextValue() * (mono - lpC);
            const float x = elevBq.process (lpC);

            delayBuf[(size_t) writePos] = x;

            const float yl = readDelay (dL.getNextValue());
            const float yr = readDelay (dR.getNextValue());

            lpL += aL.getNextValue() * (yl - lpL);
            lpR += aR.getNextValue() * (yr - lpR);

            outL[i] = lpL * gL.getNextValue();
            outR[i] = lpR * gR.getNextValue();

            wetL[(size_t) i] = wetR[(size_t) i] = mono * wet.getNextValue();

            writePos = (writePos + 1) & kMask;
        }

        reverb.processStereo (wetL.data(), wetR.data(), n);

        for (int i = 0; i < n; ++i)
        {
            outL[i] += wetL[(size_t) i];
            outR[i] += wetR[(size_t) i];
        }
    }

    static constexpr float kWetScale = 0.12f;

private:
    static constexpr int kDelayLen = 4096;      // plenty for 0.66 ms even at 384 kHz
    static constexpr int kMask = kDelayLen - 1;

    float onePoleCoef (float fc) const
    {
        return 1.0f - std::exp (-2.0f * kPi * fc / (float) sr);
    }

    float readDelay (float d) const
    {
        const float pos = (float) writePos - d;
        const int   i0  = (int) std::floor (pos);
        const float fr  = pos - (float) i0;
        return delayBuf[(size_t) (i0 & kMask)] * (1.0f - fr)
             + delayBuf[(size_t) ((i0 + 1) & kMask)] * fr;
    }

    double sr = 44100.0;
    std::vector<float> delayBuf, wetL, wetR;
    int writePos = 0;
    float lpC = 0, lpL = 0, lpR = 0;
    float lastElevDb = 1000.0f;
    Biquad elevBq;
    juce::Reverb reverb;
    juce::SmoothedValue<float> gL, gR, dL, dR, aL, aR, aC, wet;
};

// -----------------------------------------------------------------------------
//  Engine: owns both paths, smoothing, mode crossfade and output gain.
// -----------------------------------------------------------------------------
class Engine
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        maxBlockSize = std::max (1, maxBlock);
        spat.prepare (sampleRate, maxBlockSize);

        for (auto* v : { &bufPanL, &bufPanR, &bufSpatL, &bufSpatR })
            v->assign ((size_t) maxBlockSize, 0.0f);

        panSm.reset (sampleRate, 0.02);
        modeMix.reset (sampleRate, 0.06);
        gainSm.reset (sampleRate, 0.02);
        needsInit = true;
    }

    void process (float* L, float* R, int numSamples, const Params& p)
    {
        for (int offset = 0; offset < numSamples; offset += maxBlockSize)
            processChunk (L + offset, R + offset, std::min (maxBlockSize, numSamples - offset), p);
    }

private:
    void processChunk (float* L, float* R, int n, const Params& p)
    {
        if (needsInit)
        {
            panSm.setCurrentAndTargetValue (p.pan);
            gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.outGainDb));
            modeMix.setCurrentAndTargetValue (p.spatial ? 1.0f : 0.0f);
            lastSpatial = p.spatial;
            snapSpatial = p.spatial;
            needsInit = false;
        }

        if (p.spatial != lastSpatial)
        {
            modeMix.setTargetValue (p.spatial ? 1.0f : 0.0f);
            if (p.spatial) snapSpatial = true;       // start the 3D path from a clean state
            lastSpatial = p.spatial;
        }

        panSm.setTargetValue (p.pan);
        gainSm.setTargetValue (juce::Decibels::decibelsToGain (p.outGainDb));

        const bool mixing    = modeMix.isSmoothing();
        const bool wantSpat  = modeMix.getTargetValue() > 0.5f;
        const bool runPan    = mixing || ! wantSpat;
        const bool runSpat   = mixing || wantSpat;

        if (runSpat)
        {
            spat.setTargets (p, snapSpatial);
            snapSpatial = false;
            spat.process (L, R, bufSpatL.data(), bufSpatR.data(), n);
        }

        if (runPan)
        {
            for (int i = 0; i < n; ++i)
                truePan (L[i], R[i], panSm.getNextValue(), p.law, bufPanL[(size_t) i], bufPanR[(size_t) i]);
        }
        else
        {
            panSm.skip (n);
        }

        for (int i = 0; i < n; ++i)
        {
            const float m = modeMix.getNextValue();
            const float g = gainSm.getNextValue();

            float l, r;
            if (runPan && runSpat)
            {
                l = bufPanL[(size_t) i] * (1.0f - m) + bufSpatL[(size_t) i] * m;
                r = bufPanR[(size_t) i] * (1.0f - m) + bufSpatR[(size_t) i] * m;
            }
            else if (runPan)
            {
                l = bufPanL[(size_t) i];
                r = bufPanR[(size_t) i];
            }
            else
            {
                l = bufSpatL[(size_t) i];
                r = bufSpatR[(size_t) i];
            }

            L[i] = l * g;
            R[i] = r * g;
        }
    }

    int  maxBlockSize = 512;
    bool needsInit = true, lastSpatial = false, snapSpatial = false;
    Spatializer spat;
    juce::SmoothedValue<float> panSm, modeMix, gainSm;
    std::vector<float> bufPanL, bufPanR, bufSpatL, bufSpatR;
};

} // namespace sp
