# JIANT FM changelog

Custom firmware for the M-VAVE FM-1 by Juan Jiant. Newest version first.

**Release status**

| Version | Date | Status |
| --- | --- | --- |
| **V10.8.72** | 2026-11-28 | **Public** |
| V10.8.49 … V10.8.71 | 2026-11-26 … 11-28 | Beta |
| **V10.8.48** | 2026-11-25 | **Public** (now classic) |
| V10.8.43 … V10.8.47 | 2026-11-25 | Beta |
| **V10.8.42** | 2026-11-25 | **Public** |
| V10.8.26 … V10.8.41 | 2026-11-24 | Beta |
| **V10.8.25** | 2026-11-24 | **Public** |
| V10.8.22 … V10.8.24 | 2026-11-23 | Beta |
| **V10.8.21** | 2026-11-23 | **Public** |
| V10.8.1 … V10.8.20 | 2026-11-21 … 11-23 | Beta |
| **V10.7.81** | 2026-11-21 | **Public** |
| V10.7.59 … V10.7.80 | 2026-11-19 … 11-21 | Beta |
| **V10.7.58** | 2026-11-19 | **Public** |
| V10.7.57 | 2026-11-19 | Beta |
| **V10.7.56** | 2026-11-19 | **Public** |
| **V10.7.45** | 2026-11-17 | **Public** |
| **V10.7.44** | 2026-11-17 | **Public** |
| **V10.7.43** | 2026-11-17 | **Public** |
| **V10.7.42** | 2026-11-15 | **Public** |
| **V10.7.41** | 2026-11-15 | **Public** |
| V10.7.40 | 2026-11-15 | Beta |
| **V10.7.39** | 2026-11-15 | **Public** |
| **V10.7.38** | 2026-11-15 | **Public** |
| V10.7.24 … V10.7.37 | 2026-11-13 … 11-15 | Beta |
| **V10.7.23** | 2026-11-13 | **Public** |
| **V10.7.22** | 2026-11-13 | **Public** |
| V10.7.18 … V10.7.21 | 2026-11-11 … 11-12 | Beta |
| **V10.7.17** | 2026-11-10 | **Public** |
| **V10.7.16** | 2026-11-10 | **Public** |
| V10.7.7 … V10.7.15 | 2026-11-09 … 11-10 | Beta |
| **V10.7.6** | 2026-11-09 | **Public** |
| V10.6d … V10.7.5 (except V10.6z2) | 2026-11-06 … 11-09 | Beta |
| **V10.6z2** | 2026-11-07 | **Public** |
| **V10.6c** | 2026-11-03 | **Public** (the last public release before this cycle) |

If you are coming from **V10.8.48**, V10.8.72 below sums up everything since. If you are coming from **V10.6c**, read everything down to V10.6c. If you are coming from **V10.6z2**, read down to V10.6z3.
The sections marked **Compatibility notes** are the ones to check before you install.

> **A note on version numbers.** Before V10.6c the versions counted up as V10.1, V10.2 … V10.61. V10.6c started a new
> scheme: V10.6c, V10.6d … V10.6z, V10.6z2 … V10.6z10, then V10.7, V10.7.1 … V10.7.5. The **V10.6** and **V10.7** of
> 31 October 2026 are much older builds with the same names; they are covered under "Older versions" at the end.

---

## V10.8.72 — SEQ EDIT, OTT, RESOUND, VISUAL and a generator from JIANT BYTE (2026-11-28)

Includes the betas V10.8.49 to V10.8.71. If you come from V10.8.48, this is everything new.

**Compatibility notes**
- Projects with LIFE **PUMP** open with that value on **STEREO** (PUMP is gone). Every LIFE depth plays as before up to 60 and grows above it (about 2.5 times at 100): a project with LIFE above 60 sounds stronger.
- Projects saved before **V10.7** are no longer converted on the FM-1: LOAD says **OLD PROJECT: OPEN IN STUDIO** and the slot is left as it is (never erased). Open it in JIANT Studio, which converts it.
- Global settings from Felucca before JIANT 0.6 (**PER1 / PER2**) are no longer migrated: the settings start from their defaults once.
- **SEQ EDIT** replaces the SEQ TOOLS key map (SEQ held, below): STRETCH, SQUEEZE and REPEAT are ACTIONs now, and B3–E4 select bars 1–4.
- A MOD matrix route below −100 % does not open in V10.8.50 or earlier. Projects and presets from before V10.8.71 open with OTT off.
- Editor protocol **1.4** (OTT through TRACK_PARAM and DESC, the project library 95–97): older clients keep working.
- Back up your projects before updating.

**Sequencer**
- **SEQ EDIT** (SEQ held on a SEQ page): the roll with the selection; KNOB 1 START, KNOB 2 END (A3: MOVE / WIDTH), KNOB 3 the ACTION, KNOB 4 its amount; **OCT+** (or D5) does it on the selection, or on the whole sequence when there is none (**OCT−** or G3: ALL STEPS). ACTIONs: STRETCH, SQUEEZE, REPEAT, NUDGE, ROTATE, SHIFT, REVERSE, DUP, COPY, PASTE, CLEAR, CLR AUTO, CLR STEP, VEL RND, NOTE RND, HUMANIZE, TRANSP, OCTAVE, THIN, INVERT, STUTTER, ARP, MUTATE, **GEN** (the track's generator inside the selection, KNOB 4 the genre) and **RANDOM** (notes in the scale on a 1/4 … 1/32 grid; DRUM: hits).
- Keys in SEQ EDIT: F3 CLEAR · G3 ALL · A3 MOVE · B3–E4 BAR 1–4 · F4 CLR AUTO · G4–B4 the lane's CLR / REV / FILL (DRUM) · C5 DUP · D5 DO IT · E5 COPY · F5 PASTE · G5 CLR STEP; black C#5 / D#5 **NUDGE** < / > (micro-timing, 4/64 a press), **F#5 UNDO / REDO** of the last edit.
- Step keys held on the roll select a range; held + EDIT clear it. COPY / PASTE / DUP / CLEAR say the steps ("DUP 9-12").

**Effects and sound**
- **FX › OTT**, FX › TRACK's fourth page (from JIANT BYTE): a two-band upward / downward compressor on each track: **DEPTH** (0 off: no CPU, the track as before), **TIME**, **UP/DN**, **X-OVER**. LOW and HIGH drawn with their gain; HOME's ECO tags a track with OTT on. Kept with the project and the user presets.
- V10.8.72: OTT's law as JIANT BYTE's (about 10:1 above −16 dB, up to +30 dB of lift, TIME 56 by default), and the generator puts OTT on the electronic genres.
- **MATH WAVE SYNC**: the formula's rhythm locked to the tempo, every key the same pattern transposed; with SYNC, KNOB 8 is **LOOP** (OFF, 4 BAR … 1/32), the pattern repeating bar-exact. New preset **BYTE SYNC**.
- MOD matrix **AMOUNT** up to ±200 % (100 % sweeps the whole range). DRUM-X has one GAIN (LEVEL's); kits sound as before.

**Songs, sounds and saving**
- **RESOUND** (GLO + A4): new sounds for every track by its role, the notes, steps and song kept ("RESOUND #seed"); GLO + G4 brings the sounds back.
- **SAVE asks before overwriting** a part with music (**OVERWRITE A?**: OCT+ yes, OCT− no); **GLO + G4** undoes the save. Saving a generator draft makes it that project; RESTORE LAST follows it.
- The generator takes JIANT BYTE's: song templates by genre (two to five minutes, fills), each section its own automation (the kit's tone in the build, snare rolls, delay throws, stabs, slower pads in the break), the timbre moving in each section, call and response and EXPLORE's cut, NEO CLASSIC without drums; no piece over −1 dBFS before the master.

**HOME and screens**
- **HOME › MACROS** is back (ECOSYSTEM's third page): KNOB 1–4 M1–M4, **HOME + SELECT** DICE MACROS (with UNDO).
- **LIFE**: KNOB 3 **STEREO** (was PUMP): each track moves across the stereo field from its neighbours.
- **HOME › VISUAL** (BEING's second page): a full-screen picture made from FM's own graphics (FX, ENV, LFO, ECO, the creatures, ANIMAL morphing), deformed, repeated and broken by the song; KNOB 1 DENSITY, 2 WOBBLE, 3 PALETTE, 4 REACT, OCT+ a new one. It yields to the sound and costs nothing off its page; V10.8.72 draws it smoother (fewer strokes instead of skipped frames). **MENU › DISPLAY › SAVER** (OFF, 1, 3, 5, 10 MIN; 3 by default) shows it after that long untouched.
- **MENU › DISPLAY › PANEL** (AUTO / A / B) for the screen of the FM-1s sold from late 2026 (B untested on that unit); OCT− + SAVE held at power-on forces A.
- Pages without a picture show their name large (LIFE, MACROS, GLOBAL, KEY SPLIT, LEVEL, PUNCH IN, the CLAY pages); a knob turned is said whole on the footer with its page ("LIFE STEREO 100 %"). More banners (mutes, ROOT, QUANTIZE, RESTART, PROJECT n).
- The creatures redrawn through one engine (PHASE's snail and MATH's caterpillar as their originals, wobbling circles). Gone for room: the big-number face, ABOUT's AUDIO LOAD lines, the RAMP curve, the EQ curve.

**Connections and CPU**
- Studio's project library: list, rename live, copy, move and delete projects from JIANT Studio.
- USB MIDI HOST accepts more keyboards and retries the connection before NOT USB MIDI.
- Less CPU (JIANT BYTE's technique: more of the sound in RAM, the USB resampler's table halved), the sound bit for bit the same.

## V10.8.71 — OTT on every track (2026-11-28, beta)

- **FX › OTT** (FX › TRACK's fourth page, from JIANT BYTE): DEPTH, TIME, UP/DN, X-OVER; at DEPTH 0 it costs no CPU. HOME's ECO tags a track with OTT on. Editor protocol 1.4. GLO's map names A4 **RESOUND**.

## V10.8.70 — each section its own automation (2026-11-28, beta)

- The generator, from JIANT BYTE: the kit's tone rises over the build, snare rolls into the turn, a delay throw on the lead's last note, stab chords, slower pads in the break. Call and response and EXPLORE's cut are back.

## V10.8.69 — the generator's song plans (2026-11-26, beta)

- Song templates by genre, two to five minutes, with fills; the timbre moves in each section; the drums get their punch back.
- USB MIDI HOST accepts more keyboards and retries the connection.

## V10.8.68 — the generator from JIANT BYTE (2026-11-26, beta)

- The mix, DANCE and the moods, CALM and EXPLORE retouched; the build's DJ FILTER a swept low-pass; NEO CLASSIC without drums. VISUAL costs nothing off its page.

## V10.8.67 — VISUAL from FM's own pictures (2026-11-26, beta)

- VISUAL draws FX, ENV, LFO, ECO, a track's creature and ANIMAL's morph, deformed, repeated and broken; a fixed budget, lighter when the sound is busy.

## V10.8.66 — MATH LOOP in time (2026-11-26, beta)

- With MATH's WAVE on SYNC, KNOB 8 is **LOOP** (OFF, 4 BAR … 1/32): the pattern repeats locked to the bar.

## V10.8.65 — names large, the knob said with its page (2026-11-26, beta)

- Pages without a picture show their name large; a knob turned is said on the footer with its page ("LIFE STEREO 100 %", "LFO RATE 1.24 HZ").

## V10.8.64 — HOME › MACROS back, LIFE STEREO (2026-11-26, beta)

- **HOME › MACROS** (ECOSYSTEM's third page): KNOB 1–4 M1–M4, HOME + SELECT DICE MACROS with UNDO.
- LIFE KNOB 3 **STEREO** (was PUMP; a project's value now STEREO's); every LIFE depth curved above 60 (about 2.5 times at 100).

## V10.8.63 — the new screen panel (2026-11-26, beta)

- **MENU › DISPLAY › PANEL** AUTO / A / B (B for the late-2026 units, untested there); OCT− + SAVE held at power-on: A.

## V10.8.62 — MATH WAVE SYNC (2026-11-26, beta)

- **WAVE SYNC** on MATH: the formula's rhythm locked to the tempo, every key the same pattern transposed. New preset **BYTE SYNC**.

## V10.8.61 — VISUAL from the creatures (2026-11-26, beta)

- VISUAL fuses the song's own creatures and morphs them into each other; VOICE or ANIMAL in one scene in three.

## V10.8.60 — less CPU (2026-11-26, beta)

- JIANT BYTE's technique: more of the sound in RAM, the USB resampler's table halved; the sound bit for bit the same.

## V10.8.59 — VISUAL from the song (2026-11-26, beta)

- Each song its own scene, reacting to every hit. SCL lighter. Global settings from before JIANT 0.6 (PER1 / PER2) are no longer migrated.

## V10.8.58 — VISUAL composed (2026-11-26, beta)

- The data apart, a real head, the seed varies everything, PALETTE colours every layer.

## V10.8.57 — VISUAL on the whole screen (2026-11-26, beta)

- HOME › VISUAL full screen; KNOB 1 DENSITY, 2 WOBBLE, 3 PALETTE, 4 REACT; the saver another picture every 30 s.

## V10.8.56 — HOME › VISUAL and SAVER (2026-11-26, beta)

- **HOME › VISUAL** (BEING's second page; OCT+ a new figure) and **MENU › DISPLAY › SAVER** (OFF, 1, 3, 5, 10 MIN; 3 by default).

## V10.8.55 — the snail and the caterpillar (2026-11-26, beta)

- PHASE's snail and MATH's caterpillar as their originals; the creatures' circles wobble; DRUM-X's kit lighter.

## V10.8.54 — RESOUND and Studio's project library (2026-11-26, beta)

- **RESOUND** (GLO + A4): new sounds for the whole song, the notes kept; GLO + G4 brings them back.
- Studio's project library (editor commands 95–97: list, rename, copy, move, delete). More banners; OCT+'s hint on the footer.

## V10.8.53 — SEQ EDIT (2026-11-26, beta)

- **SEQ EDIT** (SEQ held): KNOB 1 / 2 the selection, KNOB 3 the ACTION, KNOB 4 its amount, OCT+ does it, F#5 UNDO / REDO; **GENERATE** and **RANDOM** among the actions (GEN on the screen).
- The pictures through one shared engine (lighter); the big-number face gone, the count-in a line.

## V10.8.52 — SAVE asks first (2026-11-26, beta)

- **OVERWRITE A?** before saving over a part with music; GLO + G4 undoes the save. A saved generator draft is that project; RESTORE LAST follows it. ARRANGE says ROW DUPLICATED.

## V10.8.51 — old projects to Studio, the matrix to ±200 % (2026-11-26, beta)

- Projects from before V10.7: **OLD PROJECT: OPEN IN STUDIO** (no longer converted on the FM-1).
- MOD matrix AMOUNT −200 … +198 %; the generator never over −1 dBFS; STRETCH / SQUEEZE / REPEAT on the held range.

## V10.8.50 — DUP and range clear (2026-11-26, beta)

- SEQ TOOLS C5 **DUP**; step keys held + EDIT clear the range; COPY / PASTE / DUP / CLEAR say the steps.

## V10.8.49 — STRETCH, SQUEEZE, REPEAT and NUDGE back (2026-11-26, beta)

- SEQ TOOLS C4 STRETCH x2, D4 SQUEEZE /2, E4 REPEAT x2; C#5 / D#5 NUDGE < / >. One GAIN on DRUM-X (LEVEL's). ABOUT's AUDIO LOAD lines and the RAMP curve gone.

## V10.8.48 — real envelopes in the generator (2026-11-25)

- The generator gives each part an envelope by its role (none at full sustain except organs and drones); the leads with little sustain and a long release.
- The generated parts use varied filter types (band-pass and high-pass with KEY TRK 100) and the track's GAIN makes up the level.

## V10.8.47 — the parts' words (2026-11-25, beta)

- PART SLOTS: KNOB 2 is **PLAY/EDIT** (while playing, it comes in on the next bar). SAVE layer: **SAVE A … D** (was STORE). PLAY's mode is **PART** (was LOOP) and a song's end **REPEAT** (was LOOP).
- The boxes A … D show **n BARS** and **PLAYING**; JAM and TAKE say **PART** and **SWITCH**. Jazz hats at half.

## V10.8.46 — the generator moves inside each section (2026-11-25, beta)

- Slow free LFOs on filter, shape and pan; envelopes by role; phonk with drift only. MENU › ABOUT shows LAST CRASH again.

## V10.8.45 — the ARP on the grid, shorter sections (2026-11-25, beta)

- The ARP runs on the transport's grid and is recorded quantised.
- The header's load is a number (0 … 99); the velocity diagnostic is gone (the velocity web page is for older versions).
- The generator's sections are half as long and never come back identical; RADIO changes every 16, 8 or 4 bars.

## V10.8.44 — GATE on reverb and delay, a LEVEL page (2026-11-25, beta)

- **GATE** on FX › REVERB and FX › DLY (OCT+ held + KNOB 4): 0 off, 16 a sixteenth, 64 a quarter, 127 two quarters; it cuts the tail when the time is up. Saved with the project.
- A **LEVEL** page after the EDIT pages on every engine: GAIN (0 … 12 dB), LEVEL, PAN. GAIN is no longer on ANALOG's and PHASE's MORE.

## V10.8.43 — RADIO's STOP, HARMONY respected (2026-11-25, beta)

- STOP on the last bar stops RADIO; the generator respects SCL › HARMONY; chord parts without the leads' vibrato.

## V10.8.42 — generator: hard bass with a donk, dream pop in two arrangements (2026-11-25)

- The generator only: HARD BASS with a donk bass, DREAM POP in two arrangements (an organ, or guitar-like) and slower, the accents kept, IDM AMBIENT from 76 BPM. No page or gesture changes.

## V10.8.41 — the generator, genre by genre (2026-11-24, beta)

- The generator retouched genre by genre (jazz, IDM GLITCH, baile funk, phonk, dubstep, cumbia, reggaeton, house, hard bass, shoegaze, dream pop, techno, EXPLORE).
- The genre **FUGA** is gone: 29 genres.
- **MOOD** progressive: CALM is light down to −25 and the drums go near −50; DANCE keeps half-time; in EXPLORE it moves the tempo.

## V10.8.40 — the tempo exact, the tracks always in phase, RESYNC (2026-11-24, beta)

- **Fix**: the tempo no longer runs ahead (up to about 11 ms a minute at 174 BPM, more at 1/32) and the tracks no longer drift apart after a change of LEN, DIV, EUCLID's STEPS, MUTATE, a variation or SWING: each track takes its place from the transport. Still free on purpose: LIVE's playheads, JUMP, FIT's own clock and an odd LEN with SWING.
- **RESYNC**: SEQ held + PLAY while playing puts every track back on its step 1 on the next bar, without stopping. GLO + PLAY is still the immediate restart.
- **MOOD** acts on the drums: CALM lowers and mutes them, DANCE raises them.

## V10.8.39 — RADIO and MOOD on the generator (2026-11-24, beta)

- **GENERATOR 4, KNOB 3 RADIO** (ON / OFF): when a song's last row ends, or after 16 bars of a loop (32 above 110 BPM), it makes the next piece and comes in on the bar, respecting ENTRY. It only advances while a generator draft plays; not with MY LOOP; started with OCT+.
- **GENERATOR 4, KNOB 4 MOOD** (CALM to DANCE): moves the energy; with GENRE on RANDOM it also biases the choice of genres. Neither RADIO nor MOOD is saved with the project.
- Each genre its own traits, IDM GLITCH far more extreme, and the kick tuned to the key.

## V10.8.38 — DJ FILTER on the MIXER, NOTE and TRANS back, GAIN on every engine (2026-11-24, beta)

- The pages SEQ › BAR, SEQ › PHRASES and GLO › SYSTEM 2 are gone.
- **DJ FILTER** on the device again: on GLO › MIXER, a short tap of OCT+ switches KNOB 4 between BPM and DJ FILT.
- **SEQ › NOTE** is back in the chain, after STEP (17 pages in SEQ). **TRANS** is on SCL › CHORD, KNOB 4.
- **GAIN** 0 to 12 dB on every engine: on the first EDIT page, OCT+ held + KNOB 4.
- **KEY TRK NOTE** in tune.
- **SWARM**: WAVE (SWARM 2, KNOB 4: eight FLOAT formulas), a tag ALONE / AIR n / PROX n / AIR OFF, and MODE >T1 … >T6: SWARM as that track's note source.

## V10.8.37 — MUTATE without a page, KEY TRK on FILTER, the factory sounds retouched (2026-11-24, beta)

- **MUTATE**: its page is gone. EVERY (OFF, 1, 2, 4, 8 bars) is a tag in EUCLID's picture ("EVERY 4"), set with OCT+ held + KNOB 4 on EUCLID. OCT+ alone mutates when let go; OCT− goes back to the anchor.
- **KEY TRK** (EDIT › FILTER): OCT+ held + KNOB 2 (CUT's) picks OFF / 50 / 100 / NOTE, with a tag on the curve. At 50 and 100 the cutoff follows the key (100: a semitone a semitone, around C4) on every type, the combs too; on NOTE the final cutoff lands on the semitones (MOD SEQ › CUT plays tuned resonances). A track's, saved with the project, OFF by default.
- **Factory sounds**: sends by role (basses dry, leads more delay than reverb, pads more reverb), a late vibrato on the leads, the plucked strings darker when played softly, GONG and MATH BELL brighter struck hard, and movement on the sounds that had none (the kits, ANIMAL, WEST's struck ones and basses). The INIT sounds and EXT as they were.

## V10.8.36 — NOISE on DIST, MIX on CHORUS, CLOUD starts with the note (2026-11-24, beta)

- **FX › DIST, KNOB 4: NOISE** (0–127, a track's): a noise that moves the gain and the tone inside the distortion, grain stuck to the note; silence stays silent. Not on the AMP type. An older project opens with NOISE 0.
- **FX › CHORUS, KNOB 4: MIX** (0–100 %, global): at 0 the chorus as it was; towards 100 each track's dry goes down by its CHORUS send and one modulated voice is left in the middle: a vibrato.
- **CLOUD reverb**: it starts with the note; half the grains read just behind the present and the others 1, 2 or 3 sixteenths back by the tempo; the tail lower (a high SIZE clipped).
- **Generator**: the mix by genre (the lead and the arp up where they were buried, less reverb on a club's stabs, the kick's DUCK stronger), a colour of its own for each piece, and EXPLORE with the lead / arp dialogue.

## V10.8.35 — MUTATE and LIVE back, compact (2026-11-24, beta)

- **SEQ › MUTATE** is back with one knob, **EVERY** (OFF, 1, 2, 4, 8 bars): EUCLID's pattern mutates from the anchor, as far as VARY says. On EUCLID or MUTATE, **OCT+** mutates now and **OCT−** goes back to the anchor; on MODES, OCT+ / OCT− are still a new variation / the one before.
- **LIVE** is back on the **black keys 1–9 of SEQ › STEP FX**, on the selected track: RATE x2, x3/2, x2/3, REVERSE, PING-PONG, RANDOM and THIN held (in on the 1/16; let go, the track is back in phase), GIRO − / + pressed.
- **EUCLID**: on a melodic track KNOB 4 is **ROLE** (MELODY, CHORDS, BASS, ARP) and the MODES page is named **MELODY**, with VARY. The picture is one ring of dots.
- **Removed for room**: the filter types **MS20** and **LP24** (a sound that used them opens with LP; the generator's EBM bass uses a resonant LP) and the **ENV › STAGE** page (LOOP and the destinations were on ENV DEST already).

## V10.8.34 — the song generator, stage 1 (2026-11-24, beta)

- Twelve more progressions (two a harmony family), the contrast section with a progression of its own and each section a register of its own.
- The lead's motif with a signature leap (a fifth or a sixth), four more syncopated cells, more call and response (the answers above the call) and a dialogue: the arp silent where the lead speaks, answering in its gaps.

## V10.8.33 — the FM-1 tells JIANT Studio which page you are on (2026-11-24, beta)

- The editor gets the section, the page and the layer the screen shows (push 43): JIANT Studio's live guide follows them.
- FX › PUNCH IN no longer draws the lane (the lane stays: REC and the generator use it).

## V10.8.32 — DRUM-X: the MATH formulas as modulators, clean (2026-11-24, beta)

- The MATH formulas used as FM modulators in DRUM-X no longer sound broken or stepped.

## V10.8.31 — DRONE CLAY: the keys (2026-11-24, beta)

- With CLAY ON, the white keys strike a gesture on the MOD picked (OCT picks the group: 0 hits, +1 tables, +2 gestures, −1 notes, −2 FM) and the last seven keys pick the drone's degree on the scale. On a DRUM track the whites are the pads.
- CLAY TIME › **RUN**: SONG, FREE, **SONG NOKEY**, **FREE NOKEY** (with NOKEY the keys play notes as usual).

## V10.8.30 — DRONE CLAY: AMOUNT 0 strikes nothing (2026-11-24, beta)

- A modulator at AMOUNT 0 no longer strikes or re-strikes the drone; at RATE HOLD the drone is never struck again.

## V10.8.29 — DRONE CLAY: the modulators' bars (2026-11-24, beta)

- Under the scope, a thin bar a modulator shows what it adds now; the bar of the MOD the knobs edit in the track's colour.

## V10.8.28 — WEST: a new DUST, and METAL (2026-11-24, beta)

- **DUST 1–95**: white noise into the folder with the core (it folds and passes the gate with it); from 48 some of it straight to the gate. It no longer crackles. **DUST 96–127: METAL**, a metallic tone tuned to the note.
- Projects and presets made with the NOISE engine are mapped to WEST again; the presets WIND, RAIN, METAL, SEA, HISS PAD, INIT NOISE and TAPE AIR are revoiced for the new DUST.

## V10.8.27 — DRONE CLAY as JIANT BYTE has it (2026-11-24, beta)

- Each modulator its own **RATE** (1/64 … 4BAR, dotted, triplets and polyrhythms), **PITCH** −24 … +36, the hits a drum on any engine, and four pages: DRONE CLAY, CLAY MOD, CLAY RHYTHM, CLAY TIME.
- The PUNCH LANE page is gone (the punch-in lane stays).

## V10.8.26 — the screen mirror, smoother (2026-11-24, beta)

- The screen mirror sends only the rows that changed.

## V10.8.25 — the emulator as an input in JIANT Studio (2026-11-24)

- In JIANT Studio › Record, the emulator can be the input: its 8 stems (as USB STEMS sends them: six tracks and the FX return) and its MIDI OUT. Firmware unchanged from V10.8.24.

## V10.8.24 — the emulator in JIANT Studio (2026-11-23)

- The emulator answers the whole editor protocol: JIANT Studio and the web editor edit the emulator, record its output and pick its MIDI input. Firmware unchanged from V10.8.23.

## V10.8.23 — screen mirror and full MOD MATRIX names (2026-11-23)

- Screen mirror: the web editor and JIANT Studio show the FM-1's screen live (protocol 78/79).
- MOD MATRIX: turning a slot's SRC or DEST shows the route with full names (e.g. "LFO > FILTER CUTOFF", "MOD WHEEL"), even with HELP OFF; E1..E8 named as the current engine's parameters (V10.8.22).
- SEQ > STEP FX: the "HOLD A KEY" hint moved off the step row.
- Sound unchanged since V10.8.21.

## V10.8.21 — song mode, DRONE CLAY, step components you play, and the screens redrawn (2026-11-23)

Includes the betas V10.8.1 to V10.8.20. If you come from V10.7.81, this is everything new.

**Compatibility notes**: projects from V10.7.81 open; STRETCH, MUTATE, GLOBAL COMP and the removed pages' settings are kept but do nothing. SEQ + SELECT now walks the pages (the sequence dice is gone; EDIT + SELECT still dices the sound).

- **Song mode** (HOME › JAM · TAKE · ARRANGE): play the four sequences A–D of every track live (F3–B3 pick A–D on
  all tracks, OCT+ GO, OCT− STOP, CAMBIO sets when a change lands). **REC** records the take; **K4 LOCK** plays it back
  and what you play over it is marked **YOU**; with a track selected, **REC** redoes that track for one round and
  **OCT−** undoes it. **TAKE** edits the changes (K1 pick, K2 move, K3 sequence, K4 bars, OCT+ new, OCT− delete). A help
  line on JAM and TAKE says what to do next (MENU › DISPLAY › HELP OFF hides it). GLO › LIVE is gone: JAM plays the
  sequences live.
- **Step components you play** (SEQ › STEP FX): the **PRESETS knob picks the step**; the 16 white keys play components
  while held — STUTTER, STUTTER /2, /4, RATCHET x2 x3 x4 x8, ARP UP / DOWN / UP-DOWN, OCTAVE UP / DOWN, RANDOM, TRANSPOSE
  +2, CHORD, MUTE — and with **REC** they are written into the step with their value and length (into COMP, or COMP 2
  when COMP is taken). A big **banner** names the key and what it does while held (also on the FX layer's punch-ins).
- **DRONE CLAY** (SEQ › CLAY, from JIANT BYTE): a sequencer per track — a very low drone in the track's own voice (RATE
  re-strikes it with the transport, or holds it) and four modulators (shape, euclid, amount, decay, gestures) aimed at
  any of the track's parameters; note gestures (ARP, CHORD, HIT) play on the scale. On MONO / LEGATO it yields the voice
  to the steps; a TIME or PITCH punch-in restarts it from its first hit.
- **Which track you are on, always**: a bar on top reads **TRACK N / ENGINE** in the track's own colour (T1 yellow, T2
  orange, T3 red, T4 magenta, T5 violet, T6 blue); the same colours on the mixer, the beings, JAM and TAKE.
- **Macros as destinations**: LFO › MACROS — KNOB 1 picks the macro, its destinations add up (16 routes in all); in the
  MOD matrix M1–M4 are destinations, not sources.
- **Redesigned screens**: FX in the HUD look (DELAY's echoes a tunnel, REVERB cells, CHORUS L/R orbit, SLICER, MASTER,
  DIST); FX › SENDS as columns; GLO › MIXER channels; ENV as layers of the last notes; LFO as a satellite writing its
  wave; EDIT › FOCUS a horizontal heat bar with the parameter's whole name and its modulation; ARRANGE and GENERATOR a
  living score (sections as wide as their bars, one lane per track); SCL › KEY the scale's whole name, its notes and
  degrees; SCL › CHORD the chord really played (D MIN7…), its notes and intervals, the seven degrees' chords. Names are
  whole where they fit (ECOSYSTEM, GENERATOR, MIXER, MODULATION…).
- **Power-on**: the boot "Six threads" with the signature drawn smooth and the version shown; the **click at power-on**
  is gone (the output opens after the audio has started, then fades in).
- **Removed for room**: GLO › LIVE, SEQ › STRETCH, MUTATE, the sequence dice (SEQ + SELECT; the sound dice stays), GLO ›
  GLOBAL COMP, ENV › TRACKS and the one-time teaching hints. Old projects open: their settings are kept and ignored.

## V10.7.81 — DRUM-X MATH, the punch-in editor, HARMONY and a clearer FX layer (2026-11-21)

Includes the betas V10.7.59 to V10.7.80. If you come from V10.7.58, this is everything new.

**Performance (FX)**
- **The FX layer is a map of named cells again**: hold FX and every effect has its name (RPT 1/8 · 1/16 · 1/32, REVERSE, FREEZE, OCT− / OCT+, 1/2 TEMPO, DEC− / DEC+, LOOP 3, TAPE STOP, STUT 1/8 · 1/16, PITCH UP / DN, ATK+, ARP, RANDOM, STRETCH, CLR AUTO), the four macros M1–M4 on top and the mutes T1–T6 below.
- **The punch-in editor** (OCT+ on FX › PUNCH IN, or FX + OCT+): each key WHAT (an effect, or a PARAM: M1–M4, CUT, PITCH, TIMBRE, VOLUME, PAN, DIST, CHORUS, DELAY, REVERB, VIBRATO on any track), HOW (HOLD, LATCH, QUANT; for a PARAM HOLD, RAMP UP / DN, SINE, STUTTER, S&H), AMOUNT and TIME (1/16 … 8 bars); two slots, A and B.
- Punch-ins REVERSE, FREEZE, TAPE STOP, PITCH UP / DOWN and STRETCH; the FX layer's knobs are the four MACROS; FX › SENDS has DIST again.

**DRUM-X**
- **MATH formulas as a sound's FM**: SOUND 3's WAVE goes past SINE FM METAL BELL to **F01–F48**, a MATH formula used as a wavetable (smooth, not harsh), its shape drawn on screen; **RATIO** (24 values, ¼ … 16 and the inharmonic ones between) and **MOD AMT** (COLOR's place). Kept with the project.
- **Two kits of formulas**: **GLITCH FM** (short, high ratios) and **MATH LAB** (DRUM-X's sounds, each one a table).
- **DICE** gives half the sounds a formula and a ratio of their own.
- DRUM-X in 8 pages (KIT, KIT 2, SOUND 1–4, CROSS MOD, FILTER); **BODY** (the shell's resonance) instead of FOLD; **GAIN** up to +12 dB, clean.

**MATH**
- **MORPH between two formulas**: ALGO B where CHIP was and MORPH from ALGO to ALGO B, cheap on the CPU. MATH's formulas compute faster.

**Harmony and sequencing**
- **SCL › HARMONY**, its own tab: PROG, LENGTH, EVOLVE, SKIP, every melodic sequence follows a progression; the chords shown in roman numerals, in colour, the one playing lit. The **CHORD** page stays (chord sets POP, JAZZ, SOUL, DREAM, QUART).
- **EUCLID › ROLE** (MELODY, CHORDS, BASS, ARP) follows the harmony the other tracks play, else SCL's key; EUCLID's melody has CONTOUR, RANGE, MOTIF and CHORD.
- **FILL** step component (LOCK's place); RAMP ends BACK, HOLD or PING.

**GEN**
- Seeds **fused** (A+B in the name), never one as it is: more variety between throws; GEN waits for the bar to end while it plays; GENE 4: KEY NEW / KEEP.
- **DUBSTEP** (the wobble's rate moving every half bar, MATH basses, formula kits), **SHOEGAZE**, **METALCORE**, **CUMBIA villera**; each genre its FX style; MATH, WEST and STRING used more; eight more progressions.

**Screens and settings**
- **EDIT › FOCUS in heat** colours (the arc and E1–E8 the colour of their value); **MORE comes before PLAY**.
- **GLO › GLOBAL** and **SYSTEM** show the oscilloscope instead of decorative bars.
- **MENU › DISPLAY › PRESETS**: **SCROLL** (the knob loads the next sound, as before) or **MENU** (it opens the PRESETS list first).
- A knob's meaning in the header (MENU › HELP) where it is not obvious; PRESETS moves a bar at a time on SEQ.
- **LIFE** (HOME › LIFE): the tracks move each other, light on the CPU.

**Compatibility notes**
- New defaults, once: LARGE off, the click while recording (REC) and COUNT-IN 1 bar. Change them in MENU if you prefer.
- Kits that used FOLD, FEEDBACK or the bus BP sound a little different (BODY, GAIN).

## V10.7.58 — more engines at half rate, lighter reverbs (2026-11-19)

Includes V10.7.57 (beta).

**Voices**
- **STRING, WEST and VOICE (vowels)** also go to half the sample rate (22 kHz) when the CPU runs short, instead of cutting notes. VOICE's WORDS and ANIMAL stay at the full rate for now.
- The half rate is decided **when a note starts** if the predicted load passes 80 %, without waiting for the processor to overrun: fewer peaks when a chord comes in.

**Lighter reverbs**
- **ROOM with MOD or WIDE** and **CLOUD** at 22 kHz: ROOM with MOD/WIDE costs about half, CLOUD a quarter less. The tail loses what is above 11 kHz, as the plain ROOM already did.
- CLOUD's grains work out their window once when born (no divide per sample).

Tested on the FM-1: 6 tracks with full CLOUD on DRUM-X, 60 % CPU.

## V10.7.56 — six tracks without glitches or lost notes, back on V10.7.23's base (2026-11-19)

Built on V10.7.23, the steadiest version, with WEST and V10.7.55's editing (preview of a step's note while stopped, larger menus).

**Voices: no track takes another's**
- A track never loses a note to another track any more. The rules: the **chord track** (found by itself: the one whose sequence has steps of three notes or more, or the first that holds three notes) and the **track you play** keep their polyphony; the **other melodic tracks are single lines** (the note and the last one's tail); DRUM-X keeps its whole kit. Up to 16 voices together.
- When the CPU runs short, nothing is cut: the voices go to **half the sample rate** (22 kHz, interpolated) first the tails, then track by track, and **FM6 down to a quarter** (11 kHz) if still needed. ANALOG, FM6, PHASE, WHEEL, ADD and MATH do it in tune; STRING, WEST, VOICE and ANIMAL not yet.
- Tails leave no noise floor: those at half the rate low-passed, those under -60 dB faded out.
- LOAD ECO keeps its shared budget of 6 voices, as before.

**Lighter**
- **ROOM** reverb always at 22 kHz: the same size and decay, half its CPU.
- **GEN** keeps its songs within the CPU (FM6 once, CLOUD to SHIMMER, shorter chord tails when a song is too heavy).

**Other**
- VOICE sings English by its letter rules (the English dictionary out, for the room these changes needed).
- WEST's CC74 / CC71: FOLD and RES.

## V10.7.45 — LIFE out, copy and paste steps with OCT (2026-11-17)

**Stability and CPU**
- **LIFE is out for now:** it could crash and raised the CPU a lot for little audible change. HOME no longer has its page and nothing of it runs in the audio. It comes back reworked in a later beta. Saved projects open as before.
- **The A#4 punch-in (was LIFE MAX)** now plays DLY ALL (layout 1), LPF (layout 2) or REV ALL (layout 3).

**Sequencer** (a user, Eugen)
- **Hold step keys and press OCT+ to copy them; hold a step and press OCT− to paste there.** The octave does not change. REC on a held step still pastes.
- **Hold a step and turn SELECT** to walk the pages its KNOB 1–4 lock ("LOCK · page"), as in 0.9.12.

**Compatibility**
- Projects and presets load as in V10.7.44. A punch-in set to LIFE MAX goes back to its layout's effect.
- Crash reports now go to the JIANT Discord (GitHub issues are closed); the README says so.

## V10.7.44 — your own punch-ins, mutes back (2026-11-17)

**Playing live**
- **Choose what each punch-in key does:** hold FX, press a key and turn SELECT to pick its effect: any of today's effects, MUTE T1–T6, LOCK, DEC−/DEC+, LPF/HPF, or nothing.
- **Three punch-in layouts**, switched with **FX + OCT+**, kept when you power off.
- **The new default layout brings the mutes back:** T1–T4 MUTE on the keys, FREEZE on G4, REV ALL on D#4.
- **FX FILTER snaps open:** a quick turn towards the centre (or across it) opens the filter fully at once, for fast transitions.
- **GLITCH sounds like before** (ratchets, reverse, octaves, gates, skips, a milder crush) and costs less CPU.

**Performance**
- **DRUM-X voices:** EDIT › VOICES 2 › VOICES now also limits a drum kit's voices to save CPU (kick, snare and clap are never cut).
- **HOME › TRACKS is lighter:** it only redraws what changes and slows down when the load is high.

**Compatibility**
- Projects and presets load as in V10.7.43. Your punch-in layouts start on the new default.

## V10.7.43 — lighter and steadier: more tracks without glitches (2026-11-17)

**Performance**
- **Much lighter on the CPU.** A busy 4-track song now costs less than in V10.7.23, the version many of you remember as the smoothest; 6 tracks play with margin again. Projects that glitched on V10.7.38–42 should play cleanly.
- Silence costs almost nothing, so the FM-1 has more headroom when you add parts live.

**Sound**
- **Cleaner endings:** drum hits and plucked sounds fade out smoothly; the slight "bitcrushed" grit in the tails is gone.
- **LIFE is now one knob:** the tracks modulate each other (FM between tracks). Low values add subtle movement, high values turn into a wild, noisy wash. At 0 it is off.
- **EROSION is simpler:** two modes, NOISE and METAL, and it costs nothing when it is at 0.
- **BALANCE removed** (the automatic mix EQ). It used a lot of CPU; a lighter version may come back later.

**Playing**
- **Presets like before:** turn PRESETS and each sound loads right away, no confirmation needed. **EDIT + PRESETS** jumps to the next engine. The list is still in SAVE › PRESETS.
- **PANIC** (hold PLAY) now also silences the reverb, delay, chorus, FREEZE and any stuck punch-in, and reminds you how to start an empty song (SAVE › TOOLS › CLEAR PROJECT).

**Fixes**
- A click that LIFE could make on loud attacks.
- Old projects with LIFE routes in the mod matrix load correctly.
- Small display and message fixes.

**Compatibility**
- Projects and presets load as before. Projects that used BALANCE, the old LIFE options or EROSION's SOFT/STEP/BREAK/DUST modes load fine; those settings are ignored or mapped to the closest new one.

## V10.7.42 — FREEZE fixed, lighter silence (2026-11-15)

- **FREEZE let go: the tracks back at the full rate.** In V10.7.41, after a FREEZE every track kept playing at half the sample rate (duller highs) until the next punch-in. Fixed.
- **CPU:** a silent block skips the master chain (idle cost below V10.7.23's), LIFE's followers are lighter and FREEZE no longer divides every sample. The sound is bit for bit the same. Every version since V10.7.23 was measured: the extra cost is BALANCE (on since V10.7.33, +3.3 %) and LIFE (+4.2 %).

**Compatibility notes**
- Projects, presets and settings load and sound as in V10.7.41.

## V10.7.41 — FM6 lighter under load, WEST RES fixed, FREEZE lighter (2026-11-15)

Covers the beta V10.7.40.

- **FM6 never reaches a glitch**: under a heavy load the dearest part's held voices (FM6 first: at half the rate it costs 60 %) go to half the sample rate as soon as the load stays past 75 % (ECO: 55 %), no longer only after the guard trips at 85 %; full rate back under 55 %. With a light load nothing changes.
- **WEST with RES high no longer breaks on the peaks**: past RES ~43 the gate's input is lowered with its resonance (x0.25 at 127); below that nothing changes.
- **FREEZE lighter**: while it holds, the tracks under it play at half the sample rate where they can.

**Compatibility notes**
- Projects load as before. Of the factory sounds only WEST's WIND, RAIN, HISS PAD, BONGO and RUBBER BASS (RES high) sound slightly different.

## V10.7.39 — GEN within the CPU, Bluetooth MIDI hardened, MIDI BASE CH, a track's voices (2026-11-15)

- **GEN keeps its songs within the FM-1's CPU.** At full ENERGY and amounts, some pieces (two FM6 parts, the CLOUD reverb, EXPLORE) cost up to 23 % more than SAVE › TOOLS › CPU TEST's song and glitched near 90 % CPU. GEN now estimates each piece's cost as it makes it and, only when it is over, makes it again with FM6 on one track (then none) and SHIMR in CLOUD's place; the steps and the mix stay as they were.
- **Bluetooth MIDI hardened** after a report of crashes with a BLE keyboard: a dense packet of aftertouch, bend or a CC no longer overflows MIDI IN and drops every held note; a Bluetooth link lost with notes held lets them go; a VOICE track whose LANG changes mid-phrase no longer reads past its word template.
- **MIDI BASE CH** (MENU › SYSTEM, 1–11): track *n* listens and sends on BASE + *n* − 1, so two FM-1s share one MIDI bus (one on 1, the other on 7).
- **A track's voices, set by hand**: EDIT › VOICES 2 › **VOICES** (KNOB 4): AUTO or 1–8, and the page shows **ALL n/8**.

**Compatibility notes**
- Projects and settings load as before (VOICES opens on AUTO, MIDI BASE CH on 1). Some GEN results at full settings now come out lighter (FM6 on fewer tracks, SHIMR instead of CLOUD).

## V10.7.38 — LIFE, WEST COAST, the new HOME, FX punch-ins, EROSION, the emulator talks to the FM-1 (2026-11-15)

Covers the betas V10.7.24 … V10.7.38.

- **HOME is the song and the band**: LIFE · MACROS · LIVE MIX · TRACKS · TIMELINE · ARRANGE · PARTS · GEN. **LIFE**: the voices alive, the tracks moving each other (LIFE past 25 % keeps growing, up to ×4 at 100 %). **TRACKS**: every track a lane (LEVEL, PAN, REV, mute and solo). **PARTS** is one grid (a column the part, a cell its track). SELECT on MACROS turns the page; HOME + SELECT there dices the macros.
- **WEST COAST replaces NOISE** (engine 11): a sine core, FM, a wavefolder and a **vactrol low-pass gate** (a quick strike and a long, darkening tail), Buchla / Serge style, drawn on EDIT as its circuit. 20 presets: plucks, BONGO, MARIMBA, bells, FOLD BASS, drones.
- **FX punch-ins redesigned** (hold FX, press a key): REVERSE, TAPE STOP, WOBBLE, DLY 1/16, COMB; PITCH UP / DN, **FREEZE** (the mix itself held as a drone), REV ALL, DLY ALL, RING, LIFE MAX and a new **GLITCH**. Holding an FX with REC armed says **REC FX AUTOM** (REC + FX clears it).
- **EDIT, the sound first**: EDIT 1 · EDIT 2 · the engine's pages, then FILTER · **EROSION** (a noise modulator on the cutoff and pitch: SOFT STEP METAL BREAK DUST) · TONE, then VOICES. **ALLOC** is automatic (VOICES 2: UNISON · VEL · IMAGE).
- **DRUM-X** in twelve pages, with its own EROSION; its sound reviewed (velocity brightens the accents, a drier snare, fuller toms, the kit's DECAY a fifth to five times each sound's).
- **Filters**: one LP24 with DRIVE (MS20 gone), KEY tracking; distortions fixed.
- **VOICE sings phonemes** (the web editor's lyrics, consonants early so the vowel lands on the step); **STRING** gets a real bow (VIOLIN, VIOLA, CELLO) and an electric guitar into an amp (FEEDBACK).
- **DRONE** (VOICES › POLY = DRONE): notes keep sounding after the key; the same note again lets it go.
- **MATH › RETRIG › TIME**: NOTE (as before) or a tempo rate 1/1 … 1/32 and triplets. **PROX** (Bluetooth): SLEW 0–100 % and SENS 0–7.
- **GEN**: a structure per seed (FORM SONG and PARTS), darker drums, more of the synth.
- **UI**: PRESETS opens a list (larger letters), HELP (HOME + OCT+), MENU in four big rows, REC and SAVE held as big tiles, EDIT held draws no map, BPM on MACROS, global SWING on MIXER.
- **The emulator talks to the FM-1**: SEND TO FM-1, GET FROM FM-1, DOWNLOAD BACKUP and IMPORT BACKUP over USB MIDI.

**Compatibility notes**
- Back up first (the editor or the installer).
- NOISE projects open as WEST in a DUST patch (the same PROGRAM CHANGE numbers). MS20 filters open as LP24 set as close as it gets.
- GLO › KEY SPLIT, ENV › STAGE and ENV › TRACKS are gone; projects that used them still load. ALLOC is ignored. The old key latch opens as DRONE.
- V10.7.30 projects that used FILTER 2's METAL, BREAK or DUST load with the same values and sound different (they modulate now).

## V10.7.23 — outlined cards, one skin, cleaner names, AFTERTOUCH 1–10 (2026-11-13)

- **Cards outlined, no grey box**: every knob card sits on the background, its shape drawn as a thin line in its value's colour (the knob just turned: 2 px).
- **COLORS is the one skin**: AIR and MENU › DISPLAY › SKIN are gone (a saved AIR loads as COLORS).
- **Names and pages cleaned**: the parts A–D are PART everywhere (SECTIONS is PART SLOTS, beside PROJECTS); FX's punch-in is PUNCH IN; MENU's INIT SYSTEM is gone (SAVE › TOOLS › CLEAR PROJECT); SEQ's RHYTHM keeps SPAN and FIT (the group is PATTERN; GENERATE is TRACK GEN); EDIT's VOICE / VOICE 2 are PLAY / PLAY 2 (POLY, GL MODE, UNISON; IMAGE moved there); FM6's DETUNEs named apart; GLO's MANO / RIELES are KEYS / RAILS; MIXER's KNOB 3 is MUTE; COMPS is G.COMP.
- **AFTERTOUCH 1 … 10** (MENU › CONTROL): how little pressure reaches the full aftertouch; the old LOW / MID / HIGH read as 3 / 5 / 8.

**Compatibility notes**
- Projects, presets and settings load as before; only page names and places changed (the manual lists where each one is). The web editor still reads library files with the old names.

## V10.7.22 — palettes, bigger type, smoother animations, fewer voices cut (2026-11-13)

Covers the betas V10.7.18 … V10.7.22.

**Sound**
- **ROOM** is a feedback delay network: dense tails that end in silence.
- **Fewer voices cut under load**: released notes hurry their release once they are 20 dB down, FM6 patches whose release never ended no longer hold a voice, and ANALOG / FM6 tails play at half the sample rate (interpolated) when the CPU is short. A kit's **KICK, SNARE and CLAP are never cut**.
- **OCT+ / OCT−** are MOD matrix sources: unrouted they bend and vibrate the pitch as before; routed, OCT+ sweeps and OCT− vibrates the destination instead.
- **The matrix's VEL follows the velocity as played**, whatever EDIT › VOICE 2's VEL is: VEL 0 % plays every note at one level and VEL still modulates.

**Screen**
- **Seven palettes of JIANT's own** (JIANT, SELVA, CONTRAST, BRASA, ABISAL, HUESO, NEON); skins **AIR** and **COLORS** only; STYLE retired (FLAT).
- **Bigger type**: card numbers at 14 px, the small face at 9 px, LARGE tall on FX and SEQ.
- **Animations move by the clock**: under load they keep their speed with fewer frames instead of freezing (freeze only past 85 % CPU); only what changed is sent to the screen.

**Pages**
- **SAVE**: TOOLS first, with **CLEAR PROJECT** (the music back to power-on; the saved project stays). SAVE SONG AS is gone: PROJECTS picks a project with KNOB 1, LOAD / ERASE / SAVE act on it, SONG or LOOP on the NAME screen's KNOB 3.
- **PRESETS**: LIST **USER** (your own presets alone, kept at power-off); a sound type goes back to ALL at power-on. The PRESETS knob loads sounds on every page but SEQ's, GEN's too (GEN's GENRE is GEN 1's KNOB 1).
- **ANALOG**: one filter page; EDIT 1 is WAVE · DETUNE · MIX · INT (SAW3 / PLS3: WAVE · INT · INT 3 · DETUNE); NOISE and MIX on FOCUS.

**Fixes**
- The **Bluetooth part** (it also holds GEN and song saving) missing is said on a full screen at power-on, and the web installer checks it after installing and writes it again if needed. The installer no longer offers the firmware file for other tools.
- AIR's HOME: the beings no longer cover the header at power-on. SAVE FORMAT ERROR says which check refused the save.

**Compatibility notes**
- Projects, presets and settings load as before. SAVE SONG AS moved into PROJECTS (above). On ANALOG, EDIT 2 is gone (its knobs are on FILTER and EDIT 1).
- Presets that route VEL in the matrix now respond to the played velocity when the sound's VEL is under 100 %.

## V10.7.17 — MY LOOP stability (2026-11-10)

- MY LOOP no longer freezes when a melodic track retains DRUM-X hits with no notes.
- TRACKS 4 excludes hidden T5/T6 from loop detection, key analysis and role assignment. Changing the active track count resets loop reuse.
- Public firmware publication now runs MY LOOP, song generator, song draft and GEN regressions before deployment.

---

## Known issues in V10.7.16

- **Velocity on the top row.** The keys C#5..G5 have no pressure sensor, so they play at the average of your recent
  playing and have no aftertouch. Velocity and aftertouch work on F3..C5.
- **AIR is noisy.** As a modulation source and for SWARM, AIR (the Bluetooth devices heard around you) jumps
  more than PROX does. Use **PROX** (your own phone, calibrated FAR / NEAR) when you can.
- **VEL on old projects.** The new VEL setting (EDIT > VOICE 2, KNOB 3) is saved in the place where MUTATE kept its
  settings. A project that still has old MUTATE settings in it may open with **VEL under 100 %** on some tracks. Check
  VEL on each track after you load an old project, and set it back to 100 % if a track feels flatter than before.
- **Installing from Android can stall.** Install from Chrome or Edge on a computer when you can. A phone with
  an OTG cable may work, but a computer is safer.
- **GLO > SYSTEM 2 only shows values.** You can't edit them there. The page tells you where to change them
  (MENU > BLUETOOTH).

---

## V10.7.16 — 2026-11-10 — public

Everything since V10.7.6 (the betas V10.7.7 … V10.7.15 included).

- **GENE › FORM MY LOOP**: your loop made a whole song. It stays as it is (the drop); GEN hears its key and chords (six
  modes, DRUM-X's tonal lanes too), reharmonizes every seed with chords that contain your notes, adds bass, chords and a
  counter-melody on empty tracks only, and lets the genre shape the other sections (its drums on your kit, its bass and
  parts). OCT+ again for another song, UNDO back to the loop. EXPLORE picks a genre per seed.
- **No more stuck notes**: keys, SCENE keys, LATCH, the arp and MIDI-in notes always release what they started.
  **PANIC**: hold PLAY ~0.7 s.
- **VOICE** with words clearer and more fluid (consonants, open vowels, no level dips).
- **Step components: TRIG** (Elektron-style trig condition: TRIG 4:4 plays only on the 4th pass).
- **DRUM-X kits dry** (no default sends), the EXTENDED FX keys no longer force delay / reverb; reverb pre-delay without
  aliasing (ROOM / SPRING with PRE > 0).
- Power-on sounds: ACOUSTIC, ANALOG PLUCK, PHASE INIT SAW, FM BASS, CHANT, WHISTLE (dry).
- Fixes from users: REC then PLAY records the loop after a GEN song; the master EQ and TAPE are saved in every section;
  USB audio on macOS 13–15; the PRESETS knob says LIST <type> when filtered; KEY SPLIT's scope no longer freezes; the
  velocity wait capped at 2.2 ms (none on DRUM tracks); BLUETOOTH reads MISSING (was NO SEG) with a reinstall message.
- GENE: güiro and LATIN PERC kit, CUMBIA RKT, REGGAETON harder, BAILE FUNK minimal, EBM / MINIMAL TECHNO / PHONK after
  reference tracks. EDIT › SOUND without the parameter list beside the creature; thermal PROX and SWARM bars.

**Compatibility notes**
- Projects in the oldest formats (JIANT 0.1–0.5, Felucca 1.0/1.1) are no longer read: open and save them again with
  V10.7.15 or earlier first. Very old user presets load as stored; a retired engine on a track loads as ANALOG.
- ADD shows the generic creature.
- A project using TRIG opened on older firmware plays that step the other way round (harmless).

## V10.7.6 — 2026-11-09 — public

**Fixed**
- SEQ > STRETCH > NUDGE (OCT+ twice, the step): a held note moves with its ties; the neighbour keeps its length; at
  step 1 and at LEN the step stays (ties can't wrap). Locks and automation move with it; UNDO puts it back.
- SEQ > COMP: the MOVE card reads whole ("-63 ST").
- Web editor: the delay times include the dotted ones (1/4D 1/8D 1/16D) in the right order.
- The automatic tests run to the end again (they had stopped early since 8 Nov), so regressions show up again.

---

## V10.7.5 — 2026-11-09 — beta

**New**
- GENE **CUMBIA** rebuilt from the owner's reference and his own loops: syncopated basses (root on 1, the fifth
  anticipating beat 3, a closing 4th or 7th), kick on 1 and 3, llamador and conga accents, the bell on 2 (and 4).
- The güiro plays on the "e" in sonidera throws (as in the owner's loops) and on the beat in villera and digital throws.
- A generated cumbia song has a break in its fill bar.

## V10.7.4 — 2026-11-09 — beta

**Changed**
- SEQ > STRETCH, NUDGE now has a third mode. Press OCT+ again to move the selected step itself one step left or right.
  So: OCT- moves the whole sequence, OCT+ moves the step's MICRO timing, and OCT+ again moves the step.

## V10.7.3 — 2026-11-09 — beta

**New**
- **VEL**: how much velocity affects each sound (EDIT > VOICE 2, KNOB 3, 0..100 %). At 0 % every note plays at a
  fixed full level. Accents still work.

**Changed**
- Factory basses are set to VEL 0 % (steady level). GENE's basses are set to 75 %.

**Compatibility notes**
- Old projects open with VEL at 100 %, so they sound as before. One exception: see "VEL on old projects" in Known issues.

## V10.7.2 — 2026-11-08 — beta

**Changed**
- GENE combines V10.6z10's variety (below) with V10.7's mix and the rules for the Latin genres.
- **CUMBIA** throws are sonidera, villera or digital. **REGGAETON** throws are classic, romántico or perreo. **DEMBOW**
  throws are classic or rápido. Each style follows its own rules: güiro, root–fifth bass, the dembow riddim, tresillo
  808, chops.

## V10.6z10 — 2026-11-08 — beta (built in parallel, merged into V10.7.2)

**Changed**
- GENE: throwing the same genre twice no longer gives you the same song.
  - The bass style changes on every throw, and so do the riffs. JAZZ gets a third walking-bass style.
  - Sounds vary more. A third of the time GENE picks another preset of the same type, never the same option twice, and
    never an INIT preset. Drive, chorus and detune spread a little wider. Phonk and baile funk always keep the
    808 COWBELL.
  - Tempo moves within a third of the genre's range around the ENERGY setting.
  - More drum variety: each genre's own extra kicks, euclidean percussion, ghost notes on the snare, a slightly varied
    first bar, and two more IDM breaks (FUNKY, THINK).
  - Chord progressions come from a family of harmonies, and the genre's own family a third of the time.
  - Later sections develop the first one (bass, chords, arp and the lead's motif vary) instead of repeating it.
  - OCT+ never brings back one of the genre's last eight songs. The same seed still gives the same song.

## V10.7.1 — 2026-11-08 — beta

**Fixed**
- Velocity and aftertouch are back to V10.6z9's behaviour. V10.7's steadier velocity made hard strikes silent and
  aftertouch weak. GENE's mix and the new manuals from V10.7 stay.

## V10.7 — 2026-11-08 — beta (prepared as a release, replaced by V10.7.1)

**New**
- GENE songs mixed like a record: part levels for each genre, level trims for each sound, stereo width, steady sounds.
- DnB gets the Amen break and a four-chord loop. The club genres get arp hooks.
- New manuals (English and Spanish), README and cheat sheets.

**Changed**
- Steadier velocity. The top row follows your dynamics. *(Undone in V10.7.1.)*
- GENE replaces SOFT PAD and BRASS, and no longer randomly changes list settings.

## V10.6z9 — 2026-11-08 — beta

**Fixed**
- DRUM-X has its punch back. The kicks have their body from before V10.6q again: NOISE is off by default, and raising it
  adds a short knock. The hats are bright again (LOFI DUST, HIP HOP, 808, ACOUSTIC, DUB ROOTS, GENE's soft kits, the JAZZ
  ride). Snares, claps and cowbells are unchanged.

## V10.6z8 — 2026-11-08 — beta

**Changed**
- If you change a sound while a song plays, the song keeps your change. The track holds it until a new GENE throw, a
  load, a jump or EDIT > RECALL. Sections still change the steps, mix and mutes.
- PROX calibration (FAR / NEAR) now takes the middle reading of 2.5 seconds. Hold the phone still while it measures.
- AIR is smoothed so it jumps less.
- GLO > SYSTEM 2 tells you where its values are edited.

## V10.6z7 — 2026-11-08 — beta

**New**
- **Key velocity.** The keys F3..C5 play as hard as you strike them and calibrate themselves as you play. MENU >
  CONTROL > VELOCITY sets the curve (SOFT / NORMAL / HARD, or OFF: every note at 100 as before), and VEL MIN sets the
  lowest level. The top row plays your recent average.
- **Aftertouch.** Press harder on a held key. It works as a modulation source (AT) and goes out over MIDI. New sounds
  start with aftertouch opening the filter.
- MENU > CONTROL > AFTERTOUCH and VEL LEARN.
- A live velocity view in the web editor.

## V10.6z6 — 2026-11-08 — beta

**New**
- SWARM plays a melody from your proximity (PROX first, then AIR), on the grid and in the track's scale. With the
  chromatic scale it plays blues with blue notes.
- SWARM gets a FAR / NEAR morph like DRUM-X: RATE, MOVE, TONE, NOISE and MOD for each side (EDIT > SWARM / SWARM 2).
- Each Bluetooth device nearby adds modulation. CHAOS sets the maximum.

## V10.6z5 — 2026-11-08 — beta

**New**
- A key-velocity test mode, off by default (the first step towards V10.6z7), with a live view in the web editor.

**Changed**
- More free space in the firmware for later features. Nothing sounds different.

## V10.6z4 — 2026-11-07 — beta

**New**
- Hold **OCT-** over a sounding note for vibrato, which fades in like a mod wheel. Hold **OCT+** to bend the note up.
  MENU > AUDIO > OCT BEND sets the range (OFF, 1, 2, 3, 4, 5, 7 or 12 semitones; 2 by default). Both are recorded and
  sent out with MIDI OUT ALL.

**Changed**
- REC keeps how you played: note lengths, legato as SLIDE, legato notes that came late put on time, velocity without
  false accents, and the first note on step 1.
- New tracks start with GATE at 100 %.

## V10.6z3 — 2026-11-07 — beta

**New**
- A stable connection for companion apps (version 1.0): apps can ask which version the device runs and follow the
  transport, punch-in FX and mutes.
- Every automatable track setting and the global settings can be controlled with high-resolution MIDI (NRPN), and they
  are recorded. With MIDI OUT ALL, turning a knob sends NRPN.

## V10.6z2 — 2026-11-07 — **PUBLIC**

**Fixed**
- Some units crashed with Bluetooth on. The second processor core now starts only on the normal power setup, and not
  after a SAFE START.

## V10.6z — 2026-11-07 — beta

**Fixed**
- **Newer FM-1s** (sold from late 2026, logo on the front) use a different flash chip. On those units saving, GENE and
  firmware updates did not work. They work now.

**Compatibility notes**
- If you have one of the newer units, install V10.6z or later. Older versions can't save on it.

## V10.6y — 2026-11-07 — beta

**Fixed**
- VOICE no longer distorts on open vowels such as "AH". Its level now sits beside ANALOG's.

**New**
- WORDS, KNOB 3 **CONS**: consonant length from 50 to 400 %. POS moves to EDIT 1 when LANG is on.

## V10.6x — 2026-11-07 — beta

**Changed**
- GENE JAZZ uses any tempo, from a ballad to bebop depending on the throw's energy. The swing gets straighter as the
  tempo rises. The screen shows half the felt tempo.

## V10.6w — 2026-11-07 — beta

**New**
- GENE **HIP HOP**: each throw is trap, drill, pluggnb, boom bap or lofi.
- GENE **JAZZ**: ride pattern, hi-hat on 2 and 4, walking bass, ii–V–I sevenths, Charleston piano comping, swing.
- GENE **TECHNO ROCK**: throws rock or 80s synth.

**Changed**
- Drums brighter, and the 808 less dark.
- The piano roll, the bar thumbnails and the LIVE rails are coloured by note, in the thermal colours.

## V10.6v — 2026-11-07 — beta

**New**
- **RAMP** step component: from a step, a setting glides to a target over 1–32 steps or 1–16 bars. You choose the curve
  (LIN, EXP, LOG, S, STEPS, ZIGZAG). It glides back to the knob at the end and on STOP.
- SWARM: OCTAVE's upper half walks the scale. Each new Bluetooth device changes the timbre and gets its own.
  New **CHAOS** knob (replaces SELF).

**Removed**
- The JUMP step component (RAMP takes its place).

## V10.6u — 2026-11-07 — beta

**Changed**
- GENE genres based on the owner's reference loops:
  - CUMBIA at 76–92 BPM, with the güiro on the "e" and a brighter kit.
  - EBM gets a half-time throw (148–156) with nonstop 16th-note bass.
  - BAILE FUNK gets its claps, the tamborzão and the mandelão (128–136).
  - PHONK gets a Brazilian phonk throw (130–156).
  - HOUSE, DEEP HOUSE and TECH HOUSE are faster.

## V10.6t — 2026-11-07 — beta

**Fixed**
- GENE songs are full again: intros with more parts, the DnB and breakbeat turnaround and hats, and EXPLORE adventurous again.
- DRUM-X kicks have more body.
- Over-loud presets levelled (INIT SAW/PWM, ORGAN, TINE EP, WHISTLE, OCARINA and others).

## V10.6s — 2026-11-07 — beta

**New**
- **SAVE > SONG > CLIPS**: a grid of 6 tracks × 4 parts. KNOB 1 picks the track and KNOB 2 the part. OCT+ launches that
  cell on the next bar, and OCT- stops the track. F3..B3 launch a whole part. REC writes each launch as a song row.

**Compatibility notes**
- **Old formats retired.** Projects from the earliest JIANT builds (before format 1.0.3) are refused and left untouched.
  Old PHYS MODEL, PERC and GM drum sounds are no longer converted. A DIGITAL sound loads as FM6 INIT.

## V10.6r — 2026-11-07 — beta

**New**
- DRUM-X **KEY MAP EXTENDED** is now the default. OCT +1 plays real fills (snare rolls, tom runs, flams, 3-3-2, a crash on
  the next downbeat). OCT -1 plays any sound chromatically (KEY MAP 2..9, BELL for cowbell melodies).
- SCL > SCENE **DRUM GEN**: GENE writes the kit's part for the genre. Each key plays BASE, DENSE, BREAK or FILL.

## V10.6q — 2026-11-07 — beta

**Changed**
- DRUM-X sound quality pass (no new features): cleaner noise, less aliasing, better 808 clap, smoother knobs.
- A classic analog-style cowbell.

## V10.6p — 2026-11-07 — beta

**Changed**
- GENE **PHONK**, **BAILE FUNK** and **EBM** rewritten to sound like the real genres: a new **808 COWBELL** preset and
  riffs for phonk, the tamborzão for baile funk, and 16th-note analog basses with heavy drums for EBM.

**Compatibility notes**
- **6 songs instead of 8.** Songs 7 and 8 are reserved for a future drum sample bank. A project saved on song 7 or 8
  opens on song 6, and you get a one-time notice if they had music.
- **Back up before you install.** The installer now asks for a backup first. The backup reads 8 or 6 songs. A restore
  tells you which songs stayed in the file.

## V10.6o — 2026-11-07 — beta

**New**
- **SAVE > SONG > TIMELINE**: your song as coloured blocks. Open a row track by track (MUTE, FROM another part, its
  variation). PLAY loops the row, and PLAY again continues the song from there. OCT+ copies the row, OCT- deletes it.
- **SAVE > SONG > PARTS**: the four parts as cards. Pick one to loop it (from the next bar). KNOB 4 switches LOOP / SONG.

**Fixed**
- A long STRUM played back too fast after recording.

**Removed**
- DRUM-X ACOUST.

## V10.6n — 2026-11-07 — beta

**Changed**
- GENE's genre list curated by the owner (25 genres plus EXPLORE and RANDOM). New: HARD BASS, TRIBAL, CUMBIA, REGGAETON,
  DEMBOW, IDM GLITCH / AMBIENT / WEIRD, FUGA, ELECTRO METAL, TECHNO ROCK, DREAM POP, PHONK, BAILE FUNK.
- EXPLORE makes rarer music that is still musical.
- SCL > SCENE's dice picks a key (mostly minor modes).

**Removed**
- The genres not in the new list.

## V10.6m — 2026-11-06 — beta

**Changed**
- GENE genres keep their own drums. EBM has many more patterns and basses. Fills only at the end of a phrase.
- The club genres are arranged the way a DJ plays them: intro, groove, break, a 4-bar build (snare roll, riser,
  high-pass, crash), the drop.

**New**
- SCL > SCENE, BASS / LEAD pattern **GEN**: GENE writes the track a line in the key, and each chord moves it.

## V10.6l — 2026-11-06 — beta

**New**
- A new "jiant fm" logo at power-on, written in the owner's own handwriting order.

**Fixed**
- GENE's first press after power-on was always the same song. Now it throws a new one.

**Changed**
- EXPLORE moves (the filter breathes) instead of throwing steps around.

## V10.6k — 2026-11-06 — beta

**Changed**
- More free space for GENE. Nothing sounds different.
- SCL > SCENE's BASS / LEAD pattern and DRUM style are saved with the project.

**Compatibility notes**
- Older projects open with the first pattern and style.

## V10.6j — 2026-11-06 — beta

**New**
- Every note has its own colour (C coldest … B hottest) on KEY, CHORD, ARP and SCENE.
- SCL > SCENE: BASS and LEAD play a pattern over the chord (KNOB 4), and the DRUM role writes a beat for each key (KNOB 4 STYLE).

## V10.6i — 2026-11-06 — beta

**Changed**
- Every engine's factory presets reworked so they move (LFOs, ENV 2, modulation, filter). The browser lists normal sounds
  first, then experimental ones, then INIT.
- ANIMAL trimmed to 29 presets.

**Compatibility notes**
- Preset numbers are kept, so projects keep their sounds and PROGRAM CHANGE still works.
- Loading a sound no longer keeps MUTATE's old modulation slots.

## V10.6h — 2026-11-06 — beta

**New**
- FX > DLY 3 TYPE **REVERSE**: each repeat plays the last moments backwards.

**Fixed**
- PROGRAM CHANGE numbers are the same as before MATH existed.
- Old LOFI PULSE LD tracks load as an ANALOG organ.
- Converted chip sounds keep their filter.
- CC74 / CC71 move the FILTER page on engines without their own brightness control.
- SWARM no longer hears its own voice as a device.

## V10.6g / V10.6f — 2026-11-06 — beta

**Changed**
- Space saved inside the firmware. Every word, sound and drawing is the same as before.

## V10.6e — 2026-11-06 — beta

**Changed**
- **LOFI is now MATH** (formulas only: BYTE, FLOAT). MATH sounds start still and in tune.
- A new power-on band: DRUM-X ACOUSTIC, ANALOG FAT BASS, PHASE GLASS PAD, FM6 TINE EP, VOICE CHOIR AAH, ANIMAL WHISTLE.
- ANIMAL's tonal voices play the note you press.

**Compatibility notes**
- An old LOFI chip sound (from a project or a user preset) loads as the nearest ANALOG sound.

## V10.6d — 2026-11-06 — beta (includes the V10.6c development builds below)

**New**
- **SWARM**: when nothing is plugged into the INPUT engine and BLUETOOTH AIR is scanning, every device heard around you
  becomes a voice (up to eight). SOURCE picks SWARM or the audio input, and both can play at once on two tracks.
- INPUT is renamed **EXT**.
- GEN is renamed **GENE**, and its page shows a turning double helix.
- From the V10.6c development builds (4–5 November):
  - **SAVE SONG AS**: copy a whole song (A–D and the arrangement) to another project.
  - **FIT / SPAN** (SEQ > LENGTH > RHYTHM): fit any number of steps into a loop length, for example 18 steps against 16.
  - **KEY SPLIT** (GLO > SYSTEM > KEY SPLIT): two tracks on the keyboard and on external MIDI.
  - Generated songs live in a draft until you save them as SONG or LOOP. **ERASE** clears a project or a section.
  - New modulation sources (track envelopes / followers) and clearer modulation destinations.
  - DRUM-X **EXTENDED** key map (one fill per key, chromatic kick and tom, hit FX).
  - VOICE in more languages.
  - USER presets keep VOICE lyrics.

**Removed**
- The developer serial console. The USB port now runs as MENU > USB STEREO.

## V10.6c — 2026-11-03 — **PUBLIC**

**Fixed**
- USB audio input (USB > HOST with an audio interface, or USB IN+OUT) works properly. Two errors in how incoming audio
  was resampled and received were fixed and tested on hardware.

**Changed** (from V10.61, included here)
- The PROX page: KNOB 1 **MIN** and KNOB 2 **MAX** narrow the field, so the phone doesn't have to go far away. KNOB 3
  **SPEED** (1–5) sets how fast PROX responds.

---

## Older versions (summary)

- **V10.47 – V10.61 (2–3 Nov 2026)**: the **INPUT** engine (USB audio in on a track) and the FOLLOW source. USB HOST
  takes audio interfaces (including 48 kHz). Synth STEREO IMAGE with its own page. DLY R TIME. The roll follows the
  playhead. Slow-tempo REPEAT. Master EQ / TAPE no longer reset on their own. A SAFE START for units that stayed dark
  after installing. GEN sounds fixed after the preset changes.
- **V10.20 – V10.46 (1–2 Nov)**: USB HOST for MIDI keyboards. Bluetooth MIDI pairing that works on Windows and Mac.
  Steady MIDI clock out. BLUETOOTH AIR. LFO DRIFT. ENV 2, LP24 filter, ANALOG PLS3 / SAW3 intervals. New DRUM-X kit HIP
  HOP. More distinct factory presets. Pictures on many pages that show what each page does. PROX smoothed.
- **V10.1 – V10.19 (30–31 Oct)**: Bluetooth arrives: BLE-MIDI in and out, and **PROX** (your phone's distance as a
  modulation source). SEQ > EUCLID and MODES replace the first MUTATE. Section ERASE. TIMELINE look and SAVE menu order.
- **V10 (30 Oct)**: new base. TIMELINE, VAR, the ADD engine. M-VAVE's official V15 / V16 firmware can be installed both ways.
- **0.9.x (17–26 Oct)**: STRING and ADD engines, the master 4-band EQ, TAPE, a redesigned song generator, VOICE, clean
  USB stems (and the JIANT Stems app).
- **0.7 – 0.8 (15–16 Oct)**: USB multitrack stems, sequencer and REC improvements, cheat sheets.
- **0.6.x (10–15 Oct)**: six tracks, SLICER, MUTATE, step components, MIDI OUT and PROGRAM CHANGE, dual-core audio,
  INIT SYSTEM (power-on at zero), editable FM6.
- **0.3 – 0.5 (9 Oct)**: DRUM-X FM, punch-in FX, reverb and grain delay, HOME macros, the SELECT dice, the master
  leveler, a filter on every engine.
- **Before 0.3 (2–8 Oct)**: Felucca betas (0.4 – 1.1.5), the web installer and the web editor.
