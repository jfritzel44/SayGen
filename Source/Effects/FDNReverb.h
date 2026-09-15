#pragma once
#include <algorithm>
#include <cmath>
#include "DelayPrimitives.h"

namespace syngen
{
// A modulated feedback delay network, replacing the Schroeder comb-and-allpass
// bank (Freeverb) the effects chain used to run. Freeverb's four fixed comb
// lengths per channel put their modal peaks far enough apart to be heard
// individually: sustained chords ring metallic and short decays flutter. An FDN
// instead couples every delay line to every other through an orthogonal matrix,
// so mode density is high from the first millisecond, and the network is
// lossless apart from the per-line gains that set the decay time.
//
// Signal order:
//   Dattorro input diffusion (4 allpasses per channel, decorrelated L/R)
//     -> 8-line FDN, Hadamard feedback, per-line low-pass and low-cut
//     -> sign-alternating stereo tap, mid/side width, wet/dry mix
//
// Jot & Chaigne, "Digital delay networks for designing artificial
// reverberators" (AES 1991); Dattorro, "Effect Design Part 1" (JAES 1997).
class FDNReverb
{
public:
    static constexpr int lines = 8;
    static constexpr int diffusers = 4;

    void prepare (double rate)
    {
        sampleRate = rate > 0.0 ? rate : 44100.0;
        modulationSamples = modulationSeconds * sampleRate;
        for (int i = 0; i < lines; ++i)
        {
            delay[i].prepare ((int) (baseDelayMs[i] * 0.001 * sampleRate + modulationSamples) + 8);
            // Staggered start phases, so no two lines sweep together and the
            // modulation never sums into one audible pitch wobble.
            modulator[i].reset (modulationHz[i] / sampleRate, (double) i / lines);
        }
        for (int channel = 0; channel < 2; ++channel)
            for (int d = 0; d < diffusers; ++d)
                diffuser[channel][d].prepare (
                    (int) (diffuserMs[d] * (channel == 0 ? 1.0 : 1.037) * 0.001 * sampleRate) + 2);
        lowCut = onePole (18.0);
        // 50 ms on every derived coefficient: size and damping can be swept
        // live without stepping the delay lengths or the feedback gains.
        smoothing = 1.0f - std::exp (-1.0f / (float) (0.05 * sampleRate));
        setParameters (0.55f, 0.5f, 1.0f, 0.35f);
        for (int i = 0; i < lines; ++i) { distance[i] = targetDistance[i]; gain[i] = targetGain[i]; }
        damping = targetDamping; wet = targetWet; dry = targetDry; width = targetWidth;
        reset();
    }

    void reset()
    {
        for (auto& line : delay) line.reset();
        for (auto& channel : diffuser) for (auto& d : channel) d.reset();
        for (auto& s : lowpassState) s = 0.0f;
        for (auto& s : highpassState) s = 0.0f;
    }

    // size 0..1 scales both the delay lengths and the decay time; damp 0..1 is
    // how fast the top end dies relative to the body; widthAmount 0..1 is the
    // stereo spread of the wet signal; mix 0..1 crossfades dry to wet.
    void setParameters (float size, float damp, float widthAmount, float mix)
    {
        size = std::clamp (size, 0.0f, 1.0f);
        damp = std::clamp (damp, 0.0f, 1.0f);
        const double scale = 0.4 + 0.6 * size;
        // RT60 tracks size superlinearly, so the low half of the knob stays in
        // room territory instead of jumping straight to a hall.
        const double rt60 = 0.25 + 5.0 * size * size;
        for (int i = 0; i < lines; ++i)
        {
            targetDistance[i] = std::max (4.0, baseDelayMs[i] * 0.001 * sampleRate * scale);
            // 10^(-3 L / (RT60 fs)): -60 dB after RT60 seconds of round trips.
            targetGain[i] = (float) std::min (0.9999,
                std::exp (-6.907755278982137 * targetDistance[i] / (rt60 * sampleRate)));
        }
        // Damping sweeps the in-loop corner from 20 kHz down to 500 Hz. The
        // filter has unity DC gain, so RT60 at the bottom end is unaffected.
        targetDamping = onePole (20000.0 * std::pow (0.025, (double) damp));
        targetWidth = std::clamp (widthAmount, 0.0f, 1.0f);
        targetWet = std::clamp (mix, 0.0f, 1.0f);
        targetDry = 1.0f - targetWet;
    }

    // right may be null for a mono bus, in which case only left is written.
    void process (float* left, float* right, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            for (int i = 0; i < lines; ++i)
            {
                distance[i] += (double) smoothing * (targetDistance[i] - distance[i]);
                gain[i] += smoothing * (targetGain[i] - gain[i]);
            }
            damping += smoothing * (targetDamping - damping);
            wet += smoothing * (targetWet - wet);
            dry += smoothing * (targetDry - dry);
            width += smoothing * (targetWidth - width);

            const float inputLeft = left[n];
            const float inputRight = right != nullptr ? right[n] : inputLeft;
            float diffusedLeft = inputLeft, diffusedRight = inputRight;
            for (int d = 0; d < diffusers; ++d)
            {
                diffusedLeft = diffuser[0][d].process (diffusedLeft, diffuserGain[d]);
                diffusedRight = diffuser[1][d].process (diffusedRight, diffuserGain[d]);
            }

            float tap[lines], node[lines];
            for (int i = 0; i < lines; ++i)
            {
                float value = delay[i].read (distance[i] + modulationSamples * modulator[i].next());
                lowpassState[i] += damping * (value - lowpassState[i]);
                value = lowpassState[i];
                // Matching low cut in every line. Without it the network
                // integrates any DC or sub-bass the synth hands it, and a long
                // decay slowly inflates into rumble.
                highpassState[i] += lowCut * (value - highpassState[i]);
                tap[i] = value - highpassState[i];
                node[i] = tap[i] * gain[i];
            }
            hadamard (node);
            for (int i = 0; i < lines; ++i)
                delay[i].write (node[i] + injection * ((i & 1) == 0 ? diffusedLeft : diffusedRight));

            // Alternating signs decorrelate the two taps without needing a
            // second network: each channel sums four lines that the other
            // channel subtracts.
            float wetLeft = outputTrim * (tap[0] - tap[2] + tap[4] - tap[6]);
            float wetRight = outputTrim * (tap[1] - tap[3] + tap[5] - tap[7]);
            const float mid = 0.5f * (wetLeft + wetRight);
            const float side = 0.5f * (wetLeft - wetRight) * width;
            wetLeft = mid + side;
            wetRight = mid - side;

            left[n] = inputLeft * dry + wetLeft * wet;
            if (right != nullptr) right[n] = inputRight * dry + wetRight * wet;
        }
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    // 1/sqrt(8) in, and enough out to land the wet level near the dry one.
    static constexpr float injection = 0.35355339f, outputTrim = 0.35f;

    float onePole (double hz) const
    {
        return (float) (1.0 - std::exp (-2.0 * pi * std::min (hz, 0.45 * sampleRate) / sampleRate));
    }

    // Orthonormal 8x8 Hadamard, in place, as a fast Walsh-Hadamard transform:
    // 24 adds rather than the 64 multiply-accumulates of the explicit matrix.
    // Orthonormal means the mixing itself is lossless, so the per-line gains
    // alone decide the decay and the network cannot run away.
    static void hadamard (float (&v)[lines])
    {
        for (int stride = 1; stride < lines; stride <<= 1)
            for (int start = 0; start < lines; start += stride << 1)
                for (int i = start; i < start + stride; ++i)
                {
                    const float a = v[i], b = v[i + stride];
                    v[i] = a + b;
                    v[i + stride] = a - b;
                }
        for (auto& x : v) x *= 0.35355339059327373f;
    }

    // Millisecond lengths chosen so no line is a simple ratio of another; the
    // network's modes stay spread rather than piling onto shared harmonics.
    static constexpr double baseDelayMs[lines] =
        { 22.1, 28.3, 33.7, 39.1, 45.3, 51.7, 58.9, 66.3 };
    static constexpr double modulationHz[lines] =
        { 0.11, 0.17, 0.23, 0.31, 0.41, 0.53, 0.61, 0.73 };
    static constexpr double diffuserMs[diffusers] = { 4.76, 3.58, 12.73, 9.30 };
    static constexpr float diffuserGain[diffusers] = { 0.75f, 0.75f, 0.625f, 0.625f };
    static constexpr double modulationSeconds = 0.00018;

    FractionalDelay delay[lines];
    SchroederAllpass diffuser[2][diffusers];
    SlowSine modulator[lines];
    double sampleRate = 44100.0, modulationSamples = 8.0;
    double distance[lines] {}, targetDistance[lines] {};
    float gain[lines] {}, targetGain[lines] {};
    float lowpassState[lines] {}, highpassState[lines] {};
    float smoothing = 0.001f, lowCut = 0.002f;
    float damping = 0.5f, targetDamping = 0.5f;
    float wet = 0.0f, targetWet = 0.0f, dry = 1.0f, targetDry = 1.0f;
    float width = 1.0f, targetWidth = 1.0f;
};
}
