#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "OscKnobLookAndFeel.h"
#include "OscWaveformLookAndFeel.h"
#include "ComboBoxLookAndFeel.h"
#include "QwertyMidiKeyboard.h"
#include "LabeledKnob.h"
#include "LedOctaveSelector.h"
#include "LcdScreen.h"
#include "SyncToggleButton.h"
#include "Oscilloscope.h"

// The editor is a thin resizable shell around Content, which draws/lays out
// at a fixed design resolution. The shell scales Content uniformly to fill
// whatever size the host window ends up at, so nothing has to reflow.
class MySynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    MySynthAudioProcessorEditor (MySynthAudioProcessor&);
    ~MySynthAudioProcessorEditor() override;

    void resized() override;

private:
    struct Content : public juce::Component,
                     public juce::Timer
    {
        explicit Content (MySynthAudioProcessor&);
        ~Content() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void timerCallback() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        void rebuildPresetMenu();
        void showSavePatchDialog();

        // Preset combo box item IDs: "Save Current Patch" is pinned first, then
        // a separator, then the real presets starting at firstPresetItemId
        static constexpr int saveCurrentPatchItemId = 1;
        static constexpr int firstPresetItemId      = 2;

        MySynthAudioProcessor& audioProcessor;
        bool midiLightOn = false;
        bool hasGrabbedFocus = false;

        // Remembers Osc 2's last non-off waveform so a click-to-toggle-off
        // can restore it, rather than always landing back on Sine.
        double osc2LastNonOffValue = 1.0;

        // Value snapshot taken on mouse-down, so mouseUp can tell a plain
        // click (value unchanged) apart from a drag that actually picked a
        // different waveform.
        double osc2ValueOnMouseDown = 0.0;

        // Only the panel section titles (Oscillators, Filter, Amp, etc) use
        // this; everywhere else keeps the system default font.
        juce::Typeface::Ptr sectionTitleTypeface;

        OscKnobLookAndFeel oscLookAndFeel;
        OscWaveformLookAndFeel oscWaveformLookAndFeel;
        ComboBoxLookAndFeel comboBoxLookAndFeel;
        QwertyMidiKeyboard qwertyKeyboard;
        juce::Image logoImage;

        juce::ComboBox presetBox;
        LcdScreen lcdScreen;
        Oscilloscope& oscilloscope;
        OutputMeter& outputMeter;
        LabeledKnob masterVolumeKnob;

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

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
    };

    static constexpr int designWidth  = 1040;
    static constexpr int designHeight = 642;

    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MySynthAudioProcessorEditor)
};
