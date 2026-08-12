#pragma once
#include <JuceHeader.h>

//==============================================================================
// Drop-in scope screen: a bezelled panel matching the synth's LCD styling,
// wrapping a juce::AudioVisualiserComponent. Owned by the processor so it
// keeps receiving audio and stays alive across editor open/close; call
// pushBuffer() from processBlock to feed it. It repaints itself on its own
// timer, so nothing else needs to drive it.
class Oscilloscope : public juce::Component
{
public:
    Oscilloscope()
    {
        scope.setBufferSize (512);
        scope.setSamplesPerBlock (2);
        scope.setColours (screenBg, trace);
        scope.setRepaintRate (30);
        addAndMakeVisible (scope);
    }

    void pushBuffer (const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() != numChannelsShown)
        {
            numChannelsShown = juce::jmax (1, buffer.getNumChannels());
            scope.setNumChannels (numChannelsShown);
        }

        scope.pushBuffer (buffer);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff15181f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (juce::Colour (0xff474e5c));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    }

    void resized() override
    {
        scope.setBounds (getLocalBounds().reduced (8));
    }

private:
    const juce::Colour screenBg { 0xff0c1f14 };
    const juce::Colour trace    { 0xff5cd68f };

    juce::AudioVisualiserComponent scope { 1 };
    int numChannelsShown = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Oscilloscope)
};
