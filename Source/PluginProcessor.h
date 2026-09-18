#pragma once
#include <JuceHeader.h>
#include "Effects/FDNReverb.h"
#include "Effects/TapeEcho.h"
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

    std::atomic<int> osc1PhaseMode { 0 }, osc2PhaseMode { 0 };
    std::atomic<float> osc1StartPhase { 0 }, osc2StartPhase { 90 }, phaseRandomness { 1 };
    std::atomic<float> unisonWidth { 0.9f }, osc2Coarse { 0 };
    std::atomic<float> osc1Level { 0.75f }, osc2Level { 0.75f };
    std::atomic<float> filterCompensation { 0.5f }, envelopeCurve { 0.65f };
    // Ladder output mode, plus the three sources the oscillator section gained:
    // pulse-width modulation, a noise generator and ring modulation.
    std::atomic<int>   filterMode   { 0 };
    std::atomic<float> pwmDepth     { 0.0f }, pwmRate { 0.6f };
    std::atomic<float> noiseLevel   { 0.0f }, noiseColour { 1.0f };
    std::atomic<float> ringModLevel { 0.0f };
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
    std::atomic<float> unisonDetuneCents { 14.0f };  // fixed per-oscillator unison spread, not user-adjustable

    // "Modern" oscillator mode: continuous saw/pulse/triangle mix + sub-
    // octave, independent per oscillator, overriding oscType/osc2Type's
    // single-waveform pick while on. See MySynthVoice for the DSP.
    std::atomic<bool>  osc1ModernOn   { false };
    std::atomic<float> osc1SawMix     { 1.0f };
    std::atomic<float> osc1PulseMix   { 0.0f };
    std::atomic<float> osc1TriMix     { 0.0f };
    std::atomic<float> osc1PulseWidth { 0.5f };
    std::atomic<bool>  osc1SubOctave  { false };
    std::atomic<bool>  osc2ModernOn   { false };
    std::atomic<float> osc2SawMix     { 1.0f };
    std::atomic<float> osc2PulseMix   { 0.0f };
    std::atomic<float> osc2TriMix     { 0.0f };
    std::atomic<float> osc2PulseWidth { 0.5f };
    std::atomic<bool>  osc2SubOctave  { false };
    std::atomic<float> envAmountOct   { 2.0f };
    std::atomic<float> fltAttack      { 0.005f };
    std::atomic<float> fltDecay       { 0.25f };
    std::atomic<float> fltSustain     { 0.2f };
    std::atomic<float> fltRelease     { 0.1f };
    std::atomic<float> overloadAmount { 0.0f };
    std::atomic<float> kbTrackAmount  { 0.0f };
    std::atomic<float> velocityCurveAmount { 0.0f };
    std::atomic<float> pitchBendSemitones  { 0.0f };
    std::atomic<float> driftAmount         { 1.0f };  // 0 = perfectly stable, 1 = full analog-style drift
    std::atomic<bool>  glideOn         { false };
    std::atomic<float> glideTimeSeconds{ 0.08f };

    // Mod wheel (MIDI CC1) position, 0-1. Scales the LFO's depth for every
    // destination - pitch, filter and amp alike (see processBlock). LFO Amount
    // sets the ceiling and the wheel decides how much of it is in play, so a
    // patch with modulation dialed in stays still until the wheel is raised,
    // the way an analog synth's vibrato does. With the wheel down the LFO has
    // no effect at all, whichever destination is selected.
    std::atomic<float> modWheelAmount { 0.0f };

    // How far a full pitch-wheel deflection bends the pitch, in semitones;
    // +/-2 (a whole tone) is the standard MIDI default
    static constexpr float pitchBendRangeSemitones = 2.0f;

    juce::MidiKeyboardState keyboardState;
    Oscilloscope oscilloscope;
    OutputMeter outputMeter;

    juce::AudioProcessorValueTreeState apvts;
    void previewMeow() { meowPreviewRequested.store (true); }

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
    MySynthVoice* monoVoice = nullptr;
    bool previousGlideOn = false;
    void applyGlideVoicing (juce::MidiBuffer& midiMessages);

    // Single free-running mod LFO, computed once per block and routed to
    // whichever destination is selected (pitch, filter cutoff, or amp gain)
    double lfoPhase      = 0.0;
    float  lfoHeldRandom = 0.0f;
    float  lastAmpGain   = 1.0f;

    juce::MidiBuffer enhancedMidi;
    juce::Synthesiser synth;
    juce::Synthesiser meowSynth;
    juce::AudioBuffer<float> meowBuffer;
    juce::MidiBuffer meowMidi;
    juce::SmoothedValue<float> meowGain, meowSourceMix;
    std::atomic<bool> meowPreviewRequested { false };
    juce::dsp::NoiseGate<float> gate;
    juce::dsp::LadderFilter<float> ladder;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    syngen::FDNReverb reverb;
    syngen::TapeEcho echo;
    // 2x oversampling around the master ladder only. Its drive reaches 10x,
    // and a saturator at the host rate folds everything it generates above
    // Nyquist back into the audio band; the voices already run at 4x, so this
    // was the one nonlinearity in the chain still aliasing at base rate.
    std::unique_ptr<juce::dsp::Oversampling<float>> ladderOversampling;
    juce::SmoothedValue<float> masterGain;
    juce::dsp::Compressor<float> comp;
    juce::dsp::Limiter<float> limiter;
    int currentProgram = 0;
    std::vector<Preset> presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MySynthAudioProcessor)
};
