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
    std::atomic<float> envAmountOct   { 2.0f };
    std::atomic<float> fltAttack      { 0.005f };
    std::atomic<float> fltDecay       { 0.25f };
    std::atomic<float> fltSustain     { 0.2f };
    std::atomic<float> fltRelease     { 0.1f };
    std::atomic<float> overloadAmount { 0.0f };
    std::atomic<float> kbTrackAmount  { 0.0f };

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

    // When 1-2 sync is on, collapse the incoming MIDI stream down to one
    // note at a time, Little Phatty style, instead of letting every note
    // ring on its own independently-synced voice
    struct HeldNote { int note; juce::uint8 velocity; int channel; };
    std::vector<HeldNote> monoNoteStack;
    int monoCurrentNote = -1;
    void applyMonoSyncVoicing (juce::MidiBuffer& midiMessages);

    // Single free-running mod LFO, computed once per block and routed to
    // whichever destination is selected (pitch, filter cutoff, or amp gain)
    double lfoPhase      = 0.0;
    float  lfoHeldRandom = 0.0f;
    float  lastAmpGain   = 1.0f;

    juce::Synthesiser synth;
    juce::dsp::NoiseGate<float> gate;
    juce::dsp::LadderFilter<float> ladder;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Reverb reverb;
    juce::dsp::Compressor<float> comp;
    juce::dsp::Limiter<float> limiter;
    int currentProgram = 0;
    std::vector<Preset> presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MySynthAudioProcessor)
};
