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
    std::atomic<int>* osc1PhaseMode = nullptr;
    std::atomic<int>* osc2PhaseMode = nullptr;
    std::atomic<float>* osc1StartPhase = nullptr;
    std::atomic<float>* osc2StartPhase = nullptr;
    std::atomic<float>* phaseRandomness = nullptr;
    std::atomic<float>* unisonWidth = nullptr;
    std::atomic<float>* osc2Coarse = nullptr;
    std::atomic<float>* osc1Level = nullptr;
    std::atomic<float>* osc2Level = nullptr;
    std::atomic<float>* filterCompensation = nullptr;
    std::atomic<float>* envelopeCurve = nullptr;
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
    std::atomic<float>* driftAmount    = nullptr;  // 0 = perfectly stable/no drift, 1 = full drift (default)
    std::atomic<int>*   osc1UnisonVoices = nullptr;  // 1..kMaxUnisonVoices stacked detuned copies of osc 1
    std::atomic<int>*   osc2UnisonVoices = nullptr;  // 1..kMaxUnisonVoices stacked detuned copies of osc 2
    std::atomic<float>* osc1UnisonDetune = nullptr;  // cents, spread of the outer osc 1 unison voices from centre
    std::atomic<float>* osc2UnisonDetune = nullptr;  // cents, spread of the outer osc 2 unison voices from centre

    // Modern mode: when on, that oscillator
    // ignores oscType/osc2Type's single-waveform pick and instead blends
    // continuous saw/pulse/triangle amounts, plus an
    // optional square sub-oscillator one octave down, phase-locked to the
    // main oscillator (like an analog flip-flop divider off its own edge).
    std::atomic<bool>*  osc1ModernOn    = nullptr;
    std::atomic<float>* osc1SawMix      = nullptr;
    std::atomic<float>* osc1PulseMix    = nullptr;
    std::atomic<float>* osc1TriMix      = nullptr;
    std::atomic<float>* osc1PulseWidth  = nullptr;
    std::atomic<bool>*  osc1SubOctave   = nullptr;
    std::atomic<bool>*  osc2ModernOn    = nullptr;
    std::atomic<float>* osc2SawMix      = nullptr;
    std::atomic<float>* osc2PulseMix    = nullptr;
    std::atomic<float>* osc2TriMix      = nullptr;
    std::atomic<float>* osc2PulseWidth  = nullptr;
    std::atomic<bool>*  osc2SubOctave   = nullptr;

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<MySynthSound*> (sound) != nullptr;
    }

    // For reproducible offline rendering; never changes JUCE's global RNG.
    void setOscillatorSeed (uint32_t seed)
    {
        motionSeed = seed != 0 ? seed : 1;
        random.setSeed (motionSeed);
        motionRate = 0;
        phaseInitialised = false;
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        renderRate = getSampleRate() * 4;
        if (motionRate != renderRate)
        {
            motionRate = renderRate;
            commonDrift.prepare (renderRate, motionSeed);
            for (int bank = 0; bank < 2; ++bank)
                for (int u = 0; u < kMaxUnisonVoices; ++u)
                    oscillators[bank][u].drift.prepare (renderRate, motionSeed + 7919u * (1 + bank * 7 + u));
        }
        glide.reset (juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber),
                     shapeVelocity (velocity, read (velocityCurve, 0)));
        count[0] = juce::jlimit (1, kMaxUnisonVoices, read (osc1UnisonVoices, 1));
        count[1] = juce::jlimit (1, kMaxUnisonVoices, read (osc2UnisonVoices, 1));
        phaseMode[0] = read (osc1PhaseMode, 0);
        phaseMode[1] = read (osc2PhaseMode, 0);
        const double startPhase[] = { read (osc1StartPhase, 0) / 360.0, read (osc2StartPhase, 90) / 360.0 };
        const double randomness = read (phaseRandomness, 1);
        for (int bank = 0; bank < 2; ++bank)
        {
            gain[bank] = 1.0f / std::sqrt ((float) count[bank]);
            for (int u = 0; u < kMaxUnisonVoices; ++u)
            {
                auto& osc = oscillators[bank][u];
                osc.spread = count[bank] <= 1 ? 0.0 : 2.0 * u / (count[bank] - 1) - 1.0;
                if (phaseMode[bank] != 2 || ! phaseInitialised)
                {
                    const double offset = phaseMode[bank] == 1 ? randomness * random.nextDouble() : 0;
                    const double phase = startPhase[bank] + (double) u / count[bank] + offset;
                    osc.wave.setPhase (phase, phase * 0.5);
                }
                // A muted interval advances phase without computing BLEP tails.
                osc.wave.clearCorrection();
                osc.detune.reset (renderRate, 0.01);
            }
            for (auto& mix : waveMix[bank]) mix.reset (renderRate, 0.005);
            width[bank].reset (renderRate, 0.005);
            tuning[bank].reset (renderRate, 0.005);
            level[bank].reset (renderRate, 0.01);
            cachedTuning[bank] = std::numeric_limits<double>::quiet_NaN();
        }
        phaseInitialised = true;
        stereoWidth.reset (renderRate, 0.01);
        driftDepth.reset (renderRate, 0.02);
        cutoffSmooth.reset (renderRate, 0.005);
        resonanceSmooth.reset (renderRate, 0.01);
        driveSmooth.reset (renderRate, 0.01);
        compensationSmooth.reset (renderRate, 0.01);
        envAmountSmooth.reset (renderRate, 0.005);
        trackingSmooth.reset (renderRate, 0.005);
        updateControls (true);
        cachedWidth = -1;
        updatePan();
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
        if (! active)
        {
            advanceIdle (numSamples);
            return;
        }
        updateControls (false);
        updateEnvelopes();
        advanceUnusedDrift (numSamples * 4);
        const bool mono = count[0] == 1 && count[1] == 1;
        const double maxCutoff = getSampleRate() * 0.45;
        for (int n = 0; n < numSamples * 4; ++n)
        {
            updateMotion();
            updatePan();
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
                const double pulseWidth = width[bank].getNextValue();
                const float bankGain = gain[bank] * (float) level[bank].getNextValue();
                for (int u = 0; u < count[bank]; ++u)
                {
                    auto& osc = oscillators[bank][u];
                    const float sample = (float) osc.wave.next (step[bank][u], mix, pulseWidth,
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
            const double compensation = compensationSmooth.getNextValue();
            const float vca = ampEnvelope.next() * (float) glide.getLevel() / std::sqrt (drive);
            float outL = ladder[0].process (mixed[0] * 0.3f * drive, G, resonance, compensation) * vca;
            float outR = mono ? outL : ladder[1].process (mixed[1] * 0.3f * drive, G, resonance, compensation) * vca;
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
                    advanceIdle (numSamples - n / 4 - 1, false);
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
        const double detune[] = { read (osc1UnisonDetune, 14), read (osc2UnisonDetune, 14) };
        target (tuning[0], pitch + 12 * (read (osc1Octave, 1) - 1), initialise);
        target (tuning[1], pitch + 12 * (read (osc2Octave, 1) - 1)
                          + read (osc2Coarse, 0) + read (detuneCents, 0) * 0.01, initialise);
        target (level[0], read (osc1Level, 0.75f), initialise);
        target (level[1], read (osc2Level, 0.75f), initialise);
        target (stereoWidth, read (unisonWidth, 0.9f), initialise);
        target (driftDepth, read (driftAmount, 1), initialise);
        target (cutoffSmooth, read (cutoffHz, 20000), initialise);
        target (resonanceSmooth, read (resonanceQ, 0.707f), initialise);
        target (driveSmooth, read (overloadAmount, 0), initialise);
        target (compensationSmooth, read (filterCompensation, 0.5f), initialise);
        target (envAmountSmooth, read (envAmountOct, 0), initialise);
        target (trackingSmooth, read (kbTrackAmount, 0), initialise);
        const bool modern[] = { read (osc1ModernOn, false), read (osc2ModernOn, false) };
        const int wave[] = { read (oscType, 0), read (osc2Type, 0) - 1 };
        const double saw[] = { read (osc1SawMix, 1), read (osc2SawMix, 1) };
        const double pulse[] = { read (osc1PulseMix, 0), read (osc2PulseMix, 0) };
        const double triangle[] = { read (osc1TriMix, 0), read (osc2TriMix, 0) };
        const bool sub[] = { read (osc1SubOctave, false), read (osc2SubOctave, false) };
        const double pw[] = { read (osc1PulseWidth, 0.5f), read (osc2PulseWidth, 0.5f) };
        for (int bank = 0; bank < 2; ++bank)
        {
            double weights[5] {};
            if (modern[bank]) { weights[1] = saw[bank]; weights[2] = pulse[bank]; weights[3] = triangle[bank]; weights[4] = sub[bank] ? 0.6 : 0; }
            else if (wave[bank] >= 0 && wave[bank] <= 3) weights[wave[bank]] = 1;
            for (int w = 0; w < 5; ++w) target (waveMix[bank][w], weights[w], initialise);
            target (width[bank], modern[bank] ? pw[bank] : 0.5, initialise);
            for (int u = 0; u < count[bank]; ++u)
                target (oscillators[bank][u].detune, std::exp2 (oscillators[bank][u].spread * detune[bank] / 1200), initialise);
        }
    }
    void updateEnvelopes()
    {
        const double curve = read (envelopeCurve, 0.65f);
        ampEnvelope.setParameters (read (attackSeconds, 0.01f), read (decaySeconds, 0.1f),
                                   read (sustainLevel, 0.7f), read (releaseSeconds, 0.05f), curve);
        filterEnvelope.setParameters (read (fltAttack, 0.005f), read (fltDecay, 0.25f),
                                      read (fltSustain, 0.2f), read (fltRelease, 0.1f), curve);
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
    void updatePan()
    {
        const double widthNow = stereoWidth.getNextValue();
        if (widthNow == cachedWidth) return;
        cachedWidth = widthNow;
        for (int bank = 0; bank < 2; ++bank)
            for (int u = 0; u < count[bank]; ++u)
            {
                auto& osc = oscillators[bank][u];
                const double angle = (osc.spread * widthNow + 1) * juce::MathConstants<double>::pi * 0.25;
                osc.panLeft = count[bank] == 1 ? 1.0f : (float) std::cos (angle);
                osc.panRight = count[bank] == 1 ? 1.0f : (float) std::sin (angle);
            }
    }
    void oscillatorSteps (double (&steps)[2][kMaxUnisonVoices])
    {
        const double common = 0.3 * commonDrift.next();
        const double amount = driftDepth.getNextValue();
        for (int bank = 0; bank < 2; ++bank)
            for (int u = 0; u < count[bank]; ++u)
            {
                auto& osc = oscillators[bank][u];
                const double cents = amount * (common + 2.0 * osc.drift.next());
                // exp(x), within 3e-10 relative error over this bounded drift.
                const double x = cents * (0.6931471805599453 / 1200);
                const double driftRatio = 1 + x * (1 + 0.5 * x);
                steps[bank][u] = juce::jlimit (1e-9, 0.45,
                    glide.getFrequency() * tuningRatio[bank] * osc.detune.getNextValue() * driftRatio / renderRate);
            }
    }
    void advanceUnusedDrift (int samples)
    {
        for (int bank = 0; bank < 2; ++bank)
            for (int u = count[bank]; u < kMaxUnisonVoices; ++u)
                oscillators[bank][u].drift.advance (samples);
    }
    void advanceIdle (int hostSamples, bool advanceUnused = true)
    {
        if (! phaseInitialised || hostSamples <= 0) return;
        const int samples = hostSamples * 4;
        if (phaseMode[0] != 2 && phaseMode[1] != 2)
        {
            commonDrift.advance (samples);
            for (int bank = 0; bank < 2; ++bank)
                for (int u = 0; u < (advanceUnused ? kMaxUnisonVoices : count[bank]); ++u)
                    oscillators[bank][u].drift.advance (samples);
            return;
        }
        if (advanceUnused) advanceUnusedDrift (samples);
        for (int n = 0; n < samples; ++n)
        {
            updateMotion();
            double steps[2][kMaxUnisonVoices] {};
            oscillatorSteps (steps);
            const double syncTime = syncEnabled && oscillators[0][0].wave.getPhase() + steps[0][0] >= 1
                ? (1 - oscillators[0][0].wave.getPhase()) / steps[0][0] : -1;
            for (int bank = 0; bank < 2; ++bank)
                for (int u = 0; u < count[bank]; ++u)
                    oscillators[bank][u].wave.advance (steps[bank][u], bank == 1 ? syncTime : -1);
        }
    }

    struct Oscillator
    {
        syngen::BandlimitedOscillator wave;
        syngen::SmoothDrift drift;
        juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> detune;
        double spread = 0;
        float panLeft = 1, panRight = 1;
    } oscillators[2][kMaxUnisonVoices];
    syngen::SmoothDrift commonDrift;
    syngen::PitchGlide glide;
    juce::Random random;
    uint32_t motionSeed = (uint32_t) random.nextInt();
    double renderRate = 176400, motionRate = 0;
    bool active = false, phaseInitialised = false, syncEnabled = false;
    int count[2] { 1, 1 }, phaseMode[2] {}, tailSamples = 33;
    float gain[2] { 1, 1 };
    double cachedTuning[2] {}, tuningRatio[2] { 1, 1 }, cachedWidth = -1;
    juce::SmoothedValue<double> waveMix[2][5], width[2], tuning[2], level[2];
    juce::SmoothedValue<double> stereoWidth, driftDepth, resonanceSmooth, driveSmooth, compensationSmooth;
    juce::SmoothedValue<double> envAmountSmooth, trackingSmooth;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> cutoffSmooth;
    syngen::FeedbackLadder ladder[2];
    syngen::Decimator4 decimator[2];
    syngen::CurveEnvelope ampEnvelope, filterEnvelope;
};
