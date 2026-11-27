JIANT FM {version} for the Akai MPC Sample
==========================================

JIANT FM V10.8.48, the FM-1's public firmware, as the MPC Sample's whole
instrument: six tracks, fourteen engines (the FM-1's twelve: DRUM-X, ANALOG,
PHASE, FM6, VOICE, ANIMAL, MATH, WHEEL, WEST, STRING, ADD, EXT; and here
SAMPLE and GRAIN), the sequencer, songs, the generator and the effects, on
JIANT's own screen. Every track has its own 8 voices on the MPC's CPU. It
runs in place of the stock app, started from the microSD card. Nothing on
the MPC itself is changed, only files on the card.

THIS IS A PREVIEW, NOT YET TRIED ON AN MPC SAMPLE: TESTERS WELCOME. A crash,
or any failure to start, puts the stock MPC app back. Back up the card
first anyway.


Requirements
------------
- MPC Sample on firmware 1.3.0.12 exactly. On any other firmware JIANT does
  not start and the stock app keeps running.
- A microSD card for the MPC.


Install
-------
1. Back up the card. If it already has a folder named "autorun" at its top
   level (Melodee, SampQOL or another app use one), move it somewhere safe:
   this one replaces it. (SampleBoot can keep several: see its page.)
2. Copy the "autorun" folder from this zip to the top level of the card.
   Do not edit or rename anything inside it: the MPC refuses an autorun
   whose files changed.
3. Put the card in the MPC and switch it on. The stock app starts first; a
   few seconds later JIANT takes over the screen.


The MPC as the FM-1's panel
---------------------------
- Pads 1-16: 16 of the FM-1's 27 keys. PAD BANK moves them: A F3-G#4,
  B A3-C5, C C#4-E5, D E4-G5 (SHIFT + PAD BANK back). The bank lights and
  the left of the screen show which.
- The three buttons above the screen: HOME, EDIT, GLO.
- PAD FX: FX. KNOB FX: LFO. CHOP: ENV. 16 LEVELS: SCL. LOOP: ARP. SEQ: SEQ.
  SAMPLE SELECT: SAVE.
- PLAY: PLAY. STOP: stops it. SEQ REC or SAMPLE REC: REC.
  MINUS / PLUS: OCT - / OCT +.
- The wheel: SELECT. SHIFT + wheel: PRESETS.
- K1, K2, K3 and the fader: KNOB 1 to 4. SHIFT + K1: ALGORITHM.
- MAIN VOLUME: MASTER.
- MIDI in from the DIN and USB ports: channels 1-6 play tracks 1-6, and
  MIDI clock follows GLO > CLOCK.
- The pages are the FM-1's (its manual: docs/MANUAL.md in the source);
  the CPU the MPC spends is shown at the right of the screen.
- Push the wheel: back to the stock app (until the next power-on).
- SAMPLE: the SAMPLE BROWSER (below).


Samples
-------
JIANT here has 16 sample slots (up to 60 s each) played by the SAMPLE
engine (one shot: ONE GATE LOOP PING REV SLICE SCRUB TAPE, triggers, a
feedback comb, DESTROY, REC), the GRAIN engine (granular, on a slot or LIVE
on the input or JIANT's own output) and a sample layer on every DRUM-X sound
(SMORPH, its page SAMPLES). Find them in PRESETS after EXT; their extra
pages are in EDIT's SAMPLE group. On an FM-1 a project with them plays
SAMPLE and GRAIN as ANALOG, and DRUM-X as its synthesized kit.

Put WAV files (16 or 24-bit, or 32-bit float; mono or stereo; any sample
rate) in JIANT/Samples on the card. JIANT/Samples/slots.json says which
file each slot holds; the browser writes it. A slot JIANT changed itself (a
recording, DESTROY) is written as JIANT/Samples/slot01.wav .. slot16.wav.

The SAMPLE BROWSER (SAMPLE opens it; the pads and these buttons are its own
while it is open):
- Pads 1-16: the slot (dim: holds a sample; bright: picked).
- The wheel: the files in JIANT/Samples.
- The buttons above the screen: CLOSE, AUDITION (again: stop), LOAD (the
  file into the slot). SAMPLE closes it too.
- ERASE: clears the slot.
- SAMPLE SELECT: what to record: LINE, MIC, USB, RESAMPLE (JIANT's output).
- SAMPLE REC: records into the slot from now; again: stop (60 s at most).


Your music
----------
JIANT keeps its flash, everything SAVE stores (songs, projects, user
presets, settings), on the card in JIANT/flash.img, written a second after
it changes and when you leave. As on the FM-1, a song GEN makes is a draft
until SAVE > PROJECT saves it; leaving drops an unsaved draft. The samples are the files in
JIANT/Samples. As on the FM-1, the music on the screen comes back at
power-on only with MENU > SYSTEM > RESTORE LAST ON.


Back to the stock app
---------------------
- Push the wheel.
- To stop JIANT starting at all, delete the "autorun" folder from the card
  (and put back any autorun folder you moved away). JIANT/flash.img stays.


Version and source
------------------
JIANT FM {version}, source commit {commit}
Source code: {source} (ports/mpc-sample)

JIANT FM is free software under the GNU General Public License, version 3
(licenses/LICENSE-GPL-3.0.txt), a fork of Felucca by Hugelton Instruments.
It runs on sample-base by keremimo, under the GNU Affero General Public
License, version 3 (licenses/LICENSE-AGPL-3.0.txt), with JUCE under the
AGPLv3: this program as a whole is conveyed under the AGPLv3 (GPLv3
section 13). Not affiliated with or endorsed by inMusic, Akai Professional
or M-VAVE.
