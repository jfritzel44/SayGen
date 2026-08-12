#pragma once
#include <JuceHeader.h>
#include <vector>

struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values; // paramID -> value (plain range)
};

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
