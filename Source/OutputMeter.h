#pragma once
#include <JuceHeader.h>

//==============================================================================
// Simple peak output meter: a bezelled vertical bar matching the synth's LCD
// styling. Owned by the processor so it keeps receiving audio and stays
// alive across editor open/close; call pushBuffer() from processBlock to
// feed it (after the master gain stage, so it reads the true final output
// level rather than the amp envelope). It repaints itself on its own timer,
// so nothing else needs to drive it.
class OutputMeter : public juce::Component,
                    private juce::Timer
{
public:
    OutputMeter()
    {
        startTimerHz (30);
    }

    ~OutputMeter() override { stopTimer(); }

    void pushBuffer (const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        currentPeak.store (peak);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff15181f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour (0xff474e5c));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

        auto bar = r.reduced (2.5f);
        auto filled = bar.withTop (bar.getBottom() - bar.getHeight() * displayedLevel);

        auto meterColour = displayedLevel >= 1.0f ? juce::Colours::red
                          : displayedLevel > 0.9f  ? juce::Colours::orange
                                                     : juce::Colour (0xff5cd68f);
        g.setColour (meterColour);
        g.fillRoundedRectangle (filled, 2.5f);
    }

private:
    void timerCallback() override
    {
        auto target = juce::jlimit (0.0f, 1.0f, currentPeak.load());

        // Instant attack, slow-ish decay so it reads like a real peak meter
        // instead of flickering with every block
        displayedLevel = target > displayedLevel ? target : displayedLevel * 0.85f;

        repaint();
    }

    std::atomic<float> currentPeak { 0.0f };
    float displayedLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputMeter)
};
