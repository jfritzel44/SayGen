#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

//==============================================================================
// Bipolar [-1, 1] LFO shapes, sampled at a normalised phase in [0, 1).
// Sample & Hold (shape 4) isn't stateless, so it's handled by the caller.
static float lfoWaveSample (int shape, double phase01)
{
    switch (shape)
    {
        case 1:  return (float) (phase01 < 0.5 ? (4.0 * phase01 - 1.0)      // Triangle
                                                : (3.0 - 4.0 * phase01));
        case 2:  return phase01 < 0.5 ? 1.0f : -1.0f;                       // Square
        case 3:  return (float) (1.0 - 2.0 * phase01);                     // Saw
        default: return (float) std::sin (phase01 * juce::MathConstants<double>::twoPi); // Sine
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MySynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "oscType", "Oscillator Type",
        juce::NormalisableRange<float> (0.0f, 3.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "osc2Type", "Oscillator 2 Type",
        juce::NormalisableRange<float> (0.0f, 4.0f, 1.0f), 0.0f));

    // Octave range: 0 = 16', 1 = 8', 2 = 4', 3 = 2'
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "osc1Octave", "Oscillator 1 Octave",
        juce::NormalisableRange<float> (0.0f, 3.0f, 1.0f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "osc2Octave", "Oscillator 2 Octave",
        juce::NormalisableRange<float> (0.0f, 3.0f, 1.0f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "oscSync", "Oscillator 1-2 Sync", false));

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

    // Little Phatty-style pre-filter drive: 0 is clean, dialing it up
    // saturates the signal harder before it hits the filter
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "overload", "Overload",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "cutoff", "Filter Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f), 20000.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "resonance", "Filter Resonance",
        juce::NormalisableRange<float> (0.5f, 10.0f, 0.01f, 0.5f), 0.707f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "envAmount", "Filter EGR Amount",
        juce::NormalisableRange<float> (-5.0f, 5.0f, 0.1f), 2.0f));

    // Keyboard tracking: how much the filter cutoff follows the played
    // note's pitch, 0 = no tracking, 1 = cutoff tracks pitch 1:1
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "kbAmount", "Filter Keyboard Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

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

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "lfoSource", "LFO Source",
        juce::StringArray { "Sine", "Triangle", "Square", "Saw", "S&H" }, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "lfoDest", "LFO Destination",
        juce::StringArray { "Off", "Pitch", "Filter Cutoff", "Amp" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "lfoRate", "LFO Rate",
        juce::NormalisableRange<float> (0.02f, 20.0f, 0.01f, 0.3f), 2.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "lfoAmount", "LFO Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "gateOn", "Gate On", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "gateThresh", "Gate Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 1.0f), -40.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "gateRatio", "Gate Ratio",
        juce::NormalisableRange<float> (1.0f, 10.0f, 0.1f), 4.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "gateAttack", "Gate Attack",
        juce::NormalisableRange<float> (0.1f, 100.0f, 0.1f, 0.5f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "gateRelease", "Gate Release",
        juce::NormalisableRange<float> (1.0f, 500.0f, 1.0f, 0.5f), 50.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "ladderOn", "Ladder Filter On", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "ladderCutoff", "Ladder Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f), 1000.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "ladderRes", "Ladder Resonance",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "ladderDrive", "Ladder Drive",
        juce::NormalisableRange<float> (1.0f, 10.0f, 0.1f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "chorusOn", "Chorus On", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "chorusRate", "Chorus Rate",
        juce::NormalisableRange<float> (0.05f, 8.0f, 0.01f, 0.5f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "chorusDepth", "Chorus Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "chorusMix", "Chorus Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "phaserOn", "Phaser On", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "phaserRate", "Phaser Rate",
        juce::NormalisableRange<float> (0.05f, 8.0f, 0.01f, 0.5f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "phaserDepth", "Phaser Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.6f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "phaserFeedback", "Phaser Feedback",
        juce::NormalisableRange<float> (0.0f, 0.9f, 0.01f), 0.3f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "phaserMix", "Phaser Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "reverbOn", "Reverb On", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "reverbSize", "Reverb Size",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.55f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "reverbDamp", "Reverb Damping",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "reverbWidth", "Reverb Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "reverbMix", "Reverb Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "compOn", "Compressor On", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "compThresh", "Compressor Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 1.0f), -20.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "compRatio", "Compressor Ratio",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f), 4.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "compAttack", "Compressor Attack",
        juce::NormalisableRange<float> (0.1f, 100.0f, 0.1f, 0.5f), 5.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "compRelease", "Compressor Release",
        juce::NormalisableRange<float> (1.0f, 500.0f, 1.0f, 0.5f), 100.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "limitOn", "Limiter On", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "limitThresh", "Limiter Threshold",
        juce::NormalisableRange<float> (-30.0f, 0.0f, 0.1f), -3.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "limitRelease", "Limiter Release",
        juce::NormalisableRange<float> (1.0f, 500.0f, 1.0f, 0.5f), 100.0f));

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
    presets = getFactoryPresets();
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

int MySynthAudioProcessor::getNumPrograms()
{
    return juce::jmax (1, (int) presets.size());
}

int MySynthAudioProcessor::getCurrentProgram() { return currentProgram; }

void MySynthAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= (int) presets.size())
        return;

    currentProgram = index;

    for (auto& [paramID, value] : presets[(size_t) index].values)
        if (auto* param = apvts.getParameter (paramID))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
}

const juce::String MySynthAudioProcessor::getProgramName (int index)
{
    return index >= 0 && index < (int) presets.size() ? presets[(size_t) index].name
                                                      : juce::String();
}

void MySynthAudioProcessor::changeProgramName (int, const juce::String&) {}

void MySynthAudioProcessor::saveCurrentPatchAsPreset (const juce::String& name)
{
    // The parameter set that defines "the patch" — matches what the
    // factory presets in Presets.h capture (oscillators, envelopes, filter)
    static const char* patchParamIDs[] =
    {
        "oscType", "osc2Type", "osc1Octave", "osc2Octave", "detune", "pitch",
        "attack", "decay", "sustain", "release",
        "cutoff", "resonance", "envAmount",
        "fltAttack", "fltDecay", "fltSustain", "fltRelease",
    };

    Preset preset;
    preset.name = name;

    for (auto* paramID : patchParamIDs)
        if (auto* param = apvts.getParameter (paramID))
            preset.values.push_back ({ paramID, param->convertFrom0to1 (param->getValue()) });

    presets.push_back (std::move (preset));
    currentProgram = (int) presets.size() - 1;
}

//==============================================================================
void MySynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lfoPhase = 0.0;
    lfoHeldRandom = 0.0f;
    lastAmpGain = 1.0f;

    synth.clearVoices();
    for (int i = 0; i < 8; ++i)
    {
        auto* voice = new MySynthVoice();
        voice->oscType        = &oscType;
        voice->osc2Type       = &osc2Type;
        voice->osc1Octave     = &osc1Octave;
        voice->osc2Octave     = &osc2Octave;
        voice->oscSync        = &oscSync;
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
        voice->overloadAmount = &overloadAmount;
        voice->kbTrackAmount  = &kbTrackAmount;
        synth.addVoice (voice);
    }

    synth.clearSounds();
    synth.addSound (new MySynthSound());

    synth.setCurrentPlaybackSampleRate (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                  (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
    gate.prepare (spec);
    gate.reset();
    ladder.prepare (spec);
    ladder.setMode (juce::dsp::LadderFilterMode::LPF24);
    ladder.reset();
    chorus.prepare (spec);
    chorus.setCentreDelay (7.0f);
    chorus.setFeedback (0.0f);
    chorus.reset();
    phaser.prepare (spec);
    phaser.setCentreFrequency (1300.0f);
    phaser.reset();
    reverb.prepare (spec);
    reverb.reset();
    comp.prepare (spec);
    comp.reset();
    limiter.prepare (spec);
    limiter.reset();
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
    osc1Octave.store ((int)std::round (apvts.getRawParameterValue ("osc1Octave")->load()));
    osc2Octave.store ((int)std::round (apvts.getRawParameterValue ("osc2Octave")->load()));
    oscSync.store (apvts.getRawParameterValue ("oscSync")->load() >= 0.5f);
    detuneCents.store (apvts.getRawParameterValue ("detune")->load());
    attackSeconds.store (apvts.getRawParameterValue ("attack")->load());
    decaySeconds.store (apvts.getRawParameterValue ("decay")->load());
    sustainLevel.store (apvts.getRawParameterValue ("sustain")->load());
    releaseSeconds.store (apvts.getRawParameterValue ("release")->load());
    resonanceQ.store (apvts.getRawParameterValue ("resonance")->load());
    envAmountOct.store (apvts.getRawParameterValue ("envAmount")->load());
    kbTrackAmount.store (apvts.getRawParameterValue ("kbAmount")->load());
    fltAttack.store (apvts.getRawParameterValue ("fltAttack")->load());
    fltDecay.store (apvts.getRawParameterValue ("fltDecay")->load());
    fltSustain.store (apvts.getRawParameterValue ("fltSustain")->load());
    fltRelease.store (apvts.getRawParameterValue ("fltRelease")->load());
    overloadAmount.store (apvts.getRawParameterValue ("overload")->load());

    // Base pitch/cutoff, before mod LFO is added in below. Read once here so
    // each sub-block's LFO contribution is applied fresh rather than
    // compounding onto whatever the previous sub-block already wrote.
    const float basePitchSemitones = apvts.getRawParameterValue ("pitch")->load();
    const float baseCutoffHz       = apvts.getRawParameterValue ("cutoff")->load();
    pitchSemitones.store (basePitchSemitones);
    cutoffHz.store (baseCutoffHz);

    // Mod LFO: one shared source, routed to whichever destination is
    // selected.
    const int   lfoDest   = (int) std::round (apvts.getRawParameterValue ("lfoDest")->load());
    const int   lfoSource = (int) std::round (apvts.getRawParameterValue ("lfoSource")->load());
    const float lfoRateHz = apvts.getRawParameterValue ("lfoRate")->load();
    const float lfoAmount = apvts.getRawParameterValue ("lfoAmount")->load();

    // Merge notes played on the computer keyboard into the MIDI stream
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

    if (!midiMessages.isEmpty())
        midiActivity = true;

    // Render in small sub-blocks instead of the whole buffer at once, so
    // pitch/cutoff/amp modulation gets updated far more often than the
    // host's block size. A once-per-block update makes the LFO audibly
    // step/zipper; updating every ~32 samples (and ramping the amp gain
    // across each sub-block) is what makes it read as continuous motion
    // instead, closer to how an analog LFO like the Little Phatty's behaves.
    constexpr int modUpdateSamples = 32;
    int startSample = 0;
    while (startSample < buffer.getNumSamples())
    {
        const int chunkSize = juce::jmin (modUpdateSamples, buffer.getNumSamples() - startSample);

        float lfoValue = 0.0f;
        if (lfoDest != 0)
            lfoValue = lfoSource == 4 ? lfoHeldRandom : lfoWaveSample (lfoSource, lfoPhase);

        lfoPhase += lfoRateHz * chunkSize / getSampleRate();
        if (lfoPhase >= 1.0)
        {
            lfoPhase = std::fmod (lfoPhase, 1.0);
            lfoHeldRandom = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        }

        if (lfoDest == 1)        // Pitch: +/- one octave at full amount
            pitchSemitones.store (basePitchSemitones + lfoValue * lfoAmount * 12.0f);
        else if (lfoDest == 2)   // Filter Cutoff: +/- 4 octaves at full amount
            cutoffHz.store (juce::jlimit (20.0f, 20000.0f,
                baseCutoffHz * std::pow (2.0f, lfoValue * lfoAmount * 4.0f)));

        synth.renderNextBlock (buffer, midiMessages, startSample, chunkSize);

        if (lfoDest == 3)        // Amp: tremolo, ramped across the sub-block
        {
            const float gain = 1.0f - lfoAmount * 0.5f * (1.0f - lfoValue);
            buffer.applyGainRamp (startSample, chunkSize, lastAmpGain, gain);
            lastAmpGain = gain;
        }

        startSample += chunkSize;
    }

    juce::dsp::AudioBlock<float> block (buffer);

    if (apvts.getRawParameterValue ("gateOn")->load() >= 0.5f)
    {
        gate.setThreshold (apvts.getRawParameterValue ("gateThresh")->load());
        gate.setRatio     (apvts.getRawParameterValue ("gateRatio")->load());
        gate.setAttack    (apvts.getRawParameterValue ("gateAttack")->load());
        gate.setRelease   (apvts.getRawParameterValue ("gateRelease")->load());
        gate.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    if (apvts.getRawParameterValue ("ladderOn")->load() >= 0.5f)
    {
        ladder.setCutoffFrequencyHz (apvts.getRawParameterValue ("ladderCutoff")->load());
        ladder.setResonance         (apvts.getRawParameterValue ("ladderRes")->load());
        ladder.setDrive             (apvts.getRawParameterValue ("ladderDrive")->load());
        ladder.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    if (apvts.getRawParameterValue ("chorusOn")->load() >= 0.5f)
    {
        chorus.setRate  (apvts.getRawParameterValue ("chorusRate")->load());
        chorus.setDepth (apvts.getRawParameterValue ("chorusDepth")->load());
        chorus.setMix   (apvts.getRawParameterValue ("chorusMix")->load());
        chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    if (apvts.getRawParameterValue ("phaserOn")->load() >= 0.5f)
    {
        phaser.setRate     (apvts.getRawParameterValue ("phaserRate")->load());
        phaser.setDepth    (apvts.getRawParameterValue ("phaserDepth")->load());
        phaser.setFeedback (apvts.getRawParameterValue ("phaserFeedback")->load());
        phaser.setMix      (apvts.getRawParameterValue ("phaserMix")->load());
        phaser.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    if (apvts.getRawParameterValue ("reverbOn")->load() >= 0.5f)
    {
        const auto mix = apvts.getRawParameterValue ("reverbMix")->load();
        juce::Reverb::Parameters params;
        params.roomSize = apvts.getRawParameterValue ("reverbSize")->load();
        params.damping  = apvts.getRawParameterValue ("reverbDamp")->load();
        params.width    = apvts.getRawParameterValue ("reverbWidth")->load();
        params.wetLevel = mix;
        params.dryLevel = 1.0f - mix;
        reverb.setParameters (params);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    if (apvts.getRawParameterValue ("compOn")->load() >= 0.5f)
    {
        comp.setThreshold (apvts.getRawParameterValue ("compThresh")->load());
        comp.setRatio     (apvts.getRawParameterValue ("compRatio")->load());
        comp.setAttack    (apvts.getRawParameterValue ("compAttack")->load());
        comp.setRelease   (apvts.getRawParameterValue ("compRelease")->load());
        comp.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    if (apvts.getRawParameterValue ("limitOn")->load() >= 0.5f)
    {
        limiter.setThreshold (apvts.getRawParameterValue ("limitThresh")->load());
        limiter.setRelease   (apvts.getRawParameterValue ("limitRelease")->load());
        limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    oscilloscope.pushBuffer (buffer);
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
