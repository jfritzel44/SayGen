#pragma once
#include <JuceHeader.h>
#include "../VelocityCurve.h"
#include "EnhancedDSP.h"
#include "OscillatorMotion.h"

struct MySynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

struct MySynthVoice : public juce::SynthesiserVoice
{
    static constexpr int kMaxUnisonVoices = 7;
    // Fixed unison spread/width and per-oscillator level/bass/curve, matching
    // this synth's previous defaults now that they're no longer user-adjustable.
    static constexpr double kUnisonDetuneCents = 14.0;
    static constexpr double kUnisonWidth = 0.9;
    static constexpr float  kOscLevel = 0.75f;
    static constexpr double kFilterCompensation = 0.5;
    static constexpr double kEnvelopeCurve = 0.65;

    std::atomic<int>*   oscType        = nullptr;
    std::atomic<int>*   osc2Type       = nullptr;  // 0 = off, 1..4 = waveform
    std::atomic<int>*   osc1Octave     = nullptr;  // 0=16', 1=8', 2=4', 3=2'
    std::atomic<int>*   osc2Octave     = nullptr;
    std::atomic<bool>*  oscSync        = nullptr;  // hard-sync osc 2 to osc 1, Little Phatty style
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
    std::atomic<float>* overloadAmount = nullptr;  // pre-filter drive, Little Phatty "Overload" style
    std::atomic<float>* kbTrackAmount  = nullptr;  // filter keyboard tracking, 0 = none, 1 = 1:1 with pitch
    std::atomic<float>* velocityCurve  = nullptr;  // 0 = linear, + boosts soft notes, - suppresses them
    std::atomic<float>* pitchBend      = nullptr;  // current pitch-wheel offset, in semitones
    std::atomic<int>*   osc1UnisonVoices = nullptr;  // 1..kMaxUnisonVoices stacked detuned copies of osc 1
    std::atomic<int>*   osc2UnisonVoices = nullptr;  // 1..kMaxUnisonVoices stacked detuned copies of osc 2

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<MySynthSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        renderRate = getSampleRate() * 4;
        glide.reset (juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber),
                     shapeVelocity (velocity, read (velocityCurve, 0)));
        count[0] = juce::jlimit (1, kMaxUnisonVoices, read (osc1UnisonVoices, 1));
        count[1] = juce::jlimit (1, kMaxUnisonVoices, read (osc2UnisonVoices, 1));
        static constexpr double startPhase[] = { 0.0, 0.25 };
        for (int bank = 0; bank < 2; ++bank)
        {
            gain[bank] = kOscLevel / std::sqrt ((float) count[bank]);
            for (int u = 0; u < kMaxUnisonVoices; ++u)
            {
                auto& osc = oscillators[bank][u];
                osc.spread = count[bank] <= 1 ? 0.0 : 2.0 * u / (count[bank] - 1) - 1.0;
                const double phase = startPhase[bank] + (double) u / count[bank];
                osc.wave.setPhase (phase, phase * 0.5);
                osc.wave.clearCorrection();
                osc.detune.reset (renderRate, 0.01);
                const double angle = (osc.spread * kUnisonWidth + 1) * juce::MathConstants<double>::pi * 0.25;
                osc.panLeft  = count[bank] == 1 ? 1.0f : (float) std::cos (angle);
                osc.panRight = count[bank] == 1 ? 1.0f : (float) std::sin (angle);
            }
            for (auto& mix : waveMix[bank]) mix.reset (renderRate, 0.005);
            tuning[bank].reset (renderRate, 0.005);
            cachedTuning[bank] = std::numeric_limits<double>::quiet_NaN();
        }
        cutoffSmooth.reset (renderRate, 0.005);
        resonanceSmooth.reset (renderRate, 0.01);
        driveSmooth.reset (renderRate, 0.01);
        envAmountSmooth.reset (renderRate, 0.005);
        trackingSmooth.reset (renderRate, 0.005);
        updateControls (true);
        for (auto& filter : ladder) filter.reset();
        for (auto& d : decimator) d.reset();
        ampEnvelope.prepare (renderRate); filterEnvelope.prepare (renderRate);
        ampEnvelope.reset(); filterEnvelope.reset();
        updateEnvelopes();
        ampEnvelope.noteOn(); filterEnvelope.noteOn();
        tailSamples = 33;
        active = true;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampEnvelope.noteOff(); filterEnvelope.noteOff();
        }
        else
        {
            ampEnvelope.reset(); filterEnvelope.reset();
            active = false;
            clearCurrentNote();
        }
    }

    void setGlideTarget (int note, float velocity, float seconds)
    {
        glide.target (juce::MidiMessage::getMidiNoteInHertz (note),
                      shapeVelocity (velocity, read (velocityCurve, 0)), seconds, renderRate);
    }

    void renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override
    {
        if (numSamples <= 0) return;
        if (! active) return;
        updateControls (false);
        updateEnvelopes();
        const bool mono = count[0] == 1 && count[1] == 1;
        const double maxCutoff = getSampleRate() * 0.45;
        for (int n = 0; n < numSamples * 4; ++n)
        {
            updateMotion();
            double step[2][kMaxUnisonVoices] {};
            oscillatorSteps (step);
            const double syncTime = syncEnabled && oscillators[0][0].wave.getPhase() + step[0][0] >= 1
                ? (1 - oscillators[0][0].wave.getPhase()) / step[0][0] : -1;
            float mixed[2] {};
            for (int bank = 0; bank < 2; ++bank)
            {
                syngen::BandlimitedOscillator::Mix mix;
                mix.sine = waveMix[bank][0].getNextValue();
                mix.saw = waveMix[bank][1].getNextValue();
                mix.pulse = waveMix[bank][2].getNextValue();
                mix.triangle = waveMix[bank][3].getNextValue();
                mix.sub = waveMix[bank][4].getNextValue();
                const float bankGain = gain[bank];
                for (int u = 0; u < count[bank]; ++u)
                {
                    auto& osc = oscillators[bank][u];
                    const float sample = (float) osc.wave.next (step[bank][u], mix, 0.5,
                                                               bank == 1 ? syncTime : -1) * bankGain;
                    mixed[0] += sample * osc.panLeft;
                    mixed[1] += sample * osc.panRight;
                }
            }
            const double tracking = trackingSmooth.getNextValue()
                * std::log2 (glide.getFrequency() / 261.6255653005986);
            const double cutoff = juce::jlimit (20.0, maxCutoff, cutoffSmooth.getNextValue()
                * std::exp2 (envAmountSmooth.getNextValue() * filterEnvelope.next() + tracking));
            const double G = syngen::FeedbackLadder::coefficient (cutoff, renderRate);
            const double resonance = (resonanceSmooth.getNextValue() - 0.5) * (4.2 / 9.5);
            const float drive = (float) (1 + 7 * driveSmooth.getNextValue());
            const float vca = ampEnvelope.next() * (float) glide.getLevel() / std::sqrt (drive);
            float outL = ladder[0].process (mixed[0] * 0.3f * drive, G, resonance, kFilterCompensation) * vca;
            float outR = mono ? outL : ladder[1].process (mixed[1] * 0.3f * drive, G, resonance, kFilterCompensation) * vca;
            decimator[0].push (outL);
            if (! mono) decimator[1].push (outR);
            if (n % 4 == 3)
            {
                outL = decimator[0].output();
                outR = mono ? outL : decimator[1].output();
                const int sample = startSample + n / 4;
                if (buffer.getNumChannels() == 1)
                    buffer.addSample (0, sample, 0.5f * (outL + outR));
                else if (buffer.getNumChannels() >= 2)
                {
                    buffer.addSample (0, sample, outL * 0.70710678f);
                    buffer.addSample (1, sample, outR * 0.70710678f);
                    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
                        buffer.addSample (ch, sample, 0.5f * (outL + outR));
                }
                if (! ampEnvelope.isActive() && --tailSamples <= 0)
                {
                    active = false;
                    clearCurrentNote();
                    return;
                }
            }
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

private:
    template <typename T> static T read (std::atomic<T>* parameter, T fallback)
    {
        return parameter ? parameter->load() : fallback;
    }
    // Float overload permits concise, integer-valued defaults for float params.
    static double read (std::atomic<float>* parameter, int fallback)
    {
        return parameter ? (double) parameter->load() : fallback;
    }
    template <typename S> static void target (S& smoother, double value, bool initialise)
    {
        if (initialise) smoother.setCurrentAndTargetValue (value);
        else smoother.setTargetValue (value);
    }
    void updateControls (bool initialise)
    {
        syncEnabled = read (oscSync, false);
        const double pitch = read (pitchSemitones, 0) + read (pitchBend, 0);
        target (tuning[0], pitch + 12 * (read (osc1Octave, 1) - 1), initialise);
        target (tuning[1], pitch + 12 * (read (osc2Octave, 1) - 1)
                          + read (detuneCents, 0) * 0.01, initialise);
        target (cutoffSmooth, read (cutoffHz, 20000), initialise);
        target (resonanceSmooth, read (resonanceQ, 0.707f), initialise);
        target (driveSmooth, read (overloadAmount, 0), initialise);
        target (envAmountSmooth, read (envAmountOct, 0), initialise);
        target (trackingSmooth, read (kbTrackAmount, 0), initialise);
        const int wave[] = { read (oscType, 0), read (osc2Type, 0) - 1 };
        for (int bank = 0; bank < 2; ++bank)
        {
            double weights[5] {};
            if (wave[bank] >= 0 && wave[bank] <= 3) weights[wave[bank]] = 1;
            for (int w = 0; w < 5; ++w) target (waveMix[bank][w], weights[w], initialise);
            for (int u = 0; u < count[bank]; ++u)
                target (oscillators[bank][u].detune, std::exp2 (oscillators[bank][u].spread * kUnisonDetuneCents / 1200), initialise);
        }
    }
    void updateEnvelopes()
    {
        ampEnvelope.setParameters (read (attackSeconds, 0.01f), read (decaySeconds, 0.1f),
                                   read (sustainLevel, 0.7f), read (releaseSeconds, 0.05f), kEnvelopeCurve);
        filterEnvelope.setParameters (read (fltAttack, 0.005f), read (fltDecay, 0.25f),
                                      read (fltSustain, 0.2f), read (fltRelease, 0.1f), kEnvelopeCurve);
    }
    void updateMotion()
    {
        glide.next();
        for (int bank = 0; bank < 2; ++bank)
        {
            const double semitones = tuning[bank].getNextValue();
            if (semitones != cachedTuning[bank])
            {
                cachedTuning[bank] = semitones;
                tuningRatio[bank] = std::exp2 (semitones / 12);
            }
        }
    }
    void oscillatorSteps (double (&steps)[2][kMaxUnisonVoices])
    {
        for (int bank = 0; bank < 2; ++bank)
            for (int u = 0; u < count[bank]; ++u)
                steps[bank][u] = juce::jlimit (1e-9, 0.45,
                    glide.getFrequency() * tuningRatio[bank] * oscillators[bank][u].detune.getNextValue() / renderRate);
    }

    struct Oscillator
    {
        syngen::BandlimitedOscillator wave;
        juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> detune;
        double spread = 0;
        float panLeft = 1, panRight = 1;
    } oscillators[2][kMaxUnisonVoices];
    syngen::PitchGlide glide;
    double renderRate = 176400;
    bool active = false, syncEnabled = false;
    int count[2] { 1, 1 }, tailSamples = 33;
    float gain[2] { 1, 1 };
    double cachedTuning[2] {}, tuningRatio[2] { 1, 1 };
    juce::SmoothedValue<double> waveMix[2][5], tuning[2];
    juce::SmoothedValue<double> resonanceSmooth, driveSmooth;
    juce::SmoothedValue<double> envAmountSmooth, trackingSmooth;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> cutoffSmooth;
    syngen::FeedbackLadder ladder[2];
    syngen::Decimator4 decimator[2];
    syngen::CurveEnvelope ampEnvelope, filterEnvelope;
};
