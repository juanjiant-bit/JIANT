# JIANT FM on the Akai MPC Sample

JIANT FM as the MPC Sample's whole instrument, independent of any other app: the firmware's core
([`ports/core`](../core)) at the MPC's 44.1 kHz in 128-frame blocks (the FM-1's own block), JIANT's 240 x 240 screen in
the middle of the MPC's 320 x 240, the MPC's pads, buttons and knobs as the FM-1's panel, MIDI in from the DIN and USB
ports, and the device's flash kept on the card. It runs in place of the stock app, started from the microSD card, on
[sample-base](https://github.com/keremimo/sample-base) by keremimo (the runtime Melodee uses: the hardware, the
threads, the SD boot and the way back to the stock app).

| | |
| --- | --- |
| [`src/jiant/jiant_app.cpp`](src/jiant/jiant_app.cpp) | the app: audio, screen, lights, MIDI, the flash on the card |
| [`src/jiant/panel_map.h`](src/jiant/panel_map.h) | the MPC's surface as the FM-1's panel (the table is at its top) |
| [`src/jiant/sample_browser.h`](src/jiant/sample_browser.h) | the SAMPLE BROWSER's controls (the table is at its top) |
| [`src/jiant/sample_files.cpp`](src/jiant/sample_files.cpp) | the slots' files: WAV in and out, `slots.json` |
| [`src/main.cpp`](src/main.cpp) | the runtime, and the tools: `--self-test`, `--preview`, `--persist-write` / `--persist-read`, `--samples-write` / `--samples-read` |
| [`release/README.txt`](release/README.txt) | what the user reads: install, the controls, where the music is kept |

## Build

```sh
ports/mpc-sample/build.sh test      # here (Linux): the app, its tests, build/preview.png
ports/mpc-sample/build.sh docker    # the MPC's ARM64 build in sample-base's image (Docker or Podman)
```

`build.sh` fetches sample-base at a pinned commit into `build/sample-base` and JUCE into `build/deps`. On an ARM64
Linux (or an ARM64 chroot under qemu-user) `build.sh` alone makes the MPC's binary; it must run on glibc 2.36 or older
(Debian bookworm, Ubuntu 22.04).

The tests: the control map and the SAMPLE BROWSER's (`tests/panel_map_test.cpp`), a note through the whole firmware
(`--self-test`, which the SD boot also runs before it starts JIANT), the screen (`--preview`), the music saved on the
card coming back in a new process (`--persist-write`, `--persist-read`), and the samples (`--samples-write`,
`--samples-read`: slots loaded from a 24-bit 48 kHz stereo and a float 22.05 kHz mono WAV through `slots.json`, a bar of
JIANT's output recorded into slot 5 and written as `slot05.wav`, found again sample for sample by a new process; the
browser drawn, `build/samples/browser.png`). The engines themselves are tested in the core (`ports/core/build.sh test`).

## Samples: SAMPLE, GRAIN and DRUM-X's sample layer

On the big CPUs (this app, the MPC plugin, the desktop plugins: `FELUCCA_HOST_POWER`, [`ports/core`](../core)) JIANT has
16 sample slots of up to 60 s each (mono or stereo, 44.1 kHz) and plays them three ways: the **SAMPLE** engine (one shot,
experimental), the **GRAIN** engine (granular) and a sample layer on every sound of **DRUM-X**. On the FM-1 none of this
exists: a project made here opens there with SAMPLE and GRAIN tracks as ANALOG and DRUM-X as its synthesized kit.

### The card

`JIANT/Samples` holds the WAVs (16 or 24-bit PCM, 32-bit float; mono or stereo; any rate: resampled to 44.1 kHz when
loaded) and `slots.json`, which names the file of each slot (`{"1": "kick.wav", "5": "slot05.wav"}`). The app loads them
at start. A slot JIANT changed itself (a recording, DESTROY) is written back as `Samples/slotNN.wav` a second after it
stops changing, and `slots.json` then names it.

### The SAMPLE BROWSER

**SAMPLE** opens it over the screen (the app owns the pads and these buttons while it is open):

| Control | In the browser |
| --- | --- |
| pads 1..16 | pick the slot (dim: it holds a sample; bright: the one picked) |
| wheel | scroll the files of `JIANT/Samples` |
| left button above the screen | CLOSE (or SAMPLE again) |
| middle button above the screen | AUDITION the file (again: stop) |
| right button above the screen | LOAD the file into the slot |
| ERASE | CLEAR the slot |
| SAMPLE SELECT | the source to record: LINE, MIC, USB, RESAMPLE (JIANT's own output) |
| SAMPLE REC | record the source into the slot from now (again: stop; 60 s at most) |

The screen shows the slot (its name, length, mono / stereo, or EMPTY), the files, the source and RECORDING with its
seconds. LINE and MIC set the MPC's input route; USB records what the input gives without a route of its own (USB-C
audio in is not wired yet).

### SAMPLE (one shot, experimental)

Pages: SAMPLE and WARP (EDIT 1 and 2), then TRIG and REC (EDIT's SAMPLE group). The picture shows the slot's name, its
waveform with START..END lit, the slices, the playheads, and RECORDING.

| Page | Control | Range (default) | What it does |
| --- | --- | --- | --- |
| SAMPLE | SLOT | 1..16 (1) | the slot played; its label reads EMPTY when the slot is empty (silent) |
| | START | 0..100 % (0) | where it starts |
| | LENGTH | 0..100 % of what follows START (100) | where it ends |
| | MODE | ONE GATE LOOP PING REV SLICE SCRUB TAPE (ONE) | see below |
| WARP | TUNE | -24..+24 st (0) | on the key's pitch: C4 plays the original speed |
| | FEEDBK | 0..100 % (0) | a comb tuned to the note inside the voice; near the top it rings by itself, soft-limited |
| | TONE | 0..100 % (50) | the comb's damping: dark .. bright |
| | DSTROY | 0..100 % (0) | the voice's output written back into the slot's working copy where it reads: each hit wears it. The loaded original is never touched; reloading the file brings it back |
| TRIG | TRIG | NORM CHOKE RATCH RAND ROUND VEL LEGAT CHNCE (NORM) | see below |
| | AMOUNT | 0..100 % (50); its label by TRIG | COUNT (RATCH: 2..16 hits), RANGE (RAND, VEL), SLOTS (ROUND: 1..16), CHANCE |
| | RATE | 1/4 1/8 1/8T 1/16 16T 1/32 (1/16) | RATCH: the hits' distance at the tempo |
| | SLICES | 2..64 (16) | SLICE: the region in so many equal slices |
| REC | REC | OFF, then IN / MST / T1..T6 x 1 2 4 8 bars or H (OFF) | arms a recording into SLOT: the device's input, JIANT's master or a track, from the next bar (at once when stopped), for so many bars (H: until set to OFF); it shows ARMED, then REC, and goes back to OFF when done |
| | WOW | 0..100 % (31) | TAPE: wow and flutter; SCRUB: SMOOTH, how slowly the playhead slides |
| | STOP | 0..100 % (50) | TAPE: the slow stop's time after the release (50 ms .. 4 s) |
| | DECAY | 0..100 % (50) | RATCH: how much each hit falls |

MODE: **ONE** plays START..END once, the release ignored. **GATE** stops on the release (the ADSR's). **LOOP** loops
the region while held, **PING** forth and back. **REV** plays it backwards once. **SLICE** cuts the region into SLICES:
C4 plays slice 1, C#4 slice 2 and so on, each at its own speed (TUNE on it). **SCRUB**: the velocity (and the part's
SHAPE: the LFO, the envelope, the matrix) sends the playhead; it plays a short loop where it stands and slides, as a
scratch, to where it is sent. **TAPE**: wow and flutter, and on the release a slow stop down to silence.

TRIG: **NORM** polyphonic. **CHOKE** one voice, each hit restarts. **RATCH** repeats the hit at RATE, COUNT times,
each DECAY lower. **RAND** a random start (SLICE: a random slice) within AMOUNT of the region. **ROUND** the next of
AMOUNT slots from SLOT at each hit (round robin). **VEL** the velocity picks the start (SLICE: the slice). **LEGAT** a new
note goes on from where the last one was. **CHNCE** each hit plays with AMOUNT's probability.

Presets: ONE SHOT, GATE, LOOP, PING PONG, REVERSE, SLICE 16, SLICE 8, RANDOM SLC, VEL SLICE, SCRUB, TAPE STOP, RATCHET,
CHOKE, ROUND ROBIN, LEGATO, CHANCE, FEEDBACK, METAL COMB, DESTROY, RESAMPLE.

### GRAIN (granular)

Each voice runs its own cloud of up to 48 grains; the key sets their pitch (C4 the original), WIDTH spreads them in
stereo. Pages: GRAIN and CLOUD, then TRAVEL, SOURCE and PITCH. The picture shows the waveform (or the last seconds of the
LIVE input), the region, the playheads and the grains.

| Page | Control | Range (default) | What it does |
| --- | --- | --- | --- |
| GRAIN | SLOT | 1..16 (1) | the slot (EMPTY: silent); LIVE sources ignore it |
| | POS | 0..100 % (0) | where the playhead's region starts (the part's SHAPE moves it); LIVE: AGO, how far back (50 ms .. 5.5 s) |
| | SCAN | -64..+63 (0) | the playhead's speed, 0 frozen; STRETCH: the factor (0.1x .. 1x .. 4x); RAND: JUMPS, how often |
| | SIZE | 0..100 % (50) | a grain's length, 5 ms .. 500 ms |
| CLOUD | DENSE | 0..100 % (55) | grains a second, 1 .. 200, overlapping |
| | SPRAY | 0..100 % (8) | random position jitter, up to +-0.5 s |
| | SPREAD | 0..100 % (0) | random detune of each grain, up to +-12 st (QUANT: octaves or fifths) |
| | REVRS | 0..100 % (0) | the share of grains played backwards |
| TRAVEL | TRAVEL | FWD REV PING RAND STRCH FREEZ KEY (FWD) | the playhead in its region: forward or backward at SCAN, forth and back, random jumps, time-stretch (real time x SCAN's factor, the pitch the key's), frozen at POS, or the note picks the place (C2 .. C6 across the region) |
| | REGION | 0..100 % of what follows POS (100) | the region's length |
| | FEEDBK | 0..100 % (0) | the cloud's output fed back as the grains' input with the slot (1 s, soft-limited) |
| | SHIMMR | OFF +OCT +5TH -OCT +2OCT (OFF) | the fed-back grains shifted: shimmer |
| SOURCE | WINDOW | HANN TRI FLAT PERC SWELL (HANN) | the grain's shape |
| | WIDTH | 0..100 % (50) | the stereo spread |
| | SOURCE | SLOT IN MST (SLOT) | the slot, or LIVE: the device's input or JIANT's master as it plays (a granular delay; FREEZ holds it) |
| | REC | as SAMPLE's | records into SLOT |
| PITCH | QUANT | FREE OCT 5TH (FREE) | SPREAD's detune free, in octaves, or in fifths and octaves |
| | DAMP | 0..100 % (100) | the feedback's tone |

Presets: CLOUD, FROZEN CLOUD, SCAN, STRETCH 0.5X, STRETCH 2X, SHIMMER, SPRAY, OCTAVES, REVERSE, GLITCH, KEY POSITION,
LIVE DELAY, LIVE FREEZE, FEEDBACK.

### DRUM-X's sample layer

On a DRUM track, EDIT's SAMPLE group (pages SAMPLES and SAMPLES 2). With SMORPH at 0 (every preset) DRUM-X is exactly
the FM-1's.

| Control | Range (default) | What it does |
| --- | --- | --- |
| SMORPH | 0..100 % (0) | each hit from its synthesized sound (0) to its sample (100) |
| BLEND | XFADE S>B B>S FOLW RING (XFADE) | a crossfade; the synth's transient on the sample's body; the sample's transient on the synth's body; the sample following the synth's amplitude; one ringing the other |
| SLOT | 1..16 (1) | the first slot |
| MAP | SLOTS LANES KEYS (SLOTS) | SLOTS: each sound its slot (kick SLOT, snare SLOT + 1 .. bell SLOT + 7; a hit at velocity 100 or more plays SLOT + 8 + its lane when that holds a sample: the velocity layer). LANES: SLOT cut in 8, each sound its slice. KEYS: SLOT cut in 16, GM note 36 + n its slice n (a sliced kit) |
| SLEVEL | 0..100 % (100) | the sample's level |

The kit's TUNE and each sound's GM pitch tune its sample, DECAY below its middle shortens it, DRIVE and the drum bus shape
the sum as ever, and a closed hat chokes the open one's sample as its synth. CHANCE, the lanes' ratchets and TOUCH are the
sequencer's, as for the synth.


## Release

```sh
python3 ports/mpc-sample/tools/make_release.py v10.7.64-mpc1 --binary <the ARM64 build>
```

writes `ports/mpc-sample/dist/JIANT-FM-MPC-Sample-<version>.zip`: an `autorun` folder for the top of the card (signed as
the MPC wants it), the README and the licences.

## Licences

JIANT is GPL-3.0-only; sample-base and JUCE are AGPL-3.0. The MPC Sample program combines them, as GPLv3 section 13
allows, and is conveyed as a whole under the AGPLv3; JIANT's own files keep the GPL.
