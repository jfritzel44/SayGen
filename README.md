<img width="1045" height="674" alt="image" src="https://github.com/user-attachments/assets/0cb596dc-8e37-4fd0-ab88-94cd2b054893" />

To design a sound with Gemini, use **Get Gemini API key**, paste your key into the top bar, and enter a sound description. Click **Generate sound**, then play notes to audition it. You can also request a change to the current sound, such as “make it warmer.” The model field accepts a Gemini model ID that supports structured output; access and quotas depend on your Google project.

**Cancel** leaves the sound alone. **Undo** restores the settings from before the last successful generation. If you edit the sound or change presets while generation is running, its result is discarded. Save a result with **Presets → Save Current Patch**.

Generation sends your description and current sound settings to Google. Your API key and description stay in memory for that synth instance and are excluded from saved patches and host project state. Generated patches preserve master volume and the velocity curve.

Build the Debug standalone target in `Builds/MacOSX/SynGen1.xcodeproj`, then run `sh Tests/run-tests.sh Debug` for the DSP, voice, and AI response-validation tests. The tests make no Gemini requests.
