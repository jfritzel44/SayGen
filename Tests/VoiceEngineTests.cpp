#include "../Source/PluginProcessor.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

static void require (bool condition, const char* message)
{
    if (! condition) { std::cerr << "FAIL: " << message << '\n'; std::exit (1); }
}

struct VoiceFixture
{
    MySynthVoice voice;
    std::atomic<bool> sync { false }, modern1 { false }, modern2 { false };
    std::atomic<int> wave { 1 }, wave2 { 2 }, unison { 1 }, phaseMode1 { 0 }, phaseMode2 { 0 };
    std::atomic<float> startPhase1 { 0 }, startPhase2 { 90 }, randomness { 1 }, spread { 14 }, stereo { 0.9f };
    std::atomic<float> pitch { 0 }, coarse { 0 }, pulseWidth { 0.5f }, pulseMix { 1 };
    std::atomic<float> sawMix { 1 }, triMix { 0 };
    std::atomic<float> drift { 0 }, level1 { 0.75f }, level2 { 0.75f }, cutoff { 20000 };
    std::atomic<float> drive { 0 }, resonance { 0.5f }, attack { 0.001f }, decay { 0.05f }, sustain { 1 }, release { 0.03f };
    VoiceFixture (double rate, float velocity = 1.0f)
    {
        voice.setOscillatorSeed (12345);
        voice.oscType = &wave; voice.osc2Type = &wave2;
        voice.osc1PhaseMode = &phaseMode1; voice.osc2PhaseMode = &phaseMode2;
        voice.osc1StartPhase = &startPhase1; voice.osc2StartPhase = &startPhase2;
        voice.phaseRandomness = &randomness; voice.unisonWidth = &stereo;
        voice.osc1UnisonDetune = &spread; voice.osc2UnisonDetune = &spread;
        voice.pitchSemitones = &pitch; voice.osc2Coarse = &coarse;
        voice.osc1ModernOn = &modern1; voice.osc2ModernOn = &modern2;
        voice.osc1PulseWidth = &pulseWidth; voice.osc2PulseWidth = &pulseWidth;
        voice.osc1PulseMix = &pulseMix; voice.osc2PulseMix = &pulseMix;
        voice.osc1SawMix = &sawMix; voice.osc2SawMix = &sawMix;
        voice.osc1TriMix = &triMix; voice.osc2TriMix = &triMix;
        voice.oscSync = &sync; voice.driftAmount = &drift;
        voice.osc1Level = &level1; voice.osc2Level = &level2; voice.cutoffHz = &cutoff;
        voice.overloadAmount = &drive; voice.resonanceQ = &resonance;
        voice.attackSeconds = &attack; voice.decaySeconds = &decay;
        voice.sustainLevel = &sustain; voice.releaseSeconds = &release;
        voice.osc1UnisonVoices = &unison; voice.osc2UnisonVoices = &unison;
        voice.setCurrentPlaybackSampleRate (rate);
        start (60, velocity);
    }
    void start (int note, float velocity = 1)
    {
        voice.startNote (note, velocity, nullptr, 0);
    }
    std::vector<float> render (int count, int chunk = 32)
    {
        juce::AudioBuffer<float> b (2, count); b.clear();
        for (int offset = 0; offset < count; offset += chunk)
            voice.renderNextBlock (b, offset, std::min (chunk, count - offset));
        std::vector<float> result (b.getReadPointer (0), b.getReadPointer (0) + count);
        for (float v : result) require (std::isfinite (v) && std::abs (v) < 8, "finite voice output");
        return result;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc == 2 && juce::String (argv[1]) == "--jump-benchmark")
    {
        for (bool advanced : { false, true })
        {
            MySynthAudioProcessor processor;
            auto set = [&] (const char* id, float value)
            {
                auto* p = processor.apvts.getParameter (id);
                p->setValueNotifyingHost (p->convertTo0to1 (value));
            };
            processor.setCurrentProgram (2); // Jump2
            set ("osc1ModernOn", advanced); set ("osc2ModernOn", advanced);
            set ("osc1SawMix", 0.28f); set ("osc1PulseMix", 0.22f);
            set ("osc1TriMix", 0.26f); set ("osc1PulseWidth", 0.98f);
            set ("osc2SawMix", 1); set ("osc2PulseMix", 0); set ("osc2TriMix", 0);
            set ("osc2PulseWidth", 0.67f);
            set ("osc1SubOctave", 1); set ("osc2SubOctave", 1);
            // Match the screenshot's bypassed effects.
            set ("reverbOn", 0); set ("limitOn", 0);
            constexpr int blockSize = 128;
            constexpr double rate = 48000;
            processor.setRateAndBufferSizeDetails (rate, blockSize);
            processor.prepareToPlay (rate, blockSize);
            juce::AudioBuffer<float> audio (2, blockSize);
            double totalMs = 0, worstMs = 0;
            float peak = 0;
            int overruns = 0;
            for (int block = 0; block < 1500; ++block)
            {
                juce::MidiBuffer midi;
                if (block % 75 == 0)
                {
                    for (int note = 60; note < 68; ++note)
                    {
                        midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
                    }
                }
                const auto start = std::chrono::steady_clock::now();
                processor.processBlock (audio, midi);
                const double ms = std::chrono::duration<double, std::milli>
                    (std::chrono::steady_clock::now() - start).count();
                totalMs += ms; worstMs = std::max (worstMs, ms);
                if (ms > 1000 * blockSize / rate) ++overruns;
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < blockSize; ++n)
                    {
                        const float sample = audio.getSample (ch, n);
                        require (std::isfinite (sample), "Jump2 mixed waves stay finite");
                        peak = std::max (peak, std::abs (sample));
                    }
            }
            std::cout << "Jump2 Advanced=" << advanced << " eight notes: average="
                      << totalMs / 1500 << " ms, worst=" << worstMs
                      << " ms, deadline=" << 1000 * blockSize / rate
                      << " ms, overruns=" << overruns << "/1500, peak=" << peak << std::endl;
            processor.releaseResources();
        }
        return 0;
    }
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        VoiceFixture loud (rate, 1.0f), soft (rate, 0.25f);
        loud.drive = soft.drive = 1;
        auto a = loud.render (8192), b = soft.render (8192);
        double error = 0, energy = 0;
        for (size_t i = 0; i < a.size(); ++i)
        {
            error += std::pow (b[i] - 0.25 * a[i], 2); energy += a[i] * a[i];
        }
        require (energy > 0.01 && error / energy < 1e-10, "velocity after nonlinear filter preserves timbre");

        VoiceFixture first (rate), second (rate);
        a = first.render (8192, 17); b = second.render (8192, 512);
        for (size_t i = 0; i < a.size(); ++i)
            require (std::abs (a[i] - b[i]) < 1e-6, "block-size invariant steady voice");

        first.level1 = first.level2 = 0;
        a = first.render ((int) rate);
        for (size_t i = a.size() - 128; i < a.size(); ++i)
            require (std::abs (a[i]) < 1e-6, "mixer silences both oscillators");

        second.voice.stopNote (0, true);
        b = second.render ((int) (rate * 0.04));
        for (size_t i = b.size() - 128; i < b.size(); ++i)
            require (b[i] == 0, "release flushes decimator to silence");

        VoiceFixture extreme (rate);
        extreme.unison = 7; extreme.sync = true; extreme.drive = 1; extreme.resonance = 10;
        for (int note : { 0, 60, 100, 127 })
        {
            extreme.start (note);
            extreme.render (4096);
        }
    }
    {
        MySynthAudioProcessor processor;
        auto set = [&] (const char* id, float v)
        { auto* p = processor.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        auto get = [&] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
        // Reproduce the retained -30 dB / 1 ms limiter and stale wet effects.
        for (int i = 0; i < (int) getFactoryPresets().size(); ++i)
        {
            const auto& preset = processor.getPresets()[(size_t) i];
            for (int mode = 0; mode < 4; ++mode)
            {
                set ("osc1ModernOn", (float) (mode & 1));
                set ("osc2ModernOn", (float) ((mode >> 1) & 1));
                set ("osc1SawMix", 0.11f); set ("osc2SubOctave", 1);
                set ("limitOn", 1); set ("limitThresh", -30); set ("limitRelease", 1);
                set ("reverbOn", 1); set ("reverbMix", 1);
                set ("delayOn", 1); set ("chorusOn", 1);
                set ("gateOn", 1); set ("gateRatio", 10);
                set ("ladderOn", 1); set ("phaserOn", 1); set ("compOn", 1);
                set ("masterVolume", -18);
                processor.setCurrentProgram (i);
                require (std::abs (get ("limitThresh") + 3) < 1e-4
                         && std::abs (get ("limitRelease") - 100) < 1e-3,
                         "preset clears extreme limiter settings");
                require (std::abs (get ("masterVolume") + 18) < 1e-4,
                         "preset preserves master volume");
                for (auto id : { "gateOn", "ladderOn", "chorusOn", "phaserOn",
                                 "reverbOn", "delayOn", "compOn", "limitOn", "reverbMix", "gateRatio" })
                {
                    auto* parameter = processor.apvts.getParameter (id);
                    float expected = parameter->convertFrom0to1 (parameter->getDefaultValue());
                    for (const auto& [key, value] : preset.values)
                        if (key == id) expected = value;
                    require (std::abs (get (id) - expected) < 1e-4,
                             "effects load from selected preset instead of previous sound");
                }
                require (get ("osc1ModernOn") == (mode & 1)
                         && get ("osc2ModernOn") == ((mode >> 1) & 1), "preset preserves oscillator mode");
                if (mode != 0)
                    for (const auto& [id, value] : preset.advancedValues)
                        require (std::abs (get (id.toRawUTF8()) - value) < 1e-4,
                                 "bass restores advanced voicing");
                else
                    for (const auto& [id, value] : preset.values)
                        require (std::abs (get (id.toRawUTF8()) - value) < 1e-3,
                                 "preset restores classic voicing");
            }
        }
        require (processor.apvts.getParameter ("enhancedEngine") == nullptr, "legacy selector removed");
        set ("osc1PhaseMode", 2); set ("osc2Coarse", 12);
        processor.setCurrentProgram (0);
        require (get ("osc1PhaseMode") == 0 && get ("osc2Coarse") == 0, "factory preset resets optional controls"); set ("osc1Level", 0.23f); set ("envelopeCurve", 0.81f);
        set ("osc1StartPhase", 142); set ("phaseRandomness", 0.42f); set ("unisonSpread", 23); set ("osc2Coarse", -7);
        juce::MemoryBlock saved; processor.getStateInformation (saved);
        set ("osc1Level", 0.8f);
        processor.setStateInformation (saved.getData(), (int) saved.getSize());
        require (std::abs (get ("osc1Level") - 0.23f) < 1e-5,
                 "sound state round-trip");
        require (get ("osc1StartPhase") == 142 && std::abs (get ("phaseRandomness") - 0.42f) < 1e-5
                 && get ("unisonSpread") == 23 && get ("osc2Coarse") == -7, "motion parameters round-trip");
        auto old = processor.apvts.copyState();
        old.removeChild (old.getChildWithProperty ("id", "osc1PhaseMode"), nullptr);
        juce::ValueTree obsolete ("PARAM");
        obsolete.setProperty ("id", "enhancedEngine", nullptr); obsolete.setProperty ("value", 0.0, nullptr);
        old.appendChild (obsolete, nullptr);
        auto xml = old.createXml(); juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary (*xml, legacy);
        processor.setStateInformation (legacy.getData(), (int) legacy.getSize());
        require (get ("osc1PhaseMode") == 0, "old session gets default phase mode");
        require (! processor.apvts.copyState().getChildWithProperty ("id", "enhancedEngine").isValid(),
                 "obsolete engine switch discarded");
        processor.setRateAndBufferSizeDetails (48000, 64);
        processor.prepareToPlay (48000, 64);
        juce::AudioBuffer<float> audio (2, 64);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 40);
        processor.processBlock (audio, midi);
        for (int n = 0; n < 40; ++n)
            require (audio.getSample (0, n) == 0, "scheduled note does not sound early");
        midi.clear();
        for (int block = 0; block < 100; ++block)
        {
            processor.processBlock (audio, midi);
            for (int n = 0; n < 64; ++n)
                require (std::isfinite (audio.getSample (0, n)), "processor renders enhanced voice");
        }
        processor.releaseResources();
        if (argc == 2)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            auto showPanel = [] (auto&& self, juce::Component& component) -> void
            {
                if (auto* button = dynamic_cast<juce::TextButton*> (&component))
                    if (button->getButtonText() == "OSC MIX") button->onClick();
                for (int i = 0; i < component.getNumChildComponents(); ++i)
                    self (self, *component.getChildComponent (i));
            };
            showPanel (showPanel, *editor);
            auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds());
            juce::FileOutputStream output { juce::File (argv[1]) };
            require (output.openedOk(), "snapshot output opens");
            require (juce::PNGImageFormat().writeImageToStream (snapshot, output), "snapshot writes");
            auto showMotion = [] (auto&& self, juce::Component& component) -> void
            {
                if (auto* button = dynamic_cast<juce::TextButton*> (&component))
                    if (button->getButtonText() == "Motion") button->onClick();
                for (int i = 0; i < component.getNumChildComponents(); ++i)
                    self (self, *component.getChildComponent (i));
            };
            showMotion (showMotion, *editor);
            juce::FileOutputStream motionOutput { juce::File (argv[1]).getSiblingFile ("syngen-motion-panel.png") };
            require (juce::PNGImageFormat().writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds()),
                                                               motionOutput), "motion snapshot writes");
        }

    }
    auto level = [] (const std::vector<float>& v, size_t from, size_t n)
    {
        double energy = 0;
        for (size_t i = from; i < from + n && i < v.size(); ++i) energy += (double) v[i] * v[i];
        return std::sqrt (energy / n);
    };
    auto same = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) if (std::abs (a[i] - b[i]) > 1e-6) return false;
        return true;
    };
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        // Jump's twin saws must render identically through Wave Mix, including
        // repeated attacks and releases. This isolates the Advanced switch
        // from differences in waveform amounts or output gain.
        VoiceFixture classicJump (rate), advancedJump (rate);
        for (auto* fixture : { &classicJump, &advancedJump })
        {
            fixture->pulseMix = 0;
            fixture->cutoff = 5200;
            fixture->attack = 0.005f;
            fixture->decay = 0.3f;
            fixture->sustain = 0.85f;
            fixture->release = 0.35f;
            fixture->resonance = 0.9f;
            fixture->drift = 1;
        }
        advancedJump.modern1 = advancedJump.modern2 = true;
        for (int note : { 60, 64, 67, 72, 67, 64, 60, 72 })
        {
            classicJump.start (note); advancedJump.start (note);
            require (same (classicJump.render (4096), advancedJump.render (4096)),
                     "Advanced twin saws match classic across repeated notes");
            classicJump.voice.stopNote (0, true); advancedJump.voice.stopNote (0, true);
            require (same (classicJump.render (2048), advancedJump.render (2048)),
                     "Advanced twin saw release matches classic");
        }

        VoiceFixture repeat (rate);
        auto initial = repeat.render (1024);
        repeat.voice.stopNote (1, false); repeat.render (777); repeat.start (60);
        require (same (initial, repeat.render (1024)), "retrigger gives repeatable attacks with drift off");
        repeat.phaseMode1 = repeat.phaseMode2 = 1;
        repeat.start (60); auto randomA = repeat.render (1024);
        repeat.start (60); auto randomB = repeat.render (1024);
        require (! same (randomA, randomB), "random mode changes attacks");
        repeat.randomness = 0; repeat.start (60);
        require (same (initial, repeat.render (1024)), "zero randomness reproduces fixed phase");
        repeat.phaseMode1 = repeat.phaseMode2 = 0; repeat.startPhase1 = 180; repeat.start (60);
        require (! same (initial, repeat.render (1024)), "start phase changes attack waveform");

        VoiceFixture freeA (rate), freeB (rate), running (rate);
        for (auto* v : { &freeA, &freeB, &running })
        {
            v->phaseMode1 = v->phaseMode2 = 2; v->drift = 1; v->start (60); v->render (1024);
        }
        freeA.voice.stopNote (1, false); freeB.voice.stopNote (1, false);
        freeA.render (777, 17); freeB.render (777, 512); running.render (777, 32);
        for (auto* v : { &freeA, &freeB, &running }) v->start (60);
        auto a = freeA.render (1024), b = freeB.render (1024), c = running.render (1024);
        require (same (a, b) && same (a, c), "free phase and drift continue through silence independent of blocks");

        // Voice stealing: juce::Synthesiser hard-stops the voice with velocity 0
        // and restarts it in the same call. Nothing in that handover may step
        // the output, and a panic (velocity 1) must still silence it outright.
        {
            // Resetting the envelope, filter and decimator on a steal used to
            // punch a hole in the output: the level collapsed by about 21 dB
            // within 3 ms and then climbed back. Measure the level either side
            // of the handover rather than sample deltas, which a sawtooth's own
            // edges dominate.
            VoiceFixture stolen (rate);
            stolen.sustain = 0.9f; stolen.decay = 0.2f; stolen.release = 0.8f;
            stolen.cutoff = 4000; stolen.resonance = 2;
            auto held = stolen.render (8192);
            const double before = level (held, held.size() - 128, 128);
            stolen.voice.stopNote (0, false);          // the steal
            stolen.start (67, 0.2f);                   // new note, much softer
            auto after = stolen.render (4096);
            require (level (after, 0, 128) > 0.5 * before, "voice steal hands over without a drop-out");
            require (level (after, 128, 128) > level (after, 1024, 128),
                     "voice steal settles down to the new note rather than climbing back up");

            VoiceFixture panicked (rate);
            panicked.render (4096);
            panicked.voice.stopNote (1, false);
            for (float v : panicked.render (4096)) require (v == 0, "a hard stop silences the voice");
        }

        // Per-voice component tolerance is fixed per seed and gated by Drift,
        // so differently seeded voices diverge when it is up and stay matched
        // sample-for-sample when it is off.
        {
            VoiceFixture matchedA (rate), matchedB (rate);
            matchedB.voice.setOscillatorSeed (98765); matchedB.start (60);
            matchedA.cutoff = matchedB.cutoff = 900;
            matchedA.resonance = matchedB.resonance = 4;
            require (same (matchedA.render (8192), matchedB.render (8192)),
                     "drift off matches every voice exactly");
            matchedA.drift = matchedB.drift = 1;
            matchedA.start (60); matchedB.start (60);
            require (! same (matchedA.render (8192), matchedB.render (8192)),
                     "drift on gives each voice its own filter trim");
        }

        // Unison must not move the voice's level or its attack. Constant-power
        // panning put 0.707 in each channel at centre while a single voice got
        // unity from its own special case, so switching unison on dropped the
        // output 3 dB and under-drove the filter by the same amount. Evenly
        // spaced start phases separately made N sawtooths sum to one sawtooth
        // at N times the pitch, so a stack began up to 13 dB down and swelled.
        {
            auto stereoLevel = [&] (int voices, float width)
            {
                VoiceFixture f (rate);
                f.unison = voices; f.stereo = width; f.wave2 = 0; f.cutoff = 20000;
                f.start (60);
                const int length = (int) rate;
                juce::AudioBuffer<float> b (2, length); b.clear();
                for (int o = 0; o < length; o += 64)
                    f.voice.renderNextBlock (b, o, std::min (64, length - o));
                const int from = length / 4;
                double energy = 0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = from; i < length; ++i)
                        energy += (double) b.getSample (ch, i) * b.getSample (ch, i);
                return std::sqrt (energy / (2 * (length - from)));
            };
            for (float width : { 0.0f, 0.9f })
            {
                const double one = stereoLevel (1, width);
                for (int voices : { 2, 5, 7 })
                {
                    const double many = stereoLevel (voices, width);
                    require (std::abs (20 * std::log10 (many / one)) < 0.5,
                             "unison keeps the voice at the same level");
                }
            }
            VoiceFixture bloom (rate);
            bloom.unison = 5; bloom.wave2 = 0; bloom.cutoff = 20000; bloom.attack = 0.001f;
            bloom.start (60);
            auto swell = bloom.render ((int) (rate * 2));
            // Scattered phases measure 0.94-1.04 here; the evenly spaced ones
            // this replaced measured 0.67-0.69 at the same three rates.
            require (level (swell, 128, (size_t) (rate * 0.01))
                     > 0.85 * level (swell, (size_t) rate, (size_t) rate),
                     "a unison stack starts at the level it settles to");
        }

        // An odd saturator rectifies an asymmetric wave into a DC offset, which
        // eats headroom and thumps at note boundaries. A narrow pulse driven
        // into the filter is the worst case: it reached -25% of RMS untreated.
        {
            VoiceFixture offset (rate);
            offset.modern1 = true; offset.wave2 = 0;
            offset.sawMix = 0; offset.pulseMix = 1; offset.pulseWidth = 0.1f;
            offset.drive = 0.6f; offset.cutoff = 6000; offset.decay = 0.001f;
            offset.start (36);
            auto y = offset.render ((int) rate);
            const size_t from = (size_t) (rate / 2), span = (size_t) (rate / 2);
            double sum = 0;
            for (size_t i = from; i < from + span && i < y.size(); ++i) sum += y[i];
            require (std::abs (sum / span) < 0.02 * level (y, from, span),
                     "the saturating filter leaves no DC behind");
        }

        VoiceFixture glideA (rate), glideB (rate);
        glideA.voice.setGlideTarget (84, 0.5f, 0.1f); glideB.voice.setGlideTarget (84, 0.5f, 0.1f);
        require (same (glideA.render (8192, 17), glideB.render (8192, 512)), "samplewise glide is block invariant");
        glideA.pitch = glideB.pitch = 12;
        glideA.wave = glideB.wave = 3;
        require (same (glideA.render (2048, 17), glideB.render (2048, 512)), "pitch and waveform smoothing are block invariant");

        VoiceFixture oscA (rate), oscB (rate);
        oscA.startPhase1 = oscB.startPhase2 = 0;
        oscA.level1 = oscB.level2 = 1; oscA.level2 = oscB.level1 = 0;
        oscA.start (60); oscB.start (60);
        require (same (oscA.render (2048), oscB.render (2048)), "oscillators match with drift off");
        oscA.drift = oscB.drift = 1; oscA.start (60); oscB.start (60);
        require (! same (oscA.render (8192), oscB.render (8192)), "each single oscillator drifts independently");

        VoiceFixture moving (rate);
        moving.unison = 7; moving.start (60);
        for (int n = 0; n < 20; ++n)
        {
            moving.modern1 = moving.modern2 = n % 2;
            moving.pulseWidth = n % 2 ? 0.02f : 0.98f;
            moving.stereo = n % 2; moving.spread = n % 2 ? 50 : 0;
            moving.coarse = n % 2 ? 24 : -24;
            moving.render (1024);
        }
        moving.stereo = 0;
        // The per-channel DC blockers hold ~32 ms of history, so after a width
        // sweep the two channels take a few hundred ms to reconverge, rather
        // than the ~20 ms the ladder alone needed.
        moving.render ((int) (rate * 0.4));
        juce::AudioBuffer<float> stereo (2, 256); stereo.clear();
        moving.voice.renderNextBlock (stereo, 0, 256);
        for (int n = 0; n < 256; ++n)
            require (std::abs (stereo.getSample (0, n) - stereo.getSample (1, n)) < 1e-5,
                     "zero unison width produces mono after settling");
    }
    for (int count : { 1, 7 })
    {
        VoiceFixture voice (48000);
        voice.unison = count; voice.drive = 0.5f; voice.resonance = 7; voice.start (60);
        auto start = std::chrono::steady_clock::now();
        voice.render (48000);
        auto ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count();
        std::cout << "1 second / 1 note / " << count << " unison: " << ms << " ms\n";
    }
    std::cout << "Voice and state tests passed\n";
}
