#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace syngen
{
// Fractionally addressed delay line on a power-of-two buffer, shared by the
// reverb network and the echo.
class FractionalDelay
{
public:
    void prepare (int maxSamples)
    {
        int size = 4;
        while (size < maxSamples + 4) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writeIndex = 0;
    }
    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); writeIndex = 0; }
    int capacity() const { return mask - 3; }
    void write (float x)
    {
        buffer[(size_t) writeIndex] = x;
        writeIndex = (writeIndex + 1) & mask;
    }
    // Catmull-Rom. Linear interpolation's high-frequency loss varies with the
    // fractional part, so a delay being swept amplitude-modulates its own top
    // end; a cubic fit is continuous in slope and does not.
    float read (double samples) const
    {
        const double clamped = std::clamp (samples, 2.0, (double) mask - 2.0);
        const double position = (double) writeIndex - clamped;
        const int index = (int) std::floor (position);
        const float t = (float) (position - index);
        const float ym1 = buffer[(size_t) ((index - 1) & mask)];
        const float y0  = buffer[(size_t) ( index      & mask)];
        const float y1  = buffer[(size_t) ((index + 1) & mask)];
        const float y2  = buffer[(size_t) ((index + 2) & mask)];
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }
private:
    std::vector<float> buffer;
    int mask = 3, writeIndex = 0;
};

// Fixed-length Schroeder allpass: flat magnitude, frequency-dependent delay.
// Cascaded, these smear an impulse into noise, which is what keeps a dry
// transient from arriving in a reverb tail as a handful of discrete echoes.
class SchroederAllpass
{
public:
    void prepare (int length)
    {
        buffer.assign ((size_t) std::max (1, length), 0.0f);
        position = 0;
    }
    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); position = 0; }
    float process (float x, float g)
    {
        const float stored = buffer[(size_t) position];
        const float v = x - g * stored;
        buffer[(size_t) position] = v;
        if (++position >= (int) buffer.size()) position = 0;
        return stored + g * v;
    }
private:
    std::vector<float> buffer;
    int position = 0;
};

// Sub-Hz sine for delay modulation. A parabola refined once is within 0.2% of
// a sine, far past what a few samples of sweep needs, and costs no library
// call per line per sample.
class SlowSine
{
public:
    void reset (double increment, double startPhase)
    {
        step = increment;
        phase = startPhase;
    }
    float next()
    {
        phase += step;
        if (phase >= 1.0) phase -= 1.0;
        const float t = 2.0f * (float) phase - 1.0f;
        const float parabola = 4.0f * t * (1.0f - std::abs (t));
        return 0.775f * parabola + 0.225f * parabola * std::abs (parabola);
    }
private:
    double phase = 0.0, step = 0.0;
};
}
