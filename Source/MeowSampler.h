#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

// Only spectral envelopes are retained. No recorded waveform reaches the output.
// Analysis happens at construction, never on the audio thread.
class SelectableMeowSound : public juce::SynthesiserSound
{
public:
    static constexpr int bands = 64, fftSize = 1024, hop = 256;
    using Frame = std::array<float, bands>;
    SelectableMeowSound (juce::AudioFormatReader& reader, int index,
                         std::atomic<float>& selection)
        : id (index), selected (selection), rate (reader.sampleRate)
    {
        const int length = (int) juce::jmin (reader.lengthInSamples,
                                            (juce::int64) (rate * 5.0));
        juce::AudioBuffer<float> source (1, length);
        reader.read (&source, 0, length, 0, true, false);
        duration = length / rate;
        juce::dsp::FFT fft (10);
        std::array<float, fftSize * 2> data {};
        for (int centre = 0; centre < length + hop; centre += hop)
        {
            data.fill (0);
            for (int n = 0; n < fftSize; ++n)
            {
                const int position = centre + n - fftSize / 2;
                if (position >= 0 && position < length)
                    data[(size_t) n] = source.getSample (0, position)
                        * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * n / (fftSize - 1)));
            }
            fft.performFrequencyOnlyForwardTransform (data.data());
            Frame frame {};
            for (int band = 0; band < bands; ++band)
            {
                const float frequency = bandFrequency (band);
                const int lo = juce::jlimit (1, fftSize / 2, (int) (frequency / 1.12f * fftSize / rate));
                const int hi = juce::jlimit (lo, fftSize / 2, (int) (frequency * 1.12f * fftSize / rate));
                float energy = 0;
                for (int bin = lo; bin <= hi; ++bin)
                    energy += data[(size_t) bin] * data[(size_t) bin];
                frame[(size_t) band] = std::sqrt (energy / (hi - lo + 1)) * (4.0f / fftSize);
            }
            frames.push_back (frame);
        }
        frames.push_back (Frame {});
    }
    bool appliesToNote (int) override { return (int) selected.load() == id; }
    bool appliesToChannel (int) override { return true; }
    static float bandFrequency (int band) { return 80.0f * std::pow (200.0f, band / float (bands - 1)); }
    float magnitude (double seconds, double frequency) const
    {
        if (frames.empty() || frequency < 80 || frequency > juce::jmin (16000.0, rate * 0.48)) return 0;
        const double time = seconds * rate / hop;
        const int a = juce::jlimit (0, (int) frames.size() - 2, (int) time);
        const float t = juce::jlimit (0.0f, 1.0f, (float) (time - a));
        const float band = (float) (std::log (frequency / 80.0) / std::log (200.0) * (bands - 1));
        const int b = juce::jlimit (0, bands - 2, (int) band);
        const float f = band - b;
        const auto interpolate = [b, f] (const Frame& frame)
        { return frame[(size_t) b] + f * (frame[(size_t) b + 1] - frame[(size_t) b]); };
        const float first = interpolate (frames[(size_t) a]);
        return first + t * (interpolate (frames[(size_t) a + 1]) - first);
    }
    double duration = 0;
private:
    int id;
    std::atomic<float>& selected;
    double rate;
    std::vector<Frame> frames;
};

class MeowSynthesisVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override
    { return dynamic_cast<SelectableMeowSound*> (sound) != nullptr; }
    void startNote (int note, float velocity, juce::SynthesiserSound*, int wheel) override
    {
        fundamental = juce::MidiMessage::getMidiNoteInHertz (note);
        gain = velocity; elapsed = phase = 0; released = false; releaseGain = 1;
        amplitudes.fill (0); increments.fill (0); remaining = 0;
        pitchWheelMoved (wheel);
    }
    void stopNote (float, bool tail) override
    { if (tail) released = true; else clearCurrentNote(); }
    void pitchWheelMoved (int value) override
    { bend = std::pow (2.0, (value - 8192) / 8192.0 * 2.0 / 12.0); remaining = 0; }
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& buffer, int start, int count) override
    {
        auto* sound = dynamic_cast<SelectableMeowSound*> (getCurrentlyPlayingSound().get());
        if (sound == nullptr) return;
        const double sampleRate = getSampleRate();
        const double frequency = fundamental * bend;
        for (int n = 0; n < count; ++n)
        {
            if (elapsed >= sound->duration || releaseGain <= 0)
            { clearCurrentNote(); break; }
            if (remaining == 0)
            {
                remaining = 32;
                for (int h = 0; h < harmonics; ++h)
                {
                    const double hz = frequency * (h + 1);
                    const float target = hz < sampleRate * 0.45
                        ? sound->magnitude (elapsed + 32.0 / sampleRate, hz) : 0;
                    increments[(size_t) h] = (target - amplitudes[(size_t) h]) / 32;
                }
            }
            // Recurrence generates the harmonic sine bank with two trig calls.
            const float sine = (float) std::sin (phase), cosine = (float) std::cos (phase);
            float s = sine, c = cosine, output = 0;
            for (int h = 0; h < harmonics; ++h)
            {
                amplitudes[(size_t) h] += increments[(size_t) h];
                if (frequency * (h + 1) < sampleRate * 0.45)
                    output += amplitudes[(size_t) h] * s;
                const float next = s * cosine + c * sine;
                c = c * cosine - s * sine; s = next;
            }
            --remaining;
            const float attack = (float) juce::jmin (1.0, elapsed / 0.005);
            const float endFade = (float) juce::jlimit (0.0, 1.0, (sound->duration - elapsed) / 0.005);
            output *= gain * attack * endFade * releaseGain;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.addSample (channel, start + n, output);
            phase += juce::MathConstants<double>::twoPi * frequency / sampleRate;
            phase = std::fmod (phase, juce::MathConstants<double>::twoPi);
            elapsed += 1.0 / sampleRate;
            if (released) releaseGain = juce::jmax (0.0f, releaseGain - (float) (1.0 / (0.08 * sampleRate)));
        }
    }
private:
    static constexpr int harmonics = 64;
    std::array<float, harmonics> amplitudes {}, increments {};
    double fundamental = 261.625565, bend = 1, phase = 0, elapsed = 0;
    float gain = 0, releaseGain = 1;
    bool released = false;
    int remaining = 0;
};
