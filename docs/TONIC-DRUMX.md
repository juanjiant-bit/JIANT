# FELUCCA TONIC — DRUM-X

Motor de batería de JIANT con la idea de Microtonic: un oscilador afinado con envolvente de pitch y FM, un
generador de ruido filtrado, sus envolventes, dos patches (A y B) por sonido y un MORPH entre ellos.
Reemplaza a DRUM por etapas, dentro del mismo motor (número 10). Así se conservan la grilla de 8 lanes, el
mapa GM, los patrones y los proyectos.

## El sonido (`firmware/src/drumx_voice.c`)

Un sonido (un lane del kit) ocupa 11 bytes (`dx_lane_t`): un byte de modo y cinco valores para A y cinco para B.

| Valor | Qué hace |
| --- | --- |
| PITCH | oscilador, MIDI 24–119 (33 Hz – 7,9 kHz), 3/4 de semitono por paso |
| PMOD | envolvente de pitch: hasta +4 octavas en el golpe, cae con 1/6 del decay (1–40 ms) |
| DECAY | caída de amplitud, 3 ms – 1,5 s (exponencial) |
| NOISE | mezcla: 0 solo oscilador, 127 solo ruido |
| COLOR | índice de FM del oscilador (en SINE, su drive) y corte del filtro del ruido (30 Hz – 16 kHz) |

El modo elige la onda (SINE, FM 1:1, METAL 1:1,47, BELL 1:2,76), el filtro del ruido (LP, BP, HP) y la
envolvente del ruido (la del oscilador, o SNAP: 1/4 de su caída).

El MORPH (0 = A, 127 = B) interpola valor por valor y se lee en cada bloque, así que se puede mover mientras
el golpe suena. Todo es punto fijo con productos de 32 bits.

## Kit por sección

Un kit de 8 sonidos ocupa 88 B. Va dentro de cada sección, que tiene 192 B libres (decisión del usuario): cada
variación A–D puede tener su propio kit.

## Etapas

- **Fase 1 — HECHO.** KIT X en DRUM toca el kit de fábrica `DX_KIT_DEF`. En KIT X, SNAP es el **MRPH**.
  TUNE (PITCH global), TONE (COLOR) y DECY (DECAY) mueven todos los lanes. Lo cubre `tests/drumx_test.c`:
  cada lane suena y termina, el MORPH funciona (también con el golpe sonando), el PITCH, el choke de hats, y
  deja demos en `build/drumx_demo/`.
- **Fase 2.** El kit pasa a guardarse en la sección (formato de proyecto nuevo, 88 B, conversión desde FUN9):
  - una página de edición por lane (lado A/B, los 5 valores y el modo);
  - MORPH y PITCH lockeables por paso;
  - DRIVE y PAN;
  - **hecho:** grupos KICK (bombo) / SNARE (snare y clap) / HAT (los dos hats) / PERC (tom, rim, bell) con mute en
    vivo: GLO sostenido + teclas negras 5–8 (D#4 F#4 G#4 A#4), LED apagado = muteado, C4 los desmutea con los tracks.
    Valen para cualquier kit de DRUM. Un golpe nuevo de un grupo muteado no suena; lo que suena se apaga en ~6 ms
    (sin click). Se guardan con la sección (byte 3208 del proyecto FUNA);
  - kits de fábrica que aproximan los kits de DRUM (STD, 80, 10, 66, 55, 77), y entonces se retiran las voces
    viejas (`drum_voice.c`), lo que libera flash.
