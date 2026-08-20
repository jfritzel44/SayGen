#pragma once
#include <JuceHeader.h>
#include "VelocityCurve.h"

//==============================================================================
// A standalone panel showing how played velocity maps to output level, as a
// draggable curve rather than a knob: click/drag anywhere in the plot and the
// curve bends to pass through that point. Backed by the "velocityCurve"
// parameter (-1 = only hard hits read loud, 0 = linear, +1 = soft touches
// read loud too), via the same shapeVelocity() the voice uses, so what's
// drawn is exactly what's heard.
//
// Lives outside the main editor's fixed layout — toggled in and out of view
// rather than permanently taking up space in it.
class VelocityCurveEditor : public juce::Component
{
public:
    explicit VelocityCurveEditor (juce::AudioProcessorValueTreeState& apvts)
        : attachment (*apvts.getParameter ("velocityCurve"),
                      [this] (float v) { curveAmount = v; repaint(); })
    {
        attachment.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (juce::Colour (0xff3a3a3a));
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (juce::Colour (0xff888888));
        g.drawRoundedRectangle (bounds, 10.0f, 1.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("Velocity Curve", bounds.withHeight (26.0f).translated (0, 8.0f),
                    juce::Justification::centred);

        auto plot = plotArea();

        // Grid
        g.setColour (juce::Colour (0xff555555));
        for (int i = 1; i < 4; ++i)
        {
            auto x = plot.getX() + plot.getWidth()  * (float) i / 4.0f;
            auto y = plot.getY() + plot.getHeight() * (float) i / 4.0f;
            g.drawVerticalLine   ((int) x, plot.getY(), plot.getBottom());
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        }
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawRect (plot, 1.0f);

        // The curve itself
        juce::Path curve;
        for (int i = 0; i <= 64; ++i)
        {
            auto x = (float) i / 64.0f;
            auto y = shapeVelocity (x, curveAmount);
            auto px = plot.getX() + x * plot.getWidth();
            auto py = plot.getBottom() - y * plot.getHeight();

            if (i == 0) curve.startNewSubPath (px, py);
            else        curve.lineTo (px, py);
        }
        g.setColour (juce::Colour (0xff5ec8f2));
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        // Draggable handle, at the curve's midpoint
        auto handleY  = shapeVelocity (0.5f, curveAmount);
        auto handlePt = juce::Point<float> (plot.getX() + 0.5f * plot.getWidth(),
                                            plot.getBottom() - handleY * plot.getHeight());
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (handlePt));

        // Axis labels
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("velocity", plot.withY (plot.getBottom() + 4.0f).withHeight (16.0f),
                    juce::Justification::centred);

        {
            juce::Graphics::ScopedSaveState save (g);
            auto centre = plot.getCentreY();
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                              plot.getX() - 16.0f, centre));
            g.drawText ("level", juce::Rectangle<float> (plot.getX() - 16.0f - 40.0f, centre - 8.0f, 80.0f, 16.0f),
                       juce::Justification::centred);
        }

        // Current amount, as plain text (drag feedback + a numeric reference
        // for dialing in the same feel by ear across sessions)
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String (curveAmount, 2), bounds.removeFromBottom (18.0f),
                   juce::Justification::centred);
    }

    void resized() override {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        attachment.beginGesture();
        dragToValue (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        dragToValue (e);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        attachment.endGesture();
    }

    // Double-click resets straight to linear, matching the reset convention
    // used elsewhere in the UI (e.g. EGR/KB Amount knobs)
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        attachment.setValueAsCompleteGesture (0.0f);
    }

private:
    juce::Rectangle<float> plotArea() const
    {
        return getLocalBounds().toFloat().reduced (30.0f).withTrimmedTop (6.0f).withTrimmedBottom (24.0f);
    }

    void dragToValue (const juce::MouseEvent& e)
    {
        auto plot = plotArea();
        if (plot.getHeight() <= 0.0f)
            return;

        // Wherever's dragged, treat its height within the plot as the
        // curve's desired output at velocity=0.5, and solve for the
        // exponent (then curveAmount) that puts the curve through that
        // point - so the curve visibly bends to follow the cursor.
        auto outputHalf = juce::jlimit (0.02f, 0.98f,
            (plot.getBottom() - e.position.y) / plot.getHeight());

        auto exponent = juce::jlimit (0.25f, 4.0f,
            std::log (outputHalf) / std::log (0.5f));

        auto newCurveAmount = juce::jlimit (-1.0f, 1.0f,
            -std::log (exponent) / std::log (4.0f));

        attachment.setValueAsPartOfGesture (newCurveAmount);
    }

    juce::ParameterAttachment attachment;
    float curveAmount = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelocityCurveEditor)
};
