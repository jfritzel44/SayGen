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
Oscillators → smoothed levels → mixer → driven feedback ladder → DC blocker
            → amplitude envelope × velocity → low-pass/downsample → effects → master
```

The DC blocker is a 5 Hz one-pole, placed after the ladder and before the VCA so
it sees a steady offset rather than one the amplitude envelope is scaling. An odd
saturator fed a zero-mean but asymmetric wave still rectifies it into DC: a 10%
pulse at 0.6 Overload reached -28% of RMS, which eats headroom, pushes the
waveform further into the saturator on one side, and thumps as notes start and
stop. It now measures under 0.5% across pulse widths and drive settings. At 5 Hz
the corner costs 0.2 dB at 20 Hz. Its ~32 ms time constant does mean the two
channels take a few hundred ms, rather than ~20 ms, to reconverge after a stereo
width change.

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
junction, and each of the four stages now saturates its own differential input
with the same curve, the way a ladder's transistor pairs do. That curve is odd
with unit slope at zero, so the small-signal response is unchanged to within
measurement noise and quiet playing sounds exactly as it did; drive and high
resonance instead compress progressively rather than meeting one clipper. At the
resonant peak with 3.5 feedback gain, gain falls from 2.0 at -80 dBFS to 0.21 at
0 dBFS, a 20 dB progressive compression, and self-oscillation now limits its own
amplitude. Each stage still settles on its input at DC, so DC gain stays unity
below saturation. The junction solve treats the stage gains as linear, so hard
drive shifts where self-oscillation starts; this is a musical nonlinear ladder
model, **not** a transistor circuit emulation.
Resonance reaches 4.2 feedback gain; self-oscillation is permitted.
Cutoff uses 5 ms smoothing; level, drive, resonance and compensation use 10 ms.
The filter envelope is applied after cutoff smoothing, preserving fast attacks.

## Effects

The effects chain was the last part of the signal path still running stock
building blocks, and it was where most of the remaining audible trouble was.

**Reverb** is now an eight-line feedback delay network in place of Freeverb.
Freeverb's four fixed comb lengths per channel put their modal peaks far enough
apart to be heard individually: sustained chords rang metallic and short decays
fluttered. An FDN couples every line to every other through an orthonormal
8×8 Hadamard, applied as a fast Walsh-Hadamard transform (24 adds rather than
64 multiply-accumulates), so mode density is high from the first millisecond and
the mixing itself is lossless — the per-line gains alone set the decay and the
network cannot run away. Four Dattorro allpasses per channel diffuse the input
first. Each line carries a one-pole low-pass for the damping control and a
matching 18 Hz low cut, without which a long decay slowly integrates whatever
sub-bass and DC the synth hands it into rumble. Delay lengths are modulated by
sub-Hz sines at ±0.18 ms, staggered in phase so no two lines sweep together;
reads are Catmull-Rom, since linear interpolation's high-frequency loss varies
with the fractional part and a swept line would amplitude-modulate its own top
end. Size scales both the line lengths and RT60 (0.25 s to 5.25 s), damping
sweeps the in-loop corner from 20 kHz to 500 Hz, and every derived coefficient
is smoothed over 50 ms so size and damping can be swept live.

Wet/dry is now an equal-power crossfade that is exactly unity dry at mix 0.
Freeverb was scaling dry by 2 and wet by 3 internally, so merely switching the
reverb on with the mix down lifted the whole output by 6 dB, and at the default
0.35 mix it ran 3.5–5.2 dB hot. Measured against a band-limited source at
48 kHz, the network now sits between −3.4 dB (size 0.3) and +0.7 dB (size 0.85)
of the dry signal when fully wet, and total output stays within 0.7 dB of dry at
the default mix. The wet-to-dry balance lands about 1.8 dB drier than Freeverb's
at the same knob position, so patches carried over may want slightly more mix.

**Delay** is a new stereo echo. Three things changed. The damping amount was
previously used directly as the feedback filter's one-pole coefficient, so
"no damping" set the coefficient to zero, stalled the filter at silence and cut
every undamped patch off after a single repeat; it now maps to a filter corner
(20 kHz down to 400 Hz) and undamped repeats decay properly. The read distance
is smoothed rather than jumped, so moving the time control glides the way tape
does instead of clicking — with the head's velocity capped at half a sample per
sample, since a plain one-pole would start a full-range jump reading at eleven
times speed. And the loop carries a 70 Hz low cut and a soft saturator, so long
feedback settings settle into a bounded, slightly compressed tail.

**Master ladder** runs inside 2× polyphase-IIR oversampling. Its drive reaches
10× and is a saturator, so at the host rate everything it generated above
Nyquist folded back into the audio band; the voices have been oversampled since
the last change, which left this as the one nonlinearity in the chain still
aliasing at base rate. The halfband costs 3.14 samples of delay (0.065 ms at
48 kHz), and is only in circuit while the effect is on, so it is not reported as
plugin latency.

**Master volume** is ramped across the block rather than applied as a
once-per-block jump, which is what made a swept volume knob buzz.

The reverb costs about 0.36% of one core and the echo 0.06%, measured on ten
seconds of stereo at 48 kHz on this machine.

## Modulation

The mod wheel (CC1) scales the LFO's depth for every destination — pitch,
filter and amp alike — rather than only gating pitch vibrato. LFO Amount sets
the ceiling and the wheel decides how much of it is in play, so a patch with
modulation dialed in stays still until the wheel is raised. With the wheel down
the LFO has no effect at all, whichever destination is selected, and there is
no on-screen wheel yet: a controller sending CC1 is currently the only way to
open it up.

## Voice allocation

Polyphony is 16. Voices cost nothing until they sound, so the ceiling only
affects sustained chords and pedalled passages, where eight ran out and the
stealing was what got heard.

Reassignment is continuous. `juce::Synthesiser` steals a voice by calling
`stopNote (0, false)` and `startNote()` back to back; its outright hard stops
(all-notes-off, sample-rate changes) pass velocity 1, which is what the engine
keys on to tell the two apart. A steal used to reset the amplitude envelope,
both ladder filters and both decimators, which punched a hole in the output: the
level collapsed about 21 dB within 3 ms and then climbed back. A steal now
carries over oscillator phase and its pending BLEP tail, filter and decimator
state, and the envelope level, and ramps the new note's velocity gain in over
5 ms, so the handover is a smooth transition into the new note. Pitch still takes
the new note immediately. A hard stop still silences the voice outright.

## Unison

Unison start phases are scattered by a deterministic hash of the voice index
rather than spaced evenly at `u / count`. Evenly spaced phases make N sawtooths
sum to a single sawtooth at N times the pitch and 1/N the amplitude, so a stack
began well below its settled level and swelled over ~100 ms as detuning pulled
the voices apart: measured over the first 10 ms against the settled level, a
3-voice stack was 7.6 dB down, a 5-voice stack 13.0 dB. Scattered phases start
incoherent, which is where the detuned stack settles anyway, leaving 1.5-2.5 dB
against 0.6 dB for a single voice. The hash returns exactly 0 for the first
voice, so a single oscillator keeps its authored start phase unchanged.

Panning is constant power normalised to unity at centre. Plain cos/sin puts
0.707 in each channel there, 3 dB below the unity a single centred voice got
from its own special case, so switching unison on made the voice 3 dB quieter
and under-drove the filter by the same amount, at every width and voice count.
Output now holds within 0.1 dB from 1 to 7 voices. A single centred voice is
unchanged, since the normalisation is exactly 1 at centre.

Each voice also carries a fixed component tolerance, derived from the same seed
its drift generators use and so reproducible offline: up to ±0.04 octaves of
filter cutoff and ±3% of resonance. Depth is scaled by **Drift**, so
`driftAmount` 0 still renders every voice identically, sample for sample.
Without it every voice is numerically identical and chords collapse into a
single phase-locked timbre.

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
Use `sh Tests/run-tests.sh Release --jump-benchmark` (or `Debug`) to measure
Jump2 with the reported Advanced mix: Osc 1 saw/pulse/triangle 0.28/0.22/0.26,
pulse width 0.98, Osc 2 saw 1.0, and both subs enabled. It retriggers eight
notes at 48 kHz in 128-sample blocks and reports processing time, missed
2.67 ms deadlines, and output peak with the limiter bypassed. Run timing
comparisons without concurrent builds. Use the Release app for playing:
the unoptimized Debug build can exceed the audio deadline with this patch.
Tests cover sample rates 44.1/48/96 kHz, finite envelope timing and live sustain,
filter stability, small-signal linearity and progressive compression of the
saturating stages, decimator pass/stop bands, nonlinear alias reduction,
post-filter velocity scaling, buffer-size invariance with drift disabled, mixer
silence, release-tail flushing, continuous voice stealing against outright hard
stops, per-voice tolerance under Drift, DC rejection with audio-band unity gain,
unison level flatness and attack coherence, extreme notes/unison/sync, scheduled
note onset and state migration. Effects tests cover, at all three sample rates,
reverb decay against the requested RT60, tail density (the flutter Freeverb was
replaced for), damping removing high frequencies rather than just shortening the
tail, width 0 collapsing to mono and width 1 decorrelating, boundedness at
maximum decay under sustained full-scale noise, echo repeat timing and
multi-repeat decay with damping off, repeats darkening as they decay, a time
sweep gliding rather than stepping, boundedness at maximum feedback,
transparency at mix 0, and mono-bus rendering for both.
Timing output is a local microbenchmark, not a DAW
performance guarantee. The Release run on this machine rendered one second at
48 kHz in approximately 41 ms for one note without unison, and 71 ms with seven
voices per oscillator; the stage saturation costs roughly 15% and 10% of those.
The eight-note Jump2 benchmark averages about 1.68 ms against a 2.67 ms deadline.
Individual blocks do exceed it, typically one to three per 1500 on this machine,
with occasional outliers past 4 ms that are far beyond anything the DSP accounts
for. Repeated runs with these changes reverted show the same spread, so read the
average rather than the worst case; the worst case here is scheduling noise. The nonlinear-filter sine test reduced its measured folded third
harmonic by 99 dB versus the same filter at 1×, down from 107 dB before the stage
saturation was added. This single-tone result is not an overall alias-rejection
specification for arbitrary patches.

Before this change, a copy of the original voice was retained outside the repo.
24 mono renders (four waveforms × sync on/off × three sample rates, with drift
disabled) matched the Legacy path sample-for-sample, including releases.

## Scope and remaining work

This is the first sound-quality foundation, not a claim of parity with Diva or
Sylenth1. Listen at matched loudness with effects disabled before judging it.
Oversampling reduces aliasing; it does not eliminate all aliasing from extreme
sync or nonlinearities, and the per-stage saturation adds some of its own.
Enhanced sync currently uses fractional reset timing at 4× and output filtering,
rather than a dedicated BLEP reset correction. Apart from the master ladder,
effects still run at the host rate; the chorus and phaser are still stock JUCE.
No selectable quality modes are exposed yet. Stealing hands over continuously but
does not crossfade two oscillator sets, so a stolen voice changes pitch abruptly.

Free-running phase options, independent oscillator drift, waveform/pitch smoothing,
sample-by-sample glide interpolation, modulation routing and broader effects work
are separate follow-ups. Existing Legacy DSP, including its original envelope
order, is retained for compatibility.

Factory Round Bass, Reggae Woman and Chameleon have optional advanced voicings,
loaded when either Wave Mix oscillator is enabled, preserving the enable states.
Round Bass blends saw/triangle; Reggae Woman adds softer triangle/pulse tones.
Chameleon uses a 40% pulse at 8' over a louder square at 16', a 250 ms filter
pluck and a held amp envelope (adapted from the Syntorial Chameleon recipe).

Selecting any preset resets all effects-panel parameters to their defaults
before applying the patch's own values. This includes limiter threshold and
release, preventing an earlier -30 dB / 1 ms setting from carrying over.
Effects specified by the patch are then enabled as authored. New user-saved
patches capture all effects parameters; older patches missing them use defaults.
Master volume is preserved. This resets controls, not stored delay/reverb tails.
