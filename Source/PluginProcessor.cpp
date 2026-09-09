#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <algorithm>

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

    // Stacks this many detuned copies of each oscillator (fixed spread, see
    // MySynthAudioProcessor::unisonDetuneCents) for a wider, chorused tone;
    // 1 = off, matching the original single-oscillator behaviour exactly
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "unisonVoices", "Unison",
        juce::NormalisableRange<float> (1.0f, (float) MySynthVoice::kMaxUnisonVoices, 1.0f), 1.0f));

    // "Modern" oscillator: continuous saw/pulse/triangle mix + sub-octave,
    // independent per oscillator, in place of oscType/osc2Type's single-
    // waveform pick when its "on" toggle is enabled. Lives in its own
    // overlay panel rather than the (already full) main oscillator row.
    auto addModernOscParams = [&layout] (const juce::String& prefix, const juce::String& label)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (
            prefix + "ModernOn", label + " Modern", false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            prefix + "SawMix", label + " Modern Saw",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            prefix + "PulseMix", label + " Modern Pulse",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            prefix + "TriMix", label + " Modern Triangle",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            prefix + "PulseWidth", label + " Modern Width",
            juce::NormalisableRange<float> (0.02f, 0.98f, 0.01f), 0.5f));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            prefix + "SubOctave", label + " Modern Sub", false));
    };
    addModernOscParams ("osc1", "Oscillator 1");
    addModernOscParams ("osc2", "Oscillator 2");

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "pitch", "Pitch",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f));

    // Analog-style pitch instability (see MySynthVoice::renderNextBlock):
    // 0 = perfectly stable, 1 = full drift (the default every existing
    // preset was authored against)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "driftAmount", "Drift",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    // Glide (portamento): forces single-note mono voicing (see
    // MySynthAudioProcessor::applyGlideVoicing) and slides the pitch of a
    // legato note into the next rather than retriggering, the way a classic
    // analog mono-bass patch plays
    layout.add (std::make_unique<juce::AudioParameterBool> (
        "glideOn", "Glide", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "glideTime", "Glide Time",
        juce::NormalisableRange<float> (0.001f, 1.5f, 0.001f, 0.4f), 0.08f));

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
        "delayOn", "Delay On", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "delayTime", "Delay Time",
        juce::NormalisableRange<float> (0.02f, 1.5f, 0.001f, 0.4f), 0.3f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "delayFeedback", "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, 0.9f, 0.01f), 0.35f));

    // How much each repeat darkens relative to the one before it (a one-pole
    // lowpass inside the feedback loop) - 0 keeps every repeat as bright as
    // the input, dialing up gives the fading, tape-echo-style character
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "delayDamp", "Delay Damp",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "delayMix", "Delay Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

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

    // Final output gain stage, applied after every effect and independent
    // of the amp envelope — a straightforward "how loud does this patch
    // come out" control rather than part of the voice itself
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "masterVolume", "Master Volume",
        juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f));

    // How played velocity maps to level: 0 = linear/untouched, positive
    // boosts quieter notes, negative suppresses them. A player-feel setting
    // rather than part of any one patch, so (like Master Volume) it's not
    // captured by "Save Current Patch"
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "velocityCurve", "Velocity Curve",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

    // Appended to preserve existing host parameter indices.
    for (auto id : { "osc1Level", "osc2Level" })
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id, juce::String (id) == "osc1Level" ? "Oscillator 1 Level" : "Oscillator 2 Level",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "filterCompensation", "Filter Bass Compensation",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "envelopeCurve", "Envelope Curve",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.65f));
    for (auto prefix : { juce::String ("osc1"), juce::String ("osc2") })
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            prefix + "PhaseMode", prefix + " Phase Mode",
            juce::StringArray { "Retrigger", "Random", "Free" }, 0));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            prefix + "StartPhase", prefix + " Start Phase",
            juce::NormalisableRange<float> (0.0f, 360.0f, 0.1f), prefix == "osc1" ? 0.0f : 90.0f));
    }
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "phaseRandomness", "Phase Randomness",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "unisonSpread", "Unison Detune",
        juce::NormalisableRange<float> (0.0f, 50.0f, 0.1f), 14.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "unisonWidth", "Unison Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.9f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "osc2Coarse", "Oscillator 2 Coarse Tune",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f));
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
    numFactoryPresets = presets.size();

    for (auto& userPreset : loadUserPresets())
        presets.push_back (std::move (userPreset));
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

    // Reset optional sound controls so patches cannot inherit another patch's settings.
    for (auto id : { "osc1Level", "osc2Level", "filterCompensation", "envelopeCurve",
                     "osc1PhaseMode", "osc2PhaseMode", "osc1StartPhase", "osc2StartPhase",
                     "phaseRandomness", "unisonSpread", "unisonWidth", "osc2Coarse" })
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getDefaultValue());

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
        "oscType", "osc2Type", "osc1Octave", "osc2Octave", "detune", "unisonVoices", "pitch", "driftAmount",
        "osc1ModernOn", "osc1SawMix", "osc1PulseMix", "osc1TriMix", "osc1PulseWidth", "osc1SubOctave",
        "osc2ModernOn", "osc2SawMix", "osc2PulseMix", "osc2TriMix", "osc2PulseWidth", "osc2SubOctave",
        "attack", "decay", "sustain", "release",
        "cutoff", "resonance", "envAmount",
        "fltAttack", "fltDecay", "fltSustain", "fltRelease",
        "glideOn", "glideTime", "overload", "kbAmount",
        "osc1Level", "osc2Level", "filterCompensation", "envelopeCurve",
        "osc1PhaseMode", "osc2PhaseMode", "osc1StartPhase", "osc2StartPhase",
        "phaseRandomness", "unisonSpread", "unisonWidth", "osc2Coarse",
    };

    Preset preset;
    preset.name     = name;
    preset.category = "User";

    for (auto* paramID : patchParamIDs)
        if (auto* param = apvts.getParameter (paramID))
            preset.values.push_back ({ paramID, param->convertFrom0to1 (param->getValue()) });

    presets.push_back (std::move (preset));
    currentProgram = (int) presets.size() - 1;

    // Persist just the user-saved patches (the factory ones are baked into
    // the binary and always come back on their own) so this one is still
    // here next time the app/plugin is opened
    saveUserPresets ({ presets.begin() + (long) numFactoryPresets, presets.end() });
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
        voice->osc1PhaseMode = &osc1PhaseMode;
        voice->osc2PhaseMode = &osc2PhaseMode;
        voice->osc1StartPhase = &osc1StartPhase;
        voice->osc2StartPhase = &osc2StartPhase;
        voice->phaseRandomness = &phaseRandomness;
        voice->unisonWidth = &unisonWidth;
        voice->osc2Coarse = &osc2Coarse;
        voice->osc1Level = &osc1Level;
        voice->osc2Level = &osc2Level;
        voice->filterCompensation = &filterCompensation;
        voice->envelopeCurve = &envelopeCurve;
        voice->oscType        = &oscType;
        voice->osc2Type       = &osc2Type;
        voice->osc1Octave     = &osc1Octave;
        voice->osc2Octave     = &osc2Octave;
        voice->oscSync        = &oscSync;
        voice->detuneCents    = &detuneCents;
        voice->osc1UnisonVoices = &unisonVoices;
        voice->osc2UnisonVoices = &unisonVoices;
        voice->osc1UnisonDetune = &unisonDetuneCents;
        voice->osc2UnisonDetune = &unisonDetuneCents;
        voice->osc1ModernOn    = &osc1ModernOn;
        voice->osc1SawMix      = &osc1SawMix;
        voice->osc1PulseMix    = &osc1PulseMix;
        voice->osc1TriMix      = &osc1TriMix;
        voice->osc1PulseWidth  = &osc1PulseWidth;
        voice->osc1SubOctave   = &osc1SubOctave;
        voice->osc2ModernOn    = &osc2ModernOn;
        voice->osc2SawMix      = &osc2SawMix;
        voice->osc2PulseMix    = &osc2PulseMix;
        voice->osc2TriMix      = &osc2TriMix;
        voice->osc2PulseWidth  = &osc2PulseWidth;
        voice->osc2SubOctave   = &osc2SubOctave;
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
        voice->velocityCurve  = &velocityCurveAmount;
        voice->pitchBend      = &pitchBendSemitones;
        voice->driftAmount    = &driftAmount;
        synth.addVoice (voice);
    }

    synth.clearSounds();
    synth.addSound (new MySynthSound());

    synth.setCurrentPlaybackSampleRate (sampleRate);
    synth.setMinimumRenderingSubdivisionSize (1, true);
    enhancedMidi.ensureSize (2048);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                  (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
    gate.prepare (spec);
    gate.reset();
    ladder.prepare (spec);
    ladder.setMode (juce::dsp::LadderFilterMode::LPF24);
    ladder.reset();
    chorus.prepare (spec);
    // 7ms sits in flanger territory - the dry/wet copies are close enough
    // together that they comb-filter into sparse, widely-spaced notches,
    // which reads as a metallic/thin swirl rather than a chorus thickening.
    // Real analog chorus (Juno, CE-1) centres around 20-30ms; that longer
    // delay packs the comb notches close enough together to sound like
    // smooth thickening instead of an audible sweep.
    chorus.setCentreDelay (22.0f);
    chorus.setFeedback (0.0f);
    chorus.reset();
    phaser.prepare (spec);
    phaser.setCentreFrequency (1300.0f);
    phaser.reset();
    reverb.prepare (spec);
    reverb.reset();
    delayLine.prepare (spec);
    delayLine.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
    delayLine.reset();
    delayDampState.assign (juce::jmax (1, (int) spec.numChannels), 0.0f);
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

void MySynthAudioProcessor::applyGlideVoicing (juce::MidiBuffer& midiMessages)
{
    if (! glideOn.load())
    {
        // Glide just got switched off (or was never on) - drop any stack
        // left over from a previous run so a later glide-on doesn't resume
        // from stale state
        if (! monoNoteStack.empty())
            monoNoteStack.clear();
        return;
    }

    const float glideSecs = glideTimeSeconds.load();

    // Finds the (normally singular) voice currently sounding, so its pitch/
    // level can be retargeted directly rather than routed back through
    // MIDI - direct calls also sidestep Synthesiser's note-number matching,
    // which would otherwise get confused once a voice's sounding note no
    // longer matches the note that originally started it.
    auto forEachActiveVoice = [this] (auto&& fn)
    {
        bool foundOne = false;
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* voice = dynamic_cast<MySynthVoice*> (synth.getVoice (i)))
                if (voice->isVoiceActive())
                {
                    fn (*voice);
                    foundOne = true;
                }
        return foundOne;
    };

    juce::MidiBuffer passThrough;

    for (const auto metadata : midiMessages)
    {
        const auto message    = metadata.getMessage();
        const auto samplePos  = metadata.samplePosition;

        if (message.isNoteOn())
        {
            const HeldNote held { message.getNoteNumber(), message.getFloatVelocity(), message.getChannel() };
            const bool wasEmpty = monoNoteStack.empty();

            // A re-press of an already-held note (e.g. a stuck-key repeat)
            // just moves it back to the top rather than stacking a duplicate
            monoNoteStack.erase (std::remove_if (monoNoteStack.begin(), monoNoteStack.end(),
                [&] (const HeldNote& n) { return n.note == held.note && n.channel == held.channel; }),
                monoNoteStack.end());
            monoNoteStack.push_back (held);

            if (wasEmpty)
            {
                // Nothing held yet: a normal trigger, full envelope attack
                passThrough.addEvent (message, samplePos);
            }
            else
            {
                // Legato: retarget the sounding voice instead of retriggering.
                // If nothing is actually active yet (e.g. the note that
                // started it hasn't been rendered in this block yet), fall
                // back to a normal trigger so the note is never silently lost.
                const bool retargeted = forEachActiveVoice ([&] (MySynthVoice& v)
                {
                    v.setGlideTarget (held.note, held.velocity, glideSecs);
                });
                if (! retargeted)
                    passThrough.addEvent (message, samplePos);
            }
        }
        else if (message.isNoteOff())
        {
            const auto note    = message.getNoteNumber();
            const auto channel = message.getChannel();

            monoNoteStack.erase (std::remove_if (monoNoteStack.begin(), monoNoteStack.end(),
                [&] (const HeldNote& n) { return n.note == note && n.channel == channel; }),
                monoNoteStack.end());

            if (monoNoteStack.empty())
            {
                // Last held note released: let the voice actually stop,
                // called directly (see forEachActiveVoice) rather than
                // passing the note-off through, since Synthesiser would try
                // to match it by note number and this voice may well be
                // sounding a different one after however many glides
                forEachActiveVoice ([&] (MySynthVoice& v)
                {
                    v.stopNote (message.getVelocity() / 127.0f, true);
                });
            }
            else
            {
                // Notes still held: glide back to whichever is now on top,
                // no retrigger
                const auto& top = monoNoteStack.back();
                forEachActiveVoice ([&] (MySynthVoice& v)
                {
                    v.setGlideTarget (top.note, top.velocity, glideSecs);
                });
            }
        }
        else
        {
            passThrough.addEvent (message, samplePos);
        }
    }

    midiMessages.swapWith (passThrough);
}

void MySynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    osc1PhaseMode.store ((int) apvts.getRawParameterValue ("osc1PhaseMode")->load());
    osc2PhaseMode.store ((int) apvts.getRawParameterValue ("osc2PhaseMode")->load());
    osc1StartPhase.store (apvts.getRawParameterValue ("osc1StartPhase")->load());
    osc2StartPhase.store (apvts.getRawParameterValue ("osc2StartPhase")->load());
    phaseRandomness.store (apvts.getRawParameterValue ("phaseRandomness")->load());
    unisonDetuneCents.store (apvts.getRawParameterValue ("unisonSpread")->load());
    unisonWidth.store (apvts.getRawParameterValue ("unisonWidth")->load());
    osc2Coarse.store (apvts.getRawParameterValue ("osc2Coarse")->load());
    osc1Level.store (apvts.getRawParameterValue ("osc1Level")->load());
    osc2Level.store (apvts.getRawParameterValue ("osc2Level")->load());
    filterCompensation.store (apvts.getRawParameterValue ("filterCompensation")->load());
    envelopeCurve.store (apvts.getRawParameterValue ("envelopeCurve")->load());

    // Sync parameters to atomics read by voices
    oscType.store ((int)std::round (apvts.getRawParameterValue ("oscType")->load()));
    osc2Type.store ((int)std::round (apvts.getRawParameterValue ("osc2Type")->load()));
    osc1Octave.store ((int)std::round (apvts.getRawParameterValue ("osc1Octave")->load()));
    osc2Octave.store ((int)std::round (apvts.getRawParameterValue ("osc2Octave")->load()));
    oscSync.store (apvts.getRawParameterValue ("oscSync")->load() >= 0.5f);
    detuneCents.store (apvts.getRawParameterValue ("detune")->load());
    driftAmount.store (apvts.getRawParameterValue ("driftAmount")->load());
    unisonVoices.store ((int) std::round (apvts.getRawParameterValue ("unisonVoices")->load()));

    osc1ModernOn.store   (apvts.getRawParameterValue ("osc1ModernOn")->load() >= 0.5f);
    osc1SawMix.store     (apvts.getRawParameterValue ("osc1SawMix")->load());
    osc1PulseMix.store   (apvts.getRawParameterValue ("osc1PulseMix")->load());
    osc1TriMix.store     (apvts.getRawParameterValue ("osc1TriMix")->load());
    osc1PulseWidth.store (apvts.getRawParameterValue ("osc1PulseWidth")->load());
    osc1SubOctave.store  (apvts.getRawParameterValue ("osc1SubOctave")->load() >= 0.5f);
    osc2ModernOn.store   (apvts.getRawParameterValue ("osc2ModernOn")->load() >= 0.5f);
    osc2SawMix.store     (apvts.getRawParameterValue ("osc2SawMix")->load());
    osc2PulseMix.store   (apvts.getRawParameterValue ("osc2PulseMix")->load());
    osc2TriMix.store     (apvts.getRawParameterValue ("osc2TriMix")->load());
    osc2PulseWidth.store (apvts.getRawParameterValue ("osc2PulseWidth")->load());
    osc2SubOctave.store  (apvts.getRawParameterValue ("osc2SubOctave")->load() >= 0.5f);
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
    velocityCurveAmount.store (apvts.getRawParameterValue ("velocityCurve")->load());
    glideOn.store (apvts.getRawParameterValue ("glideOn")->load() >= 0.5f);
    glideTimeSeconds.store (apvts.getRawParameterValue ("glideTime")->load());

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

    // Collapse to single-note mono voicing with portamento when Glide is on
    // (see applyGlideVoicing / the HeldNote stack for how); a no-op pass-
    // through otherwise
    // Event-bounded rendering keeps note, glide, bend and wheel changes at
    // their requested sample positions. Never hand future events to JUCE.
    // Render in small sub-blocks instead of the whole buffer at once, so
    // pitch/cutoff/amp modulation gets updated far more often than the
    // host's block size. A once-per-block update makes the LFO audibly
    // step/zipper; updating every ~32 samples (and ramping the amp gain
    // across each sub-block) is what makes it read as continuous motion
    // instead, closer to how an analog LFO like the Little Phatty's behaves.
    constexpr int modUpdateSamples = 32;
    auto event = midiMessages.cbegin();
    int startSample = 0;
    while (startSample < buffer.getNumSamples())
    {
        int chunkSize = juce::jmin (modUpdateSamples, buffer.getNumSamples() - startSample);
        {
            enhancedMidi.clear();
            while (event != midiMessages.cend() && (*event).samplePosition <= startSample)
            {
                const auto message = (*event).getMessage();
                enhancedMidi.addEvent (message, startSample);
                if (message.isPitchWheel())
                    pitchBendSemitones.store ((float) (message.getPitchWheelValue() - 8192)
                                             / 8192.0f * pitchBendRangeSemitones);
                else if (message.isController() && message.getControllerNumber() == 1)
                    modWheelAmount.store ((float) message.getControllerValue() / 127.0f);
                ++event;
            }
            if (event != midiMessages.cend())
                chunkSize = juce::jmin (chunkSize, (*event).samplePosition - startSample);
            applyGlideVoicing (enhancedMidi);
        }

        float lfoValue = 0.0f;
        if (lfoDest != 0)
            lfoValue = lfoSource == 4 ? lfoHeldRandom : lfoWaveSample (lfoSource, lfoPhase);

        lfoPhase += lfoRateHz * chunkSize / getSampleRate();
        if (lfoPhase >= 1.0)
        {
            lfoPhase = std::fmod (lfoPhase, 1.0);
            lfoHeldRandom = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        }

        if (lfoDest == 1)        // Pitch: +/- one octave at full amount and full mod wheel
            pitchSemitones.store (basePitchSemitones
                + lfoValue * lfoAmount * modWheelAmount.load() * 12.0f);
        else if (lfoDest == 2)   // Filter Cutoff: +/- 4 octaves at full amount
            cutoffHz.store (juce::jlimit (20.0f, 20000.0f,
                baseCutoffHz * std::pow (2.0f, lfoValue * lfoAmount * 4.0f)));

        synth.renderNextBlock (buffer, enhancedMidi, startSample, chunkSize);

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

    if (apvts.getRawParameterValue ("delayOn")->load() >= 0.5f)
    {
        const float delayTimeSec = apvts.getRawParameterValue ("delayTime")->load();
        const float feedback     = apvts.getRawParameterValue ("delayFeedback")->load();
        const float damp         = apvts.getRawParameterValue ("delayDamp")->load();
        const float mix          = apvts.getRawParameterValue ("delayMix")->load();

        delayLine.setDelay (delayTimeSec * (float) getSampleRate());

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            float& dampState = delayDampState[(size_t) ch];

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float in  = data[i];
                const float tap = delayLine.popSample (ch);

                // One-pole lowpass inside the feedback loop, so each repeat
                // comes back a little darker than the one before it (tape-
                // echo style) instead of every repeat being a pristine copy
                dampState += damp * (tap - dampState);

                delayLine.pushSample (ch, in + dampState * feedback);
                data[i] = in + (tap - in) * mix;
            }
        }
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

    // Final output gain stage: independent of the amp envelope, applied
    // after every effect so it's the true last thing to touch the signal
    const float masterVolumeDb = apvts.getRawParameterValue ("masterVolume")->load();
    buffer.applyGain (juce::Decibels::decibelsToGain (masterVolumeDb, -60.0f));

    oscilloscope.pushBuffer (buffer);
    outputMeter.pushBuffer (buffer);
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
        newState.removeChild (newState.getChildWithProperty ("id", "enhancedEngine"), nullptr);

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
