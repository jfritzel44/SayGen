#include "../Source/Oscillator/OscillatorMotion.h"
#include "../Source/Oscillator/EnhancedDSP.h"
#include <cstdlib>
#include <iostream>
#include <vector>

static void require (bool condition, const char* message)
{
    if (! condition) { std::cerr << "FAIL: " << message << '\n'; std::exit (1); }
}
int main()
{
    constexpr double pi = 3.14159265358979323846;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        syngen::SmoothDrift a, b, independent;
        a.prepare (rate, 123); b.prepare (rate, 123); independent.prepare (rate, 456);
        double last = a.next(), difference = 0;
        b.advance (1); independent.advance (1);
        for (int i = 0; i < 10000; ++i)
        {
            const double value = a.next();
            require (std::abs (value) <= 1 && std::abs (value - last) < 0.001, "bounded smooth drift");
            difference += std::abs (value - independent.next()); last = value;
        }
        b.advance (10000);
        require (a.next() == b.next(), "idle drift advancement equals individual samples");
        require (difference > 1, "separate drift seeds produce independent trajectories");
        for (int n = 0; n < (int) rate * 3; ++n) a.next();
        b.advance ((int) rate * 3);
        require (a.next() == b.next(), "drift clock crosses random segments consistently");

        syngen::PitchGlide glide;
        glide.reset (220, 1); glide.target (880, 0.5, 0.1, rate);
        for (int n = 0; n < (int) (rate * 0.05); ++n) glide.next();
        require (std::abs (glide.getFrequency() - 440) < 1e-6, "glide midpoint is halfway in semitones");
        glide.target (880, 0.5, 0.1, rate);
        for (int n = 0; n < (int) (rate * 0.05); ++n) glide.next();
        require (glide.getFrequency() == 880 && glide.getLevel() == 0.5, "repeated target does not restart glide");
        glide.target (110, 1, 0, rate);
        require (glide.getFrequency() == 110, "zero-time glide lands immediately");
    }
    // Compare residual energy outside the legal master harmonics of a synced
    // saw, using identical 4x downsampling for naive and corrected waveforms.
    auto syncAlias = [&] (bool corrected)
    {
        constexpr int size = 4096, masterBin = 137, slaveBin = 659;
        syngen::BandlimitedOscillator oscillator;
        syngen::Decimator4 decimator;
        std::vector<double> samples (size);
        const double masterStep = (double) masterBin / (size * 4);
        const double slaveStep = (double) slaveBin / (size * 4);
        double master = 0, slave = 0;
        for (int n = 0; n < size * 3; ++n)
        {
            for (int sub = 0; sub < 4; ++sub)
            {
                const double sync = master + masterStep >= 1 ? (1 - master) / masterStep : -1;
                const double value = corrected ? oscillator.next (slaveStep, {}, 0.5, sync) : 2 * slave - 1;
                decimator.push ((float) value);
                master += masterStep; if (master >= 1) master -= 1;
                slave = sync >= 0 ? slaveStep * (1 - sync) : slave + slaveStep;
                slave -= std::floor (slave);
            }
            if (n >= size * 2) samples[n - size * 2] = decimator.output();
        }
        double total = 0, mean = 0, projected = 0;
        for (double s : samples) { total += s * s / size; mean += s / size; }
        projected = mean * mean;
        for (int harmonic = 1; harmonic * masterBin < size / 2; ++harmonic)
        {
            double real = 0, imaginary = 0;
            for (int n = 0; n < size; ++n)
            {
                const double phase = 2 * pi * harmonic * masterBin * n / size;
                real += samples[n] * std::cos (phase) / size;
                imaginary += samples[n] * std::sin (phase) / size;
            }
            projected += 2 * (real * real + imaginary * imaginary);
        }
        return std::max (1e-16, (total - projected) / total);
    };
    const double reduction = 10 * std::log10 (syncAlias (false) / syncAlias (true));
    std::cout << "Sync alias-energy reduction versus naive 4x reset: " << reduction << " dB\n";
    require (reduction > 6, "fractional BLEP sync improves on oversampling alone");
    for (double step : { 0.0001, 0.03, 0.2, 0.45 })
        for (double width : { 0.02, 0.5, 0.98 })
        {
            syngen::BandlimitedOscillator osc;
            for (int n = 0; n < 10000; ++n)
            {
                const double out = osc.next (step, { 1, 1, 1, 1, 0.6 }, width, n % 7 == 0 ? 0.37 : -1);
                require (std::isfinite (out) && std::abs (out) < 8, "all waveforms remain bounded under sync");
            }
        }
    std::cout << "Oscillator motion tests passed\n";
}
