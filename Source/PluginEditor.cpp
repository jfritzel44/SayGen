#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::String oscTypeName (double val)
{
    static const char* names[] = { "Sine", "Sawtooth", "Square", "Triangle" };
    return names[juce::jlimit (0, 3, (int)std::round (val))];
}

static juce::String osc2TypeName (double val)
{
    static const char* names[] = { "Off", "Sine", "Sawtooth", "Square", "Triangle" };
    return names[juce::jlimit (0, 4, (int)std::round (val))];
}

MySynthAudioProcessorEditor::MySynthAudioProcessorEditor (MySynthAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      qwertyKeyboard (p.keyboardState),
      oscTypeKnob  (p.apvts, "oscType",  "Osc 1", &oscLookAndFeel),
      osc2TypeKnob (p.apvts, "osc2Type", "Osc 2",  &oscLookAndFeel),
      osc1OctaveSelector (p.apvts, "osc1Octave", "Octave"),
      osc2OctaveSelector (p.apvts, "osc2Octave", "Octave"),
      detuneKnob   (p.apvts, "detune",   "Detune", &oscLookAndFeel),
      pitchKnob    (p.apvts, "pitch",    "Pitch", &oscLookAndFeel),
      cutoffKnob    (p.apvts, "cutoff",    "Cutoff",    &oscLookAndFeel),
      resonanceKnob (p.apvts, "resonance", "Resonance", &oscLookAndFeel),
      attackKnob  (p.apvts, "attack",  "Attack",     &oscLookAndFeel),
      decayKnob   (p.apvts, "decay",   "Decay",      &oscLookAndFeel),
      sustainKnob (p.apvts, "sustain", "Sustain",    &oscLookAndFeel),
      releaseKnob (p.apvts, "release", "Release",    &oscLookAndFeel),
      envAmountKnob  (p.apvts, "envAmount",  "Env Amt",  &oscLookAndFeel),
      fltAttackKnob  (p.apvts, "fltAttack",  "Attack",   &oscLookAndFeel),
      fltDecayKnob   (p.apvts, "fltDecay",   "Decay",    &oscLookAndFeel),
      fltSustainKnob (p.apvts, "fltSustain", "Sustain",  &oscLookAndFeel),
      fltReleaseKnob (p.apvts, "fltRelease", "Release",  &oscLookAndFeel)
{
    logoImage = juce::ImageCache::getFromMemory (BinaryData::logo_png,
                                                 BinaryData::logo_pngSize);

    setSize (1040, 880);
    startTimerHz (30);
    qwertyKeyboard.attachTo (*this);

    oscTypeKnob.getSlider().textFromValueFunction = oscTypeName;
    addAndMakeVisible (oscTypeKnob);

    osc2TypeKnob.getSlider().textFromValueFunction = osc2TypeName;
    addAndMakeVisible (osc2TypeKnob);

    addAndMakeVisible (osc1OctaveSelector);
    addAndMakeVisible (osc2OctaveSelector);

    detuneKnob.getSlider().setTextValueSuffix (" ct");
    addAndMakeVisible (detuneKnob);

    pitchKnob.getSlider().setTextValueSuffix (" st");
    addAndMakeVisible (pitchKnob);

    cutoffKnob.getSlider().setTextValueSuffix (" Hz");
    addAndMakeVisible (cutoffKnob);

    addAndMakeVisible (resonanceKnob);

    attackKnob.getSlider().setTextValueSuffix (" s");
    addAndMakeVisible (attackKnob);

    decayKnob.getSlider().setTextValueSuffix (" s");
    addAndMakeVisible (decayKnob);

    addAndMakeVisible (sustainKnob);

    releaseKnob.getSlider().setTextValueSuffix (" s");
    addAndMakeVisible (releaseKnob);

    envAmountKnob.getSlider().setTextValueSuffix (" oct");
    addAndMakeVisible (envAmountKnob);

    fltAttackKnob.getSlider().setTextValueSuffix (" s");
    addAndMakeVisible (fltAttackKnob);

    fltDecayKnob.getSlider().setTextValueSuffix (" s");
    addAndMakeVisible (fltDecayKnob);

    addAndMakeVisible (fltSustainKnob);

    fltReleaseKnob.getSlider().setTextValueSuffix (" s");
    addAndMakeVisible (fltReleaseKnob);
}

MySynthAudioProcessorEditor::~MySynthAudioProcessorEditor()
{
    stopTimer();
}

void MySynthAudioProcessorEditor::timerCallback()
{
    // Give the qwerty keyboard focus once we're on screen
    if (! hasGrabbedFocus && isShowing())
    {
        grabKeyboardFocus();
        hasGrabbedFocus = true;
    }

    midiLightOn = audioProcessor.midiActivity.exchange (false);
    repaint();
}

void MySynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2b2b));

    // MIDI indicator
    g.setColour (midiLightOn ? juce::Colours::limegreen : juce::Colour (0xff1a4a1a));
    g.fillEllipse (12, 12, 24, 24);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("MIDI", 8, 38, 32, 16, juce::Justification::centred);

    if (logoImage.isValid())
        g.drawImage (logoImage,
                     { getWidth() / 2.0f - 112.5f, 22.5f, 225.0f, 75.0f },
                     juce::RectanglePlacement::centred);

    auto drawSection = [&g] (juce::Rectangle<float> box, const juce::String& title)
    {
        g.setColour (juce::Colour (0xff3a3a3a));
        g.fillRoundedRectangle (box, 10.0f);
        g.setColour (juce::Colour (0xff888888));
        g.drawRoundedRectangle (box, 10.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (title, box.withHeight (28.0f).toNearestInt(),
                    juce::Justification::centred);
    };

    drawSection ({ (float) getWidth() / 2.0f - 420.0f,  130.0f, 840.0f, 260.0f }, "Oscillators");
    drawSection ({ 20.0f,  410.0f, 640.0f, 445.0f }, "Filter");
    drawSection ({ 680.0f, 410.0f, 340.0f, 418.0f }, "Amp");
}

void MySynthAudioProcessorEditor::resized()
{
    // Oscillators section: each osc knob gets its octave LED bank beside it,
    // then detune and pitch
    juce::Rectangle<int> oscRow (getWidth() / 2 - 400, 162, 800, 220);
    oscTypeKnob.setBounds        (oscRow.removeFromLeft (170));
    osc1OctaveSelector.setBounds (oscRow.removeFromLeft (70));
    osc2TypeKnob.setBounds       (oscRow.removeFromLeft (170));
    osc2OctaveSelector.setBounds (oscRow.removeFromLeft (70));
    detuneKnob.setBounds         (oscRow.removeFromLeft (160));
    pitchKnob.setBounds          (oscRow);

    // Filter section: cutoff row, then its envelope row (small knobs: the
    // 151-wide boxes cap the drawn knob at 81px vs the big rows' 108px)
    juce::Rectangle<int> filterRow (40, 442, 600, 220);
    cutoffKnob.setBounds    (filterRow.removeFromLeft (200));
    resonanceKnob.setBounds (filterRow.removeFromLeft (200));
    envAmountKnob.setBounds (filterRow);

    juce::Rectangle<int> filterEnvRow (38, 662, 604, 193);
    fltAttackKnob.setBounds  (filterEnvRow.removeFromLeft (151));
    fltDecayKnob.setBounds   (filterEnvRow.removeFromLeft (151));
    fltSustainKnob.setBounds (filterEnvRow.removeFromLeft (151));
    fltReleaseKnob.setBounds (filterEnvRow);

    // Amp section: ADSR in a 2x2 grid
    attackKnob.setBounds  (699, 442, 151, 193);
    decayKnob.setBounds   (850, 442, 151, 193);
    sustainKnob.setBounds (699, 635, 151, 193);
    releaseKnob.setBounds (850, 635, 151, 193);
}
