#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MySynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "oscType", "Oscillator Type",
        juce::NormalisableRange<float> (0.0f, 3.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "osc2Type", "Oscillator 2 Type",
        juce::NormalisableRange<float> (0.0f, 4.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "detune", "Oscillator 2 Detune",
        juce::NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 7.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "pitch", "Pitch",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "attack", "Attack",
        juce::NormalisableRange<float> (0.001f, 2.0f, 0.001f, 0.4f), 0.01f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "decay", "Decay",
        juce::NormalisableRange<float> (0.001f, 2.0f, 0.001f, 0.4f), 0.1f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "sustain", "Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "release", "Release",
        juce::NormalisableRange<float> (0.001f, 3.0f, 0.001f, 0.4f), 0.05f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "cutoff", "Filter Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f), 20000.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "resonance", "Filter Resonance",
        juce::NormalisableRange<float> (0.5f, 10.0f, 0.01f, 0.5f), 0.707f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "envAmount", "Filter Env Amount",
        juce::NormalisableRange<float> (-5.0f, 5.0f, 0.1f), 2.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "fltAttack", "Filter Attack",
        juce::NormalisableRange<float> (0.001f, 2.0f, 0.001f, 0.4f), 0.005f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "fltDecay", "Filter Decay",
        juce::NormalisableRange<float> (0.001f, 2.0f, 0.001f, 0.4f), 0.25f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "fltSustain", "Filter Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "fltRelease", "Filter Release",
        juce::NormalisableRange<float> (0.001f, 3.0f, 0.001f, 0.4f), 0.1f));

    return layout;
}

MySynthAudioProcessor::MySynthAudioProcessor()
     : AudioProcessor (BusesProperties()
                      #if ! JucePlugin_IsMidiEffect
                       #if ! JucePlugin_IsSynth
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       #endif
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                      #endif
                        ),
       apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

MySynthAudioProcessor::~MySynthAudioProcessor()
{
}

//==============================================================================
const juce::String MySynthAudioProcessor::getName() const { return JucePlugin_Name; }

bool MySynthAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MySynthAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MySynthAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MySynthAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  MySynthAudioProcessor::getNumPrograms()                        { return 1; }
int  MySynthAudioProcessor::getCurrentProgram()                     { return 0; }
void MySynthAudioProcessor::setCurrentProgram (int)                 {}
const juce::String MySynthAudioProcessor::getProgramName (int)      { return {}; }
void MySynthAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void MySynthAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    synth.clearVoices();
    for (int i = 0; i < 8; ++i)
    {
        auto* voice = new MySynthVoice();
        voice->oscType        = &oscType;
        voice->osc2Type       = &osc2Type;
        voice->detuneCents    = &detuneCents;
        voice->pitchSemitones = &pitchSemitones;
        voice->attackSeconds  = &attackSeconds;
        voice->decaySeconds   = &decaySeconds;
        voice->sustainLevel   = &sustainLevel;
        voice->releaseSeconds = &releaseSeconds;
        voice->cutoffHz       = &cutoffHz;
        voice->resonanceQ     = &resonanceQ;
        voice->envAmountOct   = &envAmountOct;
        voice->fltAttack      = &fltAttack;
        voice->fltDecay       = &fltDecay;
        voice->fltSustain     = &fltSustain;
        voice->fltRelease     = &fltRelease;
        synth.addVoice (voice);
    }

    synth.clearSounds();
    synth.addSound (new MySynthSound());

    synth.setCurrentPlaybackSampleRate (sampleRate);
}

void MySynthAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MySynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif
    return true;
  #endif
}
#endif

void MySynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Sync parameters to atomics read by voices
    oscType.store ((int)std::round (apvts.getRawParameterValue ("oscType")->load()));
    osc2Type.store ((int)std::round (apvts.getRawParameterValue ("osc2Type")->load()));
    detuneCents.store (apvts.getRawParameterValue ("detune")->load());
    pitchSemitones.store (apvts.getRawParameterValue ("pitch")->load());
    attackSeconds.store (apvts.getRawParameterValue ("attack")->load());
    decaySeconds.store (apvts.getRawParameterValue ("decay")->load());
    sustainLevel.store (apvts.getRawParameterValue ("sustain")->load());
    releaseSeconds.store (apvts.getRawParameterValue ("release")->load());
    cutoffHz.store (apvts.getRawParameterValue ("cutoff")->load());
    resonanceQ.store (apvts.getRawParameterValue ("resonance")->load());
    envAmountOct.store (apvts.getRawParameterValue ("envAmount")->load());
    fltAttack.store (apvts.getRawParameterValue ("fltAttack")->load());
    fltDecay.store (apvts.getRawParameterValue ("fltDecay")->load());
    fltSustain.store (apvts.getRawParameterValue ("fltSustain")->load());
    fltRelease.store (apvts.getRawParameterValue ("fltRelease")->load());

    // Merge notes played on the computer keyboard into the MIDI stream
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

    if (!midiMessages.isEmpty())
        midiActivity = true;

    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
bool MySynthAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* MySynthAudioProcessor::createEditor()
{
    return new MySynthAudioProcessorEditor (*this);
}

//==============================================================================
void MySynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MySynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
    {
        auto newState = juce::ValueTree::fromXml (*xml);

        // State saved by an older version may lack newer parameters; fill
        // those in with their defaults so they don't restore as garbage
        for (auto* param : getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                if (! newState.getChildWithProperty ("id", ranged->paramID).isValid())
                {
                    juce::ValueTree child ("PARAM");
                    child.setProperty ("id", ranged->paramID, nullptr);
                    child.setProperty ("value",
                        (double) ranged->convertFrom0to1 (ranged->getDefaultValue()), nullptr);
                    newState.appendChild (child, nullptr);
                }

        apvts.replaceState (newState);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MySynthAudioProcessor();
}
