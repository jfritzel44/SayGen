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
    std::atomic<int>*   filterMode     = nullptr;  // syngen::FeedbackLadder::Mode
    std::atomic<float>* pwmDepth       = nullptr;  // pulse-width modulation depth, 0..1
    std::atomic<float>* pwmRate        = nullptr;  // pulse-width modulation rate, Hz
    std::atomic<float>* noiseLevel     = nullptr;  // noise summed into the filter input, 0..1
    std::atomic<float>* noiseColour    = nullptr;  // 0 pink, 1 white
    std::atomic<float>* ringModLevel   = nullptr;  // osc 1 x osc 2, summed alongside them
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

    MySynthVoice() { deriveTolerances(); }

    // For reproducible offline rendering; never changes JUCE's global RNG.
    void setOscillatorSeed (uint32_t seed)
    {
        motionSeed = seed != 0 ? seed : 1;
        random.setSeed (motionSeed);
        motionRate = 0;
        phaseInitialised = false;
        deriveTolerances();
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        // Set by stopNote() when the synthesiser is about to steal this voice
        // while it is still sounding. Everything that would step the output -
        // oscillator phase, filter and decimator state, envelope level and the
        // velocity gain - is carried over instead of being reset to silence.
        const bool continuing = reassigning;
        reassigning = false;
        renderRate = getSampleRate() * 4;
        if (motionRate != renderRate)
        {
            motionRate = renderRate;
            commonDrift.prepare (renderRate, motionSeed);
            noise.prepare (renderRate, motionSeed + 104729u);
            for (int bank = 0; bank < 2; ++bank)
                for (int u = 0; u < kMaxUnisonVoices; ++u)
                    oscillators[bank][u].drift.prepare (renderRate, motionSeed + 7919u * (1 + bank * 7 + u));
        }
        const double hz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        const double shaped = shapeVelocity (velocity, read (velocityCurve, 0));
        if (continuing)
        {
            // Pitch takes the new note immediately; the velocity gain ramps,
            // so a steal never steps the VCA.
            glide.reset (hz, glide.getLevel());
            glide.target (hz, shaped, 0.005, renderRate);
        }
        else
            glide.reset (hz, shaped);
        // Only the slots that were actually sounding carry their phase over.
        const int sounding[] = { continuing ? count[0] : 0, continuing ? count[1] : 0 };
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
                if (u < sounding[bank])
                {
                    // Keep the running phase and its pending BLEP tail: a
                    // sounding waveform has no onset to place, and restarting
                    // it is what a steal is normally heard as.
                    osc.detune.reset (renderRate, 0.01);
                    continue;
                }
                if (phaseMode[bank] != 2 || ! phaseInitialised)
                {
                    const double offset = phaseMode[bank] == 1 ? randomness * random.nextDouble() : 0;
                    const double phase = startPhase[bank] + unisonPhase (u, bank) + offset;
                    osc.wave.setPhase (phase, phase * 0.5);
                }
                // A muted interval advances phase without computing BLEP tails.
                osc.wave.clearCorrection();
                osc.detune.reset (renderRate, 0.01);
                // Scatter the width modulators too. If every unison voice
                // swept its pulse in lockstep the stack would just get wider
                // and narrower together; offset, they beat against each other,
                // which is the whole point of PWM on a stacked patch.
                osc.pwm.reset (unisonPhase (u, bank) + 0.37 * bank);
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
        pwmDepthSmooth.reset (renderRate, 0.01);
        noiseLevelSmooth.reset (renderRate, 0.01);
        noiseColourSmooth.reset (renderRate, 0.02);
        ringSmooth.reset (renderRate, 0.01);
        for (auto& weight : filterMix) weight.reset (renderRate, 0.01);
        resonanceSmooth.reset (renderRate, 0.01);
        driveSmooth.reset (renderRate, 0.01);
        compensationSmooth.reset (renderRate, 0.01);
        envAmountSmooth.reset (renderRate, 0.005);
        trackingSmooth.reset (renderRate, 0.005);
        updateControls (true);
        cachedWidth = -1;
        updatePan();
        ampEnvelope.prepare (renderRate); filterEnvelope.prepare (renderRate);
        for (auto& b : blocker) b.prepare (renderRate);
        if (! continuing)
        {
            noise.reset();
            for (auto& filter : ladder) filter.reset();
            for (auto& b : blocker) b.reset();
            for (auto& d : decimator) d.reset();
            ampEnvelope.reset(); filterEnvelope.reset();
        }
        updateEnvelopes();
        ampEnvelope.noteOn(); filterEnvelope.noteOn();
        tailSamples = 33;
        active = true;
    }

    void stopNote (float velocity, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampEnvelope.noteOff(); filterEnvelope.noteOff();
            return;
        }
        // juce::Synthesiser signals a voice steal as stopNote (0, false)
        // immediately before calling startNote() on this voice; its hard
        // stops (all-notes-off, sample-rate changes) pass velocity 1. Only a
        // steal keeps state, so a panic still silences the voice outright.
        reassigning = velocity == 0.0f && ampEnvelope.isActive();
        if (! reassigning) { ampEnvelope.reset(); filterEnvelope.reset(); }
        active = false;
        clearCurrentNote();
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
            float bankOut[2][2] {};
            const double pwmAmount = pwmDepthSmooth.getNextValue();
            for (int bank = 0; bank < 2; ++bank)
            {
                syngen::BandlimitedOscillator::Mix mix;
                mix.sine = waveMix[bank][0].getNextValue();
                mix.saw = waveMix[bank][1].getNextValue();
                mix.pulse = waveMix[bank][2].getNextValue();
                mix.triangle = waveMix[bank][3].getNextValue();
                mix.sub = waveMix[bank][4].getNextValue();
                const double baseWidth = width[bank].getNextValue();
                const float bankGain = gain[bank] * (float) level[bank].getNextValue();
                // Only shape the modulator where it can be heard; elsewhere the
                // phase still advances, so switching PWM in mid-note picks up
                // where the modulator already was rather than jumping.
                const bool modulating = pwmAmount > 0.0 && mix.pulse != 0.0;
                for (int u = 0; u < count[bank]; ++u)
                {
                    auto& osc = oscillators[bank][u];
                    double pulseWidth = baseWidth;
                    if (modulating)
                        pulseWidth = juce::jlimit (0.05, 0.95, baseWidth + pwmAmount * 0.45 * osc.pwm.next());
                    else
                        osc.pwm.advance (1);
                    const float sample = (float) osc.wave.next (step[bank][u], mix, pulseWidth,
                                                               bank == 1 ? syncTime : -1) * bankGain;
                    bankOut[bank][0] += sample * osc.panLeft;
                    bankOut[bank][1] += sample * osc.panRight;
                }
            }
            const double ring = ringSmooth.getNextValue();
            const double noiseAmount = noiseLevelSmooth.getNextValue();
            const double colour = noiseColourSmooth.getNextValue();
            // Noise is mono, so a single centred voice still takes the shared
            // filter path below.
            const float noiseSample = noiseAmount > 0.0
                ? (float) (noiseAmount * 2.0 * noise.next (colour)) : 0.0f;
            for (int channel = 0; channel < 2; ++channel)
            {
                mixed[channel] = bankOut[0][channel] + bankOut[1][channel] + noiseSample;
                // Ring modulation is summed alongside the oscillators rather
                // than replacing them, the way a classic ring-mod mixer channel
                // is, so it can be blended in against the dry pair.
                if (ring > 0.0)
                    mixed[channel] += (float) (ring * 2.0 * bankOut[0][channel] * bankOut[1][channel]);
            }
            const double tracking = trackingSmooth.getNextValue()
                * std::log2 (glide.getFrequency() / 261.6255653005986);
            const double cutoff = juce::jlimit (20.0, maxCutoff, cutoffSmooth.getNextValue() * cutoffTrim
                * std::exp2 (envAmountSmooth.getNextValue() * filterEnvelope.next() + tracking));
            const double G = syngen::FeedbackLadder::coefficient (cutoff, renderRate);
            const double resonance = (resonanceSmooth.getNextValue() - 0.5) * (4.2 / 9.5) * resonanceTrim;
            const float drive = (float) (1 + 7 * driveSmooth.getNextValue());
            syngen::FeedbackLadder::Response response;
            double dcGain = 0.0;
            for (int w = 0; w < 5; ++w)
            {
                response.weight[(size_t) w] = filterMix[w].getNextValue();
                dcGain += response.weight[(size_t) w];
            }
            // Bass compensation restores the low end that resonance takes out
            // of a lowpass. Scaled by the mode's own DC gain, it applies in
            // full to the lowpasses and the notch, and not at all to the
            // bandpasses and highpasses, which have no low end to restore and
            // where it would only be up to 14 dB of extra drive.
            const double compensation = compensationSmooth.getNextValue()
                                        * juce::jlimit (0.0, 1.0, dcGain);
            const float vca = ampEnvelope.next() * (float) glide.getLevel() / std::sqrt (drive);
            float outL = blocker[0].process (ladder[0].process (mixed[0] * 0.3f * drive, G, resonance, compensation, response)) * vca;
            float outR = mono ? outL : blocker[1].process (ladder[1].process (mixed[1] * 0.3f * drive, G, resonance, compensation, response)) * vca;
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
    // Component tolerance for this voice, fixed for its lifetime and derived
    // from the same seed the drift generators use, so offline renders stay
    // reproducible. No two analog filters are trimmed identically; without
    // this every voice is numerically identical and chords collapse into one
    // sterile, phase-locked timbre. Depth is scaled by the Drift control, so
    // driftAmount == 0 still renders a perfectly matched pair of filters.
    // Start phase of unison voice u. Evenly spaced phases make N sawtooths sum
    // to one sawtooth at N times the pitch and 1/N the amplitude: a 5-voice
    // stack began about 13 dB down and swelled over ~100 ms as detuning pulled
    // the voices apart. An incoherent scatter starts at the same level the
    // detuned stack settles to. Deterministic, and exactly 0 for the first
    // voice, so a single unvoiced oscillator keeps its authored start phase.
    static double unisonPhase (int u, int bank)
    {
        if (u == 0) return 0.0;
        uint32_t h = (uint32_t) u * 2654435761u + (uint32_t) bank * 40503u;
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
        return (double) h / 4294967296.0;
    }
    void deriveTolerances()
    {
        uint32_t h = motionSeed != 0 ? motionSeed : 1;
        auto bipolar = [&h]
        {
            h ^= h << 13; h ^= h >> 17; h ^= h << 5;
            return (double) h / 2147483647.5 - 1.0;
        };
        cutoffTolerance = 0.04 * bipolar();     // +/- 0.04 octaves, about 2.8%
        resonanceTolerance = 0.03 * bipolar();  // +/- 3% of the feedback gain
    }
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
        target (pwmDepthSmooth, read (pwmDepth, 0), initialise);
        target (noiseLevelSmooth, read (noiseLevel, 0), initialise);
        target (noiseColourSmooth, read (noiseColour, 1), initialise);
        target (ringSmooth, read (ringModLevel, 0), initialise);
        const auto response = syngen::FeedbackLadder::response (
            juce::jlimit (0, syngen::FeedbackLadder::modeCount - 1, read (filterMode, 0)));
        for (int w = 0; w < 5; ++w) target (filterMix[w], response.weight[(size_t) w], initialise);
        const double pwmIncrement = read (pwmRate, 0.6f) / renderRate;
        for (int bank = 0; bank < 2; ++bank)
            for (int u = 0; u < kMaxUnisonVoices; ++u)
                // A few percent of rate spread per voice, so a held chord's
                // widths drift apart instead of pulsing as one.
                oscillators[bank][u].pwm.setIncrement (
                    pwmIncrement * (1.0 + 0.06 * (unisonPhase (u, bank) - 0.5)));
        target (envAmountSmooth, read (envAmountOct, 0), initialise);
        target (trackingSmooth, read (kbTrackAmount, 0), initialise);
        const double tolerance = juce::jlimit (0.0, 1.0, read (driftAmount, 1));
        cutoffTrim = std::exp2 (tolerance * cutoffTolerance);
        resonanceTrim = 1.0 + tolerance * resonanceTolerance;
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
                // Constant power, normalised to unity at centre. Plain cos/sin
                // puts 0.707 in each channel there, 3 dB below the unity a
                // single centred voice used to get from its own special case,
                // so switching unison on made the voice quieter and under-drove
                // the filter by the same 3 dB. At centre this is exactly 1.
                const double angle = (osc.spread * widthNow + 1) * juce::MathConstants<double>::pi * 0.25;
                osc.panLeft = (float) (juce::MathConstants<double>::sqrt2 * std::cos (angle));
                osc.panRight = (float) (juce::MathConstants<double>::sqrt2 * std::sin (angle));
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
        for (int bank = 0; bank < 2; ++bank)
            for (int u = 0; u < kMaxUnisonVoices; ++u)
                oscillators[bank][u].pwm.advance (samples);
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
        syngen::PulseWidthLfo pwm;
        juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> detune;
        double spread = 0;
        float panLeft = 1, panRight = 1;
    } oscillators[2][kMaxUnisonVoices];
    syngen::NoiseSource noise;
    syngen::SmoothDrift commonDrift;
    syngen::PitchGlide glide;
    juce::Random random;
    uint32_t motionSeed = (uint32_t) random.nextInt();
    double renderRate = 176400, motionRate = 0;
    bool active = false, phaseInitialised = false, syncEnabled = false, reassigning = false;
    double cutoffTolerance = 0, resonanceTolerance = 0, cutoffTrim = 1, resonanceTrim = 1;
    int count[2] { 1, 1 }, phaseMode[2] {}, tailSamples = 33;
    float gain[2] { 1, 1 };
    double cachedTuning[2] {}, tuningRatio[2] { 1, 1 }, cachedWidth = -1;
    juce::SmoothedValue<double> waveMix[2][5], width[2], tuning[2], level[2];
    juce::SmoothedValue<double> stereoWidth, driftDepth, resonanceSmooth, driveSmooth, compensationSmooth;
    juce::SmoothedValue<double> envAmountSmooth, trackingSmooth;
    juce::SmoothedValue<double> pwmDepthSmooth, noiseLevelSmooth, noiseColourSmooth, ringSmooth;
    // The five ladder tap weights are smoothed rather than switched, so the
    // filter mode is a performable control instead of a click.
    juce::SmoothedValue<double> filterMix[5];
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> cutoffSmooth;
    syngen::FeedbackLadder ladder[2];
    syngen::DCBlocker blocker[2];
    syngen::Decimator4 decimator[2];
    syngen::CurveEnvelope ampEnvelope, filterEnvelope;
};
