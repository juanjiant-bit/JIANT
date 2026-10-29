Warning: truncated output (original token count: 30219)
Total output lines: 1414

Warning: truncated output (original token count: 79905)
Total output lines: 3440

# JIANT FM

**Bio-synthetic operating system for the M-VAVE FM-1** — synthesis, sequencing, mutation, performance.

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)
[![Emulator](https://img.shields.io/badge/try%20it-in%20the%20browser-ff7a00.svg)](https://juanjiant-bit.github.io/JIANT/emu/)

![JIANT FM: the signature, the screens in the COLORS skin and what's new through 0.9.2: six navigation rules, three skins, EXPLORE, VOICE words, ANIMAL, DRUM-X octaves, REC in 64ths, the DJ FILTER and USB stems](docs/jiant-hero.png)

JIANT FM is an alternative firmware for the **M-VAVE FM-1**, made for playing live and building whole songs without
a computer. It is a fork of [Felucca](https://github.com/hugelton/Felucca) 1.1.5.1 by Hügelton Instruments (GPL-3.0)
and takes its song system from [SLOOP](https://github.com/isod89/sloop-fm1).

**▶ [Try it in the browser](https://juanjiant-bit.github.io/JIANT/emu/)** · **[Interactive manual](https://juanjiant-bit.github.io/JIANT/manual/)** · **[Web installer](https://juanjiant-bit.github.io/JIANT/webapp/installer/)** · **[Web editor](https://juanjiant-bit.github.io/JIANT/webapp/editor/)**: the emulator runs the same code as
the FM-1, with sound, and is played with your computer keyboard or the mouse. The emulator, the installer and the
editor update themselves with every change merged into `main`.

## par14 hardware installation withdrawn — 2026-10-28

A tester reported an FM-1 that would not boot after installing par14. The exact revision, symptoms and cause are not yet confirmed. **Do not install par14 on hardware.** All par14 packages have been removed from the generated installers, including direct firmware download files. Automated tests and a successful package build did not establish hardware safety.

**ADD is the primary edition going forward:** public ADDITIVE par10 stays the default; the beta installer offers ADDITIVE par10d-beta2. Public SAMPLE par13b remains available unchanged as the alternative. SAMPLE development is paused. The SAMPLE browser emulator stays pinned separately through `web/EMULATOR`; this does not make par14 available for installation on an FM-1. SHIFT/CHORD below are archived descriptions of the withdrawn SAMPLE experiment, not current hardware install recommendations.

The incident investigation is tracked in [the par14 audit](docs/PAR14_INCIDENT.md). Do not erase projects or attempt blind reflashing to diagnose a non-booting device; first record its screen and USB enumeration so the recovery path can be chosen for its actual state.

## ADD15 private test build

ADDITIVE development continues from public par10, with **ADD retained and no SAMPLE engine**. The private beta restores REC/undo/quantization shortcuts, independent REC/PLAY, cursor navigation, COPY/PASTE SEQ, SHIFT/CHORD components, STRING feedback MOVE and related amplifier/envelope improvements. DRUM-X adds a first synthesized ACOUSTIC macro on EDIT > KIT 4.

The public installer and firmware, and the frozen SAMPLE emulator, are preserved byte for byte by a private-only deployment. See [the feature list, test procedure and compatibility limits](docs/ADD15_PRIVATE.md). This is a hardware test build, not a fix confirmed for the unresolved par14 incident. Back up projects before testing.

## Current versions

| Where | Version |
|---|---|
| **Public installer** | **ADDITIVE par10** (first/default), **SAMPLE par13b**, then **STANDARD par12** (stable) |
| **Preview installer** | **ADDITIVE ADD15 beta1**, beta only; ADD par10d remains a rollback option; par14 withdrawn |
| **Browser emulator** | Frozen SAMPLE par14-beta4, browser only; separate from the hardware installer |
| Earlier versions the public installer also offers | 0.9.9-par8, 0.9.9-gen2 (*ULTRA STABLE*), 0.9.9-par6, 0.9.9-par5, 0.9.17, 0.9.14, 0.9.9, 0.8.9 … |

[Public installer](https://juanjiant-bit.github.io/JIANT/webapp/installer/) · [Beta installer](https://juanjiant-bit.github.io/JIANT/preview/webapp/installer/) · [Emulator](https://juanjiant-bit.github.io/JIANT/emu/)

The installed line is **0.9.9-par**: 0.9.9's stable core with the new sequencer (seq2), the song generator rebuilt
(gen), two new engines (snd) and 0.9.18's live features ported over (par). `main` also holds the 0.9.18 development
line, which parts of the text below describe; features marked *"in the 0.9.1x line"* that par has not taken yet are
not in the installer's version. If a version gives you trouble, **0.9.9-gen2** in the list of earlier versions is the
one to go back to.

## Choose SAMPLE or ADD in the public installer

The [public installer](https://juanjiant-bit.github.io/JIANT/webapp/installer/) now offers these clearly labelled choices in its **Version** selector:

- **ADDITIVE · par10** (first/default): additive synthesis, without SAMPLE.
- **SAMPLE · par13b** (alternative, unchanged): sample playback; load samples from the editor.
- **STANDARD · par12** (after these two): stable; neither ADD nor the new SAMPLE engine.

These are existing firmware revisions, not two builds with otherwise identical features.
Back up projects before switching: engine slot 13 means ADD in par10 and SAMPLE in par13b; sounds are not converted between them.
Project backups do not include the sample audio bank; keep original WAVs. Earlier versions remain available.

## Recording controls — beta only

- Hold **REC + A3** to undo the last edit/take; repeat to redo. **GLO + G4** remains available. One global undo/redo level.
- Arming REC saves a snapshot before the take. Undo from REC disarms recording to protect the restored take.
- Hold **REC + B3** to switch recording quantization off or to 100%.
- Hold **REC + knob 2** to adjust REC Q for the selected track.
- Unquantized recording retains timing in **1/64 of a track step** (about **1.953 ms at 120 BPM / DIV 1/16**). This is storage resolution, not measured physical input accuracy; notes in the same step share an offset.

See [REC controls and timing](docs/REC_CONTROLS.md). The beta contains the new recording controls; SAMPLE par14-beta4 also includes SHIFT / CHORD. The public installer keeps yesterday's SAMPLE par13b and ADDITIVE par10.

### Beta panel controls

- **REC arms/disarms only. PLAY starts/stops playback independently.** You can enter steps with the transport stopped.
- **SELECT changes SEQ pages**, even while REC is armed. **PRESET moves the selected step horizontally**.
- In **GLO > COMPS / COMPS 2**, **PRESET selects the global step**, wrapping over its 16 steps; it does not load a sound.

## Step components — SAMPLE par14-beta4 (experimental)

All new recording controls and step components are **beta only**. SAMPLE par14-beta4 included SHIFT and CHORD but is now withdrawn from hardware installation. The [beta installer](https://juanjiant-bit.github.io/JIANT/preview/webapp/installer/) offers ADDITIVE par10d-beta2 with recording shortcuts. The public installer retains SAMPLE par13b and ADDITIVE par10.

- **SHIFT** in SEQ > COMP / COMP 2: KNOB 4 moves an event -63..+63 track steps; 0 keeps it in place. The playhead stays on its grid. A moved event replaces the destination during playback; the saved pattern is unchanged. It wraps inside LEN. KNOB 3 offers ALWAYS, FIRST, !FIRST, N:M and !N:M; KNOB 2 sets the affected range. Edits while playing take effect on the next pass.
- **CHORD**: select the chord shape with KNOB 4; the step's first note is its root. It reuses the existing scale/voicing generator and stacks with COMP 2 and the global/master lane, respecting MONO/POLY. It retains the existing mono/kit guards and four-note maximum.

Playback, replacement, early notes, ratchets, N:M, chord shapes, COMP 2, master and project save/load have automated tests. Physical FM-1 testing is still pending. Back up projects before trying the beta; older firmware does not understand the two new action codes. See [component behavior and validation](docs/STEP_EXTENSIONS.md).

## COPY SEQ / PASTE SEQ — beta only

Hold **SEQ + E5** to **COPY SEQ**; select the destination track and hold **SEQ + F5** to **PASTE SEQ**. These commands are shown in the SEQ hold subpage. They copy up to 64 steps and LEN, including notes/hits, timing, components and compatible per-step locks. The destination engine, preset and sound stay unchanged. Locks for a different engine are skipped; continuous automation is not copied. Drum and melodic phrases cannot be pasted across those two formats. Paste replaces the destination sequence and supports UNDO (**REC + A3** or **GLO + G4**). Stop song-chain playback before pasting.

Available for hardware testing in ADDITIVE **par10d-beta2**, in the beta installer only. SAMPLE par14 is withdrawn.

## Uploaded sample pack — Kit 3 (9 sounds)

The emulator now offers **Kit 3 · Uploaded pack (9)**, alongside the previous kits. It is the default only when no valid saved bank exists. To replace a saved bank, choose Kit 3 before switching on. All nine supplied sounds fit in **6116 / 6144 bytes**, using shortened tails and different sample rates; see [the slot map and conversion](web/FACTORY-PACK3.md).

The **beta SAMPLE installer** provisions this bank after a successful firmware update **only if the device reports an empty sample bank**. Existing samples are preserved. ADDITIVE and the public installer do not provision samples. The editor also offers this bank for explicit loading and sending.

SAMPLE par14-beta4 corrects a one-sample overread at full LENGTH, verified with a bounds sanitizer. This is a confirmed defect; the reported screen freeze while changing SLOT is still under investigation. Automated playback and screen-refresh stress tests do not replace testing on the physical FM-1.

## SAMPLE sampler — 0.9.9-par13b

The **par13b release** adds **SAMPLE**, a lo-fi sample playback engine. It is not included in the stable par12 firmware.
Choose **SAMPLE** in the [public installer](https://juanjiant-bit.github.io/JIANT/webapp/installer/);
manage its bank from the [web editor](https://juanjiant-bit.github.io/JIANT/webapp/editor/#sound).
Back up your projects before updating.

### Load the factory bank

1. Connect the FM-1 in the editor and open **Sound**. The sample controls appear when the connected firmware supports SAMPLE.
2. Choose **Load factory samples** to stage the bank in the browser.
3. Review the list and choose **Send to the device**. Confirm only if you want to **replace the entire stored sample bank**.
4. On an FM-1 track, select **SAMPLE** and its **KIT KEYS** preset. The twelve sounds play across **C4–B4**.

The bank contains **KICK, SNARE, HAT, CLAP, TOM, AAH, BIRD, PIANO, FLUTE, VINYL, BASS and CHORD**:
twelve original synthetic recordings, dedicated to the public domain under CC0-1.0.
You can also import your own WAV files in the editor.
See [factory sounds, rates and source](web/FACTORY-SAMPLES.md).

### Try the kits in the online emulator

The [emulator](https://juanjiant-bit.github.io/JIANT/emu/) shows **Sample bank** above the instrument. Before pressing **Switch on**, choose:

- **Saved bank / Kit 3 if empty** preserves a valid bank already saved in this browser, or loads Kit 2 when none is available.
- **Kit 2 · Voices + instruments (7)** loads the seven-sound bank with VOICE 06, VOICE 10 and VOICE 14.
- **Kit 1 · Synth (12)** loads the original synthetic factory bank.

A newly chosen kit starts on **T1 / SAMPLE / KIT KEYS**; slots begin at C4. A status line reports the number of samples actually indexed by the firmware. When preserving a saved session, press **Play kit on T1** to select the kit explicitly; this changes T1’s current sound and resets its audition level, mute, arpeggiator, transpose and quantization. Undo with **GLO + G4**.
Reload the page to choose another bank. The selected bank is kept in this browser when local storage is available.
It changes only the emulator's sample storage, never a connected FM-1.
Playback uses the existing firmware; this does not add the offline preview's interpolation.

### Storage and current limits

- **Up to 15 mono samples** share a **6144-byte bank**, including its header, using IMA ADPCM.
  Duration depends on the chosen sample rates; this is a small lo-fi bank, not long-form audio storage.
- The factory bank uses **5406 bytes**, with sounds at **2, 4 and 8 kHz**, without truncating any slot.
- Factory samples live on the website and are sent to the device's sample storage; they do not occupy the firmware image.
  **They are not installed automatically** by the installer.
- **Project backups do not include the SAMPLE audio bank. Keep your original WAV files.**
  Loading a bank does not change a track's engine or preset automatically.
- Browser/mock-MIDI and native sampler checks pass. Transfer over real FM-1 USB and persistence after power cycling
  have received owner testing; flash persistence after power cycling still needs explicit confirmation.

If automatic editor detection fails, use its **manual MIDI input/output selection**.
The sampler panel requires SAMPLE firmware; updating the web page alone does not add SAMPLE to stable firmware.

## What's new in 0.9.9-par9

**Sound**
- **Master EQ, 4 bands** (FX › EQ, after TAPE): LOW, LO MID, HI MID, HIGH, −12 … +12 dB each, wide and gentle
  (~200 Hz, 1 kHz, 4 kHz). At 0 dB it is bit for bit transparent and costs nothing. Saved with the project.
- **TAPE** on the master (FX › TAPE: TAPE, WOW): saturation, darker highs, a low bump, wow and flutter; much stronger
  at 100 since par9.
- **STRING**: a Karplus-Strong string (EXCITE NOISE / PLUCK / PICK / HAMMR / STICK, POS, STRETCH, BODY, SIZE, BOW).
  RELEASE is its damper: it rings past the step and dies out when let go. NYLON, STEEL GTR, HARP, KOTO, PIANO, MARIMBA, BOWED…
- **ADD**: additive synthesis, ten spectra (HARM ODD WIDE BELL BAR DRUM PRIME FIFTH SUB PLATE) with MORPH, PARTIAL,
  SHIFT, COMB, DECAY and MOTION. Presets PRIME ORG, QUINT, DRONE, GONG, TOTEM…
- **LOFI BYTE / FLOAT**: 48 formulas each (ALGO 0–191). MOTION past half is **SYNC**: the formula follows the bar,
  so a note only transposes it and never changes its speed.
- FILTER TYPE MS20 and COMB−, the delay's DRIVE, DRUM-X GLUE, LFO MODE TRIG / FREE.

**Performance**
- **GLO › LIVE**: the six sequences in parallel with live modifiers (MANO: the keys as a performance board; RIELES:
  six rails, each loop with its rate and length). Never destructive.
- **FX layer**: LOOP 3 / 5 / 7 and **GLITCH**; since par9 GLITCH skips on the 1/16 grid with its cycle locked to the
  bar. G5 clears the punch-in lane's automation.
- **Live REC** places a note where you heard it (the output's latency taken into account on the FM-1).
- **MIDI LEARN** with MIDI MAP FACTORY / CUSTOM ADD / CUSTOM; GLO › SYSTEM 2 shows MIDI IN at a glance.

**Generator (GEN, HOME + SELECT)**
- Rebuilt by role and by genre, tuned on the owner's references: drums in front, leads under the bass, EXPLORE
  calmer, TECHNO minimal 4×4, EBM at 92–104 BPM with six riffs and four basses that vary, DARKWAVE and ITALO a chord
  every 2 bars, slow filter sweeps on bass and pads.
- SEQ + SELECT dices a part with the same generator, on the song's harmony, never near its last 8 dicings.

**Sequencer (seq2)**
- One shared piano roll on every SEQ page; the chain **STEP · LENGTH/TRACK · MUTATE · PLAYHEADS · STRETCH · COMP · AUTO**;
  step components v2 (ACTION LENGTH CONDITION VALUE, two a step); COPY / PASTE; the footer's 16 steps.

**Stability**
- Both cores by default (MENU › SYSTEM › CPU CORES); a crash no longer switches the FM-1 to one core for good.
- The core supplies at M-VAVE's V15 levels, CPU CLOCK 360 / 320 MHz, the averaged overload guard, control rate 64.

## Reporting a crash

If the FM-1 crashes, the red screen now stays **10 seconds** (it was 4) and names the core, the VEC, PC and SP, and the bus
master that wrote or read where it should not. **Take a photo of it**, then power on and open **MENU > ABOUT**, which keeps
the CRASH line (VEC / PC / SP / bus) until power-off. Report both, with the version you installed, in a
[GitHub issue](https://github.com/juanjiant-bit/JIANT/issues/new/choose) (the bug report template asks for the rest).

> **Status of `main`: the 0.9.18 development line (the text below).** The public and beta installers use separately pinned 0.9.9-par revisions: see *Current versions* above. It builds and passes every test (Felucca's and its own). Back up the flash before installing it.
  **RATCH and ARP spill over the steps** (0.9.18) *(in the 0.9.1x line, installer: 0.9.17; not yet in 0.9.9-beta4a)*: RATCH x1–x8 (ECHO folded in: FLAT, FADE, RISE, a pitch step) and
  ARP one note at a time at its RATE play on past their step; OCT an octave a lap; SEQ › COMP frames the step it edits;
  the SEQ › BAR page is gone.
  **Every SEQ page is the piano roll** (0.9.17) *(in the 0.9.1x line, installer: 0.9.17; not yet in 0.9.9-beta4a)*: the same bars' strip and roll (a kit: the grid) on all the pages,
  the values on top, each page marking what it does on the notes: PATTERN their GATE and SWING, TRACK the notes moved
  by PITCH, PLAYHEADS the extra heads as they play, COMP the steps with a component, AUTOMATION and AUTO LIST every
  record, MUTATE when the next mutation comes. The BOTTOM skin is retired (a saved BOTTOM opens as AIR).
  **DRUM-X, VOICE and ANIMAL first** (0.9.9): the FM-1 powers on with T1 DRUM-X, T2 VOICE singing words, T3 ANIMAL's
  bird, T4 LOFI's bytebeat, T5 ANALOG's bass and T6 FM6's electric piano, and PRESETS lists the engines in that order.
  **A new skin, AIR** (0.9.7, the default of new settings): HUD's light look on every page with the knob row on top,
  the middle of the screen free; HOME's beings over the whole screen. **MENU › SCREEN › SCOPE** (OFF by default): an
  engine's EDIT pages give its creature the whole panel. **HOME's beings have faces**: three isotherms coloured by their
  own level, eyes that shut on a hit, a mouth that opens with the sound, a look up when MUTATE changes them. 15 KB of
  flash given back, nothing removed, the sound bit for bit.
  **Every engine draws its own creature** (0.9.6): ANALOG a cell whose membrane is its wave, FM6 its operators as
  little beings wired as the algorithm (also on OPERATOR and OP ENV), WHEEL nine tube worms, one a drawbar, NOISE a
  swarm, PHASE a snail whose shell is the bent phase, LOFI a caterpillar whose skin is its output and segments its bits
  (0.9.10: CHIP flattens it into a DIP chip, 1BIT all the way; every knob shows on it).
  **Messages no longer darken the screen** (0.9.5): a message is a small pill over the page, which keeps moving behind
  it, clear of the page's words; a page's hint comes only once you stay on it (never while SELECT walks past), at most
  twice ever; a layer button's tap no longer says "HOLD [X] QUICK". On JIANT each card's colour follows its value
  again, as in 0.6.
  **Three engines draw their own creature** (0.9.4): VOICE its vocal tract singing, a glowing tube in a ghost of a
  head (0.9.9; the tongue, jaw, lips and velum move with the phoneme sung, glottal pulses travel up it, waves leave the
  mouth), ANIMAL a creature that metamorphoses between bird, insect and mammal, DRUM-X its kit as
  little beings that squash, shut their eyes and shout with every hit.
  **MUTATE can go back** (0.9.3): turning MUTATE off asks to return the track to its anchor; SEQ + D5 REVERT, GLO + B4 REVERT ALL, undoable.
  **0.9.2 is driven by six rules** (see [Navigation](#navigation)): SELECT walks a section's pages as one chain, the
  section's button jumps group to group, PRESETS always loads presets, KNOB 1 picks, a button held is its layer, and
  the tabs and the footer say where you are. Every page, knob and function is where it was; only the way between them
  changed.
  **The dice starts in EXPLORE** (0.8.6): HOME + SELECT opens GEN on a choice that follows no genre and breaks every
  rule (any tempo, odd metres and polyrhythm, exotic modes, any engine on any role, DRUM-X octaves, PLAYHEADS, MUTATE),
  each seed a very different piece, still in the key its SCALE and CHORD pages show. RANDOM and the 43 genres follow.
  **DRUM-X octaves** (0.8.5): the kit stays on its keys at every OCT; above the centre it repeats in time (0.9.13: once a step, x2, x4),
  below it plays SHORT, GHOST or FLAM, each sound its own on each step, and live recording keeps exactly what you
  played. MIDI drum notes outside 35–81 now trigger these variations: see the changelog before installing.

> ⚡ **New in 0.8.3 – 0.9.2: a new look, nine engines, finer timing and a map you can learn in a minute.**
> - **Flat navigation** (0.9.2): one chain of pages per section, the groups as tabs on top (a dot a page), the
>   breadcrumb **SECTION · GROUP · page n/N** at the foot; SCL and ARP two sections; GEN three HOME pages. Clearing a
>   step is **SEQ + G5**, renaming **SAVE + C5**: EDIT is always EDIT.
> - **Three skins, MENU > SCREEN > SKIN** (0.8.9; HUD faster in 0.9.0; 0.9.7: a fourth, **AIR**, the default of new
>   settings; 0.9.17: BOTTOM retired): **COLORS** (rounded cards, each
>   card its colour, on JIANT its value's heat as in 0.6), **BOTTOM** (the picture edge to edge, a slim knob row) and **HUD**
>   (HOME's beings under everything). The **LENTE** look (0.8.5): tabs, cards and a breadcrumb in a crisp 5 × 7 pixel
>   face, values in Inter Tight. **Held layers** show the keyboard itself: MAP, KNOBS and STATE (ALGORITHM while held).
> - **VOICE sings words** (0.8.5): ES, EN or JA, each note the next syllable of a lyric you type (or the dice's).
>   **ANIMAL** (0.8.3), the ninth engine: birds, insects, frogs and beasts from physical models.
> - **DRUM-X in every octave** (0.8.5): the kit stays on its keys; above the centre it ratchets (x2, x4, x8), below it
>   plays SHORT, GHOST or FLAM, each sound its own on each step, recorded as you played.
> - **The dice starts in EXPLORE** (0.8.6): HOME + SELECT opens GEN on a choice that follows no genre and breaks every
>   rule (odd metres, exotic modes, any engine on any role, PLAYHEADS, MUTATE); RANDOM and 43 genres follow.
> - **A DJ FILTER on the master** (0.9.1, GLO › MIXER KNOB 4): low-pass to the left, high-pass to the right, out of
>   the chain in the middle; automatable, saved with the project. **REC off the grid in 64ths** of a step (0.9.1):
>   SEQ › NOTE's MICRO, −32/64 … +31/64.
> - **USB audio that keeps its time** (0.8.9): no lost half buffers (the clicks under load), no dropped packets, the
>   48 kHz resampler on the second core. **Stems**: every track to the computer (MENU > USB > AUDIO STEMS), one channel each, into
>   your DAW.
> - Every version's details: the **[Changelog](#changelog)**.

> 🫶 If you resonate with my work, consider supporting me on **[Ko-fi](https://ko-fi.com/juanjiant)** (in Argentina:
> **[Cafecito](https://cafecito.app/juanjiant)**). Thanks for all the good energy, hope you enjoy this little beast ✨🎹

## Navigation

Ten buttons, ten sections, and the same six rules in every one of them (0.9.2). The map shows every section, its
groups (the tabs on the screen) and its pages in the order SELECT walks them, with the held layers and the combos.
Click it to open it full size ([SVG](docs/navigation-map.svg), zoomable · [en español](docs/navigation-map.es.svg)).

[![JIANT FM navigation map: every section, its groups and its pages in chain order, the six rules, the held layers and the combos](docs/navigation-map.png)](docs/navigation-map.svg)

1. **SELECT walks the pages**: one chain per section, a page a detent, stopping at the first and the last.
2. **The section's button: in, or on**: from elsewhere, the page you left there; inside, the first page of the next
   group (round). FX › DLY 2 → REVERB; EDIT › FILTER → FOCUS.
3. **PRESETS loads presets**: the track's sounds everywhere, the patterns in SEQ, the genre in GEN; never a page, a
   track or a module.
4. **KNOB 1 picks**: the step, the bar, the slot, the stage, the operator, the kit's sound. The track is always
   **ALGORITHM**'s.
5. **Hold = the layer**: a button held puts its shortcuts on the keys and knobs (SEQ + G5 CLR STEP, SAVE + C5 NAME,
   GLO + G4 UNDO …). Held + SELECT: **HOME** GEN, **EDIT / SEQ / LFO / FX** their dice, **GLO** the tempo,
   **ENV / SCL / ARP / SAVE** walk their chain.
6. **The screen says where**: on top the section's groups as tabs (a dot a page), at the foot
   **SECTION · GROUP · page** and **n/N**: **FX · DELAY · DLY 2  6/16**.

The whole chapter: [the manual, § 3](docs/MANUAL.md#3-pages-and-layers) ·
[el manual, § 3](docs/MANUAL.es.md#3-páginas-y-capas). The map is generated from the firmware's own chain tables
(`firmware/src/ui_lens.c`) by `tools/gen_nav_map.py`.

## Manual

What changed in each version: the **[Changelog](#changelog)**, at the end of this page.

The full user manual, every page, layer and setting, with a careful chapter on saving: **[English](docs/MANUAL.md)** ·
**[Español](docs/MANUAL.es.md)**.

**[The interactive manual](https://juanjiant-bit.github.io/JIANT/manual/)** (Spanish and English): the FM-1 drawn
with every control (touch one: what it does alone, held and in combination, with the firmware's real screen), every
command searchable, step-by-step recipes, the navigation map and what's new by version. Its source is
[web/manual](web/manual) (the chains and screens from the firmware, `tools/gen_manual.py`).

## Cheat sheets

Two printable sheets with everything on the device. Click an image to open it full size, or download the PDF.

| Controls | Sound · FX · Mod |
| --- | --- |
| [![JIANT FM cheat sheet: controls](docs/cheatsheet-controls.png)](docs/cheatsheet-controls.png) | [![JIANT FM cheat sheet: sound, effects and modulation](docs/cheatsheet-sound.png)](docs/cheatsheet-sound.png) |
| Every knob and button, the layers with their key maps (FX, GLO, SAVE, EDIT, LFO, SCL, SEQ, REC), the combos and every page's four knobs. **[PDF](docs/cheatsheet-controls.pdf)** | The eleven engines' EDIT pages (STRING and ADD included), FILTER, VOICE, ENV, LFO, the MOD matrix, MSEQ, macros, the sequencer, scales and ARP, songs, and every effect: DIST, the SLICER bus, delay, the five reverbs, the master, TAPE and the 4-band EQ. **[PDF](docs/cheatsheet-sound.pdf)** |

## The idea

A pocket synthesizer shouldn't feel like a menu. JIANT starts from a simple question: what if the device behaved
less like a tool and more like an organism?

That's why everything in JIANT is alive and visible. The screen works like a **thermal camera**: colour isn't
decoration, it's intensity. Cold is cyan and violet, hot is red, orange, yellow and white. A high parameter burns; a
sounding voice glows; silence cools down. Each engine is a **being**, a protozoan drawn in lines whose shape comes
from what you're playing: open the filter and it grows spines, raise the resonance and its membrane vibrates, detune
the oscillators and its nuclei drift apart. HOME is an **ecosystem**: six creatures, one per track, breathing with
their own audio. You don't need to read numbers to know what's going on; you can see it.

At the same time, what is technical is shown as technical. The LFO runs at its rate, the envelope shows where the
voice is, the distortion draws its transfer curve, the delay its echoes, the reverb its response. The organic invites
you to play; the technical lets you understand what you did.

The second idea is that **randomness is an invitation, not an accident**. Every session starts with macros rolled
towards what your sounds have; HOME + SELECT writes a whole piece, EDIT + SELECT dices the sound; macro dice only land on what is playing.
The system pushes you to move things and always leaves you near something that works: the dice start from factory
presets and vary around them, never from nothing.

The third is that **everything is playable and everything is recordable**. Effects are played on the keys and land
in an automation lane; knobs record their movement; songs are built from bars and scenes, and recorded while you
play them. The synthesis engines run in real time on a chip designed for something else; the par13b beta also adds SAMPLE playback (see above).

And the last one is a discipline: **lightweight**. Every graphic is a handful of lines, every effect fits in the
audio CPU budget, the signature shown at power-on is 428 bytes. An instrument that feels big inside a small device.

## The system

### Both cores (dual core)

*Since 0.6.71, experimental.* The FM-1's chip (JieLi AC791N) has **two processor cores**; Felucca and JIANT up to
0.6.70 used one, which ran everything: the sound, the screen, the keys. Now the second one helps with the sound:

- **How the work is shared.** Every 0.7 ms block, the tracks are split between the two cores by how long each took to
  render over the last blocks (timed as they play, since 0.6.73). While the first core renders its tracks, the second
  renders the others into buffers of its own; then they are added together and go through the master on the first
  core. The drum tracks go together, on one core or the other. Since 0.6.75 the second core also runs the effect
  buses (chorus, delay, reverb), on the sends of the block before (0.7 ms later, inaudible), so they no longer wait.
- **The same sound.** The sum is exactly the one-core sum: a test plays a whole six-track song both ways and checks
  that the output is identical, bit for bit.
- **More room.** With the second core doing part of the synth work, the load meter stays lower: more tracks, voices,
  FM6 and heavy reverbs before the clicks and the cut notes start. LOAD LITE / ECO are still there for the heaviest
  songs. **12 voices** (0.9.12, after [Skiff](https://github.com/illuminus5/skiff)): while the second core plays, the
  tracks share 12 voices instead of 8 (each still at most 8; ECO 6); with one core, 8 as before.
- **Units that cannot run it.** On some FM-1s the second core now and then reads a word of memory wrong, more often
  under load (found by the Melodee and fm1-x0x developers). JIANT guards against it: every job carries a check word,
  and if the second core reads a wrong one, faults or does not answer in time, it is **stopped at once and the first
  core plays every track again**, with no crash (at most one 0.7 ms block of those tracks is lost). Since 0.6.76 the
  effects' output is checked too: far out of range, the block is muted, their lines cleared and the second core stopped
  the same way. Such a unit
  then stays on one core **until it is switched off**, and so does any unit after a crash while both cores ran.
- **What it says.** MENU > SYSTEM > ABOUT, under AUDIO LOAD: **CORES 2**, or **CORES 1** with the reason: REC AT POWER-ON,
  BAD READ, NO ANSWER, FAULT, SINCE A FAULT (an earlier one this power-on), NO START.
- **One core on purpose.** Hold **REC** while switching the FM-1 on: that session plays on one core, as before.
- **Credits.** How to start the second core safely on the FM-1 and the guard against misreads come from
  [Melodee](https://github.com/keremimo/melodee) (keremimo) and [fm1-x0x](https://github.com/charlesvestal/fm1-x0x)
  (Charles Vestal), both GPL-3.0; JIANT's version is in `firmware/hal/fm1_multicore.h`.

### A living screen

![HOME, ARRANGE, SEQ › NOTE off the grid, FX › DELAY, the DRUM-X kit and VOICE's WORDS in the COLORS skin](docs/jiant-screens.png)

*HOME with its six beings, the song in ARRANGE, SEQ › NOTE with a note 13/64 late, the delay, the DRUM-X colony and
VOICE singing its lyric, in the COLORS skin, exactly as the firmware draws them (the renders of `tests/ui_render.c`).*

- **Thermal palette**: values, curves, steps, hits and notes are coloured by intensity (cyan → blue → violet → red →
  orange → yellow → white). Selection is violet and active is orange; on JIANT each card's value and gauge are its
  amount's heat (0.9.5, as 0.6), the other palettes give each knob its own colour.
- **Skins** (MENU > SCREEN > SKIN, 0.8.9): **AIR** (0.9.7, the default of new settings: HUD's floating words on
  every page, the knob row on top under the tabs, the middle free), **COLORS** (near-black rounded cards on top, a rounded
  panel) and **HUD** (HOME's beings swim under everything, the words float over them, the knob row at the foot; on the
  SEQ pages on top, as AIR's). (0.9.17: BOTTOM, the knob row at the foot on every page, is retired; a saved BOTTOM
  opens as AIR.) Every skin wears the **LENTE** layout (0.8.5): the
  section's groups as tabs on top (a dot a page), the breadcrumb **SECTION · GROUP · page n/N** at the foot. Values
  in [Inter Tight](https://github.com/rsms/inter-tight) SemiBold, names, tabs and the breadcrumb in a crisp 5 × 7
  pixel face, small captions in [Chakra Petch](https://github.com/m4rc1e/Chakra-Petch).
- **HOME, the ecosystem**: one being per track (its species from its engine, its shape from its four knobs). Each one
  heats up, swells and moves with its own audio; the selected track is drawn bigger; muted ones go dark; in silence
  they all stay still. (0.9.7) Each wears a little face: its eyes shut on a hit, its mouth opens with its level, it
  looks up when MUTATE changes it; muted, it sleeps.
- **Engine pages**: each engine draws its own creature (0.9.4 / 0.9.6, [below](#v096)): VOICE a singing head, ANIMAL
  a bird / insect / mammal, DRUM-X its kit of little beings, ANALOG a cell, FM6 a colony of operators, WHEEL tube worms,
  NOISE a swarm, PHASE a snail, LOFI a caterpillar. (Before: the sound's being drawn in isotherms, four contours of
  the same body, cold edge, hot core, as HOME's ecosystem still shows them.) The being's knobs read as what they are: **CUT** grows spines (more harmonics, spikier), **RES** makes the membrane vibrate,
  **DTN/SPRD** splits two nuclei, **NOIS/RAND/CRSH** frays the edge, **WAVE** changes the lobes.
- **DRUM-X, the colony**: one being per kit sound, each with its own shape (round kick, spiky snare, fringed hats,
  clustered clap, star-shaped cowbell) that swells and heats up when it hits (KIT 2, KIT 3, X-MOD); on KIT, SOUND 1–4
  and HOME (0.9.4) **the kit as little beings** with eyes and mouths that squash and shout with every hit.
- **Live technical graphs**: LFO (the wave runs at its rate, with playhead and current value), ENV (a dot travels the
  curve with the sounding voice), DIST (transfer curve by TYPE and TONE response), DLY and DLY 2 (the echoes, L and
  R), REVERB and REVERB 2 (the impulse response, stereo, drawn per model), CHORUS (the three modulated lines), AUTOPAN (the swing across the stereo field), MASTER
  (clipper curve, PUNCH shape, how much DUCK pulls down), ENV/LFO DEST (bipolar columns under each knob, lit by what
  they send), VOICE (one cell per voice and the glide curve), GLOBAL (the bar in thermal columns with swing, clock and
  tuning).
- **Load meter**: five small bars next to the battery showing the current audio load.
- **Power-on**: the "Jiant FM1" signature is written stroke by stroke in thermal colours (MENU > SCREEN > ANIMATION OFF: it shows
  up whole).

### Engines
The stable par12 line has ten synthesis engines: **DRUM (DRUM-X), VOICE, ANIMAL, LOFI, ANALOG, FM6, PHASE, WHEEL, NOISE
and STRING**. ADD is available in par10; SAMPLE replaces it in par13b. The synthesis engines are listed in the order PRESETS shows them (0.9.9-par; the first six are the tracks at power-on, T1–T6).
- **ANALOG**: two oscillators with **INT** (an interval of ±24 semitones: fifths, octaves) and DTN; besides SAW, SQR,
  TRI, SIN and PWM it has **SYNC**, **RING** and **SAW3** (three saws). Its filter is per voice.
- **FM6**: Dexed's engine (imports .syx), with its algorithms drawn. Fully editable on the device (from Felucca 1.5):
  **EDIT > OPERATOR** (KNOB 1 the operator 1–6, then RATIO or FIXED frequency, FINE, LEVEL), **OP ENV** (the stage
  ATK DEC SUS REL, its RATE and LEVEL, drawn as the envelope) and **OPERATOR 2** (MODE ratio/fixed, DETUNE, VEL), the
  operator lit in the algorithm's colony of operators (0.9.6: each a little being wired as the chart was) with its
  frequency; OP ENV's operator rides its envelope. Editing makes the patch the track's own (SLOT OWN) and it is
  saved with the project; the macros (EDIT 1 / 2) still act on top. The web editor edits every byte of it too.
  **Voice bank** (from Felucca 1.4.1): import a 32-voice DX7 cartridge (.syx) in the web editor's 6-OP FM tab and press
  **Send to the device's voice bank**; on an FM6 track **SLOT** (EDIT 2, KNOB 4) then goes F1–F8, OWN, **B1–B32**
  (empty slots skipped), the voice's name on the chart. A voice loaded is copied into the track: projects and user
  presets keep it even after you send another bank. The bank lives in its own flash sector, written power-cut safe;
  the full backup does not include it (keep the .syx).
- **PHASE**: phase distortion with **FB** (the output feeds back into the phase: from hard edge to growl).
- **LOFI**: 1, 4 and 8-bit chip with **BYTE** (bytebeat: 32 formulas picked with ALGO and mutated with VAR). In
  BYTE, **MOTN** sets the speed of the formula's rhythm, **GRIT** crushes its rate, **BEND** folds time and **LOOP**
  repeats a short stretch of the formula: the noise becomes a tone tuned to the note. **FLOAT** is a floatbeat
  generator: 32 formulas of `t` made of sines (rhythm, melody and timbre at once), in the key played. **0.9.9-par6**:
  48 formulas each (ALGO 0–191), and MOTN past half is **SYNC**: the formula's rhythm follows the bar, so a note only
  transposes it.
- **STRING** (0.9.9-snd5): a Karplus-Strong string. EXCITE (NOISE, PLUCK, PICK, HAMMR, STICK), DECAY, BRIGHT, POS (where
  it is struck); BODY page: STRETCH (a stiff, metallic string), BODY, SIZE and **BOW** (a bow's noise fed into the loop).
  RELEASE is its damper: the string rings past the step and dies out when let go.
- **ADD** (0.9.9-snd5): additive. MODE picks the spectrum (HARM ODD WIDE BELL BAR DRUM PRIME FIFTH SUB PLATE), MORPH
  moves between spectra, BRIGHT and PARTIAL how many partials, SHIFT stretches them, COMB carves notches, DECAY lets
  the highs die first and MOTION sweeps the spectrum.
- **VOICE** (formants, after klattsch), **WHEEL** (drawbar organ) and **NOISE** (coloured and metallic noise).
  **VOICE sings words** (0.8.5): LANG ES / EN / JA, each note the next syllable of a lyric you type (on the device or
  from the web editor) or of phrases the dice puts together from each language's word bank; POS and LOOP automate it.
- **ANIMAL** (0.8.3): animal calls from physical models: a source that oscillates or strikes exciting a resonator,
  with a call's rhythm on top. **CMOS** is the owner's 40106 / 4093 breadboard "bird machine" (Schmitt-trigger RC
  oscillators: a slow one gates and sweeps a fast one, the exponential charge is the chirp's hook), **PULSE** an
  escapement into a resonator (crickets, katydid, cicada, frogs, toad), **WHSTL** a Hopf oscillator whose tone is born
  as the pressure crosses a threshold (canary, cardinal, dove, owl, peeper, bat), **WING** flight tones with drift and
  Doppler fly-bys (mosquito, bee, fly), **GLOTT** a glottal pulse into two formants with subharmonics and chaos (wolf,
  bark, roar, purr). GROUP makes chirps and trills, TEMP (°C) speeds insects up as warmth does (Dolbear's law); a note
  is a call. 24 presets.
- **FILTER on every engine** (EDIT > FILTER): **TYPE** LP, HP, BP or **COMB** (a comb tuned to the note you're
  playing; CUT moves it ±32 semitones and RES is how much it rings), **CUT** and **RES**. On ANALOG it's its per-voice
  filter, with DRV; on LOFI it's per voice too (its CUT and RES are the ones on this page); on the others it's a filter
  on the track that follows the LFO and the envelope (LFO/ENV DEST FLT), and its DRV is the track's DIST.

### DRUM-X: synthesized drums with morph
An 8-sound kit (BD SD CP CH OH TM RS CB) generated in real time, Microtonic style.
- Each sound has two sides, **A** and **B**; **MORPH** (KNOB 1) moves between them.
- **FOLD** runs each sound's oscillator through a wavefolder after its envelope: the hit starts bright and the tail
  returns to the clean wave. **FM** changes the timbre with harmonic FM in 8 bands; plus TUNE, TONE, DECAY, NOISE and
  DRV.
- **EDIT > SOUND 1–4** (one sound each, KNOB 1 picks it): SIDE, PITCH, PMOD; DECAY, NOISE, COLOR; **MODE**,
  **PM.TYPE** (how the pitch moves: DECAY, LONG like the 808, NOISE, SINE), DRIVE; **LAYER**, **FEEDBK** and a
  per-hit **SEND**.
- **The kit in every octave** (0.8.5): the keys play the kit on the same keys at every OCT; above the centre a key
  **ratchets** in time (x2, x4, x8), below it plays **SHORT**, **GHOST** or **FLAM**, each sound its own on each step,
  and live recording keeps exactly what you played. MIDI drum notes outside 35–81 trigger the same variations.
- **Group mutes** (KICK, SNARE, HAT, PERC) with GLO held and **per-sound mutes** with EDIT held.
- **EDIT > X-MOD** (0.5.1): a tempo-synced LFO on MORPH and RAND, which gives every hit its own MORPH and tune.
- Nine kits as the DRUM engine's presets (DRUM-X, 909 CLUB, 808 BOOM, LOFI DUST …); EDIT + SELECT rolls a new kit.

### Dice: the system invites you to move everything
- A button held + **SELECT** rolls that part's dice (0.9.2: SELECT alone walks the pages). **EDIT + SELECT rolls the
  track's sound dice**: a synth loads one of its factory presets at random and moves each engine parameter up to a
  quarter of its range; DRUM-X rolls the A and B sides of each sound around the factory kit. **SEQ / LFO / FX +
  SELECT** dice the sequence, the LFO, the effects, each shown **BEFORE** and **NOW** to keep or cancel; **GLO +
  SELECT** is the tempo. **HOME + SELECT** opens
  **GEN**, the song generator (since 0.6.91; HOME's last three pages since 0.9.2): it opens on **EXPLORE** (0.8.6: no
  genre, every rule broken: odd metres, exotic modes, any engine on any role), then RANDOM or a genre among 43 (house,
  techno, dub, UK garage, funk, post punk, jazz, ambient …), a seed, an energy, a form (a loop, parts, a club song); how much drums,
  bass, chords, arp and lead; complexity, swing, how human. **OCT+** writes the piece from them (OCT+ again: a new
  seed; a knob turned and OCT+: the same piece with that one thing changed), **OCT−** is done. Every piece has one
  key, written to the SCALE page (and the CHORD page set to its chords: the keys play along in key), drums from the
  genre's priority map, a bass archetype written against the kick, a functional or modal progression with cadences
  voiced and led voice by voice, a lead composed from a motif in periods and sentences, a sound recipe a role, the
  genre's kit and space (see v0.6.91, v0.8.3).
- **UNDO: GLO held + G4** takes back the last big change (DICE ALL, a sound diced, a clear, INIT, RECALL, a pattern or
  project loaded); pressing it again goes forward (redo). G4 breathes when there is something to undo.
- **Macros M1–M4** (LFO held; they stay latched when released until you press LFO again): every session starts with
  two random routes per track towards what its sound has. With the macros open, **F3** rolls new routes, only on
  the tracks that are playing; G3 clears them (LFO + SELECT rolls the track's LFO). They can be assigned by hand in MOD (SRC M1…M4) and are saved
  with the project.

### Punch-in FX: effects you play and record

> *In the 0.9.1x line (installer: 0.9.17), not yet in 0.9.9-beta4a:* the sweeps, TAPE STOP, the performance macros and recorded FX lanes below (0.9.15-0.9.17).
With FX held the keys become effects that act while held; on release everything goes back exactly.
- **Audio**: REPEAT 1/8 · 1/16 · 1/32, **GLITCH** (the bar cut into repeats, reverses, octave slices and gates on the
  grid, the same at the same place every time) and **TAPE STOP** (0.9.15).
- **Sweeps** (0.9.15, black keys C#5 D#5 F#5): **FILTER** (high-pass opening), **CRUSH**, **THROW** (into the delay and
  reverb): held, dry to wet over 2 bars on an S curve; let go, back in a beat.
- **Performance macros** (0.9.15, the knobs with FX): **STEPS** (every sequence 1/2, 1/4, 1/8, one step, from where it
  plays), **TRANSPOSE** (+2nd … +6th along the scale, ±octave), **MUTATE** and **FILTER** (low-pass), built up turning
  right; turn any knob back and all four are off at once.
- **On the notes, OP-Z style**: OCT− and OCT+, 1/2 TEMPO, short DECAY (also lowers sustain: plucks) and long, STUTTER
  1/8 · 1/16, ATK+ (drums too), momentary ARP (on drums, a different fill every time) and RANDOM.
- **Quantized**: they kick in on the transport's next sixteenth; REPEAT and SLICER follow the grid from the first
  moment.
- **Automatable**: with REC armed they land in a 4-bar lane per section (the note effects, GLITCH, TAPE STOP and the
  sweeps). A#4 chooses whether the note effects affect everything, the synths or the drums.

### Sequencing

> *In the 0.9.1x line (installer: 0.9.17), not yet in 0.9.9-beta4a:* the roll on every SEQ page, COMP, RATCH / ARP spilling over steps (0.9.17-0.9.18). 0.9.9-seq2b has its own SEQ chain (see *What's new*).
- 64 steps per track, piano roll, drum grid, parameter locks, chance, ratchets, slide and knob automation; live
  recording with overdub, metronome and count-in.
- **SEQ's chain** (0.9.2): PATTERN (PATTERN, STRETCH, PLAYHEADS), TRACK (TRACK, MUTATE), BAR (STEP, NOTE; 0.9.18: the
  BAR page is gone), STEP (COMP, COMP 2), AUTO (AUTOMATION, AUTO LIST); KNOB 1 all 64 steps on STEP; PRESETS loads a pattern.
- **One picture for the whole SEQ** (0.9.17): every page shows the pattern's bars in a strip and the cursor's bar as
  the piano roll (a kit: the grid), the values on top; each page marks what it does on it: PATTERN draws the notes as
  long as GATE plays them and the odd steps late by SWING, TRACK outlines where PITCH moves the notes, PLAYHEADS draws
  the extra heads as lines while they play, COMP / COMP 2 a bar over each step with that component, AUTOMATION and
  AUTO LIST a mark under each record's step (the row picked framed), MUTATE a chip with the bars to the next mutation.
  (0.9.18) COMP frames the step the component goes on, live with KNOB 1, and marks the steps with the same one.
- **Off the grid in 64ths** (0.9.1): live recording with REC Q under 100 % keeps where you played, to a 64th of a
  step; **SEQ › NOTE**'s MICRO edits it (−32/64 … +31/64), and playback honours it to the sample.
- **ROTATE** (STRETCH, KNOB 4) and **PITCH** (TRACK, KNOB 1) shift the sequence in steps and in pitch, automatable.
- **One scale for everything**: ROOT, SCALE and QUANT are the song's, the same on every melodic track (no track keeps
  a scale of its own, whatever loads it); **STRN** moves all sequences through the
  scale degrees. **ARP TRNS**: the keys transpose the sequence.
- **SEQ + REC** clears every sequence; **REC + FX / EDIT / ENV / LFO…** clears only that part's automation.

### Songs inside the device
**8 projects**, each with **4 sections (A–D)** and a song of up to 16 rows by bars (SAVE > **ARRANGE**), SLOOP style:
live sections that come in on the next bar, quick chain, SONG REC and STORE / RECALL. Each row can hold a **scene**
(mutes, DRUM-X group mutes, macros and MIDI punch-ins), saved with KNOB 4 on ARRANGE or on its own when recording with
SONG REC. The song's END plays it once (STOP), x2..x8 or forever (LOOP).

### Modulation
- **MOD matrix**: 4 slots per track. Sources: LFO, ENV, VEL, KEY, RAND, MIDI controllers, the 4 macros and STEP.
- **MSEQ**: a modulation sequencer per track (16 levels, LEN, DIV, SLEW), like a CV sequencer.
- **ENV LOOP**: with the note held, the envelope goes back to the attack when it reaches sustain: an ADSR-shaped LFO.

### Effects and master
- **DIST** per track with TYPE (SOFT, HARD, FOLD, CRUSH, RECT) and TONE; the **SLICER** on a bus: T1–T6 and the FX returns switched in (SL BUS), a pitch sequence for its repeats (SL PITCH).
- **Delay**: synced or free TIME (5 ms to 2.95 s, every division its real length up to 2.97 s; past 0.74 s the line at
  half rate, past 1.49 s a quarter: the long tails darker, as a tape's), a single TONE, **grain delay** with PITCH and
  SPRAY (at long times too), stereo WIDTH.
- **Reverb** with five models: ROOM, SPRING, **SHIMR** (shimmer, goes up an octave on every pass), **RESO** (four
  strings tuned to the song's scale) and **CLOUD** (a granular wash that blooms into a tail and can freeze); with pre-delay, modulation,
  filter and width; **chorus**, a three-voice stereo ensemble.
- **Master**: **CLIP** (saturation with level compensation: adds character, not volume), **PUNCH** (drives the drums into saturation: denser, fatter body),
  **DUCK** (the kick pulls the rest down) and an **always-on leveler** (a slow 2:1 rider with automatic make-up, from
  −9 to +6 dB: it follows the song, not each hit) before the limiter and the soft clip: quiet patches and heavy CLIP play at a consistent volume.
- **DJ FILTER** (0.9.1, GLO › MIXER KNOB 4): one bipolar knob on the whole mix, before CLIP and the leveler: a
  low-pass sweeping down to the left, a high-pass sweeping up to the right, out of the chain (bit for bit, no CPU) in
  the middle. Automatable and saved with the project.

### Knob acceleration
Turning fast sweeps the whole range; turning slowly is fine adjustment. Turn it off in MENU > SYSTEM > KNOB SPEEDUP.

### Connections and storage
- USB: class-compliant MIDI and audio (the master reaches the computer); TRS MIDI; internal, USB or TRS clock. Since
  0.8.9 the USB audio keeps its time: no half buffer lost under load (the clicks), no packet dropped, the 48 kHz
  resampler on the second core.
- **MIDI LEARN** (0.9.12, MENU › MIDI › LEARN): turn a knob, send a CC; up to ten, kept with the settings, over USB
  and TRS (from Felucca 1.5).
- **Stems** (MENU > USB > AUDIO): **STEMS** sends every track on its own channel (8: T1–T6, FX L R), **STEMS+IN** adds
  your voice through the FM-1. Record them in your DAW, one track per channel.
- 32 named user presets; the project autosaves 10 s after the last change while stopped (not at power-off).
- **Web editor** for every parameter: FM6 patches, grid, mix and full backup (all 8 songs with their sections, rows
  and scenes).

<details>
<summary><b>What changed from Felucca 1.1.5.1</b></summary>

- **New interface**: thermal palette, three skins (AIR, COLORS, HUD), the LENTE layout (tabs, cards, breadcrumb),
  beings, HOME ecosystem, live technical graphs and a power-on signature; navigation by six rules (0.9.2).
- **DRUM** is DRUM-X; Felucca's kits were retired. **CHORD** was retired (its parameters are SHIFT). **PHYS** and
  **TRIO** were retired (TRIO lives inside ANALOG; a PHYS sound plays as ANALOG's first preset).
- **Legacy sample engines**: the old Felucca SAMPLE, SLICE and GRAIN engines were retired to make room for DRUM-X,
  the effects, punch-ins and songs; their old sounds load as ANALOG. The **par13b beta** introduces a new SAMPLE engine
  and bank format (see [SAMPLE sampler](#sample-sampler--099-par13b)); it does not restore the old banks.
- **8 songs**: song 1 is the usual 4 projects, so whatever you saved with Felucca shows up there.
- **FX layer**: REVERSE, TAPE STOP, FREEZE and the harmonizer are gone; the MIDI punch-ins came in.
- **SELECT**: walks the section's pages; with a button held: HOME GEN, EDIT / SEQ / LFO / FX their dice, GLO the
  tempo, ENV / SCL / ARP / SAVE their chain.
- **EDIT held**: INIT / RECALL instead of the voice selector. One-step **UNDO** with GLO held + G4 (SAVE held: OCT+ stores the
  section, OCT− recalls it). **No layer lock.**
- **USB names**: "JIANT" (MIDI and audio; 0.7.0–0.9.11 "JIANT FM"), "JIANT Update" while updating; the web editor and the installer take the older "JIANT FM" and "Felucca" too.

</details>

## Controls

- (0.9.2) Six rules, the same in every section: **SELECT** walks the section's pages; its **button** brings you back
  to the page last shown there, or, already inside, to the next group; **PRESETS** always loads presets (sounds; SEQ:
  patterns; GEN: the genre); **KNOB 1** picks (the step, the bar, the slot); **held** is the layer; the tabs and the
  footer say where you are (**FX · DELAY · DLY 2  6/16**).
- **Holding** a button opens its quick layer: keys and knobs change function while it is held.

| Hold | Keys | Knobs |
| --- | --- | --- |
| **FX** | F3 G3 A3 REPEAT 1/8, 1/16, 1/32; B3 GLITCH; C4 TAPE STOP (0.9.15). MIDI punch-ins: D4 OCT−, E4 OCT+, F4 1/2 TEMPO, G4 DEC−, A4 DEC+, B4 C5 STUTTER 1/8 · 1/16, D5 ATK+ (everything's attack up, drums included), E5 ARP (drums: a new fill on every press), F5 RANDOM (notes and steps). Everything comes in on the transport's next 1/16. Black keys 1–6: mute T1–T6; A#4 chooses which tracks the MIDI ones affect (all, synths, drums); C#5 D#5 F#5 the sweeps FILTER, CRUSH, THROW (dry to wet over 2 bars, back in a beat). **Automate**: with REC armed and playing (not in a song), the note effects D4–F5, GLITCH, TAPE STOP and the sweeps you hold are recorded in the section's lane (64 steps of 1/16, one a step: the lowest held); REPEAT, mutes and latched effects are not; G5 erases it where the playhead passes, or entirely with the transport stopped | STEPS, TRANSPOSE, MUTATE, FILTER (performance macros: up to the right, any knob turned back cuts all four) |
| **ARP TRNS** | With the ARP mode on TRNS, the keys (and incoming MIDI) transpose the track's sequence by their interval from C4, without playing notes; the transposition stays on release | — |
| **SEQ > STRETCH · TRACK** | ROTATE (STRETCH, KNOB 4) reads the track's sequence from another step, −32 to +32 (within LEN; live recording writes where you hear it) and PITCH (TRACK, KNOB 1) transposes it ±24 semitones (not on kits). Both can be automated and are also on the SCL layer (KNOB 3 / 4) | ROTATE, PIT |
| **REC + another button** | Hold REC and press FX, EDIT, ENV, LFO, SCL, ARP or GLO: clears that part's automation on the selected track (knob movements and per-step locks; stored values stay). FX: the punch-in lane too. GLO: levels and pan of the 6 tracks. The other way round (FX held then REC) arms recording, as always | — |
| **SEQ + REC** | Hold SEQ and press REC: CLEAR ALL SEQUENCES? (OCT+ confirms): clears the steps and automation of the 6 tracks and the punch-in lane | — |
| **LFO** | — (**MACRO** layer: below, where each macro goes) | M1, M2, M3, M4 |
| **GLO** | Black keys 1–6 mute T1–T6 (latched); 7–10 DRUM group mutes: KICK, SNARE, HAT, PERC; F3–D4 solo T1–T6 while held; E4 unmutes everything; F4 tap tempo; G4 **UNDO** the last big change (again: redo) | T1–T4 level (with T5 or T6 selected: T3–T6) |
| **SCL** | Any key picks the root | ROOT, SCALE, ROTATE, PITCH |
| **EDIT** | F3 **INIT**: the track's sound goes back to factory (on DRUM, the DRUM-X kit too). G3 **RECALL**: back to the sound saved in the section (on DRUM, with its kit). Both ask for confirmation and leave the steps alone. On a DRUM track, black keys 1–8 mute each DRUM-X sound. The engine and sounds are picked in PRESETS | The engine's first 4 EDIT knobs |
| **SEQ** | On the SEQ pages: SEQ TOOLS; G5 CLR STEP (the cursor step; AUTO LIST: deletes the record) | LEN, DIV, SWING, GATE |
| **REC** | F3 CLEAR the track, G3 CLICK | CLICK |
| **HOME** | Tap: HOME; held ~0.7 s: MENU | — |
| **SAVE** | Song layer (on SAVE > ARRANGE, KNOB 4 = SCENE): F3–B3 play A–D on the next bar (stopped: at once; several in one hold: quick chain), C4–F4 store into A–D, D5 LOOP / SONG, E5 SONG REC, G5 ARRANGE, C5 NAME (renames on USER, SECTIONS, PROJECTS); OCT+ stores the playing section, OCT− recalls it | KNOB 1: project 1–8 (on release, stopped: loads its section A, replacing the music, no UNDO) |

Others: **PLAY** starts and stops; **REC** arms the selected track; **SELECT** walks the section's pages (held with
a button: HOME GEN, EDIT / SEQ / LFO / FX their dice, GLO the tempo, ENV / SCL / ARP / SAVE their pages);
**ALGORITHM** picks the track on every page; **PRESETS** loads the sound (SEQ: a pattern); **OCT− / OCT+** the octave (on action pages and dialogs: back / confirm); GLO + PLAY restarts from the
beginning.

## User manual

A practical guide to what's new, in the order you'll run into it. The [Controls](#controls) table has the details of
each layer; the complete manual is in **[docs/MANUAL.md](docs/MANUAL.md)** (en español:
**[docs/MANUAL.es.md](docs/MANUAL.es.md)**).

### 1. Getting around

> *In the 0.9.1x line (installer: 0.9.17), not yet in 0.9.9-beta4a:* the MENU as nine pages in five tabs, MIDI LEARN, BOTTOM retired (0.9.12-0.9.18); 0.9.9-beta4a has the older MENU.
- (0.9.2) Each section is one chain of pages in groups. **SELECT** walks the pages (a page a detent, stopping at the
  ends); the section's button brings you back to the page you left there and, inside, jumps to the next group's first
  page (round). The tabs on top are the groups (a dot a page), the footer **SECTION · GROUP · page n/N**. **HOME**
  takes you to the ecosystem (ECO → BEING → GEN).
- **FX in one chain**: TRACK (SENDS, DIST, AUTO PAN), BUSES, DELAY (3), REVERB (2), CHORUS, SLICER (4 pages), MASTER,
  PUNCH: SELECT walks all 16, an FX tap jumps to the next module.
- **PRESETS** always loads presets: the track's sounds, on the SEQ pages a pattern, on GEN the genre.
- **Jump through a section** from anywhere: hold ENV, SCL, ARP or SAVE and turn **SELECT** (HOME + SELECT opens GEN,
  EDIT / SEQ / LFO / FX + SELECT roll their dice, GLO + SELECT the tempo).
- **ALGORITHM** picks the track (T1–T6) from any page; track pages show the selected track.
- The four knobs drive the four columns on screen. Turning fast sweeps the whole range, slowly fine-tunes.
- **Holding** a button opens its layer (FX, GLO, SCL, LFO, EDIT, SAVE…): release it and you're back where you were.
- (0.9.18) **The MENU** (hold HOME) works the same way: nine pages of up to four settings in five tabs, **SCREEN · AUDIO ·
  MIDI · USB · SYSTEM**. SELECT walks the pages, ALGORITHM the tabs, **KNOB 1–4 the settings on screen**, OCT− / OCT+
  step the one you turned last. Each row under the cards says in plain words what it does. CALIBRATION, ABOUT and LEARN
  CLEAR are action cards: turn their knob to arm them, OCT+ does it. HOME closes it.

### 2. Reading the screen
- **HOME**: each being is a track. Its species tells the engine, its shape comes from its four knobs, and it swells
  and heats up with its own audio. The biggest one is the selected track; a dark one is muted.
- **Colours**: cold (indigo, violet) is little, hot (red, orange, cream, white) is a lot. Violet is what's selected,
  orange what's active; on JIANT each card is coloured by its value (cold low, hot high; 0.9.5, as 0.6), the other
  palettes give each knob its own colour.
- **Skin** (MENU > SCREEN > SKIN, 0.8.9): **AIR** (0.9.7, the default of new settings) HUD's look on every page with
  the knob row on top, **COLORS** the cards on top in the 0.6 colours, **HUD** HOME's beings over the whole screen under
  floating words (the knobs at the foot, on top on the SEQ pages). BOTTOM is retired (0.9.17): a saved BOTTOM opens as AIR.
- **Load meter** (top right, next to the battery): five small bars showing how much of the audio time the sound is
  using right now. The firmware's RAM is reserved entirely at boot and doesn't change, so this is what's worth
  watching: with all five lit (red, yellow) you're close to the limit and should cut voices, the CLOUD reverb or heavy
  effects. If it's often full: GLO > SYSTEM > **TRACKS 4** (the default; also MENU > AUDIO > TRACKS) leaves T5 and T6
  asleep for headroom, and **LOAD** (next to it; also MENU > …219 tokens truncated…the mix and the master, the
  FX layer, the SLICER, and the first core's copies of a track's mix and the effect buses. The second core keeps the
  flash copies (it now and then misreads RAM on some units). The code itself is the same as before (only its place
  changed), so the sound is identical; whether the peaks go down can only be measured on a real FM-1: compare PEAK,
  LATE and SHED in MENU > ABOUT with 0.6.77 on the same song. If it does not help, it goes.

### v0.6.77
- **FM6: the operator graphs follow the knobs again.** On EDIT > OP ENV the envelope's curve did not move while you
  turned RATE or LEVEL (nor when you changed the stage or the operator), and OPERATOR's chart missed its edits too:
  the screen redraws a graph only when something it shows changed, and on these pages the patch was not on that list.
  It is now, with a test that turns each knob and checks the graph redraws.

### v0.6.76
- **The effects guarded on the second core** (a tester's idea, thanks!). On some FM-1s the second core now and then
  reads a word of memory wrong; the check word guards the hand-over of each job, not the numbers the job reads. With
  the effects there since 0.6.75, a test now feeds every value of their state a wrong word (on all five reverbs):
  their lines are 16-bit and clamped, so nothing can grow through the feedback, at worst a short burst the master's
  limiter holds. Two guards came out of it: ROOM's modulation, given a wrong offset, now puts it back instead of
  reading far outside its buffers; and if the effects' output ever goes far out of range (as the known wrong read
  would make it), that block is muted, every delay and reverb line is cleared, and the second core is stopped as a
  BAD READ (one core until switched off). USB console `status`: `fx_trips`.

### v0.6.75
- **The effects on the second core.** The chorus, delay and reverb buses ran on the first core after both cores had
  rendered their tracks: a fixed piece of every block the second core could not help with (up to a quarter of the
  work, more with a heavy reverb). Now they run on the second core while the first renders its tracks, on the sends
  of the block before, so the wet signal comes 0.7 ms later than the dry one, too little to hear. With one core they
  run at the same point, so the sound is the same either way (the test still checks it bit for bit). In the
  simulation of a heavy six-track song the mean load falls from 77 % to 63 % of one core, and the peaks (the ones that
  make the LATE and SHED counts in ABOUT) are now about 18 % lower than on one core (before, they barely moved).

### v0.6.74
- **Fewer clicks.** When the FM-1 has to let a voice go (all 8 voices busy and a new note comes, or the load guard
  freeing time), it cut it in 0.7 ms: on a loud note, a bass note most, that is a click, and it happened a few times a
  second in a busy song. Now the voice fades out over ~4.4 ms along a half cosine (gentle in and out): measured on a
  55 Hz bass, the click is about 7 times smaller. A new test keeps it so. The voices fading do not count in the
  budget, so the new note never waits.

### v0.6.73
- **Both cores, better shared** (after the first tests on a real unit: mean load 54 %, but a few blocks near the limit,
  a click now and then): each track's render is now **timed** as it plays, and the tracks are split between the cores
  by what they really take (it was an estimate from their voices and engines). The **drums can go to the second core
  too** (all the DRUM tracks together, on one core or the other): they were always on the first, often the heaviest
  part of a song, which left the cores uneven.
- USB console `status`: `cpu2`, `cpu2_jobs`, `cpu2_tracks`, `cpu2_wait_max_us` (0.6.72), to see the second core at work.

### v0.6.72
- **MIDI OUT** (MENU > CONTROL > MIDI OUT, over USB): **KEYS** (as before), **CLOCK** (and the clock with START / STOP
  while the tempo is the FM-1's own: drum machines and DAWs follow it), **ALL** (and every note the FM-1 plays: the
  sequencer, the arp, FUGUE, the punch-in repeats with OCT and RANDOM, each track on its channel, every note ended),
  **OFF**.
- **PROGRAM CHANGE**: program *n* on a track's channel loads the PRESETS list's sound *n* + 1 into that track.
  (0.6.80) On **channel 16**, the song's: 0–3 jump to section A–D (on the next bar), 16–23 load project 1–8.
- MIDI check: USB / TRS MIDI IN, clock, Start / Continue / Stop and the punch-ins verified by the tests. MIDI OUT on the
  TRS jack is not possible (the jack is an input only) and neither is Bluetooth MIDI (it needs JieLi's closed SDK).

### v0.6.71
- **Both cores** (experimental, [details](#both-cores-dual-core)): the FM-1's chip has two cores and JIANT used one. Now, while the first renders its
  tracks, the second renders some synth tracks of its own (the drums stay on the first), whichever has less to do;
  the sound is bit for bit the one core's (a test checks it). The way to start it safely and the guard for units that
  cannot run it come from Melodee (keremimo) and fm1-x0x (Charles Vestal), GPL-3.0: if the second core reads a wrong
  job, faults or does not answer, it is stopped and the first plays every track again, no crash; such a unit stays on
  one core until switched off. **MENU > ABOUT** says CORES 2 or CORES 1 and why. Hold **REC** while powering on for
  one core that session.

### v0.6.70
- **LOAD ECO really lighter** (4 tracks with reverb and delay still crackled on ECO): the SPRING every reverb becomes
  on ECO was heavier than a plain ROOM; on ECO it now runs a shorter chain (4 stages instead of 10, a third of the
  reverb's cost gone). The voices all tracks share drop from **8 to 6** on ECO, and the load budget steps in sooner
  (at half the time instead of 65 %), taking only tails that are nearly silent: the peaks when every track hits at
  once no longer reach the point where the sound breaks.

### v0.6.69
- **SELECT scrolls inside the section**: alone it goes through the pages of the section you are in (EDIT's on an EDIT
  page, FX's on an FX page …), stopping at its first and last page; on HOME it opens DICE EVERYTHING? as before. The button combos stay:
  HOME / EDIT / SEQ / LFO / FX + SELECT their dice, GLO + SELECT the tempo.
- **The presentation film**: a new one for this version (tools/video/capture_promo.c, promo.html): dark hypnotic
  techno by the real engines, the screen floating in a living membrane, annotated like a specimen, and the list of all
  that is new.

### v0.6.68
- **Fix: a cleared sequence came back by itself.** With MUTATE PATTERN on, clearing a track while it played let the
  old melody return a few bars later (ORBIT mutated around the anchor taken before the clear), and DRIFT could add drum
  hits to an empty kit. Now the anchor follows the steps you write or clear, and MUTATE never fills an empty pattern.

### v0.6.67
- **SELECT scrolls the pages**: turn it alone and it goes through every page, one a detent, in the panel's order (HOME,
  EDIT, ENV, LFO, FX, SCL, ARP, SEQ, GLO, SAVE), the page's name shown below for a moment.
- **A button held + SELECT is its dice**, heard at once under **KEEP?** (OK keeps, CANCEL puts back what was there):
  **HOME** everything (DICE EVERYTHING?), **EDIT** the sound, **SEQ** a new sequence for the track (as DICE ALL writes
  a part, in its scale), **LFO** the track's LFO, **FX** its effects and the shared reverb and delay. **GLO + SELECT**
  is the tempo; ENV, SCL, ARP, SAVE + SELECT (and any button + PRESETS) jump through their pages. The macros' dice is F3.

### v0.6.66
- **Pages with SELECT**: hold a page button and turn **SELECT** to go through its pages (PRESETS still works too).
  While it pages, SELECT dices nothing, nor in the moment after you let go. GLO + SELECT stays the tempo; with LFO
  held, F3 rolls the macros.

### v0.6.65
- **Sound dice with OK / CANCEL**: EDIT + SELECT rolls the sound and you hear it at once under **KEEP DICED SOUND?**
  (DRUM: KEEP DICED KIT?). Turn SELECT again for another roll; **OCT+ OK** keeps it, **OCT− CANCEL** brings back
  the sound you had, only that track's, with the music playing on. No more sounds lost by accident.

### v0.6.64
- **FX held opens the punch-in FX and stays**: hold FX until its map opens and let go: the FX screen stays open, both
  hands free, until you press FX again. The effects are **momentary** again by default (a key plays its effect while
  held); MENU > CONTROL > FX LATCH ON makes them toggle, as in 0.6.44–0.6.63.
- **Page name at the bottom**: the page's name now shows for a moment over the footer (which says the same), so the
  header, the cards and the graph stay in sight.
- **Granules for free**: the drifting dots behind the panels move only when the panel redraws anyway (a knob, the LFO,
  the envelope); they no longer redraw the panel on their own.

### v0.6.63
- **A safer installer**: before writing, it checks the FM-1's firmware by itself (no tick box any more): official V15
  or JIANT, it installs; Felucca, baudgirl or anything else, it writes nothing and sends you to **Return to official
  V15 first** (going straight from another custom firmware has bricked an FM-1). JIANT now tags itself in the editor's
  INFO for it; older JIANT versions are recognised by their six tracks.

### v0.6.62
- **Step components with TIMES**: JUMP and REV get KNOB 4, **TIMES**: ALWAYS (as before), **x1** (once a pass, then
  the pattern plays on past it: a section twice, then the rest), **x2**, **1:2** (once, every other pass). A jump back
  no longer has to shorten the sequence. Old projects play as before (ALWAYS).

### v0.6.61
- **The page name in the header**, not over the parameters or the graph: for 0.9 s the header shows the page (and the
  track and engine at the right); on HOME a track change shows its engine, with "HOME" small.
- **Granules in the panels**: a few dots drift behind the graphs, as on HOME (LOAD FULL with ANIM ON; not on the lists
  and the step grids).

### v0.6.60
- **All black**: cards, panels, menu rows and dialogs are their text, graphs and beings on black, with no box drawn
  (the chosen row and the buttons keep their fill).

### v0.6.58
- **Page name over the cards, shorter**: the big name now covers the four cards for 0.45 s (was the graph for 0.7 s), on
  one line with the track and engine at the right; the graph stays in sight.
- **INIT SYSTEM in the MENU** (SYSTEM tab): everything back to zero for a new song, after OCT+ asks (as SAVE > TOOLS >
  NEW PROJECT; UNDO brings the music back).
- **FX LATCH on in the browser emulator too** (the punch-in FX latch was ON on the device only).
- **DRUM-X pages: the kit, a sound, then the volumes and the stereo**: KIT, KIT 2, KIT 3 (BP, FEEDBK), X-MOD, FILTER;
  SOUND 1–4; LEVELS, LEVELS 2 and **STEREO** (IMAGE, moved from KIT 3).

### v0.6.57
- **LOAD LITE / ECO also lighten the screen**: the beings, the scope and the animated graphs redraw at most every 50 ms
  (LITE, ~20 frames a second) or 70 ms (ECO, ~14), the beings' outlines with fewer points (LITE about half the lines,
  ECO a third), and values snap instead of rolling. The main loop keeps that time for the keys, the knobs and the LCD.
  A new page or track still draws at once; FULL is unchanged.

### v0.6.56
- **The landing's typography and windows** (the JIANT palette; the beings, DRUM-X and the colours stay as they are):
  values in **IBM Plex Mono**, page names and big numbers in **Michroma**, labels still in Chakra Petch. Cards, panels,
  rows, tabs and buttons are square with four **HUD corners**; the page banner is the landing's tag (a dark window,
  orange corners, the name in Michroma, the track under it). The fonts take 4 KB less flash than before; no CPU cost.

### v0.6.55
- **ALGORITHM only offers the tracks that play**: with TRACKS 4 it stops at T4, T5 and T6 no longer show up (they were
  asleep and confusing). TRACKS 6 brings them back; going back to 4 while on T5 / T6 selects T4.

### v0.6.54
- **DRUM-X pages in order**: first the whole kit, then one sound. **KIT** (EDIT 1), **KIT 2** (EDIT 2), **KIT 3** (TONE's
  BP and the kit's FEEDBK, with IMAGE), X-MOD, FILTER, **LEVELS**; then **SOUND 1–4**, KNOB 1 always the sound: SOUND
  (SIDE PITCH PMOD), SOUND 2 (DECAY NOISE COLOR), SOUND 3 (MODE PM.TYPE DRIVE), SOUND 4 (LAYER FEEDBK SEND, the bus and
  amount on one knob). 14 pages became 12; the TONE, LAYER, SEND and IMAGE pages are folded in, SOUND 3's WAVE (MODE
  has it) is gone.

### v0.6.53
- **Memory check.** Flash 392 KB of 568 KB; RAM 80 KB of 96 KB; the big-buffer pool 325 KB of 336 KB. The pool was the
  tight one (3.9 KB usable over its 8 KB reserve): the DRUM-X hits' state and the FM6 user-voice buffer moved to the
  ordinary RAM, so the pool has 11 KB to spare and the RAM 16 KB. No sound or behaviour changed.

### v0.6.52
- **FUGUE** (SEQ > FUGUE), after the Fugue Machine: three more playheads on every track, each with its RATE (1/4 .. x4),
  DIRECTION (FWD, REV, PING, RND) and INTERVAL in the scale (-14 .. +14 degrees), reading the same steps: canons,
  counterpoint, a pattern phasing against itself. Saved with the section. Cost: a few operations a block a head; RAM
  +0.4 KB.

### v0.6.51
- **LFO in time and free-running** (asked on YouTube): RATE turned left past the slowest goes in time with the tempo
  (4BAR … 1/32, triplets and a dotted eighth), locked to the bar while playing; PHASE turned left past 0 is **FREE**:
  the LFO never restarts with a note. The mod matrix moves RATE within its kind (free or in time).
- **The page's name, big**: changing page or track by hand shows the page's name in the middle for 0.7 s, with the track
  and its engine, so the pages need no memorising.
- **The power-on signature** stays a little longer (written in 0.8 s, held 0.9 s).

### v0.6.50
- **Fix: MORPH clicked when turned** (ANALOG, PHASE, LOFI). It moved in steps once a block; now it is slewed and ramped
  sample by sample across each block, including in and out of 0.

### v0.6.49
- **DRUM-X LAYER** (EDIT > LAYER, each sound): a second, out-of-tune oscillator frequency-modulated by the sound, from a
  metal edge to a clang with no note, so drums melt into the rhythm and stop fighting the harmony. **FEEDBK** per sound
  on the same page (on top of TONE's, the kit's). Saved with the kit; the dice (SELECT, DICE ALL) set both boldly.
- **Cowbell tamed**: the factory one shorter, noisier, with a LAYER, and the cowbell 4 dB under the other sounds in
  every kit (it stuck out). The factory snare and hats get a LAYER too.
- **Power-on is a clean INIT**: every track its engine's own defaults (not its first preset), every effect send at 0,
  FM6 the init voice; INIT SOUND and NEW PROJECT the same. (The power-on still loaded the presets with their sends.)
- Cost: a sound with a LAYER costs about what FEEDBK does; the factory kit with all 8 sounds ringing at once +13 % on the
  drum voices (host), nothing on sounds without one. RAM: +200 B of pool, +16 B. Older projects keep their kit as it was (no LAYER).

### v0.6.48
- **DRUM-X IMAGE** (EDIT > IMAGE on a DRUM track, one knob, automatable): sweeps the kit through its stereo placings
  (each sound on its own curve), so you turn it until the image sits right. 0 = all in the middle, as before. The kick
  stays near the middle; with DRV, DIST or the FILTER the image narrows (they work on the kit's sum).
- **DRUM-X SEND** (EDIT > SEND): each sound its own send to CHORUS, DELAY or REVERB and an AMOUNT, so one hit goes to an
  effect without the whole kit. Saved with the section's kit.
- Cost, measured: about +3 % of the mix with IMAGE on and every sound sending (host); nothing when IMAGE is 0 and no
  SEND is set. RAM +1.3 KB, no pool.

### v0.6.47
- **C.LEN** (SEQ > SHIFT, KNOB 4): the step components get a length of their own. OFF: each step its own component, as
  before. 1–64: the components of steps 1..C.LEN are read in turn as the track plays, so over a pattern of another
  length they drift across the notes (polymeter, as the OP-Z's). The COMP page marks the lane and where it is.
  Automatable. Costs nothing on the audio: a copy of the step once per step.

### v0.6.46
Feedback from users (YouTube, Reddit):
- **INIT SOUND is dry**: every effect send (DIST, CHORUS, DELAY, REVERB) at 0, so the effects are yours to add. On
  **FM6** INIT is the init voice (OP1 alone at full level, the rates at 99: a plain sine) instead of a factory patch.
- **NEW PROJECT** (it was INIT SYSTEM: SAVE > TOOLS, 5th row): a clean start, every track its engine's sound, dry, FM6
  the init voice, no steps or automation.
- **NOISE DUST** never silent: at DENS 0 it still crackles softly (DUST is only the crackle).
- **FM6 EDIT 1**: the chart says the algorithm's number also when ALG follows the patch ("ALG 05 PAT").
- **Web installer**: pick an earlier version to go back to (the last few stay there), and links to the manual
  (English, Español).

### v0.6.45
- **MORPH** (EDIT > MORPH) on ANALOG, PHASE and LOFI: the wave slides continuously into the next ones; on ANALOG and
  LOFI the SHAPE modulation sweeps it too. The page draws the morphed cycle.
- **Fix (reported on Reddit): "the sequence pitch drops by itself"**. It was SCL 2's STRN, a global transpose of
  every sequence that is easy to turn by mistake next to ROOT. Renamed **ALL TR**; when it is not 0 the piano roll
  shows an **ALL TR** badge and PLAY warns. 0 = off.

### v0.6.44
- **Punch-in FX latched by default** (asked by many): **FX LATCH** is now ON (turned on once for everyone; turn it OFF
  in MENU > CONTROL if you prefer holding). **Lock**: hold FX until its map opens and let go without touching
  anything: the FX layer stays open (**[FX] LOCKED**), as the macros latch with LFO; keys toggle effects, knobs are
  FX's macros, OCT− all off. Press FX again to unlock.

### v0.6.43
- **MUTATE** (SEQ > MUTATE, a suggestion from the community): each track's **SOUND** and **PATTERN** change by
  themselves every 1 to 32 bars, on the bar, in time with the clock; **AMOUNT** how much; **ORBIT** varies around an
  anchor (KEEP: OCT+ on the page) so the music stays itself for minutes, **DRIFT** wanders on. Never LEVEL, PAN or
  MUTE, never the kick. **FREEZE** (OCT− on the page, or GLO + A4) stops it all. The page draws the next 32 bars of
  every track; on HOME each being gets a ring filling up to its next change.

### v0.6.42
- **Step components, the second set** (SEQ > COMP): **RAND** (the note at random in the scale), **OCT** (an octave,
  every loop or every other one, by turns, at random), **TONAL** (the rest of the loop moved along the scale),
  **BEND** (the note glides over the step), **ECHO** (repeats, each softer), **PULSE** (again on the empty steps
  after it), **JUMP** (then another step), **REV** (back to step 1: ping-pong), **FILL** (only while FX is held) and
  **KILL** (cuts what the track sounds). Each with its picture on the page.

### v0.6.41
- **Smooth engine and waveform changes**: another engine fades the old one out (~12 ms) and what is held (keys, the
  sequencer's notes) goes on in the new engine, fading in, instead of being cut. A list value changed while notes
  sound (WAVE, CHIP, a type) dips for ~3 ms instead of clicking; the notes go on.

### v0.6.40
- **INIT SYSTEM** (SAVE > TOOLS, 5th row, with a dialog; UNDO brings the music back): everything to zero at once:
  every track its engine's default sound, no steps, automation or locks, the mix, effects, tempo and SLICER at their
  defaults, the song empty, macros at 0, the factory DRUM-X kit. Saved projects, user presets, MENU, CLOCK, TUNE and
  MIDI stay.
- **Power-on at zero**: the device starts as INIT SYSTEM leaves it. **RESTORE LAST** (MENU > SYSTEM) is now **OFF** by
  default; turn it ON to get the last session back at power-on (if you had set it OFF before, it reads ON once: set
  it again).

### v0.6.39
- **Step components** (OP-Z style), on a new page **SEQ > COMP** right after STEP: one per step, picked with KNOB 2.
  **ARP NOTES / SEQ / SCALE** turn a step into an arpeggio (its own notes, every note of the pattern, or the scale up
  from it; UP, DOWN, UP-DN, RND; x2 to x8 in the step; 1 to 4 octaves or 3 to 15 degrees) that goes on over the ties
  after it. **TRIG** plays a step only on some loops (1:2 .. 4:4, 1:8, FIRST, NOT FIRST). The step's own **CHANCE**
  and **RATCH** are back here too. The page draws the arpeggio's ladder (the note playing lit) and TRIG's loops.

### v0.6.38
- **Step recording, chords with a length and arpeggios**: on STEP with REC (stopped), hold a note or a chord and turn
  **KNOB 1**: it lasts that many steps, written when you let go. Press **ARP** while holding it: the chord goes into
  the roll as an arpeggio, KNOB 1 its resolution (1/16 .. 1/4), KNOB 2 the order (UP, DOWN, UP-DN, RND), KNOB 3 the
  octaves.
- **DICE ALL**: the lead never goes past G5 on any of its paths (a FRANTIC cell could reach C#6).

### v0.6.37
- **SEQ pages, lighter**: PHRASES and the step CHANCE page are out of the cycle; **SHIFT** is now the last SEQ page
  and has a third knob, **CHANCE**: the whole track's probability (100 % plays every step as written, lower thins it
  out), automatable like OFFSET and PITCH. Old projects and presets keep playing as they were.
- **Films**: the presentation video in the landing page's look, with a dark minimal techno track (low FM chords, acid,
  a cyber line).

### v0.6.36
- **DRUM-X: FM becomes FM ENV.** The old FM knob put a fixed FM over every hit and muddied the kit (the macros' dice
  liked it a lot). **FM ENV** strikes the FM with each hit and lets it go with the pitch envelope (a few ms): a beater,
  a click, a metal edge on the attack, the tail as the kit has it. Projects keep the value; its effect is the new one.
- **DRUM-X: EDIT > TONE**, a new page after X-MOD: **BP**, a band-pass on the hats, the rim and the cowbell (OFF or 15
  centres, about 1.5 to 9 kHz), and **FEEDBK**, every sound's oscillator fed back into itself (warm to growl to noise).
  Saved with the section's kit in a byte it already reserved: older projects open with both OFF, sections no bigger.
- **Films**: the features film's music is new and darker (E harmonic minor, Em(add9) Cmaj7 Am7 B7, a rolling sub, a
  low arp, the robot low and spaced) and the camera cuts to HOME's beings every two bars of a long scene; the DICE ALL
  film shows HOME's beings instead of the mixer.

### v0.6.35
- **DICE ALL: the drums as before, darker.** The chords, the variety and the guard rails of 0.6.34 stay; the drums go
  back to 0.6.33's, which sounded better: the DRUM-X kit rolled around the factory one through a soft low-pass, the
  patterns written whole (the energy no longer thins or fills them), and in the night's genres they almost always play.
  The night's genres come up four throws in five (IDM and DARK IDM still the most).
- **Two new films**: the DICE ALL film (one minute: a throw every few seconds, an animated die rolling between them,
  no names) and the features film updated; thumbnails for both (`tools/video/thumbnails.py`).

### v0.6.34
- **DICE ALL: a generator of pieces, not of conditions.** Each throw makes three choices and guard rails keep it
  musical inside the chaos (see the manual, chapter 8):
  - **A genre**: the dark ones, plus five new worlds: **SOUNDTRACK** (enveloping pads, no drums or a heartbeat),
    **HARD TECHNO**, **INDIE POP**, **ELECTRO PUNK** and **NEO CLASSICAL** (Reich's pulses and phases, Glass's additive
    arps, Pärt's tintinnabuli).
  - **An energy**: **SPACE** (two or three things, long tails: minimalism with cared-for timbres), MID, **FRANTIC**.
  - **An orchestration**: which roles play; **the drums may not play at all** (about a third of the throws).
  - Guard rails: **chords always of four notes**, **deep basses** (from E1), **arps in the middle octaves** (C3–C5:
    Alberti, Glass groups, a Reich pulse, a walk), **coherent melodies** (a motif in steps developed over four bars,
    ending on a cadence), **space between notes**, **high notes softer** and high parts quieter, the lead under G5.
  - **Seven drum kits** built on DRUM-X: 808, HARD, CLICK, INDUS, SOFT, GLITCH, ELECTRO.
  - Sounds always fit their role (never a bass sound off the bass); envelopes shaped for the role and the energy.
  - **A twist** in one throw in five: two loops phasing, a comb, the grain delay down, a polymetric lead, a drone.

### v0.6.33
- **DICE ALL: more IDM, darker, never flat.** IDM and a new **DARK IDM** (slow, phrygian, a drone, in-scale fourths,
  a few long notes) come up about 4 throws in 10; IDM's scales and progressions darker (PHRY, HARM; the flat second).
  - **A filter envelope on every sound** (strongest on the bass and the arps, their envelope made a pluck), so each
    note opens and closes instead of sitting still.
  - **The filter TYPE changes**: LP mostly, BP on chords, arps and textures, the COMB tuned to the note on arps and
    textures; a slow LFO of its own shape (SIN, TRI, S&H for the IDM arps) on the cutoff; a little drive (SOFT, FOLD)
    on some basses and leads; the drums through a soft low-pass (the hats not glassy).
  - **Delay and reverb vary**: the delay time (1/4, 1/8, 8T, 16T, 1/16), the GRAIN delay pitched an octave or a fourth
    down or a fifth up (often on IDM), the reverb type among ROOM, SPRING, RESO and CLOUD, its size, modulation and
    width; each track its own space, now and then one soaked and one dry, chorus on some chords.
  - Measured over 90 throws: brightness 1714 Hz (0.6.31) → **722 Hz**, the same loudness.

### v0.6.32
- **DICE ALL goes dark.** "Out of 30 sequences one is OK": the song ideas were shrill, cheesy, too bright and happy.
  Now they are night music: **DARK MINIMAL**, **DUB TECHNO**, **IDM**, **DARK ELECTRO**, **ACID NIGHT**, **COLD WAVE**,
  **BROKEN**, **HYPNOTIC**, **DEEP BREAKS**, in minor modes only (MIN, DOR, PHRY, HARM, MEL, MPEN; no major,
  mixolydian, lydian or blues), no pop progressions.
  - **A harmony bug fixed**: the chords were built from the scale's fifth and the lead from its third, so a v chord
    sounded over a i bass. Every part now starts from the same tonic.
  - **Chords with colour**: voiced as 7ths, 9ths without the 5th, in-scale fourths or suspended, and led to the
    nearest voicing in a middle register (no leaps); dub stabs keep one voicing, offbeat after offbeat, into the delay.
  - **Filtered**: every synth's cutoff goes down by role (chords and arps darkest), with some resonance; a slow LFO
    makes the chords breathe, the envelope plucks the arps. Bright factory sounds (bells, brass, marimbas, organs,
    chiptune) are never picked.
  - **Melodies lower and stranger**: the lead around C3–C4 in small steps, or a hypnotic euclidean ostinato that turns
    on itself (Marc Houle, The Hacker), or a few long notes; arps an octave lower without octave jumps.
  - **Grooves**: a rolling offbeat sub and a dub bass, minimal and deep-techno beats with ghosts, rims and chance,
    velocities a little human; darker delay and reverb (lower tone, more damping, no SHIMMER, CLOUD pitched down).
  - Measured over 90 throws: the mix's brightness (its spectral centroid) **1714 Hz → 1376 Hz**, the average note
    **an octave lower** (65 → 55).

### v0.6.31
- **The full manual, in English and Spanish**: [docs/MANUAL.md](docs/MANUAL.md) · [docs/MANUAL.es.md](docs/MANUAL.es.md).
  Every page and its four knobs, every layer's keys, the engines, DRUM-X, the sequencer, ARRANGE, the dice and UNDO,
  effects, modulation, performance, the MENU, MIDI, the web tools and troubleshooting, and a careful chapter on
  **saving**: what is saved where, how the autosave really works (10 s after the last change while stopped, never at
  power-off), quick stores, copying, switching projects, names and user presets.
- **SELECT on the MIXER sets the tempo**, as on GLO's other pages (GLO's button opens it); it said DICE: HOME / EDIT.
- **New README images** and **presentation video v3**: the screens as they look now, taken from the video's footage
  (song ideas, ARRANGE, UNDO, TRACKS and LOAD, one scale), with the support links at the end.
- README and cheat sheets corrected against the firmware: the autosave, ARRANGE instead of the SONG page, what the
  punch-in lane records, UNDO on GLO's G4, HOME held for MENU, the reverbs' AIR knob, DRUM-X's labels.

### v0.6.30
- **LOAD, next to TRACKS** (GLO > SYSTEM: KNOB 1 **TRACKS 4 / 6**, KNOB 2 **LOAD ECO / LITE / FULL**; also MENU > SYSTEM >
  LOAD). TRACKS says how many tracks play, LOAD how heavy the sound may get, so you choose: a lean, ultra-optimised
  system to push to the limit, or a slow one full of heavy effects. Everything is swapped only while it sounds; the
  project keeps its settings and sounds as saved back on FULL (or on another unit).
  - **FULL** (the default): everything as set.
  - **LITE**: the heavy parts swapped for their nearest light kin: SHIMMER and CLOUD reverbs as ROOM, RESO as SPRING,
    the GRAIN delay as a plain delay, FM6 two voices.
  - **ECO**: LITE, and every reverb a light SPRING (0.6.70: a shorter chain), **no SLICER** (its tracks go straight
    to the mix) and **no chorus**: their pages disappear until you leave ECO. Every synth track plays at most
    **4 voices** (the drums as ever), **6** between all tracks (8 otherwise), and the load budget steps in sooner.
  - Since 0.6.71 the second core takes part of the synth work ([both cores](#both-cores-dual-core)): FULL goes much
    further before LOAD is needed.
  - The cards of what's swapped say **LITE** or **ECO** under their value.
  - Why CPU and not RAM: the firmware reserves all its memory at boot, so nothing can be freed while it runs. What runs
    out is the audio time (the load meter, the CPU % card next to LOAD), and that's what LOAD gives back.
- 0.6.29's **6 LITE** comes up as TRACKS 6 + LOAD LITE. GLO > SYSTEM's USB card is gone (the header shows USB).
- **Ko-fi / Cafecito**: support links in the README, the web installer, the web editor and the emulator.

### v0.6.29
- **TRACKS has a middle option: 6 LITE.** GLO > SYSTEM > TRACKS (and MENU > AUDIO > TRACKS) now reads **4 FULL** (the
  default), **6 LITE** or **6 FULL** (the old 6 HIGH). With 6 LITE all six tracks play and the heaviest parts are
  swapped for their nearest light kin while they sound: the **SHIMMER** and **CLOUD** reverbs play as **ROOM**,
  **RESO** as **SPRING**, the **GRAIN** delay (DLY 2 PITCH / SPRAY) as a plain delay, and **FM6** plays two voices
  instead of four. Nothing in the project changes: back on 6 FULL (or on another unit) it sounds as saved. The cards
  that are swapped say **LITE** under their value.
- **Fixed**: when a message covered the header while the BPM was rolling, the old BPM could stay half-rolled.

### v0.6.28
- **"After clearing a sequence it still triggers notes on all tracks" — fixed.** The punch-in FX lane (effects
  recorded by holding FX keys while REC is armed) kept repeating the last notes of the cleared steps with STUTTER and
  ARP, and ARP played random fills on a DRUM track with no steps. Now a clear ends those repeats and forgets the notes,
  a DRUM track with no steps gets no fill, and an effect **latched** with FX LATCH no longer records itself into the lane
  (only one held by hand does). A section jump or quick chain waiting for the next bar is called off by a clear (it
  brought the steps back). To erase the lane itself: hold REC and press FX.
- **Clears say which track**: "T3 CLEARED", and below it "SEQ+REC: ALL TRACKS" (the others play on; SEQ held + REC
  clears all six).
- **T5 and T6 asleep (TRACKS 4) can't go unnoticed**: when a note is sent to them (a key, MIDI, their steps) or PLAY
  starts with steps on them, the screen says T5-T6 ASLEEP · GLO>SYSTEM>TRACKS 6.

### v0.6.27
- **DICE ALL never repeats itself**: the moment you press (the timer's ticks) stirs the random numbers on every throw,
  and the last 32 pieces are remembered by a fingerprint (every track's steps, engines and the tempo): a piece that
  matched one would be thrown again. **The tempo moves on every throw** (at least 3 BPM, inside the style's range),
  unless MENU > BPM LOCK is on.

### v0.6.26
- **HOME + SELECT is a song-idea generator.** Each throw picks a **style** with its own tempo, swing, scales, chord
  progressions, drums, bass, how much silence it leaves, the engines it likes and its delay and reverb: **ROBOT POP**,
  **ELECTRO**, **BIG BEAT**, **ORGANIC**, **ART ROCK**, **DUB PUNK**, **MINIMAL**, **SYNTH POP**, **IDM** (the screen
  says which). Every track gets a **role** (DRUMS, BASS, CHORDS, LEAD, ARP, TEXTURE; with TRACKS 4 the first four), a
  **new engine** the style likes for it (FM6 on two tracks at most: its CPU), a factory sound whose name fits the role,
  moved a little, its voice mode, gate, level, pan and sends, and a part written from **one harmony**: the bass on the
  chords' roots (octaves, pumping, acid with slides, walking, drones, blips), chords voiced in the scale (7ths in some
  styles; held, stabbed, pulsed or sparse), the arp over the chord's notes with holes, the lead a motif repeated with a
  variation or answered, and whole bars left silent. ORGANIC, MINIMAL and IDM put loops of odd lengths against each
  other. Drums from each style's bars with fills, ghosts and accents; IDM's euclidean, ratcheted and by chance.
  No automation and no modulation beyond the macros' routes (nothing that takes RAM). UNDO (GLO + G4) as before.

### v0.6.25
- **Interface audit, second round.**
  - **SAVE > TOOLS is a list**: KNOB 1 (or PRESETS, or any knob) moves the pick down and up, OCT+ does it (its dialog
    first). It was one knob per action.
  - **The footer on the song's and the instrument's pages** (PROJECT, ARRANGE, TOOLS, GLOBAL, SYSTEM) shows the
    project and its name, not the track's engine and sound.
  - **Clearer messages**: SAVED (NO FLASH) instead of SAVED (RAM); an empty favourites list says NONE (was --/0).
  - **Layers**: on GLO, a track asleep (TRACKS 4) has its SOLO and MUTE dim and its key quiet.

### v0.6.24
- **Interface audit, first round.**
  - **The song in one place**: SAVE > ARRANGE (SEQ > SONG was the same page twice and is gone from SEQ).
  - **Readable names**: LEVEL, SWING, CLOCK, DEPTH, STEREO, FEEDBK, SPRAY, FOLLOW, PUNCH, TRACKS, ROUTE, SCALE, QUANT,
    TRANS, OFFSET, PITCH, FILTER, SHAPE, PHASE, ORDER, VOICE, GLIDE, DEST, DETUNE, NOISE, CRUSH, DRIFT, DECAY, COLOR,
    VOWEL, BREATH, MORPH, MOTION, DRIVE, ROTOR, RATIO... instead of LVL, SWG, CLK, CDP, STER, FDBK, SPRY, FOLW...
    Library files saved by the web editor with the old names load as before.
  - **Mixer (6 columns)**: each sound's name cut to fit (no "..."); a muted track keeps its name, in the accent colour.
  - **FX in sections** (TRACK, SLICER, DELAY, REVERB, CHORUS, MASTER): a tap the next page, two quick taps the next
    section; the footer counts within the section.

### v0.6.23
- **ARRANGE: delete a row right there**: KNOB 2 (SECT) turned left of A asks **DELETE ROW?** (OCT+ yes).

### v0.6.22
- **ARRANGE (SEQ > SONG) redrawn, after SLOOP's arranger**: the song as a column of rows, each a coloured section chip
  (A, B, C, D each its own heat), its bars as blocks that fill as they play, the count on the right and a dot for its
  scene; a section with nothing stored says EMPTY. Under them **+ ADD** (how many rows of the 16 are used) and **END**.
  On the right the whole song as one strip (each row as long as its share of the bars, the play head on it) and the bars
  in all. KNOB 1 the row, 2 its section (left of A: DELETE ROW?), 3 its bars, 4 its scene.
- **END: STOP, x2 .. x8 or LOOP**: the song plays once, that many times, or round and round (KNOB 2 on END).
- **Tracks asleep (TRACKS 4) in grey**: on HOME as a muted being, on the mixer greyed with OFF.

### v0.6.21
- **HOME + SELECT dices everything** (after **DICE EVERYTHING?**, OCT+ yes / OCT− no): one musical scale for every
  engine and the effects, each track a new engine, sound and sequence in that scale (T1 a bass; the others a lead, pad
  or arpeggio; DRUM a beat), the macros' routes and the delay, reverb and chorus. Tempo, levels and mutes stay.
- **SELECT by page**: the tempo on GLO's pages (GLOBAL, SYSTEM, MIXER), the sound's dice on EDIT only, nothing on the
  others (it says where).
- **UNDO: GLO held + G4**: the last big change taken back (DICE ALL, a sound diced, a clear, INIT, RECALL, a pattern or
  project loaded); again: forward. The transport keeps playing.
- **Installer**: a warning to return to official V15 first when coming from another custom firmware (a brick was
  reported going straight from one), with the button right there; Install is enabled once you confirm the FM-1 runs
  official V15 or JIANT.

### v0.6.20
- **SCALE is global, strictly**: ROOT, SCALE and QNT are one for the whole song. Before, a project, MIDI or
  automation could leave a track in its own scale; now every melodic track follows the selected one's at once.

### v0.6.19
- **TRACKS on GLO > SYSTEM** (KNOB 2: TRKS 4 LOW / 6 HIGH), next to the tracks: the same setting as MENU > AUDIO >
  TRACKS, saved. Going to 4, T5 and T6 now fade out (they were cut: a click).
- **Less stutter under load** (a full check of the last updates):
  - the overload guard sheds a voice again only after two overloaded blocks in a row (since 0.6.17 one spike, e.g. a
    key scan, was enough: a voice cut every 3 ms, clicks and cut tails);
  - the load budget is measured on the sound alone, and it only takes tails under -50 dB (was -30: audible);
  - a track let go from a black-key mute or a solo no longer keeps the FX layer working every block.
- **GRAIN delay without clicks**: with a long TIME, a grain's new spray moved the other grain in the middle of its
  window (a click every grain); and the FX buses' quiet gate now opens and reopens with a short fade (it jumped).
- **DRUM-X DRV**: several loud hits at once with DRV high could wrap into a full-scale click; held now.
- **LATCH**: turning HOLD off or ARP on with a key down, SEQ TOOLS CLEAR and REC CLEAR ALL now let the latched notes
  go (they kept droning and stacked up); a panic also ends them on MIDI OUT.
- MENU > ABOUT's AUDIO LOAD updates live.

### v0.6.18
- **MENU > AUDIO > TRACKS: 4 LOW (the default) or 6 HIGH.** With 4, tracks 5 and 6 sleep: they keep their sound,
  steps and mix but don't play or cost anything, and the whole CPU goes to T1-T4 (selecting T5 or T6 says so). With
  6, all six play and you know you're near the limit: watch the load meter. The setting is kept with the others.
- **Release tails ring out again with a sequence running.** Since 0.6.16 the load budget took the oldest released
  voice for every new note once the audio passed 65 %, so with the sequencer playing the tails were cut. Now it only
  takes a tail that is nearly gone (under -30 dB); otherwise the note is added and the tails ring. The guard at 80 %
  stays the last line.

### v0.6.17
- **DRUM-X at half the work**: each hit computed at 22 kHz (it is low-passed at ~8 kHz anyway) and brought back to
  full rate, its noise kept at the same level: the kit costs about 45 % less, the heaviest part of a busy song.
- **The guard acts sooner**: the load budget from 65 % (was 70), and over 80 % a voice fades after a single
  overloaded block (was two in a row).
- **MENU > ABOUT shows the audio's load** (scroll with PRESETS): NOW and PEAK (% of the time the audio took), LATE
  (blocks that came too late: each one a click) and SHED (voices the guard faded), since power-on. Send a photo of
  it after a click to tell what happened.

### v0.6.16
- **Stable under load: the load budget.** The device measures how much of its time the audio takes. Past 70 %, the
  sound may not grow: a new note takes a voice (the oldest released first) instead of adding one, and CLOUD spawns
  half its grains; under 60 % for a while, back to normal. The overload guard (80 %) stays as the last line. The
  load stops climbing before the screen lags or the sound clicks, whatever you turn on.
- **Mutes and solos cost nothing**: GLO's solo and the FX layer's black-key mutes were the sound silenced after
  being made; now they are MIDI mutes: the track gets no new notes and, once faded, its voices end.
- **FM6 plays 4 voices a track** (was 6): six operators a voice, the heaviest engine.
- **DRUM-X's DRV and knee on the kit's sum** (were each hit's): the same glue on the bus, half the work.

### v0.6.15
- **No hiss at rest with CLIP up**: the clipper rounded a tail's tiny negative residue down (-1) and drove it x4: a
  constant offset with a faint hiss on one side, even with every track off. Now rounded; and the FX buses (delay,
  chorus, reverbs) fade their own last residue to silence once nothing has been sent to them for a while (1.5 s at
  -55 dB, 4 s at -36 dB, 8 s at -30 dB), so the tails' rounding never leaves a hum. CLOUD's FREEZE is never gated.
- **CLOUD always dies out**: past SIZE ~80 its loop rang on for ever (its tail fed back into its own line); now only
  the grains feed it: a long tail that ends (SIZE 126 stays a faint wash for long, close to FREEZE).
- **More headroom**: DRUM-X stops computing a hit at -66 dB (was -84), FM6 ends a released note once its carriers
  are under ~-66 dB (its six operators no longer run for seconds on an inaudible tail), and the overload guard steps
  in at 80 % of the audio's time (was 85 %): it fades a voice before the sound clicks and the screen starves.

### v0.6.14
- **No more sharp highs when changing presets**: the notes still sounding used to take the new sound's values at once
  (its filter, its envelope amounts), jumping bright and loud. Now they fade out in ~3 ms with the sound they had
  (all of it, kept the moment before the change), and the new notes start clean. Also on user presets and sections.
- **SEQ TOOLS** (SEQ held): each cell shows its key with the octave (F3 CLEAR, F4 CLR AUTO: both read "F" before),
  and the footer shows **REC: CLEAR ALL** (all six tracks' steps and automation, OCT+ confirms), which was nowhere
  on screen.
- **Key LATCH**: with ARP OFF, **ARP 2 > HOLD ON** latches the keys: the notes keep sounding when you let go, the
  next chord replaces them, HOLD OFF lets them go.
- **CLOUD costs about a third less CPU**: its grains computed every other sample (the line they read is written at
  half rate anyway), the same sound. First step against the lag and clicks with six tracks busy.

### v0.6.13
- **AIR on SHIMMER and CLOUD** (REVERB's fourth knob, PRE on ROOM / SPRING): ±24 semitones. Down, the reverb goes lower
  and darker; up, higher and airier, in pitch and in filter at once. SHIMMER's loop climbs octave + AIR on each pass
  (an octave down to two up); CLOUD's grains move with it. 0 sounds as before.
- **CLOUD's graph** is a swarm of moving dots: AIR lifts or sinks the cloud, MOD splits it into octave layers, the
  grains playing now drawn bigger.
- **AUTOPAN's graph** fixed (SIN came out wrong) and drawn like LFO's: two cycles across, the playhead and a dot on
  where the pan is now, L up, R down.
- **CHORUS STER** moves its graph: the three taps fan apart with it, and fold into one line at mono.
- **SAVE > TOOLS** says what each knob does (CLEAR SEQ, INIT SOUND, DELETE ROW n, CLEAR SONG) instead of "TURN TO PICK";
  the page name in the footer is lit, so you always see where you are.
- **LOFI BYTE / FLOAT: RETRIG** (EDIT > RETRIG, after EDIT 2): **RTRG ON** starts the formula from the top on every
  note, instead of carrying on where it was.

### v0.6.12
- **Projects in SAVE**: SAVE > PROJECT now shows the project (1–8, each a whole song) and its four **sections A–D**,
  each with its own sequences and sounds for the six tracks. KNOB 1 opens another project (stopped; the music you are
  playing stays, ready to be stored into one of its sections), KNOB 2 or PRESETS picks the section, LOAD makes it the
  music, SAVE stores the music into it. Each section shows whether it's saved, **NOW** on the one playing and the bars
  it plays in the song; the arrangement sits below in a line (A4>B8>D4>B8).
- **ARRANGE**, right after PROJECT: the project's song, sections chained by bars as in SLOOP (the same rows as
  SEQ > SONG; OCT+ plays it). To copy a section, LOAD it and SAVE it into another letter.

### v0.6.11
- **FM6 voice bank**, from Felucca 1.5.1: a whole 32-voice DX7 cartridge (.syx) sent from the web editor (6-OP FM tab,
  **Send to the device's voice bank**), picked on the device with **SLOT B1–B32** after F1–F8 and OWN (empty slots
  skipped; the voice's name above the algorithm). Loading a voice copies it into the track, so projects and presets
  keep it whatever the bank holds later. Its own flash sector (0xE7000), written so a power cut leaves the old bank or
  the new one, never half; editor commands 74–77 as Felucca's. The full backup doesn't include it: keep the .syx.

### v0.6.10
- **FM6 fully editable on the device**, brought from Felucca 1.5.1: EDIT > **OPERATOR** (operator 1–6, RATIO / FIXED,
  FINE, LEVEL), **OP ENV** (each stage's RATE and LEVEL, drawn) and **OPERATOR 2** (MODE, DTUN, VEL), on an FM6 track
  only. The patch becomes the track's own (SLOT OWN), saved as always; no new format, a few bytes of RAM. An edit keeps
  the patch's volume trim (0.6.8), so the level doesn't jump while you turn.
- **Fix**: a card could keep a stale gauge colour or width (or lose its tail with a long label) until the page was
  redrawn; its cache now holds the whole card.

### v0.6.9
- **AUTOPAN** per track (FX > AUTOPAN, after DIST): DPTH, RATE, SYNC (BEAT: 4 BARS .. 1/32 in time with the bar;
  FREE: Hz) and WAVE (SIN TRI SAW SQR S&H), through the pan's glide so even SQR doesn't click; its own graph. It
  lives in the slots of the old per-track slicer (a bus since 0.6.3): no more memory, the project format unchanged,
  every old project and preset loads with it off.
- **CHORUS > STER**: the chorus's stereo amount, 0 as it always sounded, down to mono, up to about twice as wide
  (in a stored slot that was unused).
- Cheat sheets and manual updated.

### v0.6.8
Fixes from the first users' reports:
- **No more crackles turning knobs.** Measured on every engine, a knob turned fast against the same move made smooth:
  - **FILTER** (every engine but ANALOG): going in and out of the chain (CUT at the top with RES 0, the LP open) and
    changing TYPE now crossfade, the filter starting from the sound instead of a stale state. It clicked each time CUT
    or RES crossed the open point (FM6's CUT 17x the high-frequency energy, now as smooth as a slow turn).
  - **LEVEL and PAN** glide (~3 ms) instead of stepping at each knob move.
  - **DIST** comes in smoothly from 0: DST 1..8 blend the dry sound into the drive (at 1 it was the full drive, with
    its low cut, at once).
- **FM6: SysEx voices no longer distort.** DX7 voices with several carriers at full came out up to 5x louder than the
  factory patches, and their chords kept the master's limiter about 10 dB down (that was the glitchy, noisy sound). A
  voice above the level every voice is made for is now trimmed to it, per patch and automatically: its chords
  sit where the factory patches' do (the limiter about 2 dB, as theirs). Quieter voices and the 8 factory patches are
  untouched; nothing is ever boosted. Tested with 71 classic DX7 voices.
- **SEQ TOOLS: F4 CLR AUTO** clears the selected track's automation and parameter locks (every step), the steps stay.
  It breathes when there is automation to clear. Next to F3 CLEAR (the sequence).
- **Video v2** (`tools/video/`): every effect, DRUM-X's pages, the modulators, sequencers and automation, a beat from
  soft to industrial at 132 BPM, fast montage cuts; the captions come from the capture's own script.

### v0.6.7
- **Cheat sheets** in the README: controls, and sound · effects · modulation, as images and printable PDFs.
- **Page jump fix**: once you turn PRESETS with a page button held, that button's layer (its map) stays off until you
  let the button go, even if you keep holding past the HOLD time or the first turn came at the very moment you pressed
  it. Hold it again afterwards and the layer works as always.
- **Presentation video tools** (`tools/video/`): `capture.c` plays a 60 s demo song on the real engines in the host
  build and records the FM-1's screen and audio; `compose.py` makes a 1080p video from them in the project's look.

### v0.6.6
- **Jump straight to a page**: hold a page button (ENV, LFO, FX, SCL, EDIT, GLO, SAVE, ARP, SEQ) and turn **PRESETS**
  to go through that button's pages forwards or back (FX has 13, EDIT up to 16), the title on screen; let go and you
  stay on that page. It doesn't take anything away: with a button held, PRESETS did nothing before; a tap still goes to
  the next page, and once you used a layer's knobs or keys while holding, PRESETS stays out of it.

### v0.6.5
- **RESO (reverb) changes chords now.** On RESO, the REVERB page's PRE knob becomes **PITCH** (the strings' chord
  0–14 steps up the scale) and DAMP becomes **FOLW**: OFF, or T1–T6, the chord built on the lowest note that track
  plays, so the strings follow your bass line or your chords, gliding from one to the next (the chord stays when the
  track goes quiet). The pre-delay and the damping were barely used on RESO; ROOM, SPRING, SHIMMER and CLOUD keep them.

### v0.6.4
- **LOFI FLOAT: DRV is gone, FBK takes its place.** DRV had a scaling bug that silenced the engine as soon as it was
  turned up, and it duplicated the DIST on the FILTER page anyway. **FBK** (feedback) sends the output back into the
  formula's time: the floatbeat modulates itself like FM, from a slight shimmer to chaos, a different deformation on
  every ALGO. Projects saved before keep their values (FBK starts where DRV was).

### v0.6.3
- **The SLICER is now one slicer on a bus, like a sampler's chop.** Its settings (mode, pattern, rate, depth) are the
  section's, and on **SL BUS** / **SL BUS 2** you choose with seven ON/OFF switches what goes in: **T1–T6** and
  **FX** (the delay, reverb and chorus returns). Everything ON turns it into a **stutter sequencer for the whole mix,
  after the effects**: the reverb and delay tails get cut too. With FX OFF the tails ring free over the cut.
  The routed tracks enter after their DIST, filter, level and pan; their sends still go to the effects.
- **SL PITCH**: the repeats (STUT) follow a **16-step pitch sequence** (±24 semitones, with the mod sequencer's page
  layout: KNOB 1 the step, KNOB 2 its pitch, LEN, QNT). Played like a tape (higher = faster), the loop keeps the
  step's rhythm; **QNT SCL** snaps the steps to the song's scale. One sample turns into an arpeggio.
- No limit of tracks any more (STUT on four was the 0.6.2 compromise), the same memory as before (32 KB, now a
  stereo recording of 371 ms), and less CPU: one slicer instead of one per track.
- Each section of a song keeps its own SLICER settings. Projects from before load with their slicer settings turned
  into the bus's: the first track that had it on gives the mode, pattern, rate and depth, the tracks that had it on
  go in, FX stays out.

### v0.6.2
- **SLICER memory shared**: GATE works on all six tracks as before; **STUT** (the one that records and repeats) can
  run on **up to 4 tracks at once**. A fifth STUT track plays live without repeats and the screen says
  **STUT: 4 TRACKS MAX**; when one of the four leaves STUT, the waiting one takes its place on the next step. Each
  track keeps its own pattern, rate and depth, and its sends still follow the chopped sound. This frees 16 KB of
  the big-buffer memory (pool now at 92 %, 25 KB free) for future effects.

### v0.6.1
- **MIXER: the six tracks at once** in the JIANT style, as six thermal columns (number, sound, LEVEL as the heat
  column with the meter over it, its dB, PAN and REV as two bars underneath). The other styles keep four cards that
  follow the selected track.

### v0.6.0
- **Six tracks instead of four.** T5 starts on PHASE and T6 on WHEEL (any engine can go on any track, as always),
  each with its own sequence, sound, effects sends and automation. **ALGORITHM** scrolls T1..T6.
- **Same storage, everything kept**: still 8 songs × 4 sections plus the 4 project slots, and the song sequencer
  (rows of sections with repeats and scenes) works as before, so you can build whole songs with intro, verse,
  chorus and bridge. To fit six tracks in the same space, sections are now saved **compressed**: only the steps
  that have something, and only the parameters you changed, are stored. The music at power-on takes about a
  quarter of a section; even six tracks with 4-note chords on all 64 steps fit.
- **A fill warning**: a moment after you stop editing, if the music is near the size of a section the screen says
  **SECTION 90% FULL**; if it no longer fits, **TOO FULL TO SAVE**. Saving a section in that state says
  **SECTION FULL** and changes nothing: the section already saved stays as it was. All memory is fixed at build
  time (nothing is allocated while you play), so a full section can never crash the system.
- **Projects and sections from 0.5 load as they were**: their four tracks keep their sound and steps, T5 and T6
  start empty on their default sounds.
- **GLO** (held): black keys 1–6 mute T1–T6, 7–10 the DRUM groups (KICK, SNARE, HAT, PERC); white keys F3–D4 solo
  T1–T6 while held, E4 unmutes everything, F4 tap tempo. The four knobs are T1–T4's levels, or T3–T6's when T5 or
  T6 is selected. **FX** (held): black keys 1–6 mute T1–T6.
- **MIXER** shows four tracks at a time and follows the selected one (T5 or T6 selected: T3–T6). (0.6.1: all six in the JIANT style.)
- **HOME**: six beings in the ecosystem, a bit smaller.
- **Song scenes** remember the mutes of T5 and T6 too. **Automation** (knobs and per-step locks) works on all six.
- **MIDI IN**: ROUT **CH1-6** (was CH1-4): channels 1–6 play tracks 1–6, 7–16 stay free for other instruments.
- Web editor: six tracks in the track selector, the mixer and automation.

### v0.5.5
- **FLOAT is a floatbeat generator now**: bytebeat's sibling in floats. Each of the 32 ALGOs is a formula of `t`
  made of sines, as in the floatbeat scene: `sin(t·(t>>11 ^ t>>13 & 7))`, FM whose index walks with `t`'s bits,
  melodies read from nibble tables, kicks, snares and hats inside the formula, Sierpinski partials, drive and folds,
  staircase sines, xor'd and dropped bits, risers, polyrhythms, chords whose root walks… rhythm, melody and timbre at
  once. `t` runs 32 steps per cycle of the note, so every formula plays in the key you hold (and, as in bytebeat, its
  rhythm runs faster up the keyboard). It shares BYTE's controls: **VAR** mutates `t` (8 ways), **MOTN** runs its
  slow bits faster or slower, **BEND** folds it, **LOOP** locks it into a short window; the new **DRV** folds the
  output from clean to wild.

### v0.5.4
- **FLOAT makes patterns now, not just waveforms.** Every ALGO is a step sequencer: 8 patterns (an arpeggio, a
  melody, a Sierpinski rhythm, a 5-in-8 euclid, a random walk, a `t>>9 % 13` line, a drone with 3-against-4 accents,
  octave leaps) × 4 smooth timbres (sine, FM that darkens as each note falls, wavefold, hard sync sweeping down).
  The notes are harmonics of the key you play (just intonation), the steps are the song's 1/16 notes.
- **New knob, MOTN (motion)**, on BYTE and FLOAT, in the slot that was empty: the speed of the formula's time. In
  FLOAT it goes from one step per bar to steps so fast the pattern turns into a timbre; in BYTE it speeds up or slows
  down the formula's slow bits (its rhythm and melody) without changing the pitch. 64 is as written.
- **VAR is drastic now**, on every BYTE and FLOAT formula: it mutates the formula's time in 8 different ways (stairs
  that transpose as they go, xor and or with itself, holes in the rhythm, the phrase backwards, multiplied,
  scrambled), so each formula gives many timbres and sequences.
- **The second LOFI page cleaned up**: BYTE shows MOTN · GRIT · BEND · LOOP (GRIT holds samples longer, a rate
  crusher), FLOAT MOTN · DCY · BEND · LOOP (DCY: from short plucks to legato). The second RES (already on the FILTER
  page) and the empty slot are gone. Projects saved before keep their sound (MOTN set to 64, GRIT/DCY to 0).

### v0.5.3
- **A new power-on screen**, after feedback from the community: the signature used to be drawn as straight segments
  (it looked like MS Paint next to the rest of the interface). Now each stroke is smoothed and drawn as a curve with a
  round pen that tapers at the ends, like ink; the pen's tip is a glowing nucleus; and three small beings (a
  radiolarian, an amoeba, a hydra) swim in from outside the screen and circle the signature while it is written,
  warming up as it grows and pulsing once it is done. It costs 16 bytes of RAM, and the splash now runs on the clock
  (drawing the beings does not make it longer). MENU > ANIM OFF still shows it whole at once.

### v0.5.2
- **BYTE: new formulas, no silent starts.** The 17 most familiar one-liners (`t*(42&t>>10)`, `t&t>>8` and friends)
  were replaced by less obvious ones: rhythms and arpeggios after Tejeez, xpansive's "lost in space", a melody of
  `t>>9` modulo 13, textures built from `t%24` and `t%19`, and more. Each formula now starts where it is already
  moving: 14 of the 32 used to sit silent for up to half a second after the note; now all 32 sound within 30 ms.
- **FLOAT: 32 formulas** (16 new): drifting unison, sub-and-fifth organ, self-FM, glides through partials, gated
  rhythms, CZ-style phase distortion, formants, breath, harmonic chords, breathing bells, stepped sines, PWM, random
  held partials, hard sync, three-against-four. **CHIP now sets FLOAT's resolution**: FULL (smooth, the default),
  4BIT, 4B/2, 1BIT, STEP.
- **LOFI presets in order**: browsing shows the BYTE presets, then FLOAT, then the classic chip sounds.
- **Parameter check**: NOISE's resonance is now **PEAK** (it sat next to the FILTER page's RES with the same name).
- **Lighter audio interrupt**: 0.5.1's X-MOD LFO had been compiled into the audio interrupt's inner path with its
  divisions (its cost estimate almost tripled); it now runs once per block, outside it, and the interrupt is back to
  its 0.5 budget.

### v0.5.1
- **Master no longer smears the mix.** The always-on leveler was riding every hit: measured on the demo song, its
  gain swung about 5 dB inside each beat and the limiter was working 95 % of the time. Now it follows the song's
  loudness over about a second and a half (0.3 dB of movement per beat) and leaves the limiter more headroom (active
  less than half as often, and lighter). Same loudness, transients and space come back.
- **CLOUD is bigger**: 12 overlapping grains instead of 8, each one a few cents off (a chorused shimmer), the grains
  fed back into the cloud so it keeps building, and a louder bloom.
- **RESO rings much longer**: the top of SIZE now sustains for many seconds.
- **Chorus rebuilt as an ensemble**: three voices swept a third of a cycle apart, left, right and centre: stereo
  even with WIDTH at 0, wider with it.
- **New LOFI wave: FLOAT** (floatbeat, bytebeat's smooth sibling): 16 formulas made of sines (FM, harmonic
  arpeggios, folded sines, beating pairs, plucks, pads), tuned to the note; VAR, BEND, RES and LOOP as in BYTE.
  Three new presets: FLOAT FM, FLOAT ARP, FLOAT PAD.
- **New DRUM-X page: X-MOD**, a modulator for the whole kit: a tempo-synced LFO on MORPH (RATE from 4 bars to
  1/32, DPTH, SHPE: sine, triangle, saw, ramp, sample & hold) and RAND (every hit its own MORPH and tune). Saved
  with the kit; the A–B bar shows the morph moving.

### v0.5
First public release.



