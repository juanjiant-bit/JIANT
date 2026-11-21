# JIANT FM as VST3, AU, AUv3, LV2 and an app

The whole JIANT FM firmware as an instrument plugin for DAWs and iPad hosts (AUM, GarageBand, Logic, Cubasis...), and
as a standalone app, built with JUCE 9 from the same sources as the FM-1 ([`ports/core`](../core)).

![The plugin's editor](editor.png)

Unlike the MPC plugin ([`ports/mpc-plugin`](../mpc-plugin)), the editor here is JIANT's own: its screen, live, and the
FM-1's panel as the browser emulator draws it: MASTER, SELECT, PRESETS, ALGORITHM, KNOB 1-4, the twelve buttons, OCT
- / +, the 27 keys and their lights. Mouse or fingers (several at once on an iPad), the mouse wheel on the knobs,
SHIFT-click holds a button, and the computer keyboard as the emulator's (Z..' and Q..O the keys, 1..= the buttons,
[ ] OCT, arrows SELECT / ALGORITHM).

| | |
| --- | --- |
| Formats | VST3 (macOS, Windows, Linux), AU and AUv3 (macOS), AUv3 and the app (iOS / iPadOS), LV2 (Linux), standalone |
| MIDI | channels 1-4 play tracks 1-4 (GLO > MIDI IN ROUTE CH1-4); CCs as JIANT's MIDI map |
| Sync | **Host** (default): the sequencer follows the host's transport and tempo; **Internal**: its own tempo |
| Parameters | Volume (the MASTER knob), Sync |
| Saved | in the host's project: the live section, the songs, the user presets |
| Sample rate | any (the firmware's 44.1 kHz, resampled) |
| Power | a computer's CPU: every track its own 8 voices; the firmware's own load governor on the real time ([`ports/README.md`](../README.md)) |
| One per session | the firmware is one device: a second instance stays silent and says so |

## Build

```sh
ports/juce-plugin/build.sh                    # this computer's formats -> build/juce-plugin/JiantFM_artefacts/Release
ports/juce-plugin/build.sh -DJIANT_CHECK=ON   # and the checks: tests/plugin_check.cpp, tests/editor_shot.cpp
```

JUCE 9.0.5 is fetched (or `JUCE_DIR=<a checkout>`). macOS: `-G Xcode` (AUv3). iOS:
`cmake -S ports/juce-plugin -B build/juce-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS`. `.github/workflows/ports.yml` builds
every platform on each push to `ports/`, and keeps the results as the run's artifacts.

The check (`plugin_check <plugin>`) loads the built plugin as a host loads it (JUCE's VST3 / AU hosting): its
description, a chord at 44.1, 48 and 96 kHz in blocks of 32 to 1024 frames (it sounds, then it is silent after the
note-offs, nothing non-finite), the state out and back, a second instance silent, the editor made. macOS adds Apple's
`auval`.

## Installing

- **macOS**: `JIANT FM.vst3` into `~/Library/Audio/Plug-Ins/VST3`, `JIANT FM.component` into
  `~/Library/Audio/Plug-Ins/Components`; the app carries the AUv3. The builds are not notarized: the first time,
  right-click > Open (or `xattr -dr com.apple.quarantine <the bundle>`).
- **Windows**: `JIANT FM.vst3` into `C:\Program Files\Common Files\VST3`.
- **Linux**: `JIANT FM.vst3` into `~/.vst3`, `JIANT FM.lv2` into `~/.lv2`.
- **iPad / iPhone (AUM)**: the `.ipa` is unsigned: it installs once signed with your Apple ID (Sideloadly, AltStore) or
  through TestFlight with a developer account. Then AUM lists JIANT FM among its instruments.

## Licences

JIANT is GPL-3.0-only; JUCE 9 is used under the AGPLv3. The plugin as a whole is conveyed under the AGPLv3 (GPLv3
section 13); JIANT's own files keep the GPL. The VST3 SDK JUCE carries is MIT.
