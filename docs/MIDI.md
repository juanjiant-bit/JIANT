# JIANT FM and MIDI: a practical guide

[Español](MIDI.es.md) · Firmware **V10.8.72** · Everything here was checked against the V10.8.71 source
(`firmware/src/midi_control.c`, `midi_learn.c`, `midi_uart.c`, `midi_clock.c`, `usb.c`, `usb_host.c`, `seq.c`,
`menu_items.c`, `params.c`); V10.8.72 changes nothing here. Where something could not be checked in the code it says **unverified**.

The full reference stays in the manual, [chapter 14, MIDI and USB](MANUAL.md#14-midi-and-usb). This page is about
getting things done:

1. [Play JIANT from an external MIDI keyboard](#3-play-jiant-from-a-keyboard)
2. [Modulate JIANT with CCs](#4-modulate-jiant-with-ccs)
3. [Sync and control JIANT from other gear, or control the other gear from JIANT](#5-clock-and-sync)
4. [A worked recipe: Novation Circuit Tracks, an h2midi box and TRS-A cables](#7-recipe-circuit-tracks-h2midi-and-trs-a-cables)

---

## 1. The ways in (and out)

| Connection | MIDI in | MIDI out | Clock in (GLO › GLOBAL › CLOCK) | SysEx (editor, updates) |
| --- | --- | --- | --- | --- |
| **USB, the FM-1 as a device** (to a computer, a phone, a USB host box) | yes | yes (MENU › CONTROL › MIDI OUT) | **USB +BT** | yes |
| **USB HOST** (MENU › SYSTEM › USB › HOST: a keyboard plugged straight into the FM-1) | yes | **no** | **USB +BT** | no |
| **TRS jack** (3.5 mm) | yes | **no**: the jack is an input only | **TRS** | no (ignored) |
| **Bluetooth MIDI** (MENU › SYSTEM › BLUETOOTH ON) | yes | yes (what MIDI OUT lets out) | **USB +BT** | no |

- **All the inputs are always on and merged.** There is no "MIDI IN source" setting: notes and CCs from USB, USB
  HOST, the TRS jack and Bluetooth all play the FM-1 at the same time, through the same routing. (The firmware still
  has an old `MIDI` USB / TRS global, but nothing reads it and no page shows it.) The only per-input choice is
  **which input's clock** the FM-1 follows (section 5).
- **USB device.** The FM-1 is class compliant, named **"FM-1"** (USB MIDI and USB audio in one device). Plug it into
  a computer or anything that hosts class-compliant USB MIDI. This is also the only way to use the web editor and the
  installer.
- **USB HOST** (V10.41, experimental): MENU › SYSTEM › **USB** › **HOST**, then close the MENU. The FM-1 becomes the
  host of a class-compliant USB-MIDI keyboard or controller, alone or behind a hub.
  - **Power**: the FM-1 does **not** power the port. The keyboard needs its own power supply or battery, or put a
    **powered USB hub** between them.
  - Low-speed USB devices are not supported (the screen says LOW SPEED: NOT SUPPORTED).
  - (V10.8.69) More keyboards work: an output-only MIDI interface listed before the input one, an interrupt input
    with interval 0 and any packet size from 4 to 64 bytes are accepted, and a keyboard is tried three times (a bus
    reset each, a longer pause each) before the screen says NOT USB MIDI.
  - Input only: nothing is sent to the keyboard. While HOST is on there is no USB link to a computer (no editor, no
    MIDI to the computer). HOST is never saved: the FM-1 always powers on as a USB device.
  - More: [USB HOST in the manual](MANUAL.md#usb-host-a-keyboard-or-an-audio-interface-straight-in-v1041-experimental).
- **Bluetooth MIDI**: MENU › SYSTEM › **BLUETOOTH ON**; it advertises as **JIANT FM-1** (the row shows ADV, then
  CONN). Notes, CCs, program change, bend and clock work as over USB. More:
  [Bluetooth MIDI and PROX](MANUAL.md#bluetooth-midi-and-prox-v1013v1020).

## 2. The TRS jack: type A or type B?

The FM-1's 3.5 mm jack receives MIDI at 31250 baud on one input pin (`hal/fm1_uart.h`: PH8 → UART1 RX). **The code
cannot tell you whether the jack is wired type A or type B**: that is decided by the FM-1's hardware (which ring of
the plug reaches the opto-isolator), not by the firmware. So: **unverified**.

How to find out with what you have:

1. On the FM-1 set **GLO › SYSTEM › ROUTE SEL** (every channel plays the selected track), so the channel cannot be
   the problem.
2. Connect the sender's MIDI OUT with a **TRS type A** cable (or the type A adapter the sender came with) and play
   notes.
3. If the selected track plays: the FM-1 jack works with type A. If nothing happens, try a **type B** cable or a
   **type A ↔ B converter**. Swapping tip and ring normally just gives silence, it does not damage anything, but
   check your sender's manual to be safe.
4. M-VAVE's own documentation for the FM-1, or the adapter that came in its box, may also say which type it is.

The FM-1 has no MIDI activity light for the TRS jack in V10.8.72 (the old GLO › SYSTEM 2 status page was removed in
V10.8.38), so the test is simply: does a track sound? (With a computer on the USB serial console, the counters
`uart_rx_bytes`, `uart_rx_msgs` and `uart_rx_drops` show what arrives.)

## 3. Play JIANT from a keyboard

### Which channel plays which track: ROUTE

**GLO › SYSTEM, KNOB 3: ROUTE**

| ROUTE | What happens |
| --- | --- |
| **CH1-6** (default) | Channel 1 plays track 1 … channel 6 plays track 6. **Channels 7–16 are ignored completely** (notes, CCs, bend, aftertouch, panic), so they stay free for other instruments. Two exceptions on channel 16: its PROGRAM CHANGE (sections and projects) and its NRPN (see 4.3). |
| **SEL** | **Every** channel plays the **selected track** (ALGORITHM picks it). The easiest setting for one keyboard. |

- Tracks 5 and 6 only play with **MENU › AUDIO › TRACKS 6** (with 4 TRACKS, T5 and T6 get no notes).
- Changing ROUTE from SEL to CH1-6 lets go of anything channels 7–16 were holding, so no note hangs.
- **KEY SPLIT** (GLO › KEY SPLIT and KEY SPLIT 2) also splits **one MIDI channel** over two tracks: **MIDI CH**
  (1–16) and **MIDI CUT** (the split note, 60 by default) on KEY SPLIT 2, LOW TRK / HIGH TRK and LOW OCT / HIGH OCT
  as for the keys. The split channel is heard whatever ROUTE says.

### What a keyboard can send

| Message | What JIANT does with it |
| --- | --- |
| **Note on / off** | Plays the track the channel routes to (above). Velocity is used as sent (the sound's own VEL amount decides how much it matters). A note-on with velocity 0 is a note-off. On an armed track, playing, incoming notes are **recorded** like the panel's keys. On a DRUM track the notes pick the kit's sounds (see [DRUM-X](MANUAL.md#5-drum-x)). |
| **Pitch bend** | Bends the track. Range **±2 semitones** by default; RPN 0 (CC101 0, CC100 0, then CC6 semitones 0–24 and CC38 cents 0–99) changes it per channel. DRUM tracks ignore bend. |
| **CC64 sustain** | Holds the notes (≥ 64 down). DRUM tracks ignore it. |
| **CC1 mod wheel** | The **MODW** source of the MOD matrix. It does nothing until a MOD slot uses it (section 4.4). |
| **CC11 expression** | The **EXPR** source of the MOD matrix. Until a controller sends CC11 it reads full (127). |
| **Channel aftertouch** | The **AT** source of the MOD matrix (shared with the FM-1's own key pressure: the last one wins). New sounds route AT to the filter. |
| **Polyphonic aftertouch** | **Ignored.** |
| **Program change** | Loads sound number *n* + 1 of the PRESETS list into the track that channel plays. On **channel 16**: programs 0–3 jump to section A–D, programs 16–21 load project 1–6 (stopped only). See [chapter 14](MANUAL.md#14-midi-and-usb). Bank select (CC0 / CC32) is ignored. |
| **CC120 / CC123** | All Sound Off (immediate) / All Notes Off (honours the pedal). |
| **CC121** | Reset All Controllers: bend, wheel, aftertouch, expression and pedal back to rest; the bend range is kept. |

The FM-1's **MENU › CONTROL › VELOCITY / VEL MIN / AFTERTOUCH** rows shape the FM-1's own keys, not incoming MIDI.

### Quick setups

- **One keyboard, play whichever track is selected**: ROUTE **SEL**. Pick the track with ALGORITHM.
- **One keyboard, bass on the left hand and chords on the right**: KEY SPLIT on, LOW TRK / HIGH TRK, KEY SPLIT 2
  **MIDI CH** = your keyboard's channel, **MIDI CUT** = the split note.
- **A sequencer playing several tracks**: ROUTE **CH1-6**, the sequencer's tracks on channels 1–6 (TRACKS 6 for T5 /
  T6). Its other channels (7–16) are not heard, so a drum machine on channel 10 can share the cable.

## 4. Modulate JIANT with CCs

There are four layers. They work over USB, USB HOST, TRS and Bluetooth alike, on the track the channel plays (ROUTE,
as notes). A CC moves its parameter exactly as turning the knob would: **0–127 over the parameter's whole range**
(64 is the middle of a bipolar one), and **AUTOMATION records it** as a knob move (the selected track, armed and
playing).

### 4.1 The fixed CC map (always on with MIDI MAP FACTORY or CUSTOM ADD)

Source: `firmware/src/midi_control.c`, `MIDI_CC_MAP` and `MIDI_CC_ENG` (lines 239–262 in V10.8.71).

| CC | Parameter |
| --- | --- |
| 5 | GLIDE |
| 7 | LEVEL |
| 10 | PAN |
| 16–23 | **E1–E8**: the engine's own eight values, EDIT 1 / EDIT 2's knobs whatever the engine (CC16 = KNOB 1 of EDIT 1) |
| 71 | the engine's own **resonance** (see below) |
| 72 | REL (amp release) |
| 73 | ATK (amp attack) |
| 74 | the engine's own **brightness / cutoff** (see below) |
| 75 | DEC (amp decay) |
| 91 | REV (reverb send) |
| 93 | CHORUS (chorus send) |
| 94 | DLY (delay send) |
| 102 | the post filter's cutoff (FCUT) |
| 103 | the post filter's resonance (FRES) |
| 104 | DIST (drive) |
| 105 | SUS (amp sustain level; not the pedal, which is CC64) |

**CC74 and CC71 per engine** (`MIDI_CC_ENG`): they reach one of the engine's E1–E8 slots, or nothing.

| Engine | CC74 | CC71 |
| --- | --- | --- |
| ANALOG | E5 (CUT) | E6 (RES) |
| PHASE | E3 (DCW) | — |
| MATH | the FILTER page's CUT (as CC102) | the FILTER page's RES (as CC103) |
| VOICE | — | E7 (Q) |
| WHEEL | — | — |
| STRING | E3 (BRIGHT) | E2 (DECAY) |
| DRUM | E3 (TONE) | — |
| WEST | E1 (FOLD) | E7 (RES) |
| FM6 | E3 (MLVL) | — |
| ADD | E2 (BRIGHT) | E6 (FORMNT) |
| ANIMAL | E2 (PRESS) | E3 (the next knob: TOUCH, CLICK, BUZZ or MOUTH by animal) |
| EXT | — | — |

"—": that engine ignores the CC. The slot numbers come from the code; the names in brackets are the ones that engine's
EDIT page shows there today. For the others, look at which knob the slot is on the engine's EDIT 1 / EDIT 2 page
(E1–E4 = EDIT 1's KNOB 1–4, E5–E8 = EDIT 2's).

### 4.2 MIDI MAP and MIDI LEARN: your own CCs

**MENU › CONTROL › MIDI MAP**: **FACTORY** (the map above alone), **CUSTOM ADD** (the map and your learned CCs; a
learned CC wins over the map's meaning of the same number), **CUSTOM** (your learned CCs alone: the map's CCs do
nothing unless learned).

To learn (any knob of a track's sound to any CC):

1. **MENU › CONTROL › LEARN**: **SEL TRACK** (the CC follows whichever track is selected when it arrives) or **FIXED
   TRK** (it stays on the track it was learned on). Close the MENU (HOME): the screen says LEARN: TURN A KNOB.
2. Turn the knob on the FM-1: **LEARN T2 CUT: SEND A CC**.
3. Move the controller's knob: **CC21 > T2 CUT**. Then the next knob, the next CC …
4. **LEARN OFF** when done. **LEARN CLEAR ALL** (on closing the MENU) forgets them all.

- **Ten** learned CCs at most (**LEARN FULL**). One CC sets one parameter, a parameter has one CC.
- **Never learned** (the screen says CCn RESERVED; they keep their own job): CC0, CC1, CC6, CC11, CC32, CC38, CC64,
  CC96–101, CC120–127 (`ml_free_cc` in `midi_control.c`).
- Learning while MIDI MAP is FACTORY switches it to CUSTOM ADD.
- A learned CC answers on any channel ROUTE hears (with CH1-6, not on 7–16). Kept with the settings, not in a
  project.

### 4.3 NRPN: every parameter, 14 bits

Every parameter automation can record, plus most globals, has an NRPN number:

- **Track parameter**: CC99 **0**, CC98 the parameter's number, on the channel the track plays (channel 16 also
  reaches the selected track with ROUTE CH1-6), then **CC6** (the value's top 7 bits; alone it is a 7-bit value) and
  **CC38** (the low 7 bits).
- **Global** (tempo, swing, the buses, the SLICER, the DJ FILTER …; not the clock, routing, loads or the engine
  switch): CC99 **1**, CC98 the global's number, on channel 16 or any channel the FM-1 hears.

The numbers and ranges, generated from the firmware: **[docs/NRPN.md](NRPN.md)**.

### 4.4 MOD sources: MODW, AT, EXPR

The mod wheel, aftertouch and expression pedal do not move a fixed parameter: they are **sources** of the MOD
matrix, so you decide what they do, per track.

**LFO › MOD**: KNOB 1 the SLOT (1–4), KNOB 2 **SRC** = **MODW** (CC1), **AT** (channel aftertouch) or **EXPR**
(CC11), KNOB 3 **DEST** (CUT, PITCH, AMP, E1–E8, a send …), KNOB 4 **AMT**. Example: SRC MODW, DEST CUT, AMT 40: the
wheel opens the filter. See [Modulation and macros](MANUAL.md#10-modulation-and-macros).

## 5. Clock and sync

### JIANT follows another machine

**GLO › GLOBAL, KNOB 3: CLOCK**

| CLOCK | The tempo comes from |
| --- | --- |
| **INT** (default) | the FM-1 itself (BPM) |
| **USB +BT** | the clock arriving over USB, USB HOST or Bluetooth |
| **TRS** | the clock arriving on the TRS jack |

- The FM-1 follows **24 pulses a beat**, **Start** (from step 1), **Continue** (from where it stopped) and **Stop**.
  Only the chosen input's clock and transport are heard; the other inputs still play notes.
- The tempo is measured over up to two beats and held as a whole BPM (40–240), shown as the BPM.
- If the clock stops for half a second while playing, the FM-1 stops (a pulled cable leaves nothing hanging).
- With an external clock, TAP and GLO + PLAY (restart) do nothing. Changing CLOCK stops the transport.
- **Song Position Pointer is not read**: Continue resumes where the FM-1 itself stopped, not where the master is.

### JIANT leads

**MENU › CONTROL › MIDI OUT CLOCK** (or ALL), with **CLOCK INT**: the FM-1 sends the clock (24 a beat), **Start** when
it starts and **Stop** when it stops. The clock runs while stopped too, so the follower knows the tempo before PLAY.

- It goes out over **USB** (when the FM-1 is a device connected to a host) and **Bluetooth**. **Nothing goes out on
  the TRS jack** (input only), and nothing in USB HOST mode.
- While the FM-1 follows an external clock (CLOCK USB +BT or TRS) it sends **no** clock: it never passes a clock on.

## 6. MIDI OUT

**MENU › CONTROL › MIDI OUT**

| Value | What goes out |
| --- | --- |
| **KEYS** (default) | the notes you play on the FM-1's keys |
| **CLOCK** | the keys, and the clock with Start / Stop (with CLOCK INT) |
| **ALL** | the above, and every note the FM-1 plays (sequencer, arp, PLAYHEADS, punch-in repeats with their OCT / RANDOM), the keys' pressure as channel aftertouch, the expression keys as CC1 and pitch bend, and every knob you turn on the panel as an **NRPN** (a track's on its channel, a global's on channel 16) |
| **OFF** | nothing |

- **Channels are fixed**: track 1 sends on channel 1 … track 6 on channel 6. They cannot be changed.
- Every note sent gets its note-off; STOP, a panic or a change of the setting end whatever is still on.
- With ALL, a track whose ARP is on sends the arp's notes (not the raw keys).
- Incoming MIDI is **not** sent back out (no MIDI thru, see section 8).

## 7. Recipe: Circuit Tracks, h2midi and TRS-A cables

> **Not tested on hardware yet.** The FM-1 is a class-compliant USB device (MIDI + audio), and its USB HOST accepts standard class-compliant MIDI keyboards; whether a USB-host box such as the h2midi powers and accepts the FM-1, and whether a Circuit Tracks enumerates on the FM-1's USB HOST, has not been tried. If you try it, please report what happened (Discord or a GitHub issue).

What the code guarantees on the FM-1 side is marked as such. **The Circuit Tracks and h2midi details are not from the
JIANT code**: they are from those products' usual documentation and must be checked in their own manuals
(**unverified** here).

### A. The Circuit Tracks leads, JIANT plays along

**Cables (pick one route):**

- **Route 1, TRS only (simplest).** The Circuit Tracks has its own TRS MIDI OUT (type A, per Novation; unverified
  here). Circuit **MIDI OUT** → TRS-A cable → **FM-1 TRS jack**. The h2midi is not needed for this direction. If the
  FM-1 does not respond, see [section 2](#2-the-trs-jack-type-a-or-type-b) (the FM-1 jack type is unverified).
- **Route 2, through the h2midi.** Circuit **USB** → h2midi's USB host port; h2midi's MIDI OUT → cable → **FM-1 TRS
  jack**. Same FM-1 settings as route 1. Whether the h2midi's MIDI jacks are TRS (and which type) or 5-pin DIN depends
  on the model: check its manual.
- **Route 3, no box at all (experimental).** Circuit **USB** → **FM-1 USB** with the FM-1 in **USB HOST**. The Circuit
  must power itself (battery or its own supply): the FM-1 gives no power. Untested with a Circuit Tracks.

**On the Circuit Tracks** (in its Setup view; check its manual for the exact buttons):

- Turn **MIDI clock out (Tx)** on, and note output on for the tracks you want JIANT to hear.
- Its default MIDI channels are, per Novation (unverified here): Synth 1 = ch 1, Synth 2 = ch 2, MIDI 1 = ch 3,
  MIDI 2 = ch 4, drums = ch 10. Set them to the JIANT tracks you want to play.

**On the FM-1:**

1. **GLO › SYSTEM › ROUTE CH1-6**: channel *n* plays track *n*. The Circuit's drums on channel 10 are then ignored.
   (Or **SEL**, if only one Circuit track should play the selected JIANT track.)
2. **MENU › AUDIO › TRACKS 6** if channels 5 or 6 are used.
3. **GLO › GLOBAL › CLOCK TRS** for routes 1 and 2, **CLOCK USB +BT** for route 3.
4. Optional: a MOD slot with SRC MODW / AT, or MIDI LEARN, if the Circuit's macros send CCs.

**What to expect:** press PLAY on the Circuit: the FM-1 starts with it (Start), at its tempo; STOP stops both. The
FM-1's BPM shows the Circuit's tempo. The Circuit's notes on channels 1–6 play JIANT's tracks 1–6, and are recorded
on an armed JIANT track. Pressing PLAY on the FM-1 does not start the Circuit.

### B. JIANT leads, the Circuit Tracks follows

The FM-1 can only send MIDI over **USB** (or Bluetooth), never over TRS. So the h2midi is what turns the FM-1's USB
into a MIDI cable:

- **Cables:** **FM-1 USB** → h2midi's **USB host port**; h2midi **MIDI OUT** → cable (TRS-A, or the Circuit's
  DIN adapter, depending on the h2midi's jacks) → **Circuit MIDI IN**. The FM-1 must appear to the h2midi as a
  class-compliant USB MIDI device (it is one: "FM-1"); whether the h2midi powers it and accepts it is **unverified**.
- **On the FM-1:** **CLOCK INT**, MENU › CONTROL › **MIDI OUT CLOCK** (clock and transport only) or **ALL** (also every
  note JIANT plays, track *n* on channel *n*, so JIANT's T1 can play the Circuit's Synth 1, T2 its Synth 2 …).
  Do not use USB HOST mode here: in HOST the FM-1 sends nothing.
- **On the Circuit:** **MIDI clock in (Rx)** on, and note input on for the tracks JIANT should play (check its manual).
- **What to expect:** PLAY on the FM-1 starts the Circuit (Start), at JIANT's BPM; STOP stops it. The clock keeps
  running while stopped, so the Circuit already shows the tempo.

### Both directions at once

Possible: Circuit TRS OUT → FM-1 TRS jack (notes into JIANT), and FM-1 USB → h2midi → Circuit MIDI IN (notes and
clock into the Circuit). Keep **one** master: if the FM-1 leads (CLOCK INT, MIDI OUT CLOCK / ALL), turn the Circuit's
clock out off; if the Circuit leads (FM-1 CLOCK TRS), set MIDI OUT to KEYS or OFF on the FM-1. JIANT does not echo
what it receives, so there is no MIDI loop through the FM-1 itself; a loop can still happen if the Circuit echoes its
input to its output (check its thru / echo settings).

## 8. What it cannot do (V10.8.72)

All checked in the code unless marked otherwise.

- **No MIDI OUT on the TRS jack**: the jack is an input only (the firmware sets up only a receive line).
- **No MIDI THRU / no forwarding**: incoming notes and CCs play JIANT and are not sent on; an external clock is not
  passed on (the clock goes out only with CLOCK INT).
- **No SysEx over TRS or Bluetooth**: the TRS parser drops SysEx, and Bluetooth carries channel messages and the
  clock only. The editor, backups and updates are SysEx over **USB** only.
- **No MPE**: a channel plays a whole track (ROUTE), and a track has one bend; per-note channels would spread the
  notes over different tracks (CH1-6) or share one bend (SEL).
- **No polyphonic aftertouch** (ignored); channel aftertouch only.
- **No Song Position Pointer or MIDI Time Code**: only clock, Start, Continue and Stop.
- **No bank select**: CC0 / CC32 are ignored; program change reaches the PRESETS list's first 128 sounds.
- **MIDI OUT channels are fixed** (track *n* = channel *n*), and the receive channels are ROUTE's (CH1-6 or SEL), plus
  KEY SPLIT's one channel.
- **No MIDI out in USB HOST mode**, no computer link while HOST is on, no power on the port, no low-speed devices.
- **Ten learned CCs at most.**
- **No input-source selector**: all inputs are always heard (only the clock source is chosen).
- **External tempo 40–240 BPM.**

## 9. Unverified (to check on the hardware)

- The FM-1 TRS jack's type (A or B): decided by the hardware, not visible in the firmware.
- Circuit Tracks: its TRS type, default channels, and the Setup view's clock and note settings.
- h2midi: whether its jacks are TRS (which type) or DIN, whether its host port powers and accepts the FM-1 (a USB
  MIDI + audio device).
- Circuit Tracks straight into the FM-1's USB HOST (route 3): not tested.
- The names in the CC74 / CC71 table come from the code's comments; ANIMAL's CC71 name is not confirmed.
