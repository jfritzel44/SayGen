#include "../Source/Effects/FDNReverb.h"
#include "../Source/Effects/TapeEcho.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

static void require (bool condition, const char* message)
{
    if (! condition) { std::cerr << "FAIL: " << message << '\n'; std::exit (1); }
}

static bool finite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x)) return false;
    return true;
}

static float peak (const std::vector<float>& v)
{
    float m = 0.0f;
    for (float x : v) m = std::max (m, std::abs (x));
    return m;
}

static float rms (const float* data, int count)
{
    double total = 0.0;
    for (int i = 0; i < count; ++i) total += (double) data[i] * data[i];
    return (float) std::sqrt (total / std::max (1, count));
}

// A deterministic, reproducible noise source; no dependence on the platform RNG.
struct Noise
{
    uint32_t state = 22222u;
    float next()
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) state / 2147483647.5f - 1.0f;
    }
};

// Impulse response of the reverb's wet path, one channel pair.
static void impulseResponse (syngen::FDNReverb& reverb, int samples,
                             std::vector<float>& left, std::vector<float>& right)
{
    left.assign ((size_t) samples, 0.0f);
    right.assign ((size_t) samples, 0.0f);
    left[0] = 1.0f;
    right[0] = 1.0f;
    reverb.process (left.data(), right.data(), samples);
}

static void testReverb (double rate)
{
    const int seconds = (int) rate;

    // Dry path is untouched at mix 0, once the 50 ms crossfade has settled.
    {
        syngen::FDNReverb reverb;
        reverb.prepare (rate);
        reverb.setParameters (0.7f, 0.5f, 1.0f, 0.0f);
        Noise noise;
        std::vector<float> l ((size_t) seconds), r ((size_t) seconds), copy;
        for (int i = 0; i < seconds; ++i) { l[(size_t) i] = noise.next() * 0.5f; r[(size_t) i] = l[(size_t) i]; }
        copy = l;
        reverb.process (l.data(), r.data(), seconds);
        for (int i = 0; i < seconds; ++i)
            require (std::abs (l[(size_t) i] - copy[(size_t) i]) < 1.0e-6f, "reverb mix 0 is transparent");
    }

    // Decay slope. Size 0.5 asks for RT60 = 0.25 + 5 * 0.25 = 1.5 s; measure
    // the tail's own dB-per-second between 0.4 s and 1.1 s and extrapolate.
    {
        syngen::FDNReverb reverb;
        reverb.prepare (rate);
        reverb.setParameters (0.5f, 0.0f, 1.0f, 1.0f);
        std::vector<float> l, r;
        impulseResponse (reverb, seconds * 2, l, r);
        require (finite (l) && finite (r), "reverb impulse response is finite");
        const int early = (int) (0.4 * rate), late = (int) (1.1 * rate);
        const int window = (int) (0.1 * rate);
        const float earlyRms = rms (l.data() + early, window);
        const float lateRms = rms (l.data() + late, window);
        require (earlyRms > 0.0f && lateRms > 0.0f, "reverb tail is sounding");
        const double decayDb = 20.0 * std::log10 (earlyRms / lateRms);
        const double rt60 = 60.0 * 0.7 / decayDb;
        require (rt60 > 0.9 && rt60 < 2.4, "reverb decay is close to the requested RT60");
    }

    // No flutter: the early response must be continuously dense rather than a
    // handful of discrete echoes with gaps between them. Freeverb's sparse
    // comb bank is exactly what this rules out.
    {
        syngen::FDNReverb reverb;
        reverb.prepare (rate);
        reverb.setParameters (0.6f, 0.3f, 1.0f, 1.0f);
        std::vector<float> l, r;
        impulseResponse (reverb, seconds, l, r);
        const int window = (int) (0.005 * rate);
        const int start = (int) (0.05 * rate);
        float loudest = 0.0f, quietest = 1.0e9f;
        for (int at = start; at + window < (int) (0.4 * rate); at += window)
        {
            const float level = rms (l.data() + at, window);
            loudest = std::max (loudest, level);
            quietest = std::min (quietest, level);
        }
        require (quietest > 0.0f, "reverb tail has no silent gaps");
        require (loudest / quietest < 12.0f, "reverb tail is dense, not fluttering");
    }

    // Damping darkens the tail rather than simply shortening it.
    {
        double bright = 0.0, dark = 0.0;
        for (int pass = 0; pass < 2; ++pass)
        {
            syngen::FDNReverb reverb;
            reverb.prepare (rate);
            reverb.setParameters (0.6f, pass == 0 ? 0.0f : 1.0f, 1.0f, 1.0f);
            std::vector<float> l, r;
            impulseResponse (reverb, seconds, l, r);
            // Ratio of first-difference energy to signal energy: a broadband
            // high-frequency measure that needs no transform.
            const int start = (int) (0.2 * rate), count = (int) (0.2 * rate);
            double difference = 0.0, total = 0.0;
            for (int i = start; i < start + count; ++i)
            {
                const double d = l[(size_t) i] - l[(size_t) (i - 1)];
                difference += d * d;
                total += (double) l[(size_t) i] * l[(size_t) i];
            }
            (pass == 0 ? bright : dark) = difference / std::max (1.0e-20, total);
        }
        require (dark < bright * 0.5, "reverb damping removes high frequencies");
    }

    // Width 0 collapses the wet signal to mono for a mono-identical input.
    {
        syngen::FDNReverb reverb;
        reverb.prepare (rate);
        reverb.setParameters (0.6f, 0.4f, 0.0f, 1.0f);
        std::vector<float> l, r;
        impulseResponse (reverb, seconds / 2, l, r);
        for (size_t i = 0; i < l.size(); ++i)
            require (std::abs (l[i] - r[i]) < 1.0e-6f, "reverb width 0 is mono");
    }

    // Width 1 decorrelates the channels.
    {
        syngen::FDNReverb reverb;
        reverb.prepare (rate);
        reverb.setParameters (0.6f, 0.4f, 1.0f, 1.0f);
        std::vector<float> l, r;
        impulseResponse (reverb, seconds, l, r);
        const int start = (int) (0.1 * rate), count = (int) (0.5 * rate);
        double cross = 0.0, energyL = 0.0, energyR = 0.0;
        for (int i = start; i < start + count; ++i)
        {
            cross += (double) l[(size_t) i] * r[(size_t) i];
            energyL += (double) l[(size_t) i] * l[(size_t) i];
            energyR += (double) r[(size_t) i] * r[(size_t) i];
        }
        const double correlation = cross / std::sqrt (std::max (1.0e-20, energyL * energyR));
        require (std::abs (correlation) < 0.5, "reverb width 1 decorrelates the channels");
    }

    // Longest decay, full-scale noise, ten seconds: bounded and finite. The
    // Hadamard mixing is orthonormal, so this is a check that nothing in the
    // damping or injection path breaks that.
    {
        syngen::FDNReverb reverb;
        reverb.prepare (rate);
        reverb.setParameters (1.0f, 0.0f, 1.0f, 1.0f);
        Noise noise;
        std::vector<float> l ((size_t) seconds), r ((size_t) seconds);
        float loudest = 0.0f;
        for (int block = 0; block < 10; ++block)
        {
            for (int i = 0; i < seconds; ++i) { l[(size_t) i] = noise.next(); r[(size_t) i] = noise.next(); }
            reverb.process (l.data(), r.data(), seconds);
            require (finite (l) && finite (r), "reverb stays finite under sustained drive");
            loudest = std::max (loudest, std::max (peak (l), peak (r)));
        }
        require (loudest < 8.0f, "reverb output stays bounded at maximum decay");
    }

    // Mono bus: a null right pointer must not be dereferenced.
    {
        syngen::FDNReverb reverb;
        reverb.prepare (rate);
        reverb.setParameters (0.6f, 0.4f, 1.0f, 1.0f);
        std::vector<float> l ((size_t) seconds, 0.0f);
        l[0] = 1.0f;
        reverb.process (l.data(), nullptr, seconds);
        require (finite (l) && peak (l) > 0.0f, "reverb renders a mono bus");
    }
}

static void testEcho (double rate)
{
    const int seconds = (int) rate;
    const int channels = 2;

    // Undamped repeats survive. The delay this replaced used the damping
    // amount directly as its one-pole coefficient, so damp 0 pinned the
    // feedback filter at zero and every patch with damping off got exactly one
    // repeat instead of a full decaying train.
    {
        syngen::TapeEcho echo;
        echo.prepare (rate, channels);
        echo.setParameters (0.1f, 0.6f, 0.0f, 1.0f);
        std::vector<float> l ((size_t) seconds, 0.0f), r ((size_t) seconds, 0.0f);
        l[0] = 1.0f; r[0] = 1.0f;
        float* pointers[2] { l.data(), r.data() };
        echo.process (pointers, channels, seconds);
        require (finite (l), "echo impulse response is finite");
        float repeats[4] {};
        for (int k = 1; k <= 3; ++k)
        {
            const int centre = (int) (0.1 * rate * k);
            const int guard = (int) (0.002 * rate);
            for (int i = centre - guard; i < centre + guard; ++i)
                repeats[k] = std::max (repeats[k], std::abs (l[(size_t) i]));
        }
        require (repeats[1] > 0.3f, "echo produces a first repeat with damping off");
        require (repeats[2] > 0.15f, "echo produces a second repeat with damping off");
        require (repeats[3] > 0.07f, "echo produces a third repeat with damping off");
        require (repeats[2] < repeats[1] && repeats[3] < repeats[2], "echo repeats decay");
    }

    // Repeat spacing matches the requested time.
    {
        syngen::TapeEcho echo;
        echo.prepare (rate, channels);
        echo.setParameters (0.25f, 0.4f, 0.2f, 1.0f);
        std::vector<float> l ((size_t) seconds, 0.0f), r ((size_t) seconds, 0.0f);
        l[0] = 1.0f; r[0] = 1.0f;
        float* pointers[2] { l.data(), r.data() };
        echo.process (pointers, channels, seconds);
        int located = 1;
        float loudest = 0.0f;
        for (int i = 2; i < seconds; ++i)
            if (std::abs (l[(size_t) i]) > loudest) { loudest = std::abs (l[(size_t) i]); located = i; }
        require (std::abs (located - 0.25 * rate) < 0.003 * rate, "echo repeat lands on the requested time");
    }

    // Damping darkens each successive repeat.
    {
        syngen::TapeEcho echo;
        echo.prepare (rate, channels);
        echo.setParameters (0.1f, 0.7f, 1.0f, 1.0f);
        Noise noise;
        std::vector<float> l ((size_t) seconds, 0.0f), r ((size_t) seconds, 0.0f);
        for (int i = 0; i < (int) (0.02 * rate); ++i)
        { l[(size_t) i] = noise.next() * 0.5f; r[(size_t) i] = l[(size_t) i]; }
        float* pointers[2] { l.data(), r.data() };
        echo.process (pointers, channels, seconds);
        auto brightness = [&] (int start, int count)
        {
            double difference = 0.0, total = 0.0;
            for (int i = start; i < start + count; ++i)
            {
                const double d = l[(size_t) i] - l[(size_t) (i - 1)];
                difference += d * d;
                total += (double) l[(size_t) i] * l[(size_t) i];
            }
            return difference / std::max (1.0e-20, total);
        };
        const int count = (int) (0.02 * rate);
        require (brightness ((int) (0.4 * rate), count) < brightness ((int) (0.1 * rate), count),
                 "echo repeats darken as they decay");
    }

    // A time change glides rather than jumping. Fed a 200 Hz sine, the echo's
    // own output is smooth sample to sample; an abruptly retargeted read
    // pointer would put a step of up to twice the amplitude in it. The sweep
    // runs across the full range, so this also covers the velocity cap: even
    // the widest jump only reads the line about half again as fast.
    {
        syngen::TapeEcho echo;
        echo.prepare (rate, channels);
        echo.setParameters (0.5f, 0.5f, 0.3f, 1.0f);
        std::vector<float> l ((size_t) seconds, 0.0f), r ((size_t) seconds, 0.0f);
        float* pointers[2] { l.data(), r.data() };
        double phase = 0.0;
        const double increment = 200.0 / rate;
        auto fill = [&]
        {
            for (int i = 0; i < seconds; ++i)
            {
                l[(size_t) i] = 0.3f * (float) std::sin (2.0 * 3.14159265358979323846 * phase);
                r[(size_t) i] = l[(size_t) i];
                phase += increment;
                if (phase >= 1.0) phase -= 1.0;
            }
        };
        fill();
        echo.process (pointers, channels, seconds);   // prime the line
        float largestStep = 0.0f;
        for (int block = 0; block < 8; ++block)
        {
            echo.setParameters (block % 2 == 0 ? 0.05f : 0.9f, 0.5f, 0.3f, 1.0f);
            fill();
            echo.process (pointers, channels, seconds);
            require (finite (l), "echo stays finite across a time sweep");
            for (int i = 1; i < seconds; ++i)
                largestStep = std::max (largestStep, std::abs (l[(size_t) i] - l[(size_t) (i - 1)]));
        }
        // A 200 Hz sine at 0.3 steps by 0.008 per sample at 44.1 kHz, and the
        // fastest the swept head reads it is 1.5x that.
        require (largestStep < 0.05f, "echo time changes glide instead of clicking");
    }

    // Maximum feedback, sustained full-scale input: the in-loop saturator has
    // to hold the tail bounded.
    {
        syngen::TapeEcho echo;
        echo.prepare (rate, channels);
        echo.setParameters (0.2f, 1.0f, 0.0f, 1.0f);
        Noise noise;
        std::vector<float> l ((size_t) seconds), r ((size_t) seconds);
        float* pointers[2] { l.data(), r.data() };
        float loudest = 0.0f;
        for (int block = 0; block < 20; ++block)
        {
            for (int i = 0; i < seconds; ++i) { l[(size_t) i] = noise.next(); r[(size_t) i] = noise.next(); }
            echo.process (pointers, channels, seconds);
            require (finite (l) && finite (r), "echo stays finite at maximum feedback");
            loudest = std::max (loudest, std::max (peak (l), peak (r)));
        }
        require (loudest < 4.0f, "echo output stays bounded at maximum feedback");
    }

    // Dry path is untouched at mix 0.
    {
        syngen::TapeEcho echo;
        echo.prepare (rate, channels);
        echo.setParameters (0.3f, 0.5f, 0.3f, 0.0f);
        Noise noise;
        std::vector<float> l ((size_t) seconds), r ((size_t) seconds);
        for (int i = 0; i < seconds; ++i) { l[(size_t) i] = noise.next() * 0.5f; r[(size_t) i] = l[(size_t) i]; }
        const std::vector<float> copy = l;
        float* pointers[2] { l.data(), r.data() };
        echo.process (pointers, channels, seconds);
        for (int i = 0; i < seconds; ++i)
            require (std::abs (l[(size_t) i] - copy[(size_t) i]) < 1.0e-6f, "echo mix 0 is transparent");
    }

    // Mono bus.
    {
        syngen::TapeEcho echo;
        echo.prepare (rate, 1);
        echo.setParameters (0.15f, 0.5f, 0.3f, 1.0f);
        std::vector<float> l ((size_t) seconds, 0.0f);
        l[0] = 1.0f;
        float* pointers[1] { l.data() };
        echo.process (pointers, 1, seconds);
        require (finite (l) && peak (l) > 0.0f, "echo renders a mono bus");
    }
}

int main()
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        testReverb (rate);
        testEcho (rate);
    }
    std::cout << "Effects tests passed\n";
    return 0;
}
