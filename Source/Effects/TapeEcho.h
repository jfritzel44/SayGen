#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "DelayPrimitives.h"

namespace syngen
{
// Stereo feedback echo with a glided delay time.
//
// Three things separate this from the plain delay line it replaces. The read
// distance is smoothed rather than jumped, so moving the time control glides
// between settings the way tape does instead of clicking. The damping control
// is mapped to a filter corner instead of being used as the one-pole
// coefficient directly, which previously meant that "no damping" set the
// coefficient to zero and stalled the feedback filter at silence, cutting the
// repeats off after one. And the loop carries a low cut and a soft saturator,
// so long feedback settings settle into a steady, slightly compressed tail
// rather than pumping up whatever sub-bass and DC the synth handed them.
class TapeEcho
{
public:
    void prepare (double rate, int channels)
    {
        sampleRate = rate > 0.0 ? rate : 44100.0;
        const int count = std::max (1, channels);
        line.resize ((size_t) count);
        state.assign ((size_t) count, State {});
        for (auto& l : line) l.prepare ((int) (maximumSeconds * sampleRate) + 8);
        lowCut = onePole (70.0);
        // ~80 ms: fast enough to feel responsive on the knob, slow enough that
        // the pitch glide reads as tape rather than as a jump.
        timeSmoothing = 1.0 - std::exp (-1.0 / (0.08 * sampleRate));
        gainSmoothing = (float) (1.0 - std::exp (-1.0 / (0.02 * sampleRate)));
        distance = targetDistance = 0.3 * sampleRate;
        damping = targetDamping = onePole (5000.0);
        feedback = targetFeedback = 0.0f;
        mix = targetMix = 0.0f;
        reset();
    }

    void reset()
    {
        for (auto& l : line) l.reset();
        for (auto& s : state) s = State {};
    }

    void setParameters (float seconds, float feedbackAmount, float damp, float mixAmount)
    {
        targetDistance = std::clamp ((double) seconds, 0.001, maximumSeconds) * sampleRate;
        targetFeedback = std::clamp (feedbackAmount, 0.0f, 0.99f);
        // 0 leaves the repeats as bright as the input; 1 pulls the in-loop
        // corner down to 400 Hz, so each pass is audibly darker than the last.
        targetDamping = onePole (20000.0 * std::pow (0.02, (double) std::clamp (damp, 0.0f, 1.0f)));
        targetMix = std::clamp (mixAmount, 0.0f, 1.0f);
    }

    void process (float* const* channelData, int numChannels, int numSamples)
    {
        const int count = std::min (numChannels, (int) line.size());
        for (int n = 0; n < numSamples; ++n)
        {
            distance += timeSmoothing * (targetDistance - distance);
            damping += gainSmoothing * (targetDamping - damping);
            feedback += gainSmoothing * (targetFeedback - feedback);
            mix += gainSmoothing * (targetMix - mix);
            for (int ch = 0; ch < count; ++ch)
            {
                auto& s = state[(size_t) ch];
                const float input = channelData[ch][n];
                const float tap = line[(size_t) ch].read (distance);
                s.lowpass += damping * (tap - s.lowpass);
                s.highpass += lowCut * (s.lowpass - s.highpass);
                line[(size_t) ch].write (input + saturate ((s.lowpass - s.highpass) * feedback));
                channelData[ch][n] = input + (tap - input) * mix;
            }
        }
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    static constexpr double maximumSeconds = 2.0;

    // Odd, unit slope at zero, saturating to exactly +/-1 by |x| >= 3, so
    // quiet repeats are untouched and a runaway feedback setting still lands
    // on a bounded tail instead of clipping into the output stage.
    static float saturate (float x)
    {
        const float b = std::clamp (x, -3.0f, 3.0f);
        const float squared = b * b;
        return b * (27.0f + squared) / (27.0f + 9.0f * squared);
    }

    float onePole (double hz) const
    {
        return (float) (1.0 - std::exp (-2.0 * pi * std::min (hz, 0.45 * sampleRate) / sampleRate));
    }

    struct State { float lowpass = 0.0f, highpass = 0.0f; };
    std::vector<FractionalDelay> line;
    std::vector<State> state;
    double sampleRate = 44100.0, distance = 0.0, targetDistance = 0.0, timeSmoothing = 0.001;
    float gainSmoothing = 0.001f, lowCut = 0.01f;
    float damping = 0.5f, targetDamping = 0.5f;
    float feedback = 0.0f, targetFeedback = 0.0f;
    float mix = 0.0f, targetMix = 0.0f;
};
}
