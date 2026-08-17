#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

//==============================================================================
// Shared look for every knob-level label - LabeledKnob's title (Attack, Decay,
// KB Amount, ...), LedOctaveSelector's title/row numbers, and
// SyncToggleButton's "1-2 Sync". One place to change the size or typeface for
// all of them at once instead of editing each component separately.
namespace KnobLabelStyle
{
    constexpr float fontSize = 13.0f;

    inline juce::Typeface::Ptr typeface()
    {
        static const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor (
            BinaryData::BarlowCondensedMedium_ttf, BinaryData::BarlowCondensedMedium_ttfSize);
        return typeface;
    }

    inline juce::FontOptions font()
    {
        return juce::FontOptions (fontSize).withTypeface (typeface());
    }
}
