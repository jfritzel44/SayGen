#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>
#include <vector>

namespace syngen::ai
{
inline juce::var object (std::initializer_list<std::pair<juce::Identifier, juce::var>> fields)
{
    auto* result = new juce::DynamicObject();
    for (const auto& field : fields) result->setProperty (field.first, field.second);
    return juce::var (result);
}

using Patch = std::vector<std::pair<juce::String, float>>;

inline bool allowed (const juce::String& id)
{
    // Preserve the player's output level and keyboard feel.
    return id != "masterVolume" && id != "velocityCurve";
}

inline Patch snapshot (juce::AudioProcessor& processor)
{
    Patch result;
    for (auto* base : processor.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (base))
            if (allowed (p->paramID))
                result.emplace_back (p->paramID, p->convertFrom0to1 (p->getValue()));
    return result;
}

inline void apply (juce::AudioProcessorValueTreeState& state, const Patch& patch)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    for (const auto& [id, value] : patch)
        if (auto* p = state.getParameter (id); p != nullptr && allowed (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
}

// Validate the entire response before any parameter is touched.
inline juce::Result parsePatch (const juce::String& text,
                                juce::AudioProcessorValueTreeState& state, Patch& patch)
{
    patch.clear();
    juce::var root;
    if (juce::JSON::parse (text, root).failed())
        return juce::Result::fail ("The AI returned invalid JSON. Try again.");
    auto* values = root.getProperty ("parameters", {}).getDynamicObject();
    if (values == nullptr || values->getProperties().size() == 0)
        return juce::Result::fail ("The AI returned no sound parameters. Try a more specific description.");
    Patch checked;
    for (const auto& entry : values->getProperties())
    {
        auto id = entry.name.toString();
        auto* p = state.getParameter (id);
        if (p == nullptr || ! allowed (id))
            return juce::Result::fail ("The AI returned an unsupported parameter: " + id);
        const auto& v = entry.value;
        const bool booleanParameter = dynamic_cast<juce::AudioParameterBool*> (p) != nullptr;
        if (! (v.isDouble() || v.isInt() || v.isInt64() || (v.isBool() && booleanParameter)))
            return juce::Result::fail ("The AI returned a non-numeric value for " + id);
        auto value = static_cast<double> (v);
        const auto& range = p->getNormalisableRange();
        if (! std::isfinite (value) || value < range.start || value > range.end)
            return juce::Result::fail ("The AI returned an out-of-range value for " + id);
        checked.emplace_back (id, range.snapToLegalValue (static_cast<float> (value)));
    }
    patch = std::move (checked);
    return juce::Result::ok();
}

inline juce::var schema (juce::AudioProcessor& processor)
{
    auto properties = object ({});
    for (auto* base : processor.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (base))
        {
            if (! allowed (p->paramID)) continue;
            const auto& range = p->getNormalisableRange();
            auto description = p->getName (100);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p))
                for (int i = 0; i < choice->choices.size(); ++i)
                    description += "; " + juce::String (i) + "=" + choice->choices[i];
            const bool integer = range.interval == 1.0f;
            auto definition = object ({ { "type", integer ? "integer" : "number" }, { "minimum", range.start },
                                       { "maximum", range.end }, { "description", description } });
            properties.getDynamicObject()->setProperty (p->paramID, definition);
        }
    return object ({ { "type", "object" },
        { "properties", object ({ { "parameters", object ({ { "type", "object" },
            { "properties", properties }, { "additionalProperties", false } }) } }) },
        { "required", juce::Array<juce::var> { "parameters" } }, { "additionalProperties", false } });
}

inline juce::String instructions (juce::AudioProcessor& processor)
{
    auto current = object ({});
    for (const auto& [id, value] : snapshot (processor))
        current.getDynamicObject()->setProperty (id, value);
    return "You design patches for SynGen, a two-oscillator subtractive synthesizer. "
           "Return only JSON {\"parameters\":{parameterID:numericValue,...}} in physical units, not normalized values. "
           "For a new sound, specify all relevant oscillator, envelope, filter, modulation and effect settings, "
           "including turning off unwanted effects. For relative requests like 'make it warmer', modify the current patch. "
           "oscType: 0=sine,1=saw,2=square,3=triangle. osc2Type: 0=off,1=sine,2=saw,3=square,4=triangle. "
           "osc1Octave/osc2Octave: 0=16 foot,1=8 foot,2=4 foot,3=2 foot. "
           "ModernOn=1 overrides the basic waveform with SawMix/PulseMix/TriMix; set ModernOn=0 to use basic waves. "
           "Envelope times are seconds, cutoff is Hz, detune is cents, pitch is semitones, envAmount is octaves. "
           "Glide forces mono legato. LFO depth requires the player's mod wheel; PWM runs independently. "
           "Use moderate resonance and drive; avoid excessive effect feedback. "
           "A warm pluck typically uses saw, cutoff about 1200 Hz, fast attack, 0.3s decay, low sustain, positive filter envelope. "
           "A soft pad typically uses detuned saw/triangle, slow attack/release, modest lowpass cutoff and reverb. "
           "Work within this synth's capabilities; you are not generating samples or hearing the result. "
           "Current patch: " + juce::JSON::toString (current, true);
}

struct Request
{
    juce::String model, key, prompt, system;
    juce::var outputSchema;
};

inline juce::var requestBody (const Request& r)
{
    auto parts = [] (const juce::String& text)
    { return object ({ { "parts", juce::Array<juce::var> { object ({ { "text", text } }) } } }); };
    return object ({ { "systemInstruction", parts (r.system) },
        { "contents", juce::Array<juce::var> { parts (r.prompt) } },
        { "generationConfig", object ({ { "temperature", 0.3 }, { "maxOutputTokens", 8192 },
            { "responseMimeType", "application/json" }, { "responseJsonSchema", r.outputSchema } }) } });
}

inline juce::Result extractResponse (const juce::String& response, juce::String& text)
{
    text.clear();
    juce::var root;
    if (juce::JSON::parse (response, root).failed())
        return juce::Result::fail ("The AI service returned an unreadable response.");
    {
        auto candidates = root.getProperty ("candidates", {});
        if (candidates.size() == 0 || candidates[0].getProperty ("finishReason", {}).toString() != "STOP")
            return juce::Result::fail ("Gemini could not finish this sound. Rephrase the description and try again.");
        auto parts = candidates[0].getProperty ("content", {}).getProperty ("parts", {});
        if (auto* array = parts.getArray())
            for (const auto& part : *array)
                if (! static_cast<bool> (part.getProperty ("thought", false)))
                    text += part.getProperty ("text", {}).toString();
    }
    return text.isNotEmpty() ? juce::Result::ok() : juce::Result::fail ("The AI returned no patch.");
}

// Owns all network state. No processor/UI pointers ever cross into this worker.
class Generation : public juce::Thread
{
public:
    explicit Generation (const Request& r)
        : juce::Thread ("AI sound generation"),
          stream (juce::URL ("https://generativelanguage.googleapis.com/v1beta/models/" + r.model + ":generateContent")
                      .withPOSTData (juce::JSON::toString (requestBody (r))), true)
    {
        stream.withConnectionTimeout (120000).withNumRedirectsToFollow (0)
              .withExtraHeaders ("Content-Type: application/json\r\n"
                  "x-goog-api-key: " + r.key + "\r\n");
    }
    ~Generation() override { cancel(); stopThread (-1); }
    void cancel() { signalThreadShouldExit(); stream.cancel(); }
    bool finished() const { return done.load(); }
    juce::String patchText, error; // Read only after finished() acquires done.
private:
    void run() override
    {
        if (threadShouldExit()) { done.store (true); return; }
        if (! stream.connect (nullptr))
            error = "Cannot reach Gemini. Check your internet connection and try again.";
        else if (const auto status = stream.getStatusCode(); status < 200 || status >= 300)
        {
            if (status == 429) error = "Gemini quota reached. Wait and retry, or check your Google project quota.";
            else if (status == 401 || status == 403) error = "API access denied. Check your Gemini API key.";
            else if (status == 404) error = "Model not found. Check the Gemini model name.";
            else error = "AI request failed (HTTP " + juce::String (status) + "). Check the model and API key.";
        }
        else
        {
            juce::MemoryOutputStream body;
            char buffer[4096];
            constexpr size_t limit = 256 * 1024;
            while (! threadShouldExit() && ! stream.isExhausted())
            {
                auto count = stream.read (buffer, sizeof (buffer));
                if (count <= 0) break;
                body.write (buffer, static_cast<size_t> (count));
                if (body.getDataSize() > limit) { error = "AI response exceeded the size limit."; break; }
            }
            if (error.isEmpty() && ! threadShouldExit())
            {
                auto result = extractResponse (body.toUTF8(), patchText);
                if (result.failed()) error = result.getErrorMessage();
            }
        }
        done.store (true);
    }
    juce::WebInputStream stream;
    std::atomic<bool> done { false };
};
}
