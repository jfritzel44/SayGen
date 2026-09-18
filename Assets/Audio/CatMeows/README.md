# MeowSynth 9000 — CatMeows prototype library

440 original, unmodified WAV recordings from 21 cats, downloaded on 2026-09-17.
All files were decoded and checked: mono, 16-bit PCM, 8,000 Hz; 805.684 seconds
total (13.4 minutes), 14.49 MB unpacked. Individual clips span 1.086–4.002 seconds.
These are low-bandwidth research recordings suitable for prototyping a lo-fi
sample layer. They have not been auditioned or curated as instrument presets.
The synth's Meow Library menu selects any of these recordings. Preview auditions
the selected meow through the shared effects and master output; Meow Level adjusts
its blend with the oscillators. Off disables new meow notes. MIDI note 60 plays at
original speed, with other notes transposing playback by semitones. Samples play
once, with a short attack and note-off release; held notes do not loop. Existing
voices finish with their original sample when selection changes. Trimming,
normalization, root-pitch detection, and preset curation remain future work.

Selection and level are stored in host sessions and saved user patches. Factory
presets start with the meow layer off. The bank is embedded in the plugin, so no
external sample path is needed at runtime. Rebuild the generated bank and menu
with `python3 Scripts/generate-meow-bank.py` from the project root. Keep the file
ordering stable: sample indices are part of saved patches.

## Source and attribution

CatMeows: A Publicly-Available Dataset of Cat Vocalizations, version 1.0.2.
Creators: Luca Andrea Ludovico, Stavros Ntalampiras, Giorgio Presti,
Simona Cannas, Monica Battini, and Silvana Mattiello (University of Milan).

- Record: https://zenodo.org/records/4008297
- DOI: https://doi.org/10.5281/zenodo.4008297
- Archive: https://zenodo.org/records/4008297/files/dataset.zip
- Published archive MD5 (verified): `b5fa911bcd6514e39dfef6876f747df4`

Reference: Ludovico et al. (2021), “CatMeows: A Publicly-Available Dataset of
Cat Vocalizations,” MultiMedia Modeling, LNCS 12573, pp. 230–243.

## Usage restriction

The source record's Terms of use states: “The dataset is open access for
scientific research and non-commercial purposes.” It also requires acknowledgment
and a suitable citation in scientific publications. Some publication descriptions
mention CC BY 4.0, but the current download record includes the non-commercial
restriction. Treat these assets as research/non-commercial prototype material;
resolve the terms with the authors or replace the samples before commercial release.
This file records provenance and does not grant additional rights.

## Index

`manifest.csv` contains relative filenames, recording context, cat/breed/sex IDs,
owner ID, session, vocalization number, audio format, duration, and per-file SHA-256.
Original filenames are retained: `C_NNNNN_BB_SS_OOOOO_RXX.wav`.

- Context: B = brushing; F = waiting for food; I = isolation.
- Breed: MC = Maine Coon; EU = European Shorthair.
- Sex: FI/FN = female intact/neutered; MI/MN = male intact/neutered.
- R = recording session; XX = vocalization number.

The source's optional extras archive is not included; it contains other sounds
and uncut vocalization sequences.
