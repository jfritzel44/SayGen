#pragma once
#include <JuceHeader.h>

struct MySynthSound : public juce::SynthesiserSound
{
    MySynthSound() {}
    bool appliesToNote    (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

struct MySynthVoice : public juce::SynthesiserVoice
{
    std::atomic<int>*   oscType        = nullptr;
    std::atomic<int>*   osc2Type       = nullptr;  // 0 = off, 1..4 = waveform
    std::atomic<int>*   osc1Octave     = nullptr;  // 0=16', 1=8', 2=4', 3=2'
    std::atomic<int>*   osc2Octave     = nullptr;
    std::atomic<float>* pitchSemitones = nullptr;
    std::atomic<float>* attackSeconds  = nullptr;
    std::atomic<float>* decaySeconds   = nullptr;
    std::atomic<float>* sustainLevel   = nullptr;
    std::atomic<float>* releaseSeconds = nullptr;
    std::atomic<float>* cutoffHz       = nullptr;
    std::atomic<float>* resonanceQ     = nullptr;
    std::atomic<float>* detuneCents    = nullptr;  // osc 2 detune
    std::atomic<float>* envAmountOct   = nullptr;  // filter env sweep, in octaves
    std::atomic<float>* fltAttack      = nullptr;
    std::atomic<float>* fltDecay       = nullptr;
    std::atomic<float>* fltSustain     = nullptr;
    std::atomic<float>* fltRelease     = nullptr;

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<MySynthSound*>(s) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int) override
    {
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        amplitude = velocity;
        phase  = 0.0;
        phase2 = 0.0;

        // Per-voice analog-style drift: each note lands a hair off pitch
        // (up to +/- 2.5 cents), like mismatched voice cards on a polysynth
        drift = std::pow (2.0, (juce::Random::getSystemRandom().nextDouble() * 5.0 - 2.5) / 1200.0);

        adsr.setSampleRate (getSampleRate());
        adsr.setParameters ({ attackSeconds  ? attackSeconds->load()  : 0.01f,
                              decaySeconds   ? decaySeconds->load()   : 0.1f,
                              sustainLevel   ? sustainLevel->load()   : 0.7f,
                              releaseSeconds ? releaseSeconds->load() : 0.05f });
        adsr.noteOn();

        filterEnv.setSampleRate (getSampleRate());
        filterEnv.setParameters ({ fltAttack  ? fltAttack->load()  : 0.005f,
                                   fltDecay   ? fltDecay->load()   : 0.25f,
                                   fltSustain ? fltSustain->load() : 0.2f,
                                   fltRelease ? fltRelease->load() : 0.1f });
        filterEnv.noteOn();

        filter.prepare ({ getSampleRate(), 512, 1 });
        filter.setType (juce::dsp::StateVariableTPTFilter<float>::Type::lowpass);
        filter.reset();

        active = true;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();  // keep rendering until the release tail finishes
            filterEnv.noteOff();
        }
        else
        {
            adsr.reset();
            filterEnv.reset();
            active = false;
            clearCurrentNote();
        }
    }

    void renderNextBlock (juce::AudioBuffer<float>& buffer,
                          int startSample, int numSamples) override
    {
        if (!active) return;

        const double twoPi = juce::MathConstants<double>::twoPi;

        const double semitones  = pitchSemitones ? (double) pitchSemitones->load() : 0.0;
        const double multiplier = std::pow (2.0, semitones / 12.0);
        const auto baseDelta = twoPi * frequency * multiplier * drift / getSampleRate();

        // Octave range switches, Little Phatty style: 16' is an octave below
        // the played note, 8' is unison, 4' and 2' one and two octaves up
        const double range1 = std::exp2 ((osc1Octave ? osc1Octave->load() : 1) - 1.0);
        const double range2 = std::exp2 ((osc2Octave ? osc2Octave->load() : 1) - 1.0);
        auto phaseDelta = baseDelta * range1;

        const double detune = detuneCents ? (double) detuneCents->load() : 0.0;
        auto phaseDelta2 = baseDelta * range2 * std::pow (2.0, detune / 1200.0);

        const float maxCutoff  = (float) (getSampleRate() * 0.45);
        const float baseCutoff = juce::jlimit (20.0f, maxCutoff,
                                               cutoffHz ? cutoffHz->load() : 20000.0f);
        const float envOctaves = envAmountOct ? envAmountOct->load() : 0.0f;
        filter.setResonance (resonanceQ ? resonanceQ->load() : 0.707f);

        int type  = oscType  ? oscType->load()  : 0;
        int type2 = osc2Type ? osc2Type->load() : 0;  // 0 = off

        const double dt1 = phaseDelta  / twoPi;
        const double dt2 = phaseDelta2 / twoPi;

        for (int i = startSample; i < startSample + numSamples; ++i)
        {
            double sample = waveSample (type, phase / twoPi, dt1);
            if (type2 > 0)
                sample += waveSample (type2 - 1, phase2 / twoPi, dt2);

            // Filter env sweeps the cutoff up/down by envOctaves at full swing
            filter.setCutoffFrequency (juce::jlimit (20.0f, maxCutoff,
                baseCutoff * (float) std::exp2 (envOctaves * filterEnv.getNextSample())));

            auto out = (float)(sample * amplitude * 0.3) * adsr.getNextSample();
            out = filter.processSample (0, out);
            out = std::tanh (1.5f * out) / 1.5f;  // gentle analog-style rounding
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.addSample (ch, i, out);

            phase  = std::fmod (phase  + phaseDelta,  twoPi);
            phase2 = std::fmod (phase2 + phaseDelta2, twoPi);
        }

        if (! adsr.isActive())
        {
            active = false;
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

private:
    // Smooths a waveform discontinuity over one sample either side so it
    // doesn't alias. t = normalised phase [0,1), dt = increment per sample.
    static double polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            auto x = t / dt;
            return x + x - x * x - 1.0;
        }
        if (t > 1.0 - dt)
        {
            auto x = (t - 1.0) / dt;
            return x * x + x + x + 1.0;
        }
        return 0.0;
    }

    static double waveSample (int type, double t, double dt)
    {
        switch (type)
        {
            case 1:  return (1.0 - 2.0 * t) + polyBlep (t, dt);   // Sawtooth
            case 2:  return (t < 0.5 ? 1.0 : -1.0)                 // Square
                        + polyBlep (t, dt)
                        - polyBlep (std::fmod (t + 0.5, 1.0), dt);
            case 3:  return t < 0.5                                // Triangle
                        ? (4.0 * t - 1.0)
                        : (3.0 - 4.0 * t);
            default: return std::sin (t * juce::MathConstants<double>::twoPi);
        }
    }

    juce::ADSR adsr;
    juce::ADSR filterEnv;
    juce::dsp::StateVariableTPTFilter<float> filter;
    double frequency = 440.0;
    double amplitude = 0.0;
    double phase     = 0.0;
    double phase2    = 0.0;
    double drift     = 1.0;
    bool   active    = false;
};
