#pragma once
#include <JuceHeader.h>
#include <vector>

struct Preset
{
    juce::String name;
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
            preset.name = presetXml->getStringAttribute ("name");

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
        { "Jump",
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

        // "Memories" — Jump's engine dropped an octave with a darker, resonant
        // filter; warmer and more nostalgic
        { "Memories",
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

        // "Lasers" — saw sub against a square lead with a slow, resonant
        // filter sweep that dives on release; sci-fi zap tails
        { "Lasers",
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
    };
    return presets;
}
