#pragma once
#include <JuceHeader.h>

//==============================================================================
// Sylenth1-style LCD effects screen: an effect list with enable checkboxes
// down the left, and the selected effect's controls on the right. Clicking an
// effect's row (or its checkbox) brings up that effect's settings. Effects
// are listed in signal-chain order.
class LcdScreen : public juce::Component
{
public:
    LcdScreen (juce::AudioProcessorValueTreeState& apvts)
    {
        listViewport.setViewedComponent (&listComponent, false);
        listViewport.setScrollBarsShown (true, false);
        listViewport.setScrollBarThickness (7);
        listViewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId,
                                                       ink.withAlpha (0.7f));
        addAndMakeVisible (listViewport);

        initEffect (0, "GATE",   "gateOn",   apvts);
        addKnob (0, apvts, "gateThresh",  "THRESH", " dB", 0);
        addKnob (0, apvts, "gateRatio",   "RATIO",  "",    1);
        addKnob (0, apvts, "gateAttack",  "ATT",    " ms", 1);
        addKnob (0, apvts, "gateRelease", "REL",    " ms", 0);

        initEffect (1, "LADDER", "ladderOn", apvts);
        addKnob (1, apvts, "ladderCutoff", "CUTOFF", " Hz", 0);
        addKnob (1, apvts, "ladderRes",    "RES",    "",    2);
        addKnob (1, apvts, "ladderDrive",  "DRIVE",  "",    1);

        initEffect (2, "CHORUS", "chorusOn", apvts);
        addKnob (2, apvts, "chorusRate",  "RATE",  " Hz", 2);
        addKnob (2, apvts, "chorusDepth", "DEPTH", "",    2);
        addKnob (2, apvts, "chorusMix",   "MIX",   "",    2);

        initEffect (3, "PHASER", "phaserOn", apvts);
        addKnob (3, apvts, "phaserRate",     "RATE",  " Hz", 2);
        addKnob (3, apvts, "phaserDepth",    "DEPTH", "",    2);
        addKnob (3, apvts, "phaserFeedback", "FDBK",  "",    2);
        addKnob (3, apvts, "phaserMix",      "MIX",   "",    2);

        initEffect (4, "REVERB", "reverbOn", apvts);
        addKnob (4, apvts, "reverbSize",  "SIZE",  "", 2);
        addKnob (4, apvts, "reverbDamp",  "DAMP",  "", 2);
        addKnob (4, apvts, "reverbWidth", "WIDTH", "", 2);
        addKnob (4, apvts, "reverbMix",   "MIX",   "", 2);

        initEffect (5, "COMP",   "compOn",   apvts);
        addKnob (5, apvts, "compThresh",  "THRESH", " dB", 0);
        addKnob (5, apvts, "compRatio",   "RATIO",  "",    1);
        addKnob (5, apvts, "compAttack",  "ATT",    " ms", 1);
        addKnob (5, apvts, "compRelease", "REL",    " ms", 0);

        initEffect (6, "LIMIT",  "limitOn",  apvts);
        addKnob (6, apvts, "limitThresh",  "THRESH", " dB", 1);
        addKnob (6, apvts, "limitRelease", "REL",    " ms", 0);

        updateVisibility();
    }

    ~LcdScreen() override
    {
        for (auto& effect : effects)
        {
            effect.onButton.setLookAndFeel (nullptr);
            for (int i = 0; i < effect.numKnobs; ++i)
                effect.knobs[i].slider.setLookAndFeel (nullptr);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        // Bezel
        g.setColour (juce::Colour (0xff15181f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (juce::Colour (0xff474e5c));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        // Screen glass
        auto screen = screenArea().toFloat();
        g.setColour (screenBg);
        g.fillRoundedRectangle (screen, 4.0f);
        g.setColour (juce::Colour (0xff5a6a7a));
        g.drawRoundedRectangle (screen, 4.0f, 1.0f);

        // Header strip
        auto header = headerArea();
        g.setColour (cellBg);
        g.fillRect (header);
        g.setColour (ink);
        g.drawRect (header);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (" EFFECTS", header, juce::Justification::centredLeft);
        g.drawText (effects[selectedEffect].name + juce::String (" "),
                    header, juce::Justification::centredRight);

        // Divider between list and controls
        g.setColour (ink.withAlpha (0.5f));
        auto list = listArea();
        g.drawVerticalLine (list.getRight() + 5, (float) list.getY(),
                            (float) screenArea().getBottom() - 5.0f);
    }

    void resized() override
    {
        listViewport.setBounds (listArea());
        listComponent.setSize (listArea().getWidth() - listViewport.getScrollBarThickness() - 2,
                               numEffects * rowHeight);

        for (int i = 0; i < numEffects; ++i)
        {
            effects[i].onButton.setBounds (rowArea (i).removeFromLeft (22).reduced (2));
            layoutKnobs (effects[i]);
        }
    }

private:
    //==============================================================================
    // Flat dark-blue-on-pale-blue drawing so controls read as part of the LCD
    struct LcdLookAndFeel : public juce::LookAndFeel_V4
    {
        LcdLookAndFeel()
        {
            setColour (juce::Slider::textBoxTextColourId,    juce::Colour (0xff1c3a5e));
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                               float pos, float startAngle, float endAngle,
                               juce::Slider&) override
        {
            auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
            auto size   = 0.75f * juce::jmin (bounds.getWidth(), bounds.getHeight());
            auto knob   = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());

            g.setColour (juce::Colour (0xffc2d0dd));
            g.fillEllipse (knob);
            g.setColour (juce::Colour (0xff1c3a5e));
            g.drawEllipse (knob.reduced (0.75f), 1.5f);

            auto angle  = startAngle + pos * (endAngle - startAngle);
            auto centre = knob.getCentre();
            auto tip    = centre.getPointOnCircumference (size * 0.5f - 3.0f, angle);
            g.drawLine ({ centre, tip }, 2.5f);
        }

        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                               bool highlighted, bool) override
        {
            auto box = button.getLocalBounds().toFloat();
            auto size = juce::jmin (box.getWidth(), box.getHeight());
            box = juce::Rectangle<float> (size, size).withCentre (box.getCentre());

            g.setColour (juce::Colour (highlighted ? 0xffd4e0ea : 0xffc2d0dd));
            g.fillRect (box);
            g.setColour (juce::Colour (0xff1c3a5e));
            g.drawRect (box, 1.5f);

            if (button.getToggleState())
            {
                juce::Path tick;
                tick.startNewSubPath (box.getRelativePoint (0.22f, 0.52f));
                tick.lineTo          (box.getRelativePoint (0.45f, 0.75f));
                tick.lineTo          (box.getRelativePoint (0.80f, 0.25f));
                g.strokePath (tick, juce::PathStrokeType (1.5f));
            }
        }
    };

    // Scrollable effect list: draws the rows and hosts the enable checkboxes,
    // sized to the full row count and placed inside the viewport
    struct EffectList : public juce::Component
    {
        EffectList (LcdScreen& ownerToUse) : owner (ownerToUse) {}

        void paint (juce::Graphics& g) override
        {
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            for (int i = 0; i < numEffects; ++i)
            {
                auto row = owner.rowArea (i);
                if (i == owner.selectedEffect)
                {
                    g.setColour (owner.highlight);
                    g.fillRoundedRectangle (row.reduced (0, 1).toFloat(), 3.0f);
                }
                g.setColour (i == owner.selectedEffect ? juce::Colours::white : owner.ink);
                g.drawText (owner.effects[i].name, row.withTrimmedLeft (26),
                            juce::Justification::centredLeft);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            auto index = e.y / rowHeight;
            if (index >= 0 && index < numEffects)
                owner.selectEffect (index);
        }

        LcdScreen& owner;
    };

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    static constexpr int maxKnobs = 4;

    struct Effect
    {
        juce::String name;
        juce::ToggleButton onButton;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
        Knob knobs[maxKnobs];
        int numKnobs = 0;
    };

    void initEffect (int index, const juce::String& name, const juce::String& onParamID,
                     juce::AudioProcessorValueTreeState& apvts)
    {
        auto& effect = effects[index];
        effect.name = name;
        effect.onButton.setLookAndFeel (&lcdLookAndFeel);
        effect.onButton.onClick = [this, index] { selectEffect (index); };
        listComponent.addAndMakeVisible (effect.onButton);
        effect.onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, onParamID, effect.onButton);
    }

    void addKnob (int effectIndex, juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& parameterID, const juce::String& title,
                  const juce::String& suffix, int decimals)
    {
        auto& effect = effects[effectIndex];
        jassert (effect.numKnobs < maxKnobs);
        auto& knob = effect.knobs[effect.numKnobs++];

        knob.slider.setLookAndFeel (&lcdLookAndFeel);
        knob.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 12);
        knob.slider.setTextValueSuffix (suffix);
        knob.slider.setNumDecimalPlacesToDisplay (decimals);
        addAndMakeVisible (knob.slider);

        knob.label.setText (title, juce::dontSendNotification);
        knob.label.setJustificationType (juce::Justification::centred);
        knob.label.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        knob.label.setColour (juce::Label::textColourId, ink);
        addAndMakeVisible (knob.label);

        knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, parameterID, knob.slider);
    }

    void layoutKnobs (Effect& effect)
    {
        auto area  = controlsArea();
        auto cellW = juce::jmin (65, area.getWidth() / juce::jmax (1, effect.numKnobs));
        auto x     = area.getX() + (area.getWidth() - cellW * effect.numKnobs) / 2;

        for (int i = 0; i < effect.numKnobs; ++i)
        {
            juce::Rectangle<int> cell (x + i * cellW, area.getY(), cellW, area.getHeight());
            effect.knobs[i].label.setBounds  (cell.removeFromTop (12));
            effect.knobs[i].slider.setBounds (cell.reduced (2));
        }
    }

    void selectEffect (int index)
    {
        selectedEffect = index;
        updateVisibility();
        repaint();
        listComponent.repaint();
    }

    void updateVisibility()
    {
        for (int e = 0; e < numEffects; ++e)
            for (int i = 0; i < effects[e].numKnobs; ++i)
            {
                effects[e].knobs[i].slider.setVisible (e == selectedEffect);
                effects[e].knobs[i].label.setVisible  (e == selectedEffect);
            }
    }

    //==============================================================================
    juce::Rectangle<int> screenArea() const  { return getLocalBounds().reduced (8); }

    juce::Rectangle<int> headerArea() const
    {
        return screenArea().reduced (4).removeFromTop (19);
    }

    juce::Rectangle<int> listArea() const
    {
        auto screen = screenArea().reduced (4);
        screen.removeFromTop (23);
        return screen.removeFromLeft (95);
    }

    // Row bounds in the list component's local coordinates
    juce::Rectangle<int> rowArea (int index) const
    {
        return { 0, index * rowHeight, listComponent.getWidth(), rowHeight };
    }

    juce::Rectangle<int> controlsArea() const
    {
        auto screen = screenArea().reduced (4);
        screen.removeFromTop (23);
        screen.removeFromLeft (95 + 8);
        return screen;
    }

    //==============================================================================
    const juce::Colour screenBg  { 0xffaebfd0 };
    const juce::Colour cellBg    { 0xffc2d0dd };
    const juce::Colour ink       { 0xff1c3a5e };
    const juce::Colour highlight { 0xff4878a8 };

    static constexpr int numEffects = 7;
    static constexpr int rowHeight  = 20;

    LcdLookAndFeel lcdLookAndFeel;
    Effect effects[numEffects];
    EffectList listComponent { *this };
    juce::Viewport listViewport;

    int selectedEffect = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LcdScreen)
};
