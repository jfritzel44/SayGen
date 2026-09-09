#pragma once
#include <JuceHeader.h>
#include "LabeledKnob.h"
#include "SyncToggleButton.h"

// Wave mixing and oscillator motion share one compact, two-page overlay.
class ModernOscEditor : public juce::Component
{
public:
    explicit ModernOscEditor (juce::AudioProcessorValueTreeState& state, juce::LookAndFeel* look)
        : modern1 (state, "osc1ModernOn", "Modern"), modern2 (state, "osc2ModernOn", "Modern"),
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
          voices (state, "unisonVoices", "Voices", look), coarse (state, "osc2Coarse", "Coarse Tune", look)
    {
        mixControls = { &modern1, &modern2, &sub1, &sub2, &saw1, &saw2, &pulse1, &pulse2,
                        &tri1, &tri2, &width1, &width2, &level1, &level2, &bass, &curve };
        motionControls = { &mode1, &mode2, &phase1, &phase2, &randomness, &drift, &spread, &stereo, &voices, &coarse };
        for (auto* c : mixControls) addAndMakeVisible (c);
        for (auto* c : motionControls) addChildComponent (c);
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
        mixButton.onClick = [this] { showMotion (false); };
        motionButton.onClick = [this] { showMotion (true); };
        addAndMakeVisible (mixButton); addAndMakeVisible (motionButton);
        showMotion (false);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff3a3a3a));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 10);
        g.setColour (juce::Colour (0xff888888));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10, 1);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (15));
        g.drawText ("Oscillators & Sound", 300, 12, 670, 26, juce::Justification::centred);
        g.setFont (juce::FontOptions (13));
        g.drawText ("Osc 1", 24, 104, 60, 20, juce::Justification::centredLeft);
        g.drawText ("Osc 2", 24, 224, 60, 20, juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (11));
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        if (motionVisible)
        {
            g.drawText ("Phase Mode", 90, 76, 130, 18, juce::Justification::centred);
            g.drawText ("Phase Mode", 90, 196, 130, 18, juce::Justification::centred);
            g.drawText ("Phase settings and voice count apply to new notes. Detune, stereo and drift update live.",
                        24, 308, 950, 20, juce::Justification::centred);
        }
        else
            g.drawText ("Modern blends saw, pulse and triangle. Level, Bass and Env Curve work with every waveform.",
                        24, 308, 950, 20, juce::Justification::centred);
    }
    void resized() override
    {
        mixButton.setBounds (20, 12, 105, 28);
        motionButton.setBounds (135, 12, 105, 28);
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
    void showMotion (bool motion)
    {
        motionVisible = motion;
        for (auto* c : mixControls) c->setVisible (! motion);
        for (auto* c : motionControls) c->setVisible (motion);
        mixButton.setToggleState (! motion, juce::dontSendNotification);
        motionButton.setToggleState (motion, juce::dontSendNotification);
        repaint();
    }
    SyncToggleButton modern1, modern2, sub1, sub2;
    LabeledKnob saw1, saw2, pulse1, pulse2, tri1, tri2, width1, width2, level1, level2, bass, curve;
    LabeledKnob phase1, phase2, randomness, drift, spread, stereo, voices, coarse;
    juce::ComboBox mode1, mode2;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment1, modeAttachment2;
    juce::TextButton mixButton { "Wave Mix" }, motionButton { "Motion" };
    std::vector<juce::Component*> mixControls, motionControls;
    bool motionVisible = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModernOscEditor)
};
