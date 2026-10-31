// Offline checks of the DSP in SpatialEngine.h. Run: ./SpatialPanTests
#include <juce_core/juce_core.h>
#include "SpatialEngine.h"
#include <cstdio>
#include <random>

static int failures = 0;
#define CHECK(cond, ...)                                                       \
    do { const bool ok_ = (cond);                                              \
         std::printf("  [%s] ", ok_ ? "PASS" : "FAIL"); std::printf(__VA_ARGS__); std::printf("\n"); \
         if (! ok_) ++failures; } while (0)

static double rms (const std::vector<float>& v, size_t from = 0)
{
    double s = 0; size_t n = 0;
    for (size_t i = from; i < v.size(); ++i) { s += (double) v[i] * v[i]; ++n; }
    return std::sqrt (s / (double) std::max<size_t> (1, n));
}

// Power at a set of frequencies (Goertzel), as a fraction of total power.
// Used to measure "how much of the signal lives in this band".
static double bandFraction (const std::vector<float>& v, double sr, double f0, double f1, size_t from = 3000)
{
    const size_t n = v.size() - from;
    double total = 0;
    for (size_t i = from; i < v.size(); ++i) total += (double) v[i] * v[i];

    double band = 0; int bins = 0;
    for (double f = f0; f <= f1; f += 250.0, ++bins)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, c = 2.0 * std::cos (w);
        double s1 = 0, s2 = 0;
        for (size_t i = from; i < v.size(); ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; }
        band += (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) n;   // ~ n/2 * amplitude^2 per bin
    }
    return (band / bins) / std::max (total / (double) n * (double) n, 1e-12);
}
// spectral tilt: high band (5-15 kHz) relative to a low band (0.3-2 kHz)
static double highBand (const std::vector<float>& v)
{
    return bandFraction (v, 48000.0, 5000.0, 15000.0) / bandFraction (v, 48000.0, 300.0, 2000.0);
}

static std::vector<float> noise (size_t n, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-0.5f, 0.5f);
    std::vector<float> v (n);
    for (auto& x : v) x = d (rng);
    return v;
}

struct Result { std::vector<float> L, R; };

// run noise (same in both channels = mono source) through the engine
static Result run (const sp::Params& p, double sr = 48000.0, size_t n = 48000, bool stereoNoise = false)
{
    sp::Engine e;
    e.prepare (sr, 512);
    auto inL = noise (n, 1);
    auto inR = stereoNoise ? noise (n, 2) : inL;
    for (size_t off = 0; off < n; off += 512)
    {
        const int len = (int) std::min<size_t> (512, n - off);
        e.process (inL.data() + off, inR.data() + off, len, p);
    }
    return { inL, inR };
}

// lag (in samples) at which R best matches L; positive = R is later than L
static int bestLag (const std::vector<float>& a, const std::vector<float>& b, int maxLag = 60)
{
    int best = 0; double bestV = -1e30;
    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        double s = 0;
        for (size_t i = 2000; i < a.size() - 2000; ++i)
            s += (double) a[i] * b[(size_t) ((int) i + lag)];
        if (s > bestV) { bestV = s; best = lag; }
    }
    return best;
}

int main()
{
    std::printf ("== True stereo pan ==\n");
    {
        float l, r;
        sp::truePan (0.7f, -0.3f, 0.0f, 1, l, r);
        CHECK (l == 0.7f && r == -0.3f, "centre leaves the signal untouched");

        sp::truePan (1.0f, 0.0f, 1.0f, 0, l, r);
        CHECK (l == 0.0f && r == 1.0f, "left-only signal panned hard right ends fully on the right (nothing lost)");

        sp::truePan (0.6f, 0.4f, 1.0f, 0, l, r);
        CHECK (l == 0.0f && std::abs (r - 1.0f) < 1e-6f, "hard right with 0 dB law: R = L + R (%.2f)", r);

        sp::truePan (0.6f, 0.4f, -1.0f, 0, l, r);
        CHECK (r == 0.0f && std::abs (l - 1.0f) < 1e-6f, "hard left folds R into L (%.2f)", l);

        // mono source: total power at any position, per pan law
        for (int law = 1; law <= 3; ++law)
        {
            double worst = 0, atEdge = 0;
            for (float p = 0; p <= 1.0001f; p += 0.05f)
            {
                sp::truePan (1.0f, 1.0f, p, law, l, r);
                const double db = 10.0 * std::log10 ((l * l + r * r) / 2.0);
                worst = std::max (worst, std::abs (db));
                if (p > 0.99f) atEdge = db;
            }
            const char* names[] = { "0", "-3", "-4.5", "-6" };
            std::printf ("      law %s dB: mono power at hard pan %+.2f dB, max deviation across sweep %.2f dB\n",
                         names[law], atEdge, worst);
            if (law == 1)
                CHECK (worst < 1e-3, "-3 dB law keeps a mono source at constant power over the whole sweep");
        }

        // compare with plain balance: a wide stereo signal loses a channel entirely
        sp::Params p; p.pan = 1.0f; p.law = 0;
        auto out = run (p, 48000.0, 24000, true);
        CHECK (rms (out.L, 3000) < 1e-6, "hard right: left output silent");
        auto in2 = noise (24000, 1), in3 = noise (24000, 2);
        double expectR = 0;
        for (size_t i = 3000; i < 24000; ++i) expectR += std::pow (in2[i] + in3[i], 2.0);
        expectR = std::sqrt (expectR / 21000.0);
        CHECK (std::abs (rms (out.R, 3000) / expectR - 1.0) < 0.02, "hard right: right output = L+R of the input");
    }

    std::printf ("== 3D spatializer ==\n");
    {
        sp::Params base; base.spatial = true; base.room = 0.0f; base.distance = 0.0f; base.distLevel = 0.0f;

        auto p = base; p.azimuthDeg = 90.0f;
        auto o = run (p);
        const int lagRight = bestLag (o.R, o.L);   // L later than R when source on the right
        std::printf ("      source at +90: ITD = %d samples (%.3f ms), L/R level = %.1f dB\n",
                     lagRight, 1000.0 * lagRight / 48000.0, 20 * std::log10 (rms (o.L, 3000) / rms (o.R, 3000)));
        CHECK (lagRight >= 28 && lagRight <= 34, "source right: left ear delayed ~0.65 ms (got %d samples)", lagRight);
        CHECK (rms (o.R, 3000) > 1.5 * rms (o.L, 3000), "source right: right ear clearly louder");
        CHECK (highBand (o.L) < 0.7 * highBand (o.R), "source right: far (left) ear is duller (head shadow)");

        p.azimuthDeg = -90.0f;
        o = run (p);
        const int lagLeft = bestLag (o.L, o.R);
        CHECK (lagLeft >= 28 && lagLeft <= 34, "source left: mirror image (%d samples)", lagLeft);
        CHECK (rms (o.L, 3000) > 1.5 * rms (o.R, 3000), "source left: left ear louder");

        p.azimuthDeg = 0.0f;
        auto front = run (p);
        CHECK (std::abs (rms (front.L, 3000) / rms (front.R, 3000) - 1.0) < 0.01, "front: centred");

        p.azimuthDeg = 180.0f;
        auto behind = run (p);
        CHECK (std::abs (rms (behind.L, 3000) / rms (behind.R, 3000) - 1.0) < 0.01, "behind: centred");
        CHECK (highBand (behind.L) < 0.6 * highBand (front.L), "behind is darker than front (HF share %.2e vs %.2e)",
               highBand (behind.L), highBand (front.L));

        // loudness steady around the head
        p.azimuthDeg = 45.0f;
        auto side = run (p);
        const double pw = std::sqrt (std::pow (rms (side.L, 3000), 2) + std::pow (rms (side.R, 3000), 2));
        const double pf = std::sqrt (std::pow (rms (front.L, 3000), 2) + std::pow (rms (front.R, 3000), 2));
        CHECK (std::abs (20 * std::log10 (pw / pf)) < 1.5, "overall loudness stays within 1.5 dB moving around the head (%.2f dB)",
               20 * std::log10 (pw / pf));

        // distance
        auto near = base; near.distance = 0.0f; near.distLevel = 1.0f;
        auto far = base;  far.distance = 1.0f;  far.distLevel = 1.0f;
        auto on = run (near), of = run (far);
        const double dropDb = 20 * std::log10 (rms (of.L, 3000) / rms (on.L, 3000));
        std::printf ("      far vs close, direct sound only: %.1f dB, HF share %.2e -> %.2e\n",
                     dropDb, highBand (on.L), highBand (of.L));
        CHECK (dropDb < -25.0, "far is much quieter than close");
        CHECK (highBand (of.L) < 0.5 * highBand (on.L), "far is duller than close (air absorption)");

        // room: reverb adds energy and spreads it in time
        auto dry = base; dry.distance = 1.0f; dry.distLevel = 1.0f; dry.room = 0.0f;
        auto wetP = dry;  wetP.room = 1.0f;
        auto od = run (dry), ow = run (wetP);
        const double extra = 20 * std::log10 (rms (ow.L, 3000) / rms (od.L, 3000));
        std::printf ("      room 100%% at far distance adds %.1f dB over the direct sound; total level vs input %.1f dB\n",
                     extra, 20 * std::log10 (rms (ow.L, 3000) / rms (noise (48000, 1), 3000)));
        CHECK (extra > 3.0, "room reverb is clearly audible when far");

        // elevation
        auto lo = base; lo.elevationDeg = -60.0f;
        auto hi = base; hi.elevationDeg = 60.0f;
        auto ol = run (lo), oh = run (hi);
        const double eh = bandFraction (oh.L, 48000.0, 6000.0, 9000.0), el = bandFraction (ol.L, 48000.0, 6000.0, 9000.0);
        CHECK (eh > 1.5 * el, "higher sources are brighter around 7.5 kHz than lower ones (%.2f dB apart)", 10 * std::log10 (eh / el));
    }

    std::printf ("== Robustness ==\n");
    {
        sp::Engine e;
        e.prepare (44100.0, 256);
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        bool bad = false;
        float peak = 0;
        std::vector<float> L (700), R (700);                  // odd block size, bigger than prepared max
        for (int block = 0; block < 400; ++block)
        {
            sp::Params p;
            p.spatial = (block / 20) % 2 == 1;               // switch modes often
            p.pan = u (rng) * 2 - 1;
            p.law = (int) (u (rng) * 3.99f);
            p.azimuthDeg = u (rng) * 360 - 180;
            p.distance = u (rng);
            p.elevationDeg = u (rng) * 180 - 90;
            p.room = u (rng);
            p.distLevel = u (rng);
            p.outGainDb = u (rng) * 36 - 24;
            for (size_t i = 0; i < L.size(); ++i) { L[i] = u (rng) * 2 - 1; R[i] = u (rng) * 2 - 1; }
            e.process (L.data(), R.data(), (int) L.size(), p);
            for (size_t i = 0; i < L.size(); ++i)
            {
                if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) bad = true;
                peak = std::max ({ peak, std::abs (L[i]), std::abs (R[i]) });
            }
        }
        CHECK (! bad, "no NaN/inf with random automation and constant mode switching (peak %.1f)", peak);
        CHECK (peak < 50.0f, "output stays bounded");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
