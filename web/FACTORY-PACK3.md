# Uploaded sample pack 3

Nine sounds from the owner's `samples-1.zip`, supplied for inclusion in JIANT's emulator and stock bank. Source rights were not specified; this bank is not labelled CC0.

The original files are stereo 24-bit PCM, 44.1 kHz. Conversion correctly decodes signed 24-bit PCM, mixes to mono, trims leading silence, applies filtered polyphase resampling, removes DC and adds short fades. Peaks are normalized to approximately -1 dBFS. This is a compact bank of excerpts, not the full original recordings.

| Key | Slot | Source | Rate | Length |
|---|---|---|---:|---:|
| C4 | GUITAR | A03_chorus-guitar-pluck.wav | 8000 | 220 ms |
| C#4 | BASS | A07_bass-short-2.wav | 6000 | 180 ms |
| D4 | HAT08 | A08.wav | 12000 | 80 ms |
| D#4 | OPEN12 | A12.wav | 8000 | 130 ms |
| E4 | TAMBO11 | A11.wav | 8000 | 110 ms |
| F4 | A02 | A02.wav | 4000 | 220 ms |
| F#4 | VOICE06 | A06.wav | 6000 | 240 ms |
| G4 | VOICE10 | A10.wav | 4000 | 320 ms |
| G#4 | A04 | A04.wav | 2000 | 1200 ms |

IMA ADPCM uses **6116 / 6144 bytes**, all nine slots intact. The rates and lengths are explicit so the codec cannot silently truncate later sounds.

The beta SAMPLE installer loads this bank after installation only when the connected device reports an empty bank. Any existing bank is preserved. A failed sample transfer is reported separately from firmware installation. ADDITIVE and the public installer do not provision samples. To replace an existing bank, choose Pack 3 in the editor and explicitly send it.
