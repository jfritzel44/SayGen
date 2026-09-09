#pragma once
#include <JuceHeader.h>
#include <vector>

struct Preset
{
    juce::String name;
    // One of the fixed factory categories ("Bass", "Poly Short", "Poly
    // Long", "Synth Lead"), used to group the preset menu under section
    // headings. Left empty for user-saved patches, which get their own
    // trailing "User" section instead - see
    // MySynthAudioProcessorEditor::Content::rebuildPresetMenu().
    juce::String category;
    std::vector<std::pair<juce::String, float>> values; // paramID -> value (plain range)
};

//==============================================================================
// User-saved patches (via "Save Current Patch") live outside the binary, in
// one shared file, so they show up the same way whether you're in the
// standalone app, the AU, or the VST3, and survive a restart.
inline juce::File getUserPresetsFile()
{
    // JUCE's userApplicationDataDirectory resolves to bare ~/Library on
    // macOS, so the usual "Application Support" subfolder is added by hand
    // to land in the conventional per-app location
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                  #if JUCE_MAC
                   .getChildFile ("Application Support")
                  #endif
                   .getChildFile ("SynGen1");
    dir.createDirectory();
    return dir.getChildFile ("UserPresets.xml");
}

inline std::vector<Preset> loadUserPresets()
{
    std::vector<Preset> result;
    auto file = getUserPresetsFile();

    if (! file.existsAsFile())
        return result;

    if (auto xml = juce::XmlDocument::parse (file))
    {
        for (auto* presetXml : xml->getChildIterator())
        {
            if (! presetXml->hasTagName ("PRESET"))
                continue;

            Preset preset;
            preset.name     = presetXml->getStringAttribute ("name");
            preset.category = presetXml->getStringAttribute ("category");

            for (auto* paramXml : presetXml->getChildIterator())
                if (paramXml->hasTagName ("PARAM"))
                    preset.values.push_back ({ paramXml->getStringAttribute ("id"),
                                               (float) paramXml->getDoubleAttribute ("value") });

            result.push_back (std::move (preset));
        }
    }

    return result;
}

inline void saveUserPresets (const std::vector<Preset>& userPresets)
{
    juce::XmlElement root ("USERPRESETS");

    for (auto& preset : userPresets)
    {
        auto* presetXml = root.createNewChildElement ("PRESET");
        presetXml->setAttribute ("name", preset.name);
        presetXml->setAttribute ("category", preset.category);

        for (auto& [paramID, value] : preset.values)
        {
            auto* paramXml = presetXml->createNewChildElement ("PARAM");
            paramXml->setAttribute ("id", paramID);
            paramXml->setAttribute ("value", (double) value);
        }
    }

    root.writeTo (getUserPresetsFile());
}

inline const std::vector<Preset>& getFactoryPresets()
{
    // "Jump" — big OB-Xa-style polysynth: two sawtooths spread wide,
    // bright filter with a little envelope bite, fast attack, ringing release
    static const std::vector<Preset> presets =
    {
        { "Jump", "Poly Short",
          {
              { "oscType",    1.0f },     // Sawtooth
              { "osc2Type",   2.0f },     // Sawtooth
              { "osc1Octave", 1.0f },     // 8'
              { "osc2Octave", 1.0f },     // 8'
              { "detune",     12.0f },
              { "pitch",      0.0f },
              { "attack",     0.005f },
              { "decay",      0.3f },
              { "sustain",    0.85f },
              { "release",    0.35f },
              { "cutoff",     5200.0f },
              { "resonance",  0.9f },
              { "envAmount",  1.2f },
              { "fltAttack",  0.003f },
              { "fltDecay",   0.35f },
              { "fltSustain", 0.6f },
              { "fltRelease", 0.3f },
          } },

        // "Jump New" — a chorused take on "Jump": same twin-saw OB-Xa engine,
        // but with chorus on to fatten and spread the stack the way the
        // record's chorused Oberheim reads, plus a touch more detune and a
        // longer release so chords bloom and decay together
        { "Jump New", "Poly Short",
          {
              { "oscType",    1.0f },     // Sawtooth
              { "osc2Type",   2.0f },     // Sawtooth
              { "osc1Octave", 1.0f },     // 8'
              { "osc2Octave", 1.0f },     // 8'
              { "detune",     14.0f },
              { "pitch",      0.0f },
              { "attack",     0.005f },
              { "decay",      0.3f },
              { "sustain",    0.85f },
              { "release",    0.4f },
              { "cutoff",     5500.0f },
              { "resonance",  1.1f },
              { "envAmount",  1.2f },
              { "fltAttack",  0.003f },
              { "fltDecay",   0.35f },
              { "fltSustain", 0.6f },
              { "fltRelease", 0.35f },
              { "chorusOn",     1.0f },
              { "chorusRate",   0.8f },
              { "chorusDepth",  0.35f },
              { "chorusMix",    0.55f },
          } },

        // "Jump2" — a warmer, fuller take on "Jump": the phaser is gone
        // (its sweeping notches were reading as thin/metallic rather than
        // analog-fat), cutoff is pulled back and resonance eased off so the
        // top end rounds out instead of glaring, and a slower, deeper chorus
        // does the widening so the stack thickens without flanging
        { "Jump2", "Poly Short",
          {
              { "oscType",    1.0f },     // Sawtooth
              { "osc2Type",   2.0f },     // Sawtooth
              { "osc1Octave", 1.0f },     // 8'
              { "osc2Octave", 1.0f },     // 8'
              { "detune",     16.0f },
              { "pitch",      0.0f },
              { "attack",     0.008f },
              { "decay",      0.35f },
              { "sustain",    0.88f },
              { "release",    0.5f },
              { "cutoff",     3600.0f },
              { "resonance",  0.9f },
              { "envAmount",  1.0f },
              { "fltAttack",  0.006f },
              { "fltDecay",   0.4f },
              { "fltSustain", 0.65f },
              { "fltRelease", 0.45f },
              { "chorusOn",     1.0f },
              { "chorusRate",   0.4f },
              { "chorusDepth",  0.55f },
              { "chorusMix",    0.7f },
              { "reverbOn",     1.0f },
              { "reverbSize",   0.35f },
              { "reverbDamp",   0.7f },
              { "reverbWidth",  1.0f },
              { "reverbMix",    0.12f },
          } },

        // "Memories" — Jump's engine dropped an octave with a darker, resonant
        // filter; warmer and more nostalgic
        { "Memories", "Poly Short",
          {
              { "oscType",    1.0f },     // Sawtooth
              { "osc2Type",   2.0f },     // Sawtooth
              { "osc1Octave", 0.0f },     // 16'
              { "osc2Octave", 1.0f },     // 8'
              { "detune",     12.0f },
              { "pitch",      0.0f },
              { "attack",     0.005f },
              { "decay",      0.3f },
              { "sustain",    0.85f },
              { "release",    0.35f },
              { "cutoff",     2469.0f },
              { "resonance",  2.7f },
              { "envAmount",  1.2f },
              { "fltAttack",  0.003f },
              { "fltDecay",   0.35f },
              { "fltSustain", 0.6f },
              { "fltRelease", 0.3f },
          } },

        // "Drift" — slow-swelling poly pad: wide detune for a lush, chorused
        // spread, a gentle filter that opens in step with the long amp
        // attack, and a long tail so chords wash into each other
        { "Drift", "Poly Long",
          {
              { "oscType",    1.0f },     // Sawtooth
              { "osc2Type",   4.0f },     // Triangle
              { "osc1Octave", 1.0f },     // 8'
              { "osc2Octave", 1.0f },     // 8'
              { "detune",     15.0f },
              { "pitch",      0.0f },
              { "attack",     0.9f },
              { "decay",      0.6f },
              { "sustain",    0.8f },
              { "release",    1.8f },
              { "cutoff",     3200.0f },
              { "resonance",  1.2f },
              { "envAmount",  0.8f },
              { "fltAttack",  0.7f },
              { "fltDecay",   0.8f },
              { "fltSustain", 0.7f },
              { "fltRelease", 1.5f },
          } },

        // "Lasers" — saw sub against a square lead with a slow, resonant
        // filter sweep that dives on release; sci-fi zap tails
        { "Lasers", "Synth Lead",
          {
              { "oscType",    1.0f },     // Sawtooth
              { "osc2Type",   3.0f },     // Square
              { "osc1Octave", 0.0f },     // 16'
              { "osc2Octave", 1.0f },     // 8'
              { "detune",     0.0f },
              { "pitch",      0.0f },
              { "attack",     0.168f },
              { "decay",      0.3f },
              { "sustain",    0.26f },
              { "release",    1.347f },
              { "cutoff",     2918.0f },
              { "resonance",  3.0f },
              { "envAmount",  1.2f },
              { "fltAttack",  0.978f },
              { "fltDecay",   1.815f },
              { "fltSustain", 0.19f },
              { "fltRelease", 1.469f },
          } },

        // "Got a Match" - Minimoog-style mono lead, after Chick Corea's lead
        // line on the Elektric Band's "Got a Match?" (played on a real
        // Minimoog): twin saws just barely detuned for that in-tune-but-fat
        // beat, a bright but musical filter with some bite on the attack,
        // and glide for the legato slides between notes. Vibrato is dialed
        // in but silent until the mod wheel is raised (see
        // MySynthAudioProcessor::modWheelAmount) - a real player brings it
        // in by hand rather than it running constantly under a held note.
        { "Got a Match", "Synth Lead",
          {
              { "oscType",    1.0f },     // Sawtooth
              { "osc2Type",   2.0f },     // Sawtooth
              { "osc1Octave", 1.0f },     // 8'
              { "osc2Octave", 1.0f },     // 8'
              { "detune",     9.0f },
              { "pitch",      0.0f },
              { "attack",     0.008f },
              { "decay",      0.15f },
              { "sustain",    0.9f },
              { "release",    0.25f },
              { "overload",   0.18f },
              { "kbAmount",   0.4f },
              { "cutoff",     4200.0f },
              { "resonance",  2.4f },
              { "envAmount",  1.4f },
              { "fltAttack",  0.01f },
              { "fltDecay",   0.3f },
              { "fltSustain", 0.6f },
              { "fltRelease", 0.25f },
              { "glideOn",    1.0f },
              { "glideTime",  0.09f },
              { "lfoSource",  0.0f },     // Sine
              { "lfoDest",    1.0f },     // Pitch
              { "lfoRate",    5.2f },
              { "lfoAmount",  0.09f },
          } },

        // "Elektric Lead" - a punchier, more aggressive cousin of "Got a
        // Match": saw against square for extra edge, more drive and filter
        // bite, a quicker glide - suited to faster fusion solo lines.
        // Vibrato is likewise silent until the mod wheel is raised.
        { "Elektric Lead", "Synth Lead",
          {
              { "oscType",    1.0f },     // Sawtooth
              { "osc2Type",   3.0f },     // Square
              { "osc1Octave", 1.0f },     // 8'
              { "osc2Octave", 1.0f },     // 8'
              { "detune",     6.0f },
              { "pitch",      0.0f },
              { "attack",     0.004f },
              { "decay",      0.12f },
              { "sustain",    0.88f },
              { "release",    0.18f },
              { "overload",   0.3f },
              { "kbAmount",   0.4f },
              { "cutoff",     5200.0f },
              { "resonance",  3.2f },
              { "envAmount",  2.0f },
              { "fltAttack",  0.006f },
              { "fltDecay",   0.22f },
              { "fltSustain", 0.5f },
              { "fltRelease", 0.2f },
              { "glideOn",    1.0f },
              { "glideTime",  0.06f },
              { "lfoSource",  0.0f },     // Sine
              { "lfoDest",    1.0f },     // Pitch
              { "lfoRate",    5.8f },
              { "lfoAmount",  0.1f },
          } },

        // "Mischievous" - after the Little Phatty factory lead of the same
        // name (a Jan Hammer-ish patch): triangle against a square pitched
        // an octave up gives it a hollow, vocal quality rather than a
        // straight buzzy saw lead, and resonance is pushed hard enough that
        // the filter env sweep reads as a "wow" instead of a plain bite.
        // Glide is on for the bent, talking quality that style of lead
        // relies on; vibrato is dialed in but (like the other leads) stays
        // silent until the mod wheel is raised.
        { "Mischievous", "Synth Lead",
          {
              { "oscType",    3.0f },     // Triangle
              { "osc2Type",   3.0f },     // Square
              { "osc1Octave", 1.0f },     // 8'
              { "osc2Octave", 2.0f },     // 4'
              { "detune",     4.0f },
              { "pitch",      0.0f },
              { "attack",     0.006f },
              { "decay",      0.2f },
              { "sustain",    0.85f },
              { "release",    0.3f },
              { "overload",   0.22f },
              { "kbAmount",   0.35f },
              { "cutoff",     3000.0f },
              { "resonance",  4.0f },
              { "envAmount",  2.2f },
              { "fltAttack",  0.015f },
              { "fltDecay",   0.35f },
              { "fltSustain", 0.45f },
              { "fltRelease", 0.3f },
              { "glideOn",    1.0f },
              { "glideTime",  0.1f },
              { "lfoSource",  0.0f },     // Sine
              { "lfoDest",    1.0f },     // Pitch
              { "lfoRate",    5.5f },
              { "lfoAmount",  0.08f },
          } },

        // "Resofest 1" - after Surge XT's own factory lead of the same name
        // (surge-main/resources/data/patches_factory/Leads/Resofest 1.fxp,
        // decoded directly): the original stacks three separate Classic
        // oscillators at one pitch for its thickness - our Unison knob at 3
        // voices is the direct equivalent, so Osc 2 is left off entirely.
        // Resonance sits near (but capped just shy of) self-oscillation on
        // SynGen1's own Moog-style ladder filter, matching how close to the
        // edge Surge's own resonance (0.99 of 1.0) sits on its ladder. A
        // fast, deep filter envelope sweeps every note wide open before
        // settling back to a dark sustain (fltSustain 0, matching the
        // original's envelope-decays-to-nothing shape), and Drift is off -
        // the real patch's own "Drift" parameter is 0, so it never wanders
        // in pitch the way SynGen1's always-on analog drift previously would
        // have. Surge files this under "Monosynth (hard)", hence Glide.
        // The original also routes the mod wheel to the oscillator's own
        // shape for extra growl live - SynGen1 has no mod-wheel-to-
        // oscillator routing yet, so that part isn't reproduced here.
        { "Resofest 1", "Synth Lead",
          {
              { "oscType",       1.0f },   // Sawtooth
              { "osc2Type",      0.0f },   // Off - unison below covers the "3 oscillators"
              { "osc1Octave",    1.0f },   // 8'
              { "osc2Octave",    1.0f },   // 8'
              { "unisonVoices",  3.0f },
              { "pitch",         0.0f },
              { "driftAmount",   0.0f },   // matches the original patch's own Drift = 0
              { "attack",        0.003f },
              { "decay",         0.08f },
              { "sustain",       1.0f },
              { "release",       0.04f },
              { "overload",      0.3f },
              { "kbAmount",      0.5f },
              { "cutoff",        1300.0f },
              { "resonance",     9.0f },
              { "envAmount",     3.0f },
              { "fltAttack",     0.004f },
              { "fltDecay",      1.8f },
              { "fltSustain",    0.0f },
              { "fltRelease",    0.25f },
              { "glideOn",       1.0f },
              { "glideTime",     0.035f },
              { "delayOn",       1.0f },
              { "delayTime",     0.32f },
              { "delayFeedback", 0.42f },
              { "delayDamp",     0.35f },
              { "delayMix",      0.28f },
          } },

        // "Round Bass" - Chameleon/Prophet-style mono bass: twin detuned
        // saws down at 16', a fast filter pluck that dives to a dark, round
        // sustain, and a short snappy glide for the legato slides that
        // define the style
        { "Round Bass", "Bass",
          {
              { "oscType",    1.0f },     // Sawtooth
              { "osc2Type",   2.0f },     // Sawtooth
              { "osc1Octave", 0.0f },     // 16'
              { "osc2Octave", 0.0f },     // 16'
              { "detune",     8.0f },
              { "pitch",      0.0f },
              { "attack",     0.002f },
              { "decay",      0.18f },
              { "sustain",    0.75f },
              { "release",    0.12f },
              { "cutoff",     900.0f },
              { "resonance",  2.6f },
              { "envAmount",  2.0f },
              { "fltAttack",  0.008f },
              { "fltDecay",   0.17f },
              { "fltSustain", 0.15f },
              { "fltRelease", 0.1f },
              { "glideOn",    1.0f },
              { "glideTime",  0.055f },
          } },

        // "Reggae Woman" - Boogie On Reggae Woman-style TONTO/Moog bass:
        // rounder and softer than Round Bass, less filter snap, a longer
        // glide for its more legato slides
        { "Reggae Woman", "Bass",
          {
              { "oscType",    1.0f },     // Sawtooth
              { "osc2Type",   2.0f },     // Sawtooth
              { "osc1Octave", 0.0f },     // 16'
              { "osc2Octave", 0.0f },     // 16'
              { "detune",     5.0f },
              { "pitch",      0.0f },
              { "attack",     0.006f },
              { "decay",      0.25f },
              { "sustain",    0.85f },
              { "release",    0.2f },
              { "cutoff",     650.0f },
              { "resonance",  2.2f },
              { "envAmount",  1.6f },
              { "fltAttack",  0.003f },
              { "fltDecay",   0.3f },
              { "fltSustain", 0.35f },
              { "fltRelease", 0.18f },
              { "glideOn",    1.0f },
              { "glideTime",  0.14f },
          } },

        // "Chameleon" - after Herbie Hancock's ARP Odyssey bass line on the
        // Head Hunters title track: twin saws standing in for the Odyssey's
        // two VCOs, barely detuned for a bit of edge rather than a smooth
        // blend, run into a resonant filter with a fast, deep envelope
        // sweep so every note "pops" open before settling back down - the
        // percussive, vocal-sounding quack the part is built around.
        // Overload adds the mild grit real Odyssey filters have driven
        // hard, and a short glide handles the line's slides between notes.
        { "Chameleon", "Bass",
          {
              { "oscType",    1.0f },     // Sawtooth
              { "osc2Type",   2.0f },     // Sawtooth
              { "osc1Octave", 0.0f },     // 16'
              { "osc2Octave", 0.0f },     // 16'
              { "detune",     6.0f },
              { "pitch",      0.0f },
              { "attack",     0.002f },
              { "decay",      0.22f },
              { "sustain",    0.55f },
              { "release",    0.12f },
              { "overload",   0.35f },
              { "kbAmount",   0.3f },
              { "cutoff",     800.0f },
              { "resonance",  4.5f },
              { "envAmount",  3.0f },
              { "fltAttack",  0.002f },
              { "fltDecay",   0.22f },
              { "fltSustain", 0.15f },
              { "fltRelease", 0.13f },
              { "glideOn",    1.0f },
              { "glideTime",  0.06f },
          } },
    };
    return presets;
}
