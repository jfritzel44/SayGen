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
    std::cout << "Enhanced DSP tests passed\n";
}
