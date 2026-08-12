#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

class OscKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OscKnobLookAndFeel()
    {
        oscImage = juce::ImageCache::getFromMemory (BinaryData::osc_png,
                                                    BinaryData::osc_pngSize);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        if (! oscImage.isValid())
            return;

        const int textHeight = 26;
        auto angle = startAngle + sliderPos * (endAngle - startAngle);
        auto size  = (float) juce::jmin (width, height - textHeight);
        // Top-align the knob and tuck the value text right under it, so the
        // cluster hugs the title label above instead of floating in the box
        auto cx    = x + width * 0.5f;
        auto cy    = y + size * 0.5f;
        // The source art is slightly non-square (985x1005); scale each axis to
        // the same target size so it renders as a true circle centred on the
        // rotation point, otherwise the knob wobbles as it turns.
        auto scaleX = size / (float) oscImage.getWidth();
        auto scaleY = size / (float) oscImage.getHeight();

        g.drawImageTransformed (oscImage,
            juce::AffineTransform::scale (scaleX, scaleY)
                .followedBy (juce::AffineTransform::translation (cx - size * 0.5f,
                                                                  cy - size * 0.5f))
                .followedBy (juce::AffineTransform::rotation (angle, cx, cy)));

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (9.5f));
        g.drawText (slider.getTextFromValue (slider.getValue()),
                    x, y + (int) size + 4, width, textHeight - 2,
                    juce::Justification::centred);
    }

private:
    juce::Image oscImage;
};
