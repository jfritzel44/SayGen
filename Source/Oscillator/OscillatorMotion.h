#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace syngen
{
// A persistent, bounded random trajectory. Cubic interpolation has zero slope
// at its endpoints; the clock is in samples, independent of host buffer sizes.
class SmoothDrift
{
public:
    void prepare (double rate, uint32_t seed)
    {
        sampleRate = rate;
        randomState = seed != 0 ? seed : 1;
        from = randomValue(); to = randomValue();
        length = nextLength(); position = 0;
    }
    double next()
    {
        advance (1);
        const double t = (double) position / length;
        return from + (to - from) * t * t * (3.0 - 2.0 * t);
    }
    void advance (int samples)
    {
        while (samples >= length - position)
        {
            samples -= length - position;
            from = to; to = randomValue();
            length = nextLength(); position = 0;
        }
        position += samples;
    }
private:
    uint32_t random()
    {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return randomState;
    }
    double randomValue() { return (double) random() / 4294967295.0 * 2.0 - 1.0; }
    int nextLength() { return std::max (1, (int) (sampleRate * (0.8 + 0.4 * randomValue()))); }
    uint32_t randomState = 1;
    double sampleRate = 44100.0, from = 0.0, to = 0.0;
    int length = 44100, position = 0;
};

// White-to-pink noise, generated at the oscillators' rate so the decimator
// band-limits it along with everything else.
//
// The pink path is Paul Kellett's three one-pole economy filter, which
// approximates -3 dB/octave to within about 0.3 dB across the audio band. Its
// published coefficients are for 44.1 kHz, and the voice runs at four times
// whatever the host is using, so each pole is re-solved for the actual rate
// (p' = p^(44100/rate)) and its input gain rescaled by sqrt((1-p'^2)/(1-p^2))
// to hold that band's output power constant. Without that the filter would get
// progressively shallower as the sample rate rose and the "pink" setting would
// drift towards white.
class NoiseSource
{
public:
    void prepare (double rate, uint32_t seed)
    {
        randomState = seed != 0 ? seed : 1;
        const double exponent = 44100.0 / (rate > 0.0 ? rate : 44100.0);
        for (int i = 0; i < poles; ++i)
        {
            const double p = std::pow (referencePole[i], exponent);
            pole[i] = p;
            gain[i] = referenceGain[i] * std::sqrt ((1.0 - p * p)
                                                    / (1.0 - referencePole[i] * referencePole[i]));
        }
        reset();
    }
    void reset() { for (auto& s : state) s = 0.0; }
    // colour 0 is pink, 1 is white; both sides are trimmed to equal RMS so the
    // control changes tone without changing level.
    double next (double colour)
    {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        const double white = (double) randomState / 2147483647.5 - 1.0;
        double pink = white * directGain;
        for (int i = 0; i < poles; ++i)
        {
            state[i] = pole[i] * state[i] + white * gain[i];
            pink += state[i];
        }
        const double c = std::clamp (colour, 0.0, 1.0);
        // Pink is derived from this same white sample, so the two are partly
        // correlated and a plain crossfade loses about 2 dB in the middle
        // rather than holding level. Dividing by the RMS a correlation of
        // `correlation` predicts flattens the control end to end.
        const double compensation = 1.0 / std::sqrt (1.0 - 2.0 * c * (1.0 - c) * (1.0 - correlation));
        return (pink * pinkTrim * (1.0 - c) + white * whiteTrim * c) * compensation;
    }
private:
    static constexpr int poles = 3;
    static constexpr double referencePole[poles] { 0.99765, 0.96300, 0.57000 };
    static constexpr double referenceGain[poles] { 0.0990460, 0.2965164, 1.0526913 };
    static constexpr double directGain = 0.1848;
    // Measured so the two ends of the colour control match in RMS, and the
    // measured correlation between the pink and white paths.
    static constexpr double pinkTrim = 0.122, whiteTrim = 0.36, correlation = 0.30;
    uint32_t randomState = 1;
    double pole[poles] { referencePole[0], referencePole[1], referencePole[2] };
    double gain[poles] { referenceGain[0], referenceGain[1], referenceGain[2] };
    double state[poles] {};
};

// Free-running sine for pulse-width modulation, one per oscillator. A parabola
// refined once is within 0.2% of a sine, which is far past what a width sweep
// needs and avoids a library call per oscillator per sample.
class PulseWidthLfo
{
public:
    void reset (double startPhase) { phase = startPhase - std::floor (startPhase); }
    void setIncrement (double perSample) { step = perSample; }
    double next()
    {
        phase += step;
        if (phase >= 1.0) phase -= std::floor (phase);
        const double t = 2.0 * phase - 1.0;
        const double parabola = 4.0 * t * (1.0 - std::abs (t));
        return 0.775 * parabola + 0.225 * parabola * std::abs (parabola);
    }
    void advance (int samples) { phase += step * samples; phase -= std::floor (phase); }
private:
    double phase = 0.0, step = 0.0;
};

// Constant-time, linear-in-semitones portamento. A repeated target does not
// restart an in-flight glide. New targets start from the current pitch/velocity.
class PitchGlide
{
public:
    void reset (double hz, double velocity)
    {
        frequency = targetFrequency = hz;
        level = targetLevel = velocity;
        remaining = 0;
    }
    void target (double hz, double velocity, double seconds, double sampleRate)
    {
        if (hz == targetFrequency && velocity == targetLevel) return;
        targetFrequency = hz; targetLevel = velocity;
        remaining = std::max (0, (int) std::llround (seconds * sampleRate));
        if (remaining == 0) { reset (hz, velocity); return; }
        pitchRatio = std::exp (std::log (hz / frequency) / remaining);
        levelStep = (velocity - level) / remaining;
    }
    void next()
    {
        if (remaining == 0) return;
        frequency *= pitchRatio;
        level += levelStep;
        if (--remaining == 0) { frequency = targetFrequency; level = targetLevel; }
    }
    double getFrequency() const { return frequency; }
    double getLevel() const { return level; }
private:
    double frequency = 440, targetFrequency = 440, level = 1, targetLevel = 1;
    double pitchRatio = 1, levelStep = 0;
    int remaining = 0;
};
}

namespace syngen
{
// Two-sample polynomial BLEP/BLAMP corrections at fractional event times.
// Waveform jumps and slope changes are treated separately, including those
// introduced by hard sync. The caller supplies one sample interval at 4x rate.
class BandlimitedOscillator
{
public:
    struct Mix { double sine = 0, saw = 1, pulse = 0, triangle = 0, sub = 0; };
    void setPhase (double p, double sub)
    {
        phase = wrap (p); subPhase = wrap (sub); correction = 0;
    }
    double getPhase() const { return phase; }
    double getSubPhase() const { return subPhase; }
    double next (double step, const Mix& mix, double width, double syncTime = -1)
    {
        step = std::clamp (step, 1.0e-9, 0.45);
        width = std::clamp (width, 0.02, 0.98);
        double out = sample (phase, mix, width) + mix.sub * pulse (subPhase, 0.5) + correction;
        correction = 0;
        auto jump = [&] (double time, double height, double slope)
        {
            const double before = 1.0 - time;
            out += 0.5 * height * before * before + slope * before * before * before / 6.0;
            correction += -0.5 * height * time * time + slope * time * time * time / 6.0;
        };
        auto segment = [&] (double p, double sub, double start, double duration)
        {
            auto edge = [&] (double location, double initial, double increment, double height, double slope)
            {
                const double distance = location - initial;
                if (distance > 0 && distance <= increment * duration)
                    jump (start + distance / increment, height, slope);
            };
            edge (1, p, step, -2 * (mix.saw + mix.pulse), 8 * step * mix.triangle);
            if (mix.pulse != 0)
                for (double at : { 1.0 - width, 2.0 - width })
                    edge (at, p, step, 2 * mix.pulse, 0);
            if (mix.triangle != 0)
                for (double at : { 0.5, 1.5 })
                    edge (at, p, step, 0, -8 * step * mix.triangle);
            if (mix.sub != 0)
            {
                edge (1, sub, step * 0.5, -2 * mix.sub, 0);
                for (double at : { 0.5, 1.5 })
                    edge (at, sub, step * 0.5, 2 * mix.sub, 0);
            }
        };
        if (syncTime >= 0 && syncTime <= 1)
        {
            segment (phase, subPhase, 0, syncTime);
            const double before = wrap (phase + step * syncTime);
            const double subBefore = wrap (subPhase + step * 0.5 * syncTime);
            jump (syncTime, sample (0, mix, width) - sample (before, mix, width)
                              + mix.sub * (pulse (0, 0.5) - pulse (subBefore, 0.5)),
                  step * (slope (0, mix) - slope (before, mix)));
            segment (0, 0, syncTime, 1 - syncTime);
        }
        else
            segment (phase, subPhase, 0, 1);
        advance (step, syncTime);
        return out;
    }
    void advance (double step, double syncTime = -1)
    {
        if (syncTime >= 0 && syncTime <= 1)
        {
            phase = step * (1 - syncTime);
            subPhase = phase * 0.5;
        }
        else
        {
            phase = wrap (phase + step);
            subPhase = wrap (subPhase + step * 0.5);
        }
    }
    void clearCorrection() { correction = 0; }
private:
    static double wrap (double p) { return p - std::floor (p); }
    static double pulse (double p, double width) { return (p >= 1 - width ? 2.0 : 0.0) - 2 * width; }
    static double sample (double p, const Mix& m, double width)
    {
        double value = m.saw * (2 * p - 1) + m.pulse * pulse (p, width)
                       + m.triangle * (p < 0.5 ? 4 * p - 1 : 3 - 4 * p);
        if (m.sine != 0) value += m.sine * std::sin (twoPi * p);
        return value;
    }
    static double slope (double p, const Mix& m)
    {
        double value = 2 * m.saw + (p < 0.5 ? 4 : -4) * m.triangle;
        if (m.sine != 0) value += m.sine * twoPi * std::cos (twoPi * p);
        return value;
    }
    static constexpr double twoPi = 6.28318530717958647692;
    double phase = 0, subPhase = 0, correction = 0;
};
}
