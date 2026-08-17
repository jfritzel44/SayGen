#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

//==============================================================================
// Draws Osc 1's waveform knob as a physical pointer knob plus four fixed
// waveform icons drawn around it, one on each 90-degree axis: saw up,
// square right, triangle down, sine left. Rather than rotating one source
// photo at draw time (which softened/dimmed the rotated orientations),
// four separately pre-rotated images are used, one per orientation, drawn
// with no transform.
class OscWaveformLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OscWaveformLookAndFeel()
    {
        knobImages[0] = juce::ImageCache::getFromMemory (BinaryData::osc_left_png,  BinaryData::osc_left_pngSize);
        knobImages[1] = juce::ImageCache::getFromMemory (BinaryData::osc_up_png,    BinaryData::osc_up_pngSize);
        knobImages[2] = juce::ImageCache::getFromMemory (BinaryData::osc_right_png, BinaryData::osc_right_pngSize);
        knobImages[3] = juce::ImageCache::getFromMemory (BinaryData::osc_down_png,  BinaryData::osc_down_pngSize);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float, float, float, juce::Slider& slider) override
    {
        const float cx = x + width * 0.5f;
        const float cy = y + height * 0.5f;

        // Shared between Osc 1 (range 0-3: sine/saw/square/triangle) and
        // Osc 2 (range 0-4: off/sine/saw/square/triangle). Osc 2's extra
        // "off" state has no dedicated icon, so it's shown as a dimmed
        // sine-facing knob with none of the four icons lit.
        const bool hasOffState = slider.getMaximum() > 3.5;
        const double rawValue  = slider.getValue();
        const bool isOff       = hasOffState && rawValue < 0.5;
        const int type = juce::jlimit (0, 3, (int) std::round (hasOffState ? rawValue - 1.0 : rawValue));

        // Drawn before the icons below: JUCE's drawImage() takes its opacity
        // from the Graphics context's *current fill colour's alpha*
        // (CoreGraphicsContext::drawImage uses state->fillType.getOpacity()),
        // so if this ran after drawIcon()'s g.setColour(...withAlpha(...))
        // calls, it would silently inherit whatever alpha the last icon
        // stroke used instead of drawing fully opaque.
        const auto& knobImage = knobImages[(size_t) type];
        if (knobImage.isValid())
        {
            auto knobSize = (float) juce::jmin (width, height) * 0.5f;
            auto bounds = juce::Rectangle<float> (knobSize, knobSize).withCentre ({ cx, cy });

            g.drawImage (knobImage, bounds, juce::RectanglePlacement::centred);
        }

        // Icon centres are inset from the slider's own edges (rather than
        // the knob's bounding square) so their strokes don't get clipped.
        constexpr float margin = 7.0f;
        drawIcon (g, Waveform::saw,      cx,                              y + iconH * 0.5f + margin,      !isOff && type == 1);
        drawIcon (g, Waveform::square,   x + width - iconW * 0.5f - margin, cy,                            !isOff && type == 2);
        drawIcon (g, Waveform::triangle, cx,                              y + height - iconH * 0.5f - margin, !isOff && type == 3);
        drawIcon (g, Waveform::sine,     x + iconW * 0.5f + margin,      cy,                                !isOff && type == 0);
    }

private:
    enum class Waveform { sine, saw, square, triangle };

    static constexpr float iconW = 10.45f, iconH = 7.6f;

    void drawIcon (juce::Graphics& g, Waveform wave, float cx, float cy, bool selected)
    {
        juce::Rectangle<float> r (iconW, iconH);
        r.setCentre (cx, cy);

        juce::Path p;
        switch (wave)
        {
            case Waveform::sine:
                p.startNewSubPath (r.getX(), r.getCentreY());
                p.cubicTo (r.getX() + r.getWidth() * 0.25f, r.getY(),
                          r.getX() + r.getWidth() * 0.25f, r.getY(),
                          r.getCentreX(), r.getCentreY());
                p.cubicTo (r.getX() + r.getWidth() * 0.75f, r.getBottom(),
                          r.getX() + r.getWidth() * 0.75f, r.getBottom(),
                          r.getRight(), r.getCentreY());
                break;

            case Waveform::saw:
                p.startNewSubPath (r.getX(), r.getBottom());
                p.lineTo (r.getRight(), r.getY());
                p.lineTo (r.getRight(), r.getBottom());
                break;

            case Waveform::square:
                p.startNewSubPath (r.getX(), r.getBottom());
                p.lineTo (r.getX(), r.getY());
                p.lineTo (r.getCentreX(), r.getY());
                p.lineTo (r.getCentreX(), r.getBottom());
                p.lineTo (r.getRight(), r.getBottom());
                p.lineTo (r.getRight(), r.getY());
                break;

            case Waveform::triangle:
                p.startNewSubPath (r.getX(), r.getBottom());
                p.lineTo (r.getCentreX(), r.getY());
                p.lineTo (r.getRight(), r.getBottom());
                break;
        }

        g.setColour (juce::Colours::white.withAlpha (selected ? 0.95f : 0.45f));
        g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Image knobImages[4];
};
