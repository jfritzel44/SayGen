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
      lcdScreen (p.apvts),
      oscilloscope (p.oscilloscope),
      oscTypeKnob  (p.apvts, "oscType",  "Osc 1", &oscLookAndFeel),
      osc2TypeKnob (p.apvts, "osc2Type", "Osc 2",  &oscLookAndFeel),
      osc1OctaveSelector (p.apvts, "osc1Octave", "Octave"),
      osc2OctaveSelector (p.apvts, "osc2Octave", "Octave"),
      oscSyncButton (p.apvts, "oscSync", "1-2 Sync"),
      detuneKnob   (p.apvts, "detune",   "Detune", &oscLookAndFeel),
      pitchKnob    (p.apvts, "pitch",    "Pitch", &oscLookAndFeel),
      overloadKnob (p.apvts, "overload", "Overload", &oscLookAndFeel),
      kbAmountKnob (p.apvts, "kbAmount", "KB Amount", &oscLookAndFeel),
      cutoffKnob    (p.apvts, "cutoff",    "Cutoff",    &oscLookAndFeel),
      resonanceKnob (p.apvts, "resonance", "Resonance", &oscLookAndFeel),
      attackKnob  (p.apvts, "attack",  "Attack",     &oscLookAndFeel),
      decayKnob   (p.apvts, "decay",   "Decay",      &oscLookAndFeel),
      sustainKnob (p.apvts, "sustain", "Sustain",    &oscLookAndFeel),
      releaseKnob (p.apvts, "release", "Release",    &oscLookAndFeel),
      envAmountKnob  (p.apvts, "envAmount",  "EGR Amount",  &oscLookAndFeel),
      fltAttackKnob  (p.apvts, "fltAttack",  "Attack",   &oscLookAndFeel),
      fltDecayKnob   (p.apvts, "fltDecay",   "Decay",    &oscLookAndFeel),
      fltSustainKnob (p.apvts, "fltSustain", "Sustain",  &oscLookAndFeel),
      fltReleaseKnob (p.apvts, "fltRelease", "Release",  &oscLookAndFeel),
      lfoRateKnob    (p.apvts, "lfoRate",    "LFO Rate", &oscLookAndFeel),
      lfoAmountKnob  (p.apvts, "lfoAmount",  "Amount",   &oscLookAndFeel)
{
    logoImage = juce::ImageCache::getFromMemory (BinaryData::logo_png,
                                                 BinaryData::logo_pngSize);

    setSize (1040, 642);
    startTimerHz (30);
    qwertyKeyboard.attachTo (*this);

    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff3a3a3a));
    presetBox.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff888888));
    presetBox.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    presetBox.setColour (juce::ComboBox::arrowColourId,      juce::Colours::white);
    rebuildPresetMenu();
    presetBox.onChange = [this]
    {
        auto id = presetBox.getSelectedId();
        if (id == saveCurrentPatchItemId)
        {
            // Don't leave "Save Current Patch" showing as if it were a
            // selected preset — revert to whatever's actually loaded first
            presetBox.setSelectedId (audioProcessor.getCurrentProgram() + firstPresetItemId,
                                     juce::dontSendNotification);
            showSavePatchDialog();
            return;
        }

        auto index = id - firstPresetItemId;
        if (index >= 0)
            audioProcessor.setCurrentProgram (index);
    };
    addAndMakeVisible (presetBox);
    addAndMakeVisible (lcdScreen);
    addAndMakeVisible (oscilloscope);

    oscTypeKnob.getSlider().textFromValueFunction = oscTypeName;
    addAndMakeVisible (oscTypeKnob);

    osc2TypeKnob.getSlider().textFromValueFunction = osc2TypeName;
    addAndMakeVisible (osc2TypeKnob);

    addAndMakeVisible (osc1OctaveSelector);
    addAndMakeVisible (osc2OctaveSelector);

    addAndMakeVisible (oscSyncButton);

    detuneKnob.getSlider().setTextValueSuffix (" ct");
    addAndMakeVisible (detuneKnob);

    pitchKnob.getSlider().setTextValueSuffix (" st");
    addAndMakeVisible (pitchKnob);

    addAndMakeVisible (overloadKnob);
    addAndMakeVisible (kbAmountKnob);

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

    lfoRateKnob.getSlider().setTextValueSuffix (" Hz");
    addAndMakeVisible (lfoRateKnob);
    addAndMakeVisible (lfoAmountKnob);

    auto styleComboBox = [] (juce::ComboBox& box)
    {
        box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff3a3a3a));
        box.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff888888));
        box.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
        box.setColour (juce::ComboBox::arrowColourId,      juce::Colours::white);
    };

    lfoSourceBox.addItemList ({ "Sine", "Triangle", "Square", "Saw", "S&H" }, 1);
    styleComboBox (lfoSourceBox);
    lfoSourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (p.apvts, "lfoSource", lfoSourceBox);
    addAndMakeVisible (lfoSourceBox);

    lfoDestBox.addItemList ({ "Off", "Pitch", "Cutoff", "Amp" }, 1);
    styleComboBox (lfoDestBox);
    lfoDestAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (p.apvts, "lfoDest", lfoDestBox);
    addAndMakeVisible (lfoDestBox);
}

MySynthAudioProcessorEditor::~MySynthAudioProcessorEditor()
{
    stopTimer();
}

void MySynthAudioProcessorEditor::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addItem ("Save Current Patch", saveCurrentPatchItemId);
    presetBox.addSeparator();

    int itemId = firstPresetItemId;
    for (auto& preset : audioProcessor.getPresets())
        presetBox.addItem (preset.name, itemId++);
}

void MySynthAudioProcessorEditor::showSavePatchDialog()
{
    auto* aw = new juce::AlertWindow ("Save Current Patch",
                                      "Enter a name for this patch:",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", "", "Name:");
    aw->getTextEditor ("name")->setInputRestrictions (25);
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
    {
        if (result == 1)
        {
            auto name = aw->getTextEditorContents ("name").substring (0, 25).trim();
            if (name.isNotEmpty())
            {
                audioProcessor.saveCurrentPatchAsPreset (name);
                rebuildPresetMenu();
                presetBox.setSelectedId (audioProcessor.getCurrentProgram() + firstPresetItemId,
                                         juce::dontSendNotification);
            }
        }
    }), true);
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

    // Menu bar
    g.setColour (juce::Colour (0xff1f1f1f));
    g.fillRect (0, 0, getWidth(), 40);
    g.setColour (juce::Colour (0xff555555));
    g.drawHorizontalLine (40, 0.0f, (float) getWidth());

    // MIDI indicator
    g.setColour (midiLightOn ? juce::Colours::limegreen : juce::Colour (0xff1a4a1a));
    g.fillEllipse (12, 46, 20, 20);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("MIDI", 6, 68, 32, 14, juce::Justification::centred);

    // Logo and oscilloscope sit in a row above the effects panel, spanning
    // the same width as (and centred with) the LCD screen beneath them
    if (logoImage.isValid())
        g.drawImage (logoImage,
                     { 342.0f, 48.0f, 110.0f, 44.0f },
                     juce::RectanglePlacement::centred);

    // "Select Preset" label sits beside the preset menu, which is below
    // the logo/oscilloscope row
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Select Preset", 342, 100, 110, 26, juce::Justification::centredLeft);

    auto drawSection = [&g] (juce::Rectangle<float> box, const juce::String& title)
    {
        g.setColour (juce::Colour (0xff3a3a3a));
        g.fillRoundedRectangle (box, 10.0f);
        g.setColour (juce::Colour (0xff888888));
        g.drawRoundedRectangle (box, 10.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (13.2f));
        // Shrink the centring box so the text sits nearer the top, leaving
        // ~5px more breathing room below it before the knobs start.
        g.drawText (title, box.withHeight (18.0f).toNearestInt(),
                    juce::Justification::centred);
    };

    drawSection ({ 150.0f, 286.0f, 620.0f, 124.0f }, "Oscillators");
    drawSection ({ 20.0f, 286.0f, 120.0f, 342.0f }, "Filter Mod");
    drawSection ({ 150.0f, 418.0f, 460.0f, 210.0f }, "Filter");
    drawSection ({ 630.0f, 418.0f, 260.0f, 210.0f }, "Amp");
    drawSection ({ 900.0f, 286.0f, 120.0f, 342.0f }, "Modulation");
}

void MySynthAudioProcessorEditor::resized()
{
    // Preset menu sits below the logo/scope row, to the right of its label
    presetBox.setBounds (460, 100, 238, 26);

    // LCD effects screen at the top center, beneath the preset menu row
    lcdScreen.setBounds (getWidth() / 2 - 178, 134, 356, 144);

    // Oscilloscope sits to the right of the logo, above the effects panel,
    // matching the LCD's right edge
    oscilloscope.setBounds (462, 48, 236, 44);

    // Oscillators section: each osc knob gets its octave LED bank beside it,
    // then the 1-2 sync toggle, then detune and pitch, all on one row
    juce::Rectangle<int> oscRow (158, 310, 604, 88);
    oscTypeKnob.setBounds        (oscRow.removeFromLeft (110));
    osc1OctaveSelector.setBounds (oscRow.removeFromLeft (64));
    osc2TypeKnob.setBounds       (oscRow.removeFromLeft (110));
    osc2OctaveSelector.setBounds (oscRow.removeFromLeft (64));
    oscSyncButton.setBounds      (oscRow.removeFromLeft (56));
    detuneKnob.setBounds         (oscRow.removeFromLeft (100));
    pitchKnob.setBounds          (oscRow.removeFromLeft (100));

    // Filter Mod panel left of the filter: Overload, EGR (envelope) amount,
    // and keyboard tracking amount stacked in their own column. It runs
    // from the top of the Oscillators section down to the bottom of the
    // Filter/Amp panels, so the knobs get to stay full size
    overloadKnob.setBounds  (30, 326, 100, 88);
    envAmountKnob.setBounds (30, 427, 100, 88);
    kbAmountKnob.setBounds  (30, 528, 100, 88);

    // Filter section: cutoff/resonance row, then its envelope row
    juce::Rectangle<int> filterRow (280, 442, 200, 88);
    cutoffKnob.setBounds    (filterRow.removeFromLeft (100));
    resonanceKnob.setBounds (filterRow);

    juce::Rectangle<int> filterEnvRow (180, 532, 400, 88);
    fltAttackKnob.setBounds  (filterEnvRow.removeFromLeft (100));
    fltDecayKnob.setBounds   (filterEnvRow.removeFromLeft (100));
    fltSustainKnob.setBounds (filterEnvRow.removeFromLeft (100));
    fltReleaseKnob.setBounds (filterEnvRow);

    // Amp section: ADSR in a 2x2 grid
    attackKnob.setBounds  (660, 442, 100, 88);
    decayKnob.setBounds   (760, 442, 100, 88);
    sustainKnob.setBounds (660, 532, 100, 88);
    releaseKnob.setBounds (760, 532, 100, 88);

    // Modulation section: mirrors Filter Mod's tall single column on the
    // opposite side. Source sits right under Rate, Destination right under
    // Amount, since they're a pair (source/rate shape the LFO, amount/dest
    // decide where and how much of it lands).
    lfoRateKnob.setBounds   (910, 326, 100, 88);
    lfoSourceBox.setBounds  (910, 424, 100, 26);
    lfoAmountKnob.setBounds (910, 460, 100, 88);
    lfoDestBox.setBounds    (910, 558, 100, 26);
}
