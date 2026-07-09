#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "OscKnobLookAndFeel.h"
#include "QwertyMidiKeyboard.h"
#include "LabeledKnob.h"

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
    MySynthAudioProcessor& audioProcessor;
    bool midiLightOn = false;
    bool hasGrabbedFocus = false;

    OscKnobLookAndFeel oscLookAndFeel;
    QwertyMidiKeyboard qwertyKeyboard;

    LabeledKnob oscTypeKnob;
    LabeledKnob osc2TypeKnob;
    LabeledKnob detuneKnob;
    LabeledKnob pitchKnob;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MySynthAudioProcessorEditor)
};
