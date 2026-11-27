**JIANT FM V10.8.48 for the Akai MPC Sample: preview.** The FM-1's latest public firmware, V10.8.48, as the MPC
Sample's whole instrument, with the sample engines only a bigger CPU can run. Free and GPL-3.0.
**Not yet tried on an MPC Sample: testers welcome.**

| File | What it is |
| --- | --- |
| `JIANT-FM-MPC-Sample-V10.8.48-mpc1.zip` | JIANT FM for the **Akai MPC Sample**, started from the microSD card (README inside). |

**New since the previous MPC preview (v0.9.18):** the whole V10.8 firmware.

- **Engines**: STRING (Karplus-Strong), ADD (additive), WEST (west-coast folding), MATH (bytebeat), EXT (the audio
  input on a track, and SWARM), beside DRUM-X, ANALOG, PHASE, FM6, VOICE and ANIMAL; presets that move by themselves.
- **Songs**: TIMELINE, CLIPS, ARRANGE, scenes; a whole song or a loop saved in SAVE > PROJECT > PROJECTS; GEN's songs
  are drafts until saved.
- **GENE**, the song generator, with many more genres (cumbia, EBM, baile funk, phonk, hip hop, jazz, techno rock ...).
- **Sequencer**: EUCLID and MODES, RAMP locks, DRONE CLAY, recording that keeps how you play.
- **Screen**: seven palettes, larger type and values, the ECOSYSTEM and BEING pages.
- **DRUM-X**: a better kit, the EXTENDED key map with every sound chromatic.

**Only on the MPC Sample's bigger CPU** (not on the FM-1):

- **Every track its own 8 voices** (48 in all), no CPU cutbacks; the load measured on the MPC's CPU, CPU on the screen.
- **SAMPLE**: one shot with experimental modes (GATE, LOOP, PING, REV, SLICE, SCRUB, TAPE), triggers (CHOKE, RATCHET,
  RANDOM, ROUND, VEL, LEGATO, CHANCE), a feedback comb, DESTROY and resampling.
- **GRAIN**: granular, with travel modes (FWD, REV, PING, RANDOM, STRETCH, FREEZE, KEY), time-stretch, feedback with
  shimmer, LIVE on the input or the master, and resampling.
- **DRUM-X's sample layer**: SMORPH from synth to sample on every sound, five blends, slots, slices or a keyed kit.
- **16 sample slots** (60 s each), recording, and the **SAMPLE BROWSER** on the SAMPLE button; WAVs from
  `JIANT/Samples` on the card.

A project made on the MPC opens on an FM-1: SAMPLE and GRAIN tracks play as ANALOG there, DRUM-X as its synth kit.

**Install (microSD card):**

1. MPC Sample on firmware 1.3.0.12 (on another firmware JIANT does not start and the stock app keeps running).
2. Back up the card. Move away any `autorun` folder at its top.
3. Unzip and copy the `autorun` folder to the top of the card; do not change anything inside it.
4. Put WAVs in `JIANT/Samples` if you like. Switch the MPC on: the stock app starts, then JIANT takes over the screen.
5. Push the wheel to go back to the stock app; delete `autorun` to uninstall (`JIANT/flash.img` keeps your music).

**Try it in the browser first:** https://juanjiant-bit.github.io/JIANT/emu/devices.html#mpc

Problems and test reports: [Issues](https://github.com/juanjiant-bit/JIANT/issues). Sources: [`ports/`](https://github.com/juanjiant-bit/JIANT/tree/main/ports).

---

**JIANT FM V10.8.48 para la Akai MPC Sample: preview.** El último firmware público de la FM-1, V10.8.48, como el
instrumento entero de la MPC Sample, con los motores de samples que solo un CPU más grande puede correr. Gratis, GPL-3.0.
**Todavía no se probó en una MPC Sample: buscamos testers.**

**Nuevo desde la preview anterior para la MPC (v0.9.18):** todo el firmware V10.8.

- **Motores**: STRING (Karplus-Strong), ADD (aditivo), WEST (plegado west coast), MATH (bytebeat), EXT (la entrada de
  audio en una pista, y SWARM), junto a DRUM-X, ANALOG, PHASE, FM6, VOICE y ANIMAL; presets que se mueven solos.
- **Temas**: TIMELINE, CLIPS, ARRANGE, escenas; un tema entero o un loop se guarda en SAVE > PROJECT > PROJECTS; los
  temas de GEN son borradores hasta que los guardás.
- **GENE**, el generador de temas, con muchos más géneros (cumbia, EBM, baile funk, phonk, hip hop, jazz, techno rock …).
- **Secuenciador**: EUCLID y MODES, locks con RAMP, DRONE CLAY, la grabación respeta cómo tocás.
- **Pantalla**: siete paletas, letras y valores más grandes, las páginas ECOSYSTEM y BEING.
- **DRUM-X**: un kit mejor, el mapa de teclas EXTENDED con cada sonido cromático.

**Solo en el CPU más grande de la MPC Sample** (no en la FM-1):

- **Cada pista con sus 8 voces** (48 en total), sin recortes por CPU; la carga medida en el CPU de la MPC, CPU en pantalla.
- **SAMPLE**: one shot con modos experimentales (GATE, LOOP, PING, REV, SLICE, SCRUB, TAPE), disparos (CHOKE, RATCHET,
  RANDOM, ROUND, VEL, LEGATO, CHANCE), un comb con feedback, DESTROY y resampleo.
- **GRAIN**: granular, con recorridos (FWD, REV, PING, RANDOM, STRETCH, FREEZE, KEY), time-stretch, feedback con
  shimmer, LIVE sobre la entrada o el master, y resampleo.
- **La capa de samples de DRUM-X**: SMORPH de la síntesis al sample en cada sonido, cinco mezclas, slots, slices o un
  kit por teclas.
- **16 slots de samples** (60 s cada uno), grabación y el **SAMPLE BROWSER** en el botón SAMPLE; WAVs desde
  `JIANT/Samples` en la tarjeta.

Un proyecto hecho en la MPC abre en una FM-1: las pistas SAMPLE y GRAIN suenan ahí como ANALOG, DRUM-X como su kit
sintetizado.

**Instalación (tarjeta microSD):**

1. MPC Sample con el firmware 1.3.0.12 (con otro firmware JIANT no arranca y sigue la app de fábrica).
2. Hacé un backup de la tarjeta. Si arriba de todo hay una carpeta `autorun`, movela a otro lado.
3. Descomprimí y copiá la carpeta `autorun` arriba de todo en la tarjeta; no cambies nada adentro.
4. Si querés, poné WAVs en `JIANT/Samples`. Prendé la MPC: arranca la app de fábrica y después JIANT toma la pantalla.
5. Apretá la rueda para volver a la app de fábrica; borrá `autorun` para desinstalar (`JIANT/flash.img` guarda tu música).

**Probalo antes en el navegador:** https://juanjiant-bit.github.io/JIANT/emu/devices.html#mpc

Problemas e informes de prueba: [Issues](https://github.com/juanjiant-bit/JIANT/issues).
