#pragma once
#include <JuceHeader.h>
#include "Oscillator/Oscillator.h"

//==============================================================================
class MySynthAudioProcessor : public juce::AudioProcessor
{
public:
    MySynthAudioProcessor();
    ~MySynthAudioProcessor() override;

    std::atomic<bool>  midiActivity   { false };
    std::atomic<int>   oscType        { 0 };
    std::atomic<int>   osc2Type       { 0 };
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

    juce::MidiKeyboardState keyboardState;

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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::Synthesiser synth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MySynthAudioProcessor)
};
