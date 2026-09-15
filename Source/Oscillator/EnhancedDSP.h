#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace syngen
{
// Four trapezoidal one-poles with a saturating, implicitly solved feedback
// junction, each stage saturating its own differential input. This is a musical
// ladder model, not a transistor circuit emulation: the junction solve treats
// the stage gains as linear, so hard drive shifts where self-oscillation starts.
// TPT foundation: Vadim Zavalishin, The Art of VA Filter Design, chapter 4.
class FeedbackLadder
{
public:
    void reset() { state.fill (0.0); }

    // Smooth rational tanh approximation: odd, unit slope at zero, saturating
    // to exactly +/-1 at |x| >= 3. Cheap enough to run at every ladder stage.
    static double saturate (double x)
    {
        const double b = std::clamp (x, -3.0, 3.0);
        const double squared = b * b;
        return b * (27.0 + squared) / (27.0 + 9.0 * squared);
    }
    // Analytic derivative of saturate(), zero where the input is clamped.
    static double saturateSlope (double x)
    {
        if (x <= -3.0 || x >= 3.0) return 0.0;
        const double squared = x * x;
        return (squared - 9.0) * (squared - 9.0) / (9.0 * (squared + 3.0) * (squared + 3.0));
    }

    float process (float input, double G, double resonance, double compensation)
    {
        const double G2 = G * G, G4 = G2 * G2;
        const double S = (1.0 - G) * (G2 * G * state[0] + G2 * state[1]
                                                         + G * state[2] + state[3]);
        const double k = std::clamp (resonance, 0.0, 4.2);
        const double x = input * (1.0 + k * std::clamp (compensation, 0.0, 1.0));
        // Solve u = saturate(x - k * (G^4*u + S)). Monotonic residual,
        // bracketed Newton: bounded work and a bounded junction signal.
        double lo = -1.0, hi = 1.0;
        double u = std::clamp ((x - k * S) / (1.0 + k * G4), lo, hi);
        for (int iteration = 0; iteration < 8; ++iteration)
        {
            const double junction = x - k * (G4 * u + S);
            const double t = saturate (junction);
            const double derivative = saturateSlope (junction);
            const double residual = u - t;
            if (std::abs (residual) < 1.0e-9) break;
            if (residual > 0.0) hi = u; else lo = u;
            const double candidate = u - residual / (1.0 + k * G4 * derivative);
            u = candidate > lo && candidate < hi ? candidate : 0.5 * (lo + hi);
        }
        for (auto& s : state)
        {
            // Each stage saturates its own differential input, the way a
            // ladder's transistor pairs do, so drive and high resonance
            // compress progressively instead of meeting one clipper at the
            // feedback junction. saturate() is odd with unit slope at zero,
            // so quiet signals keep the linear response and unity DC gain.
            const double v = (saturate (u) - saturate (s)) * G;
            u = v + s;
            s = u + v;
        }
        return (float) u;
    }

    static double coefficient (double cutoff, double sampleRate)
    {
        const double g = std::tan (3.14159265358979323846
                                   * std::clamp (cutoff / sampleRate, 0.000001, 0.45));
        return g / (1.0 + g);
    }
private:
    std::array<double, 4> state {};
};

// One-pole DC blocker. An odd saturator fed a zero-mean but asymmetric wave -
// a narrow pulse, say - still rectifies it into a DC offset, which eats
// headroom, pushes the waveform further into the saturator on one side, and
// thumps as notes start and stop. Placed after the ladder and before the VCA,
// so it sees a steady offset rather than one the amplitude envelope is scaling.
class DCBlocker
{
public:
    void prepare (double rate) { pole = 1.0 - 2.0 * 3.14159265358979323846 * 5.0 / rate; }
    void reset() { previousInput = previousOutput = 0.0; }
    float process (float x)
    {
        const double output = x - previousInput + pole * previousOutput;
        previousInput = x;
        previousOutput = output;
        return (float) output;
    }
private:
    double pole = 0.9998, previousInput = 0.0, previousOutput = 0.0;
};

// Finite-duration exponential segments. Time remains in seconds at every
// rendering rate; retriggers start from the current level. Parameters can be
// changed during a note without resetting the envelope's clock.
class CurveEnvelope
{
public:
    void prepare (double rate) { sampleRate = rate; smoothing = 1.0 - std::exp (-1.0 / (0.005 * rate)); }
    void reset() { value = start = progress = 0.0; stage = idle; }
    void setParameters (double a, double d, double s, double r, double c)
    {
        attack = std::max (0.0001, a); decay = std::max (0.0001, d);
        sustainTarget = std::clamp (s, 0.0, 1.0); release = std::max (0.0001, r);
        curveTarget = std::clamp (c, 0.0, 1.0);
        if (stage == idle) { sustain = sustainTarget; curve = curveTarget; }
    }
    void noteOn() { begin (attacking); }
    void noteOff() { if (stage != idle) begin (releasing); }
    bool isActive() const { return stage != idle; }
    float next()
    {
        sustain += smoothing * (sustainTarget - sustain);
        curve += smoothing * (curveTarget - curve);
        if (std::abs (curveTarget - curve) < 1e-6) curve = curveTarget;
        if (stage == idle) return 0.0f;
        if (stage == sustaining) { value = sustain; return (float) value; }
        const double duration = stage == attacking ? attack : stage == decaying ? decay : release;
        const double k = 5.0 * curve;
        if (k != cachedK || duration != cachedDuration)
        {
            cachedK = k; cachedDuration = duration;
            exponential = std::exp (-k * progress);
            step = std::exp (-k / (duration * sampleRate));
            denominator = -std::expm1 (-k);
        }
        progress = std::min (1.0, progress + 1.0 / (duration * sampleRate));
        exponential *= step;
        const double shaped = progress >= 1.0 ? 1.0
            : k < 1.0e-5 ? progress : (1.0 - exponential) / denominator;
        const double target = stage == attacking ? 1.0 : stage == decaying ? sustain : 0.0;
        value = start + (target - start) * shaped;
        if (progress >= 1.0)
        {
            if (stage == attacking) begin (decaying);
            else if (stage == decaying) stage = sustaining;
            else { stage = idle; value = 0.0; }
        }
        return (float) value;
    }
private:
    enum Stage { idle, attacking, decaying, sustaining, releasing } stage = idle;
    void begin (Stage s) { start = value; progress = 0.0; stage = s; cachedDuration = -1.0; }
    double cachedK = -1.0, cachedDuration = -1.0, exponential = 1.0, step = 1.0, denominator = 1.0;
    double sampleRate = 44100.0, smoothing = 0.001;
    double attack = 0.01, decay = 0.1, sustain = 0.7, release = 0.1;
    double sustainTarget = 0.7, curve = 0.65, curveTarget = 0.65;
    double value = 0.0, start = 0.0, progress = 0.0;
};

// Streaming 4:1 decimator. Blackman-windowed sinc, unity DC gain. The
// transition is centred at 0.1125 cycles/internal sample (0.45 output rate).
// 129 taps give 16 output samples of group delay. No audio-thread allocation.
class Decimator4
{
public:
    static constexpr int taps = 129;
    Decimator4()
    {
        double total = 0.0;
        for (int i = 0; i < taps; ++i)
        {
            const double n = i - (taps - 1) * 0.5;
            const double phase = 2.0 * pi * i / (taps - 1);
            const double window = 0.42 - 0.5 * std::cos (phase) + 0.08 * std::cos (2.0 * phase);
            weights[i] = (float) ((n == 0.0 ? 0.225 : std::sin (2.0 * pi * 0.1125 * n) / (pi * n)) * window);
            total += weights[i];
        }
        for (auto& w : weights) w /= (float) total;
    }
    void reset() { history.fill (0.0f); position = 0; }
    void push (float x) { history[position] = x; position = (position + 1) % taps; }
    float output() const
    {
        float result = 0.0f;
        int index = position;
        for (int i = 0; i < taps; ++i)
        {
            if (--index < 0) index = taps - 1;
            result += weights[i] * history[index];
        }
        return result;
    }
private:
    static constexpr double pi = 3.14159265358979323846;
    std::array<float, taps> weights {}, history {};
    int position = 0;
};
}
