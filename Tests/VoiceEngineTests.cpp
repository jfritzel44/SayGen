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
    std::atomic<bool> sync { false };
    std::atomic<int> wave { 1 }, wave2 { 2 }, unison { 1 };
    std::atomic<float> pitch { 0 }, cutoff { 20000 };
    std::atomic<float> drive { 0 }, resonance { 0.5f }, attack { 0.001f }, decay { 0.05f }, sustain { 1 }, release { 0.03f };
    VoiceFixture (double rate, float velocity = 1.0f)
    {
        voice.oscType = &wave; voice.osc2Type = &wave2;
        voice.pitchSemitones = &pitch;
        voice.oscSync = &sync;
        voice.cutoffHz = &cutoff;
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

        set ("cutoff", 4000.0f);
        juce::MemoryBlock saved; processor.getStateInformation (saved);
        set ("cutoff", 8000.0f);
        processor.setStateInformation (saved.getData(), (int) saved.getSize());
        require (std::abs (get ("cutoff") - 4000.0f) < 1e-3, "sound state round-trip");

        auto old = processor.apvts.copyState();
        old.removeChild (old.getChildWithProperty ("id", "cutoff"), nullptr);
        juce::ValueTree obsolete ("PARAM");
        obsolete.setProperty ("id", "enhancedEngine", nullptr); obsolete.setProperty ("value", 0.0, nullptr);
        old.appendChild (obsolete, nullptr);
        auto xml = old.createXml(); juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary (*xml, legacy);
        processor.setStateInformation (legacy.getData(), (int) legacy.getSize());
        require (get ("cutoff") == 20000.0f, "old session gets default cutoff");
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
            auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds());
            juce::FileOutputStream output { juce::File (argv[1]) };
            require (output.openedOk(), "snapshot output opens");
            require (juce::PNGImageFormat().writeImageToStream (snapshot, output), "snapshot writes");
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
        require (same (initial, repeat.render (1024)), "retrigger gives repeatable attacks");

        VoiceFixture glideA (rate), glideB (rate);
        glideA.voice.setGlideTarget (84, 0.5f, 0.1f); glideB.voice.setGlideTarget (84, 0.5f, 0.1f);
        require (same (glideA.render (8192, 17), glideB.render (8192, 512)), "samplewise glide is block invariant");
        glideA.pitch = glideB.pitch = 12;
        glideA.wave = glideB.wave = 3;
        require (same (glideA.render (2048, 17), glideB.render (2048, 512)), "pitch and waveform smoothing are block invariant");
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
