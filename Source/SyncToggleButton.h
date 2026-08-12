#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

//==============================================================================
// A push-to-toggle switch styled after the Little Phatty's OSC 2 SYNC button:
// shows toggle_on/toggle_off artwork and drives a bool APVTS parameter.
class SyncToggleButton : public juce::Component
{
public:
    SyncToggleButton (juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& parameterID,
                      const juce::String& title)
        : attachment (*apvts.getParameter (parameterID), [this] (float v)
                      { on = v >= 0.5f; repaint(); }),
          titleText (title)
    {
        onImage  = cropToContent (juce::ImageCache::getFromMemory (BinaryData::toggle_on_png,
                                                                    BinaryData::toggle_on_pngSize));
        offImage = cropToContent (juce::ImageCache::getFromMemory (BinaryData::toggle_off_png,
                                                                    BinaryData::toggle_off_pngSize));
        attachment.sendInitialUpdate();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (titleText, getLocalBounds().withHeight (18),
                    juce::Justification::centred);

        const auto& img = on ? onImage : offImage;
        if (img.isValid())
        {
            // Cropped to content (see cropToContent), so this size *is* the
            // visible button size now, unlike the padded source artwork.
            // 10% larger than the original 24px icon.
            constexpr float iconSize = 24.0f * 1.10f;
            auto iconArea = getLocalBounds().withTrimmedTop (20);
            auto iconRect = juce::Rectangle<float> (iconSize, iconSize)
                                .withCentre (iconArea.getCentre().toFloat());
            g.drawImage (img, iconRect, juce::RectanglePlacement::centred);
        }
    }

private:
    // The on/off artwork pads its button graphic differently within the
    // source canvas, so centring the raw images shifts the button visibly
    // between states. Crop each down to its actual content first so both
    // land on the same centre regardless of how they were exported.
    static juce::Image cropToContent (const juce::Image& src)
    {
        if (!src.isValid())
            return src;

        auto argb = src.convertedToFormat (juce::Image::ARGB);
        juce::Image::BitmapData data (argb, juce::Image::BitmapData::readOnly);

        int minX = argb.getWidth(), minY = argb.getHeight(), maxX = -1, maxY = -1;
        for (int y = 0; y < argb.getHeight(); ++y)
            for (int x = 0; x < argb.getWidth(); ++x)
                if (data.getPixelColour (x, y).getAlpha() > 10)
                {
                    minX = juce::jmin (minX, x); maxX = juce::jmax (maxX, x);
                    minY = juce::jmin (minY, y); maxY = juce::jmax (maxY, y);
                }

        if (maxX < minX || maxY < minY)
            return src;

        return argb.getClippedImage ({ minX, minY, maxX - minX + 1, maxY - minY + 1 });
    }

    juce::ParameterAttachment attachment;
    juce::String titleText;
    juce::Image onImage, offImage;
    bool on = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SyncToggleButton)
};
