#pragma once
#include <JuceHeader.h>

//==============================================================================
// Shapes a linear [0,1] MIDI velocity into the amplitude a voice actually
// uses. curveAmount is in [-1, 1]: 0 leaves velocity untouched (linear), a
// positive amount boosts quieter notes so a soft touch still reads loud, a
// negative amount suppresses them so only hard hits read loud. Shared between
// the DSP (Oscillator.h) and the curve editor UI so what's drawn is exactly
// what's heard.
inline float shapeVelocity (float velocity, float curveAmount)
{
    const float exponent = std::pow (4.0f, -curveAmount);
    return std::pow (juce::jlimit (0.0001f, 1.0f, velocity), exponent);
}
