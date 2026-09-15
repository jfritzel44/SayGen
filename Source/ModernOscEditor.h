#pragma once
#include <JuceHeader.h>
#include "LabeledKnob.h"
#include "SyncToggleButton.h"

// Wave mixing and oscillator motion share one compact, two-page overlay.
class ModernOscEditor : public juce::Component
{
public:
    explicit ModernOscEditor (juce::AudioProcessorValueTreeState& state, juce::LookAndFeel* look)
        : osc1ModernParam (state.getParameter ("osc1ModernOn")),
          osc2ModernParam (state.getParameter ("osc2ModernOn")),
          modern1 (state, "osc1ModernOn", "Modern"), modern2 (state, "osc2ModernOn", "Modern"),
          sub1 (state, "osc1SubOctave", "Sub"), sub2 (state, "osc2SubOctave", "Sub"),
          saw1 (state, "osc1SawMix", "Saw", look), saw2 (state, "osc2SawMix", "Saw", look),
          pulse1 (state, "osc1PulseMix", "Pulse", look), pulse2 (state, "osc2PulseMix", "Pulse", look),
          tri1 (state, "osc1TriMix", "Triangle", look), tri2 (state, "osc2TriMix", "Triangle", look),
          width1 (state, "osc1PulseWidth", "Pulse Width", look), width2 (state, "osc2PulseWidth", "Pulse Width", look),
          level1 (state, "osc1Level", "Level", look), level2 (state, "osc2Level", "Level", look),
          bass (state, "filterCompensation", "Bass", look), curve (state, "envelopeCurve", "Env Curve", look),
          phase1 (state, "osc1StartPhase", "Start Phase", look), phase2 (state, "osc2StartPhase", "Start Phase", look),
          randomness (state, "phaseRandomness", "Phase Rand", look), drift (state, "driftAmount", "Drift", look),
          spread (state, "unisonSpread", "Detune", look), stereo (state, "unisonWidth", "Stereo", look),
          voices (state, "unisonVoices", "Voices", look), coarse (state, "osc2Coarse", "Coarse Tune", look),
          pwmDepth (state, "pwmDepth", "PWM Depth", look), pwmRate (state, "pwmRate", "PWM Rate", look),
          noiseLevel (state, "noiseLevel", "Noise", look), noiseColour (state, "noiseColour", "Colour", look),
          ringMod (state, "ringMod", "Ring Mod", look)
    {
        mixControls = { &modern1, &modern2, &sub1, &sub2, &saw1, &saw2, &pulse1, &pulse2,
                        &tri1, &tri2, &width1, &width2, &level1, &level2, &bass, &curve };
        motionControls = { &mode1, &mode2, &phase1, &phase2, &randomness, &drift, &spread, &stereo, &voices, &coarse };
        sourceControls = { &pwmDepth, &pwmRate, &noiseLevel, &noiseColour, &ringMod };
        for (auto* c : mixControls) addAndMakeVisible (c);
        for (auto* c : motionControls) addChildComponent (c);
        for (auto* c : sourceControls) addChildComponent (c);
        for (auto* mode : { &mode1, &mode2 })
        {
            mode->addItemList ({ "Retrigger", "Random", "Free" }, 1);
            mode->setTooltip ("Retrigger: repeatable phase. Random: varied attacks. Free: continues through silence.");
        }
        modeAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "osc1PhaseMode", mode1);
        modeAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "osc2PhaseMode", mode2);
        phase1.getSlider().setTextValueSuffix (" deg");
        phase2.getSlider().setTextValueSuffix (" deg");
        coarse.getSlider().setTextValueSuffix (" st");
        spread.getSlider().setTextValueSuffix (" ct");
        level1.getSlider().setTooltip ("Oscillator level before drive and filtering");
        level2.getSlider().setTooltip ("Oscillator level before drive and filtering");
        randomness.getSlider().setTooltip ("Random mode's phase range: zero to a full cycle");
        phase1.getSlider().setTooltip ("Initial phase in degrees; Free mode retains phase after its first note");
        phase2.getSlider().setTooltip ("Initial phase in degrees; Free mode retains phase after its first note");
        drift.getSlider().setTooltip ("Independent, smoothly wandering oscillator tuning");
        spread.getSlider().setTooltip ("Detune of the outer unison voices, in cents");
        stereo.getSlider().setTooltip ("Unison width: mono to full stereo");
        bass.getSlider().setTooltip ("Restore bass as resonance increases");
        curve.getSlider().setTooltip ("Envelope curvature: linear to exponential");
        pwmRate.getSlider().setTextValueSuffix (" Hz");
        pwmDepth.getSlider().setTooltip ("Sweeps pulse width. Each voice has its own modulator, "
                                         "so a stack beats against itself instead of pulsing as one");
        pwmRate.getSlider().setTooltip ("Speed of the per-voice pulse-width sweep");
        noiseLevel.getSlider().setTooltip ("Noise summed into the filter alongside the oscillators");
        noiseColour.getSlider().setTooltip ("Noise tone: fully left is pink, fully right is white");
        ringMod.getSlider().setTooltip ("Osc 1 multiplied by Osc 2, mixed in alongside them");
        mixButton.onClick = [this] { showPage (0); };
        motionButton.onClick = [this] { showPage (1); };
        sourceButton.onClick = [this] { showPage (2); };
        addAndMakeVisible (mixButton); addAndMakeVisible (motionButton);
        addAndMakeVisible (sourceButton);

        advancedToggle.setTooltip ("Turns Wave Mix on for both oscillators at once. While on, "
                                    "it replaces the Osc 1 / Osc 2 waveform knobs on the main "
                                    "screen, which are disabled and greyed out until this is off.");
        advancedToggle.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a2a));
        advancedToggle.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3f8f4f));
        advancedToggle.onClick = [this]
        {
            const bool turnOn = ! (modern1.isOn() && modern2.isOn());
            if (osc1ModernParam != nullptr) osc1ModernParam->setValueNotifyingHost (turnOn ? 1.0f : 0.0f);
            if (osc2ModernParam != nullptr) osc2ModernParam->setValueNotifyingHost (turnOn ? 1.0f : 0.0f);
        };
        addAndMakeVisible (advancedToggle);
        modern1.onToggle = [this] { refreshAdvancedToggle(); };
        modern2.onToggle = [this] { refreshAdvancedToggle(); };
        refreshAdvancedToggle();

        showPage (0);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff3a3a3a));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 10);
        g.setColour (juce::Colour (0xff888888));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10, 1);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (13));
        // Only the two per-oscillator pages have rows to label.
        if (page != 2)
        {
            g.drawText ("Osc 1", 24, 104, 60, 20, juce::Justification::centredLeft);
            g.drawText ("Osc 2", 24, 224, 60, 20, juce::Justification::centredLeft);
        }
        g.setFont (juce::FontOptions (11));
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        if (page == 2)
        {
            // Group headings, so the five knobs read as three sources rather
            // than one undifferentiated row.
            g.setFont (juce::FontOptions (13));
            g.setColour (juce::Colours::white);
            g.drawText ("Pulse width", 150, 66, 205, 20, juce::Justification::centred);
            g.drawText ("Noise", 410, 66, 205, 20, juce::Justification::centred);
            g.drawText ("Ring mod", 670, 66, 100, 20, juce::Justification::centred);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.setFont (juce::FontOptions (13.2f));
            g.drawFittedText ("Pulse width modulation needs a pulse in the mix to hear - raise Pulse on the "
                              "Wave Mix page, or pick the pulse waveform. Noise and ring modulation are summed "
                              "in alongside the oscillators, before the filter.",
                              24, 292, 950, 40, juce::Justification::centred, 2, 1.0f);
        }
        else if (page == 1)
        {
            g.drawText ("Phase Mode", 90, 76, 130, 18, juce::Justification::centred);
            g.drawText ("Phase Mode", 90, 196, 130, 18, juce::Justification::centred);
            g.drawText ("Phase settings and voice count apply to new notes. Detune, stereo and drift update live.",
                        24, 308, 950, 20, juce::Justification::centred);
        }
        else
        {
            g.setFont (juce::FontOptions (13.2f));
            g.drawFittedText ("Wave Mix replaces the Osc 1 / Osc 2 knobs on the main screen with a saw/pulse/"
                        "triangle blend. Level, Bass and Env Curve still apply either way.",
                        24, 292, 950, 40, juce::Justification::centred, 2, 1.0f);
        }
    }
    void resized() override
    {
        mixButton.setBounds (20, 12, 105, 28);
        motionButton.setBounds (135, 12, 105, 28);
        sourceButton.setBounds (250, 12, 105, 28);
        advancedToggle.setBounds (365, 12, 150, 28);
        pwmDepth.setBounds (150, 92, 100, 88);
        pwmRate.setBounds (255, 92, 100, 88);
        noiseLevel.setBounds (410, 92, 100, 88);
        noiseColour.setBounds (515, 92, 100, 88);
        ringMod.setBounds (670, 92, 100, 88);
        auto row = [] (int y, SyncToggleButton& modern, LabeledKnob& saw, LabeledKnob& pulse,
                       LabeledKnob& tri, LabeledKnob& width, SyncToggleButton& sub, LabeledKnob& level)
        {
            modern.setBounds (90, y, 60, 88); saw.setBounds (155, y, 90, 88);
            pulse.setBounds (250, y, 90, 88); tri.setBounds (345, y, 90, 88);
            width.setBounds (440, y, 100, 88); sub.setBounds (545, y, 60, 88);
            level.setBounds (625, y, 100, 88);
        };
        row (72, modern1, saw1, pulse1, tri1, width1, sub1, level1);
        row (192, modern2, saw2, pulse2, tri2, width2, sub2, level2);
        bass.setBounds (785, 72, 90, 88); curve.setBounds (885, 72, 90, 88);
        mode1.setBounds (90, 104, 130, 28); mode2.setBounds (90, 224, 130, 28);
        phase1.setBounds (240, 72, 100, 88); phase2.setBounds (240, 192, 100, 88);
        randomness.setBounds (380, 72, 100, 88); drift.setBounds (485, 72, 90, 88);
        spread.setBounds (580, 72, 90, 88); stereo.setBounds (675, 72, 90, 88);
        voices.setBounds (770, 72, 90, 88); coarse.setBounds (350, 192, 110, 88);
    }
private:
    void refreshAdvancedToggle()
    {
        const bool bothOn = modern1.isOn() && modern2.isOn();
        advancedToggle.setToggleState (bothOn, juce::dontSendNotification);
        advancedToggle.setButtonText (bothOn ? "Advanced Osc: ON" : "Advanced Osc: OFF");
    }
    void showPage (int which)
    {
        page = which;
        for (auto* c : mixControls) c->setVisible (which == 0);
        for (auto* c : motionControls) c->setVisible (which == 1);
        for (auto* c : sourceControls) c->setVisible (which == 2);
        mixButton.setToggleState (which == 0, juce::dontSendNotification);
        motionButton.setToggleState (which == 1, juce::dontSendNotification);
        sourceButton.setToggleState (which == 2, juce::dontSendNotification);
        repaint();
    }
    juce::RangedAudioParameter* osc1ModernParam = nullptr;
    juce::RangedAudioParameter* osc2ModernParam = nullptr;
    SyncToggleButton modern1, modern2, sub1, sub2;
    LabeledKnob saw1, saw2, pulse1, pulse2, tri1, tri2, width1, width2, level1, level2, bass, curve;
    LabeledKnob phase1, phase2, randomness, drift, spread, stereo, voices, coarse;
    LabeledKnob pwmDepth, pwmRate, noiseLevel, noiseColour, ringMod;
    juce::ComboBox mode1, mode2;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment1, modeAttachment2;
    juce::TextButton mixButton { "Wave Mix" }, motionButton { "Motion" }, sourceButton { "Sources" };
    // Master switch: turns Wave Mix on/off for both oscillators together,
    // since a mismatched on/off/on split between them is rarely wanted and
    // was confusing users about why the main-screen knobs stopped doing
    // anything.
    juce::TextButton advancedToggle { "Advanced Osc: OFF" };
    std::vector<juce::Component*> mixControls, motionControls, sourceControls;
    int page = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModernOscEditor)
};
