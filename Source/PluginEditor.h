#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "OscKnobLookAndFeel.h"
#include "QwertyMidiKeyboard.h"
#include "LabeledKnob.h"
#include "LedOctaveSelector.h"
#include "LcdScreen.h"
#include "SyncToggleButton.h"
#include "Oscilloscope.h"

class MySynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::Timer
{
public:
    MySynthAudioProcessorEditor (MySynthAudioProcessor&);
    ~MySynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void rebuildPresetMenu();
    void showSavePatchDialog();

    // Preset combo box item IDs: "Save Current Patch" is pinned first, then
    // a separator, then the real presets starting at firstPresetItemId
    static constexpr int saveCurrentPatchItemId = 1;
    static constexpr int firstPresetItemId      = 2;

    MySynthAudioProcessor& audioProcessor;
    bool midiLightOn = false;
    bool hasGrabbedFocus = false;

    OscKnobLookAndFeel oscLookAndFeel;
    QwertyMidiKeyboard qwertyKeyboard;
    juce::Image logoImage;

    juce::ComboBox presetBox;
    LcdScreen lcdScreen;
    Oscilloscope& oscilloscope;

    LabeledKnob oscTypeKnob;
    LabeledKnob osc2TypeKnob;
    LedOctaveSelector osc1OctaveSelector;
    LedOctaveSelector osc2OctaveSelector;
    SyncToggleButton oscSyncButton;
    LabeledKnob detuneKnob;
    LabeledKnob pitchKnob;
    LabeledKnob overloadKnob;
    LabeledKnob kbAmountKnob;
    LabeledKnob cutoffKnob;
    LabeledKnob resonanceKnob;
    LabeledKnob attackKnob;
    LabeledKnob decayKnob;
    LabeledKnob sustainKnob;
    LabeledKnob releaseKnob;
    LabeledKnob envAmountKnob;
    LabeledKnob fltAttackKnob;
    LabeledKnob fltDecayKnob;
    LabeledKnob fltSustainKnob;
    LabeledKnob fltReleaseKnob;

    // Modulation section: one LFO, routed to a chosen destination
    LabeledKnob lfoRateKnob;
    LabeledKnob lfoAmountKnob;
    juce::ComboBox lfoSourceBox;
    juce::ComboBox lfoDestBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoSourceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoDestAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MySynthAudioProcessorEditor)
};
