#pragma once
#include <JuceHeader.h>

//==============================================================================
// A rotary knob with a title label above it, attached to an APVTS parameter.
// Owns its slider, label, and attachment, and cleans up its look-and-feel,
// so adding a control to an editor is one member plus one setBounds:
//
//     LabeledKnob pitchKnob { apvts, "pitch", "Pitch", &lookAndFeel };
//
// Customise the underlying slider through getSlider().
class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& parameterID,
                 const juce::String& title,
                 juce::LookAndFeel* lookAndFeelToUse = nullptr)
        : attachment (apvts, parameterID, slider)
    {
        if (lookAndFeelToUse != nullptr)
            slider.setLookAndFeel (lookAndFeelToUse);

        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (slider);

        label.setText (title, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (10.5f));
        addAndMakeVisible (label);
    }

    ~LabeledKnob() override
    {
        slider.setLookAndFeel (nullptr);
    }

    juce::Slider& getSlider()   { return slider; }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds  (b.getX() + 2,  b.getY() + 2,  b.getWidth() - 4, 18);
        slider.setBounds (b.getX() + 10, b.getY() + 20, b.getWidth() - 20,
                          b.getHeight() - 22);
    }

private:
    juce::Slider slider;
    juce::Label  label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabeledKnob)
};
