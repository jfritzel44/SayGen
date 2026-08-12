#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

//==============================================================================
// A vertical bank of four LEDs mimicking the Little Phatty octave switch.
// Rows read 2 / 4 / 8 / 16 from the top (footage: 2' is two octaves up,
// 16' one octave down); the parameter stores 0 = 16' .. 3 = 2'.
// Click or drag onto a row to select it.
class LedOctaveSelector : public juce::Component
{
public:
    LedOctaveSelector (juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& parameterID,
                       const juce::String& title)
        : attachment (*apvts.getParameter (parameterID), [this] (float v)
                      { selected = juce::jlimit (0, 3, (int) std::round (v)); repaint(); }),
          titleText (title)
    {
        ledOn  = juce::ImageCache::getFromMemory (BinaryData::led_on_png,
                                                  BinaryData::led_on_pngSize);
        ledOff = juce::ImageCache::getFromMemory (BinaryData::led_off_png,
                                                  BinaryData::led_off_pngSize);
        attachment.sendInitialUpdate();
    }

    void mouseDown (const juce::MouseEvent& e) override { selectRowAt (e.y); }
    void mouseDrag (const juce::MouseEvent& e) override { selectRowAt (e.y); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (titleText, getLocalBounds().withTrimmedTop (2).withHeight (18),
                    juce::Justification::centred);

        static const char* names[] = { "2", "4", "8", "16" };
        for (int row = 0; row < 4; ++row)
        {
            auto r = rowBounds (row);
            auto led = r.removeFromLeft (r.getHeight()).reduced (2).toFloat();

            const auto& img = (valueForRow (row) == selected) ? ledOn : ledOff;
            if (img.isValid())
                g.drawImage (img, led);

            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (10.5f));
            g.drawText (names[row], r, juce::Justification::centredLeft);
        }
    }

private:
    // Same vertical region LabeledKnob gives its slider, so the LED bank
    // lines up with the knobs beside it
    juce::Rectangle<int> rowsArea() const
    {
        return getLocalBounds().withTrimmedTop (20).withTrimmedBottom (2);
    }

    juce::Rectangle<int> rowBounds (int row) const
    {
        auto area = rowsArea();
        auto rowH = area.getHeight() / 4;
        return { area.getX(), area.getY() + row * rowH, area.getWidth(), rowH };
    }

    static int valueForRow (int row) { return 3 - row; }

    void selectRowAt (int y)
    {
        auto area = rowsArea();
        auto row  = juce::jlimit (0, 3, (y - area.getY()) / (area.getHeight() / 4));
        auto value = (float) valueForRow (row);

        if ((int) value != selected)
            attachment.setValueAsCompleteGesture (value);
    }

    juce::ParameterAttachment attachment;
    juce::String titleText;
    juce::Image ledOn, ledOff;
    int selected = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LedOctaveSelector)
};
