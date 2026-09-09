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
    auto same = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) if (std::abs (a[i] - b[i]) > 1e-6) return false;
        return true;
    };
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        VoiceFixture repeat (rate);
        auto initial = repeat.render (1024);
        repeat.voice.stopNote (0, false); repeat.render (777); repeat.start (60);
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
        freeA.voice.stopNote (0, false); freeB.voice.stopNote (0, false);
        freeA.render (777, 17); freeB.render (777, 512); running.render (777, 32);
        for (auto* v : { &freeA, &freeB, &running }) v->start (60);
        auto a = freeA.render (1024), b = freeB.render (1024), c = running.render (1024);
        require (same (a, b) && same (a, c), "free phase and drift continue through silence independent of blocks");

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
        moving.render ((int) (rate * 0.02));
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
