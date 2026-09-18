#include "../Source/PluginProcessor.h"
#include "../Source/AISoundDesigner.h"
#include <cstdlib>
#include <iostream>

static void require (bool condition, const char* message)
{
    if (! condition) { std::cerr << "FAIL: " << message << '\n'; std::exit (1); }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    MySynthAudioProcessor processor;
    auto& state = processor.apvts;
    using namespace syngen::ai;
    const auto original = snapshot (processor);
    Patch patch;
    require (parsePatch (R"({"parameters":{"cutoff":1200,"oscType":1,"oscSync":true}})", state, patch).wasOk(),
             "valid physical parameter values and boolean switches are accepted");
    require (snapshot (processor) == original, "parsing never changes the sound");
    const auto volume = state.getParameter ("masterVolume")->getValue();
    const auto velocity = state.getParameter ("velocityCurve")->getValue();
    syngen::ai::apply (state, patch);
    require (std::abs (state.getRawParameterValue ("cutoff")->load() - 1200.0f) < 0.1f,
             "patch uses physical units");
    require (state.getRawParameterValue ("oscSync")->load() == 1.0f, "boolean switch is applied");
    require (state.getParameter ("masterVolume")->getValue() == volume
             && state.getParameter ("velocityCurve")->getValue() == velocity,
             "generation preserves output level and keyboard feel");
    syngen::ai::apply (state, original);
    require (snapshot (processor) == original, "undo restores the original sound");

    for (const auto* bad : {
             "not json", "[]", "{}", R"({"parameters":[]})", R"({"parameters":{}})",
             R"({"parameters":{"cutoff":1200,"unknown":1}})",
             R"({"parameters":{"cutoff":1200,"masterVolume":0}})",
             R"({"parameters":{"velocityCurve":1}})", R"({"parameters":{"cutoff":"1200"}})",
             R"({"parameters":{"cutoff":null}})", R"({"parameters":{"oscType":true}})",
             R"({"parameters":{"cutoff":-1}})", R"({"parameters":{"cutoff":1e100}})" })
    {
        patch = original;
        require (parsePatch (bad, state, patch).failed(), "malformed or unsupported patch is rejected");
        require (patch.empty(), "invalid response cannot leave a partial patch to apply");
        require (snapshot (processor) == original, "invalid response preserves the sound");
    }
    require (parsePatch (R"({"parameters":{"oscType":1.7}})", state, patch).wasOk()
             && patch.front().second == 2.0f, "discrete values snap to legal settings");

    juce::String text;
    require (extractResponse (R"({"candidates":[{"finishReason":"STOP","content":{"parts":[{"thought":true,"text":"private thought"},{"text":"{\"parameters\":"},{"text":"{\"cutoff\":1200}}"}]}}]})", text).wasOk(),
             "completed response is extracted");
    require (parsePatch (text, state, patch).wasOk(), "thought parts are excluded and text parts joined");
    for (const auto* bad : { "not json", "{}", R"({"candidates":[]})",
             R"({"candidates":[{"finishReason":"MAX_TOKENS","content":{"parts":[{"text":"{}"}]}}]})",
             R"({"candidates":[{"finishReason":"STOP","content":{"parts":[]}}]})" })
        require (extractResponse (bad, text).failed() && text.isEmpty(), "unfinished or empty response is rejected");

    const auto outputSchema = schema (processor);
    const auto properties = outputSchema["properties"]["parameters"]["properties"];
    require (! properties.hasProperty ("masterVolume") && ! properties.hasProperty ("velocityCurve"),
             "protected parameters are absent from the schema");
    require (properties["oscType"]["type"].toString() == "integer", "discrete settings request integers");
    processor.aiApiKey = "test-key-never-persist";
    processor.aiDescription = "test-prompt-never-persist";
    Request request { processor.aiModel, processor.aiApiKey, processor.aiDescription,
                      instructions (processor), outputSchema };
    const auto body = juce::JSON::toString (requestBody (request));
    require (! body.contains (processor.aiApiKey) && body.contains (processor.aiDescription),
             "key stays out of the request body");
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    const auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
    require (xml != nullptr && ! xml->toString().contains (processor.aiApiKey)
             && ! xml->toString().contains (processor.aiDescription), "credentials and prompt never enter host state");

    // Cancelling before the worker starts must avoid any network request.
    Generation cancelled (request);
    cancelled.cancel();
    require (cancelled.startThread(), "cancelled worker starts for cleanup");
    require (cancelled.waitForThreadToExit (2000), "cancelled worker exits promptly");
    require (cancelled.finished() && cancelled.patchText.isEmpty(), "cancelled worker produces no patch");
    std::cout << "AI sound designer tests passed\n";
}
