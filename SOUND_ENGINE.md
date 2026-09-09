# Enhanced sound engine

Open **OSC MIX** and turn **Enhanced** on, then play a new note. Engine selection
is latched per note, so switching it does not reset a sounding filter or envelope.
New plugin instances use Enhanced. Existing factory/user presets and sessions
without the new parameter explicitly select Legacy. Save Current Patch captures
the engine, mixer levels, bass compensation, envelope curve, Overload and keyboard
tracking. Existing parameter IDs and indices have been preserved.

## Controls and signal path

- **Osc 1 Level / Osc 2 Level:** independent pre-filter levels, 0–1, default 0.75.
  They affect both balance and the signal driving the filter, in classic and modern
  oscillator modes. The Modern toggles still control waveform blending only.
- **Overload:** 1–8× input drive with partial output compensation. Enhanced removes
  the original pair of permanently active asymmetric pre-filter waveshapers.
- **Bass:** 0–1 resonance gain compensation. Zero allows bass loss as resonance
  rises; one restores the small-signal DC gain. Default 0.5.
- **Env Curve:** 0 is linear, 1 is strongly exponential; default 0.65. Applies to
  both amp and filter ADSRs. Sustain and curve edits are smoothed during a note;
  timing edits take effect without restarting the segment.

Enhanced signal flow:

```text
Oscillators → smoothed levels → mixer → driven feedback ladder
            → amplitude envelope × velocity → low-pass/downsample → effects → master
```

The entire voice, including waveform generation, hard sync, nonlinear filtering
and envelopes, runs at 4× the host sample rate. The output uses a 129-tap Blackman
windowed-sinc decimator, with 16 host samples of filter group delay (0.33 ms at
48 kHz). Its tail is drained before retiring a voice. Centered single-voice
oscillators share one filter; stacked unison uses separate left/right filters.
Enhanced removes random per-note panning; the unison stereo spread remains.
Enhanced dispatches MIDI at event boundaries, avoiding the original renderer’s
repeated/early delivery of future events to JUCE’s synthesiser. Pitch bend and
mod wheel are also applied at their event boundaries.

The four-pole filter uses trapezoidal integrators and a bracketed Newton solve at
its nonlinear feedback junction. A smooth rational tanh approximation bounds the
junction. Resonance reaches 4.2 feedback gain; self-oscillation is permitted.
This is a musical nonlinear ladder model, **not** a transistor circuit emulation.
Cutoff uses 5 ms smoothing; level, drive, resonance and compensation use 10 ms.
The filter envelope is applied after cutoff smoothing, preserving fast attacks.

TPT background: [Vadim Zavalishin, The Art of VA Filter Design](https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.2.pdf).

## Build and verify

```sh
xcodebuild -project Builds/MacOSX/SynGen1.xcodeproj \
  -target 'SynGen1 - Standalone Plugin' -configuration Release build CODE_SIGNING_ALLOWED=NO
sh Tests/run-tests.sh Release
open Builds/MacOSX/build/Release/SynGen1.app
```

`Tests/run-tests.sh` also accepts `Debug`, after building that configuration.
An optional second argument writes an editor snapshot to an absolute PNG path.
Tests cover sample rates 44.1/48/96 kHz, finite envelope timing and live sustain,
filter stability, decimator pass/stop bands, nonlinear alias reduction, post-filter
velocity scaling, buffer-size invariance with drift disabled, mixer silence,
release-tail flushing, extreme notes/unison/sync, scheduled note onset and state
migration. Timing output is a local microbenchmark, not a DAW performance guarantee.
The Release run on this machine rendered one second at 48 kHz in approximately
22 ms for one note without unison, and 55 ms with seven voices per oscillator.
The nonlinear-filter sine test reduced its measured folded third harmonic by
107 dB versus the same filter at 1×. This single-tone result is not an overall
alias-rejection specification for arbitrary patches.

Before this change, a copy of the original voice was retained outside the repo.
24 mono renders (four waveforms × sync on/off × three sample rates, with drift
disabled) matched the Legacy path sample-for-sample, including releases.

## Scope and remaining work

This is the first sound-quality foundation, not a claim of parity with Diva or
Sylenth1. Listen at matched loudness with effects disabled before judging it.
Oversampling reduces aliasing; it does not eliminate all aliasing from extreme
sync or nonlinearities. Enhanced sync currently uses fractional reset timing at
4× and output filtering, rather than a dedicated BLEP reset correction. Effects
still run at the host rate. No selectable quality modes are exposed yet.

Free-running phase options, independent oscillator drift, waveform/pitch smoothing,
sample-by-sample glide interpolation, voice-stealing crossfades, modulation
routing and broader effects work are separate follow-ups. Existing Legacy DSP,
including its original envelope order, is retained for compatibility.
