#pragma once
#include <JuceHeader.h>

//==============================================================================
// Turns the computer keyboard into a MIDI keyboard by feeding note on/offs
// into a MidiKeyboardState. Reusable from any component that can take
// keyboard focus:
//
//     QwertyMidiKeyboard qwerty { processor.keyboardState };
//     qwerty.attachTo (*this);          // e.g. in an editor constructor
//
// The processor merges the state into its MIDI stream each block:
//
//     keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);
//
// The default layout maps q w e r t y u i o p a s d f g h j k l z x c v b n m
// to consecutive white keys starting at C1.
class QwertyMidiKeyboard : private juce::KeyListener,
                           private juce::FocusChangeListener
{
public:
    explicit QwertyMidiKeyboard (juce::MidiKeyboardState& stateToUse,
                                 int midiChannelToUse = 1,
                                 float velocityToUse = 0.9f)
        : state (stateToUse), midiChannel (midiChannelToUse), velocity (velocityToUse)
    {
        const juce::String keys ("qwertyuiopasdfghjklzxcvbnm");
        static const int whiteKeySemitones[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int baseNote = 36; // C1 (with middle C = C4 = 60)

        for (int i = 0; i < keys.length(); ++i)
            mappings.add ({ keys[i],
                            baseNote + 12 * (i / 7) + whiteKeySemitones[i % 7],
                            false });
    }

    ~QwertyMidiKeyboard() override { detach(); }

    void attachTo (juce::Component& comp)
    {
        detach();
        target = &comp;
        comp.addKeyListener (this);
        comp.setWantsKeyboardFocus (true);
        juce::Desktop::getInstance().addFocusChangeListener (this);
    }

    void detach()
    {
        allNotesOff();

        if (target != nullptr)
        {
            juce::Desktop::getInstance().removeFocusChangeListener (this);
            target->removeKeyListener (this);
            target = nullptr;
        }
    }

    void allNotesOff()
    {
        for (auto& m : mappings)
            if (std::exchange (m.isDown, false))
                state.noteOff (midiChannel, m.note, 0.0f);
    }

private:
    struct Mapping
    {
        juce::juce_wchar key;
        int note;
        bool isDown;
    };

    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        // Consume presses of mapped keys (including auto-repeats) so they
        // don't reach other handlers or beep.
        auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

        for (auto& m : mappings)
            if (m.key == c)
                return true;

        return false;
    }

    bool keyStateChanged (bool, juce::Component*) override
    {
        bool used = false;

        for (auto& m : mappings)
        {
            const auto upper = juce::CharacterFunctions::toUpperCase (m.key);
            const bool down  = juce::KeyPress::isKeyCurrentlyDown ((int) m.key)
                            || juce::KeyPress::isKeyCurrentlyDown ((int) upper);

            if (down != m.isDown)
            {
                m.isDown = down;

                if (down)
                    state.noteOn  (midiChannel, m.note, velocity);
                else
                    state.noteOff (midiChannel, m.note, 0.0f);

                used = true;
            }
        }

        return used;
    }

    // Release held notes if focus moves away, otherwise they'd hang.
    void globalFocusChanged (juce::Component* focused) override
    {
        if (target == nullptr
            || focused == nullptr
            || (focused != target && ! target->isParentOf (focused)))
            allNotesOff();
    }

    juce::MidiKeyboardState& state;
    juce::Component* target = nullptr;
    int midiChannel;
    float velocity;
    juce::Array<Mapping> mappings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QwertyMidiKeyboard)
};
