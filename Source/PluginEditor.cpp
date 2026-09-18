#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MeowNames.h"

static const juce::Colour panelColour        (0xff303637);
static const juce::Colour panelOutlineColour (0xff454e4e);

static juce::String osc2TypeName (double val)
{
    static const char* names[] = { "Off", "Sine", "Sawtooth", "Square", "Triangle" };
    return names[juce::jlimit (0, 4, (int)std::round (val))];
}

//==============================================================================
MySynthAudioProcessorEditor::Content::Content (MySynthAudioProcessor& p)
    : audioProcessor (p),
      qwertyKeyboard (p.keyboardState),
      meowLevel (p.apvts, "meowLevel", "Meow Level", &oscLookAndFeel),
      lcdScreen (p.apvts),
      oscilloscope (p.oscilloscope),
      outputMeter (p.outputMeter),
      masterVolumeKnob (p.apvts, "masterVolume", "Master", &oscLookAndFeel),
      oscTypeKnob  (p.apvts, "oscType",  "Osc 1", &oscWaveformLookAndFeel),
      osc2TypeKnob (p.apvts, "osc2Type", "Osc 2",  &oscWaveformLookAndFeel),
      osc1OctaveSelector (p.apvts, "osc1Octave", "Octave"),
      osc2OctaveSelector (p.apvts, "osc2Octave", "Octave"),
      oscSyncButton (p.apvts, "oscSync", "1-2 Sync"),
      detuneKnob   (p.apvts, "detune",   "Detune", &oscLookAndFeel),
      pitchKnob    (p.apvts, "pitch",    "Pitch", &oscLookAndFeel),
      unisonKnob   (p.apvts, "unisonVoices", "Unison", &oscLookAndFeel),
      glideButton   (p.apvts, "glideOn",   "Glide"),
      glideTimeKnob (p.apvts, "glideTime", "Glide Time", &oscLookAndFeel),
      overloadKnob (p.apvts, "overload", "Overload", &oscLookAndFeel),
      kbAmountKnob (p.apvts, "kbAmount", "KB Amount", &oscLookAndFeel),
      cutoffKnob    (p.apvts, "cutoff",    "Cutoff",    &oscLookAndFeel),
      resonanceKnob (p.apvts, "resonance", "Resonance", &oscLookAndFeel),
      attackKnob  (p.apvts, "attack",  "Attack",     &oscLookAndFeel),
      decayKnob   (p.apvts, "decay",   "Decay",      &oscLookAndFeel),
      sustainKnob (p.apvts, "sustain", "Sustain",    &oscLookAndFeel),
      releaseKnob (p.apvts, "release", "Release",    &oscLookAndFeel),
      envAmountKnob  (p.apvts, "envAmount",  "Env Amount",  &oscLookAndFeel),
      fltAttackKnob  (p.apvts, "fltAttack",  "Attack",   &oscLookAndFeel),
      fltDecayKnob   (p.apvts, "fltDecay",   "Decay",    &oscLookAndFeel),
      fltSustainKnob (p.apvts, "fltSustain", "Sustain",  &oscLookAndFeel),
      fltReleaseKnob (p.apvts, "fltRelease", "Release",  &oscLookAndFeel),
      lfoRateKnob    (p.apvts, "lfoRate",    "LFO Rate", &oscLookAndFeel),
      lfoAmountKnob  (p.apvts, "lfoAmount",  "Amount",   &oscLookAndFeel),
      velocityPanel  (p.apvts),
      modernOscPanel (p.apvts, &oscLookAndFeel)
{
    sectionTitleTypeface = juce::Typeface::createSystemTypefaceFor (
        BinaryData::EurostileExtendedBlack_ttf, BinaryData::EurostileExtendedBlack_ttfSize);

    logoImage = juce::ImageCache::getFromMemory (BinaryData::logo_png,
                                                 BinaryData::logo_pngSize);

    startTimerHz (30);
    qwertyKeyboard.attachTo (*this);

    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setLookAndFeel (&comboBoxLookAndFeel);
    presetBox.setColour (juce::ComboBox::backgroundColourId, panelColour);
    presetBox.setColour (juce::ComboBox::outlineColourId,    panelOutlineColour);
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
    addAndMakeVisible (outputMeter);

    masterVolumeKnob.getSlider().setTextValueSuffix (" dB");
    addAndMakeVisible (masterVolumeKnob);

    // A deliberate edit on the main panel hands this oscillator back to
    // its waveform selector. Gesture callbacks exclude preset/state updates
    // and also cover wheel and accessibility edits.
    auto useBasicOscillator = [this] (const char* parameterID)
    {
        auto* parameter = audioProcessor.apvts.getParameter (parameterID);
        if (parameter != nullptr && parameter->getValue() >= 0.5f)
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (0.0f);
            parameter->endChangeGesture();
            refreshOscTypeKnobs();
        }
    };
    oscTypeKnob.getSlider().onDragStart = [useBasicOscillator]
    { useBasicOscillator ("osc1ModernOn"); };
    osc2TypeKnob.getSlider().onDragStart = [useBasicOscillator]
    { useBasicOscillator ("osc2ModernOn"); };

    addAndMakeVisible (oscTypeKnob);

    osc2TypeKnob.getSlider().textFromValueFunction = osc2TypeName;
    osc2TypeKnob.getSlider().onValueChange = [this]
    {
        auto value = osc2TypeKnob.getSlider().getValue();
        auto isOff = value < 0.5;
        if (! isOff)
            osc2LastNonOffValue = value;
        osc2TypeKnob.setStatusOverride (isOff ? "Off" : "", juce::Colour (0xffe0524f));
    };
    osc2TypeKnob.getSlider().onValueChange();
    // Clicking (rather than dragging) the knob toggles it straight to Off,
    // or back to whatever waveform it last had, without needing to drag
    // all the way around to 0.
    osc2TypeKnob.getSlider().addMouseListener (this, false);
    addAndMakeVisible (osc2TypeKnob);

    addAndMakeVisible (osc1OctaveSelector);
    addAndMakeVisible (osc2OctaveSelector);

    addAndMakeVisible (oscSyncButton);

    detuneKnob.getSlider().setTextValueSuffix (" ct");
    addAndMakeVisible (detuneKnob);

    pitchKnob.getSlider().setTextValueSuffix (" st");
    addAndMakeVisible (pitchKnob);

    unisonKnob.getSlider().setTextValueSuffix (" vox");
    addAndMakeVisible (unisonKnob);

    addAndMakeVisible (glideButton);

    glideTimeKnob.getSlider().setTextValueSuffix (" s");
    addAndMakeVisible (glideTimeKnob);

    addAndMakeVisible (overloadKnob);

    // Double-click to snap straight to 0 (no filter contribution), so it's
    // quick to neutralise while dialing in oscillators/cutoff/resonance
    kbAmountKnob.getSlider().setDoubleClickReturnValue (true, 0.0);
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
    envAmountKnob.getSlider().setDoubleClickReturnValue (true, 0.0);
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

    auto styleComboBox = [this] (juce::ComboBox& box)
    {
        box.setLookAndFeel (&comboBoxLookAndFeel);
        box.setColour (juce::ComboBox::backgroundColourId, panelColour);
        box.setColour (juce::ComboBox::outlineColourId,    panelOutlineColour);
        box.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
        box.setColour (juce::ComboBox::arrowColourId,      juce::Colours::white);
    };

    filterModeBox.addItemList ({ "LP 24", "LP 12", "BP 24", "BP 12", "HP 24", "HP 12", "Notch" }, 1);
    styleComboBox (meowBox);
    meowBox.addItemList (meowNames(), 1);
    meowBox.setTooltip ("Resynthesize a meow with pitched harmonics. Replaces the synth source; Off restores oscillators.");
    meowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (p.apvts, "meowSample", meowBox);
    addAndMakeVisible (meowBox);
    addAndMakeVisible (meowPreview);
    addAndMakeVisible (meowLevel);
    meowPreview.onClick = [this] { audioProcessor.previewMeow(); };
    styleComboBox (filterModeBox);
    filterModeBox.setTooltip ("Which taps of the ladder are mixed to the output. The filter core, "
                              "its drive and its resonance are the same in every mode.");
    filterModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (p.apvts, "filterMode", filterModeBox);
    addAndMakeVisible (filterModeBox);

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

    velocityButton.setColour (juce::TextButton::buttonColourId, panelColour);
    velocityButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    velocityButton.onClick = [this]
    {
        // The two overlays share the same screen area, so opening one
        // closes the other rather than letting them stack
        modernOscPanel.setVisible (false);
        velocityPanel.setVisible (! velocityPanel.isVisible());
    };
    addAndMakeVisible (velocityButton);

    modernOscButton.setColour (juce::TextButton::buttonColourId, panelColour);
    modernOscButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    modernOscButton.setTooltip ("Advanced Oscillator Settings: Wave Mix's saw/pulse/triangle "
                                 "blend and Motion's phase/unison controls");
    modernOscButton.onClick = [this]
    {
        velocityPanel.setVisible (false);
        modernOscPanel.setVisible (! modernOscPanel.isVisible());
    };
    addAndMakeVisible (modernOscButton);

    // Added last so they draw/receive clicks on top of everything they
    // overlap; start hidden since they're overlays, not part of the
    // always-visible layout
    addChildComponent (velocityPanel);
    addChildComponent (modernOscPanel);
}

MySynthAudioProcessorEditor::Content::~Content()
{
    stopTimer();

    meowBox.setLookAndFeel (nullptr);
    presetBox.setLookAndFeel (nullptr);
    filterModeBox.setLookAndFeel (nullptr);
    lfoSourceBox.setLookAndFeel (nullptr);
    lfoDestBox.setLookAndFeel (nullptr);
}

void MySynthAudioProcessorEditor::Content::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addItem ("Save Current Patch", saveCurrentPatchItemId);
    presetBox.addSeparator();

    // Grouped under section headings by category rather than one flat list.
    // Item IDs are still firstPresetItemId + the preset's index in the
    // underlying vector (not its position in this grouped listing), since
    // that's what setCurrentProgram()/getCurrentProgram() key off of.
    static const juce::StringArray factoryCategoryOrder { "Bass", "Poly Short", "Poly Long", "Synth Lead" };

    auto& presets = audioProcessor.getPresets();

    for (auto& category : factoryCategoryOrder)
    {
        bool addedHeading = false;
        for (int i = 0; i < (int) presets.size(); ++i)
        {
            if (presets[(size_t) i].category != category)
                continue;
            if (! addedHeading)
            {
                presetBox.addSectionHeading (category);
                addedHeading = true;
            }
            presetBox.addItem (presets[(size_t) i].name, firstPresetItemId + i);
        }
    }

    // Anything outside the four factory categories - currently just patches
    // saved via "Save Current Patch" - gets its own trailing section
    bool addedUserHeading = false;
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        if (factoryCategoryOrder.contains (presets[(size_t) i].category))
            continue;
        if (! addedUserHeading)
        {
            presetBox.addSectionHeading ("User");
            addedUserHeading = true;
        }
        presetBox.addItem (presets[(size_t) i].name, firstPresetItemId + i);
    }
}

void MySynthAudioProcessorEditor::Content::showSavePatchDialog()
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

void MySynthAudioProcessorEditor::Content::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == &osc2TypeKnob.getSlider())
        osc2ValueOnMouseDown = osc2TypeKnob.getSlider().getValue();
}

void MySynthAudioProcessorEditor::Content::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent != &osc2TypeKnob.getSlider())
        return;

    auto& slider = osc2TypeKnob.getSlider();

    if (! slider.isEnabled())
        return;

    // Only toggle if the drag (if any) didn't actually land on a different
    // waveform - comparing values rather than trusting JUCE's own drag
    // detection, since that alone wasn't reliably telling a plain click from
    // a drag here.
    if (slider.getValue() != osc2ValueOnMouseDown)
        return;

    auto isOff = slider.getValue() < 0.5;
    slider.setValue (isOff ? osc2LastNonOffValue : 0.0, juce::sendNotificationSync);
}

void MySynthAudioProcessorEditor::Content::timerCallback()
{
    // Give the qwerty keyboard focus once we're on screen
    if (! hasGrabbedFocus && isShowing())
    {
        grabKeyboardFocus();
        hasGrabbedFocus = true;
    }

    midiLightOn = audioProcessor.midiActivity.exchange (false);
    repaint();

    refreshOscTypeKnobs();
}

// Keep waveform controls available while Wave Mix is active. Their user
// gestures switch the corresponding oscillator back to its basic waveform.
void MySynthAudioProcessorEditor::Content::refreshOscTypeKnobs()
{
    static const juce::Colour advancedColour (0xff5aa9e6);
    static const juce::String advancedTooltip (
        "Wave Mix active. Adjust this knob to use the basic waveform instead.");

    const bool osc1Advanced = audioProcessor.apvts.getRawParameterValue ("osc1ModernOn")->load() >= 0.5f;
    if (osc1Advanced != osc1ShowingAdvanced)
    {
        osc1ShowingAdvanced = osc1Advanced;
        oscTypeKnob.setStatusOverride (osc1Advanced ? "Adv" : "", advancedColour);
        oscTypeKnob.getSlider().setTooltip (osc1Advanced ? advancedTooltip : juce::String());
    }

    const bool osc2Advanced = audioProcessor.apvts.getRawParameterValue ("osc2ModernOn")->load() >= 0.5f;
    if (osc2Advanced != osc2ShowingAdvanced)
    {
        osc2ShowingAdvanced = osc2Advanced;
        if (osc2Advanced)
        {
            osc2TypeKnob.setStatusOverride ("Adv", advancedColour);
            osc2TypeKnob.getSlider().setTooltip (advancedTooltip);
        }
        else
        {
            osc2TypeKnob.getSlider().setTooltip (juce::String());
            // Restores "Off"/"" per the knob's actual current value
            osc2TypeKnob.getSlider().onValueChange();
        }
    }
}

void MySynthAudioProcessorEditor::Content::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff222829));

    // Menu bar
    g.setColour (juce::Colour (0xff1b2021));
    g.fillRect (0, 0, getWidth(), 40);
    g.setColour (juce::Colour (0xff394142));
    g.drawHorizontalLine (40, 0.0f, (float) getWidth());

    g.setColour (juce::Colour (0xffe5e8df));
    g.setFont (juce::FontOptions (16.0f));
    g.drawText ("Meow Resynthesis", 720, 65, 290, 26, juce::Justification::centredLeft);

    // MIDI indicator
    g.setColour (midiLightOn ? juce::Colour (0xff79cba5) : juce::Colour (0xff1a4a1a));
    g.fillEllipse (12, 46, 20, 20);
    g.setColour (juce::Colour (0xffe5e8df));
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
    g.setColour (juce::Colour (0xffe5e8df));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Select Preset", 342, 100, 110, 26, juce::Justification::centredLeft);

    auto drawSection = [&g, this] (juce::Rectangle<float> box, const juce::String& title)
    {
        g.setColour (panelColour);
        g.fillRoundedRectangle (box, 10.0f);
        g.setColour (panelOutlineColour);
        g.drawRoundedRectangle (box.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (juce::Colour (0xffe5e8df));
        // Sentence case: only the very first character capitalised, not
        // every word (so "Filter Mod" reads as "Filter mod").
        auto displayTitle = title.toLowerCase();
        if (displayTitle.isNotEmpty())
            displayTitle = displayTitle.substring (0, 1).toUpperCase() + displayTitle.substring (1);

        // The wider/bolder Eurostile face doesn't fit every title at one
        // fixed size within its panel's width (e.g. "Modulation" vs the
        // narrower "Filter Mod" panel), so shrink just enough to fit rather
        // than guessing a single size small enough for the longest title.
        float titleSize = 10.5f;
        // Very slight horizontal squeeze on the section titles, purely
        // cosmetic (the fit-shrinking loop below is unrelated).
        constexpr float titleHorizontalScale = 0.95f;
        auto titleFont = juce::FontOptions (titleSize).withTypeface (sectionTitleTypeface)
                                                        .withHorizontalScale (titleHorizontalScale);
        while (titleSize > 7.0f
               && juce::GlyphArrangement::getStringWidthInt (juce::Font (titleFont), displayTitle) > box.getWidth() - 8.0f)
        {
            titleSize -= 0.5f;
            titleFont = juce::FontOptions (titleSize).withTypeface (sectionTitleTypeface)
                                                       .withHorizontalScale (titleHorizontalScale);
        }

        g.setFont (titleFont);
        // Padding above the text before it starts, then shrink the centring
        // box so it sits nearer the top, leaving breathing room below it
        // before the knobs start.
        g.drawText (displayTitle, box.withTrimmedTop (6.0f).withHeight (18.0f).toNearestInt(),
                    juce::Justification::centred);
    };

    drawSection ({ 150.0f, 286.0f, 620.0f, 124.0f }, "Oscillators");
    drawSection ({ 20.0f, 286.0f, 120.0f, 342.0f }, "Filter Mod");
    drawSection ({ 780.0f, 286.0f, 110.0f, 124.0f }, "Master");
    drawSection ({ 150.0f, 418.0f, 460.0f, 210.0f }, "Filter");
    drawSection ({ 620.0f, 418.0f, 270.0f, 210.0f }, "Amp");
    drawSection ({ 900.0f, 286.0f, 120.0f, 342.0f }, "Modulation");
}

void MySynthAudioProcessorEditor::Content::resized()
{
    // Preset menu sits below the logo/scope row, to the right of its label
    presetBox.setBounds (460, 100, 238, 26);
    meowBox.setBounds (720, 100, 290, 26);
    meowPreview.setBounds (720, 140, 100, 28);
    meowLevel.setBounds (850, 145, 100, 88);

    // LCD effects screen at the top center, beneath the preset menu row
    lcdScreen.setBounds (getWidth() / 2 - 178, 134, 356, 144);

    // Oscilloscope sits to the right of the logo, above the effects panel,
    // matching the LCD's right edge
    oscilloscope.setBounds (462, 48, 236, 44);

    // Keep the control labels below the section heading, with a shared
    // baseline. Waveform selectors use a little extra vertical room for
    // the icons around their knobs while staying inside the panel.
    juce::Rectangle<int> oscRow (158, 312, 604, 88);
    oscTypeKnob.setBounds        (oscRow.removeFromLeft (110).withWidth (104).withHeight (94));
    osc1OctaveSelector.setBounds (oscRow.removeFromLeft (58));
    osc2TypeKnob.setBounds       (oscRow.removeFromLeft (110).withWidth (104).withHeight (94).translated (-10, 0));
    osc2OctaveSelector.setBounds (oscRow.removeFromLeft (58).translated (-10, 0));
    oscSyncButton.setBounds      (oscRow.removeFromLeft (46));
    detuneKnob.setBounds         (oscRow.removeFromLeft (78));
    pitchKnob.setBounds          (oscRow.removeFromLeft (78));
    unisonKnob.setBounds         (oscRow);

    // Master panel: volume knob plus a peak meter reading the true final
    // output (post master gain), sitting above Amp between Oscillators
    // and Modulation
    masterVolumeKnob.setBounds (790, 310, 70, 88);
    outputMeter.setBounds      (865, 310, 15, 88);

    // Filter Mod panel left of the filter: Overload, EGR (envelope) amount,
    // and keyboard tracking amount stacked in their own column. It runs
    // from the top of the Oscillators section down to the bottom of the
    // Filter/Amp panels, so the knobs get to stay full size
    overloadKnob.setBounds  (30, 326, 100, 88);
    envAmountKnob.setBounds (30, 427, 100, 88);
    kbAmountKnob.setBounds  (30, 528, 100, 88);

    // Filter section: cutoff/resonance/glide across the top row, and their
    // envelope counterparts directly below in the same four columns, so
    // Cutoff lines up with Attack and Resonance lines up with Decay.
    // Mode sits in the panel's header row, right of the centred "Filter"
    // title, so the two knob rows below keep their full size.
    filterModeBox.setBounds (486, 421, 110, 21);
    constexpr int filterColWidth = 444 / 4;
    juce::Rectangle<int> filterRow (158, 446, 444, 84);
    cutoffKnob.setBounds    (filterRow.removeFromLeft (filterColWidth));
    resonanceKnob.setBounds (filterRow.removeFromLeft (filterColWidth));
    glideButton.setBounds   (filterRow.removeFromLeft (filterColWidth));
    glideTimeKnob.setBounds (filterRow);

    juce::Rectangle<int> filterEnvRow (158, 536, 444, 84);
    fltAttackKnob.setBounds  (filterEnvRow.removeFromLeft (filterColWidth));
    fltDecayKnob.setBounds   (filterEnvRow.removeFromLeft (filterColWidth));
    fltSustainKnob.setBounds (filterEnvRow.removeFromLeft (filterColWidth));
    fltReleaseKnob.setBounds (filterEnvRow);

    // Amp section: ADSR in a 2x2 grid
    attackKnob.setBounds  (644, 446, 100, 84);
    decayKnob.setBounds   (766, 446, 100, 84);
    sustainKnob.setBounds (644, 536, 100, 84);
    releaseKnob.setBounds (766, 536, 100, 84);

    // Modulation section: mirrors Filter Mod's tall single column on the
    // opposite side. Source sits right under Rate, Destination right under
    // Amount, since they're a pair (source/rate shape the LFO, amount/dest
    // decide where and how much of it lands).
    lfoRateKnob.setBounds   (910, 326, 100, 88);
    lfoSourceBox.setBounds  (910, 424, 100, 26);
    lfoAmountKnob.setBounds (910, 460, 100, 88);
    lfoDestBox.setBounds    (910, 558, 100, 26);

    // Velocity: a button in the header toggles a panel that overlays the
    // whole control area beneath it (same span as Filter Mod through
    // Modulation), rather than taking up permanent space in the layout
    velocityButton.setBounds (930, 8, 90, 24);
    velocityPanel.setBounds  (20, 286, 1000, 342);

    modernOscButton.setBounds (826, 8, 90, 24);
    modernOscPanel.setBounds  (20, 286, 1000, 342);
}

//==============================================================================
MySynthAudioProcessorEditor::MySynthAudioProcessorEditor (MySynthAudioProcessor& p)
    : AudioProcessorEditor (&p), content (p)
{
    addAndMakeVisible (content);

    setResizable (true, true);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 3, designHeight * 3);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);

    setSize (designWidth, designHeight);
}

MySynthAudioProcessorEditor::~MySynthAudioProcessorEditor()
{
}

void MySynthAudioProcessorEditor::resized()
{
    auto scale = (float) getWidth() / (float) designWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, designWidth, designHeight);
}
