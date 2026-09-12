#pragma once
#include <JuceHeader.h>
#include "Oscillator/Oscillator.h"
#include "Oscilloscope.h"
#include "OutputMeter.h"
#include "Presets.h"

//==============================================================================
class MySynthAudioProcessor : public juce::AudioProcessor
{
public:
    MySynthAudioProcessor();
    ~MySynthAudioProcessor() override;

    std::atomic<bool>  midiActivity   { false };
    std::atomic<int>   oscType        { 0 };
    std::atomic<int>   osc2Type       { 0 };
    std::atomic<int>   osc1Octave     { 1 };
    std::atomic<int>   osc2Octave     { 1 };
    std::atomic<bool>  oscSync        { false };
    std::atomic<float> pitchSemitones { 0.0f };
    std::atomic<float> attackSeconds  { 0.01f };
    std::atomic<float> decaySeconds   { 0.1f };
    std::atomic<float> sustainLevel   { 0.7f };
    std::atomic<float> releaseSeconds { 0.05f };
    std::atomic<float> cutoffHz       { 20000.0f };
    std::atomic<float> resonanceQ     { 0.707f };
    std::atomic<float> detuneCents    { 7.0f };
    std::atomic<int>   unisonVoices      { 1 };      // shared by both oscillators, 1..MySynthVoice::kMaxUnisonVoices

    std::atomic<float> envAmountOct   { 2.0f };
    std::atomic<float> fltAttack      { 0.005f };
    std::atomic<float> fltDecay       { 0.25f };
    std::atomic<float> fltSustain     { 0.2f };
    std::atomic<float> fltRelease     { 0.1f };
    std::atomic<float> overloadAmount { 0.0f };
    std::atomic<float> kbTrackAmount  { 0.0f };
    std::atomic<float> velocityCurveAmount { 0.0f };
    std::atomic<float> pitchBendSemitones  { 0.0f };
    std::atomic<bool>  glideOn         { false };
    std::atomic<float> glideTimeSeconds{ 0.08f };

    // Mod wheel (MIDI CC1) position, 0-1. Gates the LFO's pitch-vibrato
    // depth (see processBlock) so a patch with vibrato dialed in stays
    // still until the wheel is raised, the way a real analog synth's
    // vibrato works, rather than warbling for the entire time a note is
    // held. Filter/amp LFO destinations aren't gated by it - those are
    // meant to run continuously once dialed in (auto-wah, tremolo).
    std::atomic<float> modWheelAmount { 0.0f };

    // How far a full pitch-wheel deflection bends the pitch, in semitones;
    // +/-2 (a whole tone) is the standard MIDI default
    static constexpr float pitchBendRangeSemitones = 2.0f;

    juce::MidiKeyboardState keyboardState;
    Oscilloscope oscilloscope;
    OutputMeter outputMeter;

    juce::AudioProcessorValueTreeState apvts;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Factory presets plus any patches saved this session via the "Save
    // Current Patch" menu item
    const std::vector<Preset>& getPresets() const { return presets; }
    void saveCurrentPatchAsPreset (const juce::String& name);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Presets before this index are the built-in factory ones (baked into
    // the binary, nothing to persist); presets from here on are user-saved
    // patches, written to disk in saveCurrentPatchAsPreset()
    size_t numFactoryPresets = 0;

    // When Glide is on, collapse the incoming MIDI stream down to one note
    // at a time: the first note-on in a phrase triggers normally, but a
    // note-on played while another is still held retargets the currently
    // sounding voice's pitch (and ramps its level toward the new velocity)
    // instead of retriggering the envelope, producing the legato portamento
    // slide classic analog mono-bass patches rely on. A note-off only
    // actually releases the voice once every held note in the stack has
    // been let go; otherwise it retargets back to whichever note is still
    // held, again without retriggering.
    struct HeldNote { int note; float velocity; int channel; };  // velocity normalised [0, 1], matching startNote()'s
    std::vector<HeldNote> monoNoteStack;
    void applyGlideVoicing (juce::MidiBuffer& midiMessages);

    // Single free-running mod LFO, computed once per block and routed to
    // whichever destination is selected (pitch, filter cutoff, or amp gain)
    double lfoPhase      = 0.0;
    float  lfoHeldRandom = 0.0f;
    float  lastAmpGain   = 1.0f;

    juce::MidiBuffer enhancedMidi;
    juce::Synthesiser synth;
    juce::dsp::NoiseGate<float> gate;
    juce::dsp::LadderFilter<float> ladder;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float> delayLine;
    std::vector<float> delayDampState;  // one-pole lowpass state in the feedback path, per channel
    juce::dsp::Compressor<float> comp;
    juce::dsp::Limiter<float> limiter;
    int currentProgram = 0;
    std::vector<Preset> presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MySynthAudioProcessor)
};
