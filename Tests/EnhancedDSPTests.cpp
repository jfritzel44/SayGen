#include "../Source/Oscillator/EnhancedDSP.h"
#include <cstdlib>
#include <iostream>

static void require (bool condition, const char* message)
{
    if (! condition) { std::cerr << "FAIL: " << message << '\n'; std::exit (1); }
}

int main()
{
    constexpr double pi = 3.14159265358979323846;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        syngen::CurveEnvelope env;
        env.prepare (rate * 4);
        for (double curve : { 0.0, 0.65, 1.0 })
        {
            env.reset(); env.setParameters (0.01, 0.1, 0.4, 0.05, curve); env.noteOn();
            float last = 0;
            for (int n = 0; n < (int) (rate * 4 * 0.01); ++n)
            {
                float v = env.next();
                require (v >= last && v <= 1.00001f, "monotonic attack"); last = v;
            }
            require (last > 0.999f, "attack duration at oversampled rate");
            for (int n = 0; n < (int) (rate * 4 * 0.11); ++n) env.next();
            require (std::abs (env.next() - 0.4f) < 1e-5, "decay reaches sustain");
            env.setParameters (0.01, 0.1, 0.8, 0.05, curve);
            for (int n = 0; n < (int) (rate * 4 * 0.1); ++n) env.next();
            require (env.next() > 0.799f, "live sustain update");
            env.noteOff();
            for (int n = 0; n < (int) (rate * 4 * 0.05) + 2; ++n) env.next();
            require (! env.isActive() && env.next() == 0, "finite silent release");
        }
        for (double res : { 0.0, 2.0, 3.9, 4.2 })
            for (double gain : { 0.0, 0.3, 4.0, 20.0 })
            {
                syngen::FeedbackLadder filter;
                for (int n = 0; n < 40000; ++n)
                {
                    const double cutoff = 20 * std::pow (1000.0, (n % 4000) / 3999.0);
                    const float x = (float) (gain * std::sin (2 * pi * 1200 * n / (rate * 4)));
                    const float y = filter.process (x, filter.coefficient (cutoff, rate * 4), res, 1.0);
                    require (std::isfinite (y) && std::abs (y) < 4.0f, "filter stable under swept cutoff and drive");
                }
            }
    }
    // Per-stage saturation must leave the small-signal response exactly where
    // the linear model had it, then compress progressively as the level rises,
    // including at the resonant peak where a single junction clipper barely
    // acts. Measured at the cutoff, which is where the stage gains dominate.
    {
        constexpr double rate = 192000, cutoff = 1000;
        const double G = syngen::FeedbackLadder::coefficient (cutoff, rate);
        auto peakGain = [&] (double level, double resonance)
        {
            syngen::FeedbackLadder filter;
            double peak = 0;
            for (int n = 0; n < 120000; ++n)
            {
                const float y = filter.process ((float) (level * std::sin (2 * pi * cutoff * n / rate)),
                                                G, resonance, 0);
                if (n > 90000) peak = std::max (peak, (double) std::abs (y));
            }
            return peak / level;
        };
        const double tiny = peakGain (1e-5, 3.5), quiet = peakGain (1e-4, 3.5);
        require (std::abs (tiny / quiet - 1) < 0.001, "small-signal resonant gain stays linear");
        double previous = quiet;
        for (double level : { 1e-2, 0.1, 0.3, 1.0 })
        {
            const double gain = peakGain (level, 3.5);
            require (gain > 0 && gain < previous, "driven ladder gain compresses monotonically");
            previous = gain;
        }
        require (previous < 0.2 * quiet, "hard drive compresses the resonant peak by at least 14 dB");
        std::cout << "Ladder resonant peak gain: quiet " << quiet << ", driven " << previous << '\n';
        // Each stage settles on its input at DC, so the cascade still passes a
        // small steady level at unity and only the junction bounds a hot one.
        for (double level : { 0.001, 1.0 })
        {
            syngen::FeedbackLadder filter;
            float y = 0;
            for (int n = 0; n < 120000; ++n) y = filter.process ((float) level, G, 0, 0);
            require (level > 0.5 ? y < 0.8f && y > 0.7f : std::abs (y / level - 1) < 1e-4,
                     "ladder DC gain is unity below saturation and bounded above it");
        }
    }
    // The DC blocker has to remove a steady offset outright while leaving the
    // audio band alone: an odd saturator fed an asymmetric wave rectifies it
    // into DC, and at 5 Hz the corner is far below anything musical.
    {
        syngen::DCBlocker blocker;
        blocker.prepare (192000);
        float y = 0;
        for (int n = 0; n < 400000; ++n) y = blocker.process (1.0f);
        require (std::abs (y) < 1e-3f, "DC blocker rejects a steady offset");
        for (double tone : { 50.0, 1000.0 })
        {
            blocker.reset();
            double peak = 0;
            for (int n = 0; n < 200000; ++n)
            {
                const float out = blocker.process ((float) std::sin (2 * pi * tone * n / 192000.0));
                if (n > 100000) peak = std::max (peak, (double) std::abs (out));
            }
            require (std::abs (peak - 1) < 0.01, "DC blocker passes the audio band at unity");
        }
    }
    for (double f : { 0.0, 0.05, 0.14, 0.2, 0.4 })
    {
        syngen::Decimator4 d;
        double energy = 0;
        int count = 0;
        for (int n = 0; n < 20000; ++n)
        {
            d.push ((float) std::cos (2 * pi * f * n));
            if (n > 1000 && n % 4 == 3) { auto y = d.output(); energy += y * y; ++count; }
        }
        const double rms = std::sqrt (energy / count);
        if (f == 0) require (std::abs (rms - 1) < 1e-5, "decimator unity DC gain");
        else if (f < 0.1) require (std::abs (rms - std::sqrt (0.5)) < 0.001, "decimator passband");
        else require (rms < 0.0005, "decimator stopband below -66 dB");
        std::cout << "Decimator f=" << f << " RMS=" << rms << '\n';
    }
    // Coherent 10 kHz-class sine driven into the nonlinear ladder. Measure
    // the folded third harmonic at 48 kHz, relative to the wanted fundamental.
    auto aliasRatio = [&] (int factor)
    {
        syngen::FeedbackLadder filter;
        syngen::Decimator4 decimator;
        constexpr int length = 4096, fundamental = 853, alias = 1537;
        double real[2] {}, imag[2] {};
        const double G = filter.coefficient (18000, 48000 * factor);
        for (int n = 0; n < length * 2; ++n)
        {
            float y = 0;
            for (int sub = 0; sub < factor; ++sub)
            {
                const double time = n + (double) sub / factor;
                y = filter.process ((float) (4 * std::sin (2 * pi * fundamental * time / length)), G, 0, 0);
                if (factor == 4) decimator.push (y);
            }
            if (factor == 4) y = decimator.output();
            if (n >= length)
                for (int bin = 0; bin < 2; ++bin)
                {
                    const double phase = 2 * pi * (bin == 0 ? fundamental : alias) * n / length;
                    real[bin] += y * std::cos (phase); imag[bin] += y * std::sin (phase);
                }
        }
        return std::hypot (real[1], imag[1]) / std::hypot (real[0], imag[0]);
    };
    const double improvement = 20 * std::log10 (aliasRatio (1) / aliasRatio (4));
    require (improvement > 20, "oversampling reduces folded third harmonic by at least 20 dB");
    std::cout << "Driven-filter alias reduction: " << improvement << " dB\n";
    // --- Ladder output modes -------------------------------------------------
    // Each mode is a weighted sum of taps the ladder already computes, so the
    // checks here are on the shape of the response, not on the filter core,
    // which is unchanged and covered above.
    {
        const double rate = 48000.0 * 4;
        // Magnitude response at a given frequency, measured by driving the
        // filter with a sine until it settles and taking the steady-state RMS.
        auto magnitude = [&] (int mode, double cutoff, double frequency)
        {
            syngen::FeedbackLadder filter;
            const double G = syngen::FeedbackLadder::coefficient (cutoff, rate);
            const auto response = syngen::FeedbackLadder::response (mode);
            const int settle = (int) (rate * 0.25), measure = (int) (rate * 0.25);
            double total = 0.0;
            for (int n = 0; n < settle + measure; ++n)
            {
                // Small signal, so the saturating stages stay in their linear
                // region and this measures the mode, not the drive.
                const float x = (float) (0.001 * std::sin (2.0 * pi * frequency * n / rate));
                const float y = filter.process (x, G, 0.0, 0.0, response);
                if (n >= settle) total += (double) y * y;
            }
            return std::sqrt (total / measure) / (0.001 / std::sqrt (2.0));
        };

        const double cutoff = 1000.0;
        // Lowpasses pass the bottom and stop the top; highpasses the reverse;
        // bandpasses roll off on both sides; the notch cuts only at cutoff.
        require (magnitude (syngen::FeedbackLadder::lowpass24, cutoff, 100.0) > 0.95, "LP24 passes below cutoff");
        require (magnitude (syngen::FeedbackLadder::lowpass24, cutoff, 8000.0) < 0.02, "LP24 stops above cutoff");
        require (magnitude (syngen::FeedbackLadder::lowpass12, cutoff, 100.0) > 0.95, "LP12 passes below cutoff");
        require (magnitude (syngen::FeedbackLadder::lowpass12, cutoff, 8000.0) < 0.1, "LP12 stops above cutoff");
        require (magnitude (syngen::FeedbackLadder::highpass24, cutoff, 100.0) < 0.02, "HP24 stops below cutoff");
        require (magnitude (syngen::FeedbackLadder::highpass24, cutoff, 8000.0) > 0.9, "HP24 passes above cutoff");
        require (magnitude (syngen::FeedbackLadder::highpass12, cutoff, 100.0) < 0.1, "HP12 stops below cutoff");
        require (magnitude (syngen::FeedbackLadder::highpass12, cutoff, 8000.0) > 0.9, "HP12 passes above cutoff");

        // A 24 dB/octave slope must be twice as steep as a 12 dB/octave one.
        // Two octaves below cutoff for the highpasses, measured as a ratio
        // between one and two octaves out, keeps both well inside the stopband.
        auto slopeDb = [&] (int mode, double near, double far)
        {
            return 20.0 * std::log10 (magnitude (mode, cutoff, near) / magnitude (mode, cutoff, far));
        };
        const double lp12Slope = slopeDb (syngen::FeedbackLadder::lowpass12, 4000.0, 8000.0);
        const double lp24Slope = slopeDb (syngen::FeedbackLadder::lowpass24, 4000.0, 8000.0);
        std::cout << "Ladder slope per octave: LP12 " << lp12Slope << " dB, LP24 " << lp24Slope << " dB\n";
        require (lp12Slope > 10.0 && lp12Slope < 14.0, "LP12 rolls off about 12 dB per octave");
        require (lp24Slope > 22.0 && lp24Slope < 26.0, "LP24 rolls off about 24 dB per octave");

        // Gain at cutoff is exact for a cascade of identical one-poles: each
        // stage contributes 1/sqrt(2), so two poles give 1/2 and four give 1/4,
        // and the highpasses mirror them. The bandpass normalisation is chosen
        // to put the peak at exactly unity.
        require (std::abs (magnitude (syngen::FeedbackLadder::lowpass12, cutoff, cutoff) - 0.5) < 0.005,
                 "LP12 is -6 dB at cutoff");
        require (std::abs (magnitude (syngen::FeedbackLadder::lowpass24, cutoff, cutoff) - 0.25) < 0.005,
                 "LP24 is -12 dB at cutoff");
        require (std::abs (magnitude (syngen::FeedbackLadder::highpass12, cutoff, cutoff) - 0.5) < 0.005,
                 "HP12 is -6 dB at cutoff");
        require (std::abs (magnitude (syngen::FeedbackLadder::highpass24, cutoff, cutoff) - 0.25) < 0.005,
                 "HP24 is -12 dB at cutoff");

        for (int mode : { syngen::FeedbackLadder::bandpass12, syngen::FeedbackLadder::bandpass24 })
            require (std::abs (magnitude (mode, cutoff, cutoff) - 1.0) < 0.005,
                     "bandpass peaks at unity on its centre");
        // Four octaves out. A two-pole bandpass only approaches 6 dB/octave
        // well away from centre, so its skirts are much gentler than the
        // four-pole's 12 dB/octave, and the bounds differ accordingly.
        require (magnitude (syngen::FeedbackLadder::bandpass12, cutoff, 30.0) < 0.1,
                 "BP12 rejects the bottom");
        require (magnitude (syngen::FeedbackLadder::bandpass12, cutoff, 16000.0) < 0.2,
                 "BP12 rejects the top");
        require (magnitude (syngen::FeedbackLadder::bandpass24, cutoff, 30.0) < 0.02,
                 "BP24 rejects the bottom");
        require (magnitude (syngen::FeedbackLadder::bandpass24, cutoff, 16000.0) < 0.05,
                 "BP24 rejects the top");
        require (magnitude (syngen::FeedbackLadder::bandpass24, cutoff, 250.0)
                 < 0.6 * magnitude (syngen::FeedbackLadder::bandpass12, cutoff, 250.0),
                 "BP24 is the more selective of the two");

        const int notch = syngen::FeedbackLadder::notch;
        require (magnitude (notch, cutoff, cutoff) < 0.01, "notch nulls at cutoff");
        require (magnitude (notch, cutoff, 60.0) > 0.9, "notch passes well below cutoff");
        require (magnitude (notch, cutoff, 16000.0) > 0.9, "notch passes well above cutoff");

        // The DC gain of each mode is what scales the bass-compensation
        // control in the voice, so it has to be exactly 1 for the lowpasses
        // and the notch and exactly 0 for the bandpasses and highpasses.
        for (int mode = 0; mode < syngen::FeedbackLadder::modeCount; ++mode)
        {
            const auto response = syngen::FeedbackLadder::response (mode);
            double sum = 0.0;
            for (double w : response.weight) sum += w;
            const bool passesDC = mode == syngen::FeedbackLadder::lowpass24
                               || mode == syngen::FeedbackLadder::lowpass12
                               || mode == syngen::FeedbackLadder::notch;
            require (std::abs (sum - (passesDC ? 1.0 : 0.0)) < 1e-12, "mode DC gain is exact");
        }

        // The default overload and an explicit LP24 response must be the same
        // filter, so nothing that already existed changed behaviour.
        syngen::FeedbackLadder plain, tapped;
        const double G = syngen::FeedbackLadder::coefficient (1200.0, rate);
        const auto lp24 = syngen::FeedbackLadder::response (syngen::FeedbackLadder::lowpass24);
        for (int n = 0; n < 20000; ++n)
        {
            const float x = (float) std::sin (0.01 * n) * 0.7f;
            require (plain.process (x, G, 3.0, 0.5) == tapped.process (x, G, 3.0, 0.5, lp24),
                     "default response is bit-identical to LP24");
        }
    }

    std::cout << "Enhanced DSP tests passed\n";
}
