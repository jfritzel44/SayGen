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

        const int textHeight = 20;
        auto angle = startAngle + sliderPos * (endAngle - startAngle);
        auto size  = (float) juce::jmax (0, juce::jmin (width - 6, height - textHeight - 6));
        // Top-align the knob and tuck the value text right under it, so the
        // cluster hugs the title label above instead of floating in the box
        auto cx    = x + width * 0.5f;
        auto cy    = y + 3.0f + size * 0.5f;
        if (size <= 0.0f)
            return;

        const auto radius = size * 0.5f;
        juce::Path track;
        track.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff161c1c));
        g.strokePath (track, juce::PathStrokeType (2.0f));

        // Bipolar parameters fill outward from their neutral position.
        const auto neutralAngle = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0
            ? startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle)
            : startAngle;
        juce::Path valueArc;
        valueArc.addCentredArc (cx, cy, radius, radius, 0.0f,
                               juce::jmin (neutralAngle, angle), juce::jmax (neutralAngle, angle), true);
        const auto arcOpacity = slider.isEnabled() ? 0.95f : 0.3f;
        juce::ColourGradient arcGradient (
            juce::Colour (0xffc3ffe0).withAlpha (arcOpacity), cx - radius, cy - radius,
            juce::Colour (0xff299b73).withAlpha (arcOpacity), cx + radius, cy + radius,
            false);
        arcGradient.addColour (0.5, juce::Colour (0xff79cba5).withAlpha (arcOpacity));
        g.setGradientFill (arcGradient);
        g.strokePath (valueArc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        size -= 7.0f;
        // The source art is slightly non-square (985x1005); scale each axis to
        // the same target size so it renders as a true circle centred on the
        // rotation point, otherwise the knob wobbles as it turns.
        auto scaleX = size / (float) oscImage.getWidth();
        auto scaleY = size / (float) oscImage.getHeight();

        g.setColour (juce::Colours::white.withAlpha (slider.isEnabled() ? 1.0f : 0.4f));
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageTransformed (oscImage,
            juce::AffineTransform::scale (scaleX, scaleY)
                .followedBy (juce::AffineTransform::translation (cx - size * 0.5f,
                                                                  cy - size * 0.5f))
                .followedBy (juce::AffineTransform::rotation (angle, cx, cy)));

        g.setColour (juce::Colour (0xffe5e8df));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (slider.getTextFromValue (slider.getValue()),
                    x, y + height - textHeight, width, textHeight,
                    juce::Justification::centred);
    }

private:
    juce::Image oscImage;
};
