# JIANT FM

**Bio-synthetic operating system for the M-VAVE FM-1** — síntesis, secuencia, mutación, performance.

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)
[![Emulador](https://img.shields.io/badge/probalo-en%20el%20navegador-ff7a00.svg)](https://juanjiant-bit.github.io/JIANT/)

![JIANT FM en el emulador](docs/jiant-screens.png)

*HOME, DRUM-X, la capa MACRO, los punch-in FX, el piano roll y la capa de canción, tal como los dibuja el
firmware (paleta térmica JIANT).*

JIANT FM es un firmware alternativo para el **M-VAVE FM-1**, pensado para tocar en vivo y armar canciones enteras
sin computadora. Batería sintetizada con morph, efectos que se tocan con las teclas y se graban, macros, canciones
por compases y un master con carácter, todo a mano en el aparato.

Es un fork de [Felucca](https://github.com/hugelton/Felucca) 1.1.5.1 de Hügelton Instruments (GPL-3.0) y toma de
[SLOOP](https://github.com/isod89/sloop-fm1) el sistema de canciones.

**▶ [Probalo en el navegador](https://juanjiant-bit.github.io/JIANT/)** · **[Instalador web](https://juanjiant-bit.github.io/JIANT/webapp/installer/)** · **[Editor web](https://juanjiant-bit.github.io/JIANT/webapp/editor/)**: el emulador corre el mismo código que el
FM-1, con sonido, y se maneja con el teclado de la compu o el mouse. Se actualiza solo con cada cambio que entra a
`main`.

> **Estado: en desarrollo (v0.1).** Compila y pasa todos los tests (los de Felucca y los propios), pero
> **todavía no se probó en un FM-1 real**. Antes de instalarlo, hacé un backup del flash.

## Qué lo hace distinto

### DRUM-X: batería sintetizada con morph
Un kit de 8 sonidos (BD SD CP CH OH TM RS CB) generado en tiempo real, sin samples, al estilo Microtonic.
- Cada sonido tiene dos lados, **A** y **B**; **MORPH** (KNOB 1, siempre a mano) se mueve entre los dos.
- **WARP** deforma todo el kit con FM y feedback; **FM** cambia el timbre de cada sonido con FM armónica (8 bandas de
  ratio, de grave y redondo a metálico); además TUNE, TONE, DECAY, NOISE y DRIVE.
- **Mutes por grupo** (KICK, SNARE, HAT, PERC) con GLO sostenido, y **mute por sonido** con EDIT sostenido.
- **EDIT sostenido → INIT / RECALL**: el kit vuelve al de fábrica o al guardado en la sección.

### Punch-in FX: los efectos se tocan y se graban
Con FX sostenido, las teclas son efectos que actúan mientras se mantienen y al soltar todo vuelve exacto.
- **De audio**: REPEAT 1/8 · 1/16 · 1/32, LPF y HPF.
- **Sobre las notas, tipo OP-Z**: OCT− y OCT+, 1/2 TEMPO, DECAY corto y largo, STUTTER 1/16 · 1/32, **ATK+** (sube el
  ataque de todo), ARP momentáneo y RANDOM (mueve notas y también pasos).
- **Cuantizados a la grilla**: un efecto entra en la próxima semicorchea del transporte; el REPEAT y el SLICER
  (gates y stutters por track) siguen la grilla del transporte desde el primer momento, aunque cambies el RATE o el
  tempo con la canción sonando, así no hay saltos al tocarlos.
- **Objetivo**: A#4 elige si afectan a todo, solo a los sintes o solo a la batería.
- **Automatizables**: con REC armado se graban en una lane de 4 compases por sección.
- **Perillas**: FILTER, CRUSH, THROW y DEPTH.

### Macros M1–M4: el sistema te invita a moverlo todo
LFO sostenido abre la capa **MACRO**: las cuatro perillas son M1–M4 y la pantalla muestra a dónde va cada una
("T1 CUT+62 T4 MRPH−31").
- **HOME dos veces**: las macros quedan fijas en HOME (perillas y pantalla) hasta tocar HOME otra vez.
- **Una sesión nueva ya viene modulada**: al encender, cada track sin rutas de macro recibe dos al azar hacia lo que su
  sonido tiene (parámetros del engine, CUT, SHP, envíos, algo de PITCH), repartidas para que cada macro mueva dos
  tracks. Girás M1–M4 y todo se deforma; después afinás en MOD o lo guardás con el proyecto.
- **Dado**: con LFO sostenido, F3 tira rutas nuevas y G3 las borra (las rutas propias de cada track quedan).
- **Asignar una macro**: en la página MOD de un track elegí SRC M1…M4 y su destino, con amount de −100 a +100.
- **Destinos**: pitch, filtro, forma, amplitud, pan, envíos, LFO, cualquiera de los 8 parámetros del engine (MORPH,
  WARP, …) y el master (CLIP y PNCH).
- **Alcance**: una macro puede mover varias cosas en varios tracks a la vez.
- **Guardado**: se guardan con el proyecto.

### Perillas con aceleración
Girar rápido barre el rango entero (hasta ×16 en los parámetros de 0 a 127); girar lento es ajuste fino, de a un
paso. Viene encendido; se apaga en MENU > KNOB ACCEL.

### Secuencias que se mueven
- **SEQ > SHIFT**: corre la secuencia de cada track en pasos (**OFS**) y en altura (**PIT**). Los dos son
  automatizables; también están en SCL sostenido.
- **Una escala para todo**: ROOT y SCALE elegidos en cualquier track valen para todos los tracks melódicos (la batería
  no usa escala). **STRN** (SCL 2) mueve todas las secuencias hacia arriba o abajo por los grados de esa escala.
- **ARP TRNS**: con el arpegiador en TRNS, las teclas transponen la secuencia sin tocar notas.
- **Edición**: 64 pasos por track, piano roll, grilla de batería, parameter locks, chance, ratchets, slide y
  automatización de perillas.
- **Grabación**: en vivo con overdub, metrónomo y count-in.
- **SEQ + REC** borra todas las secuencias de una vez para empezar de cero; **REC + FX / EDIT / ENV / LFO…** borra solo
  la automatización de esa parte.

### Canciones dentro del aparato
**8 canciones**, cada una con **4 secciones (A–D)** y una cadena de filas por compases, al estilo SLOOP. Con SAVE
sostenido:
- **Secciones en vivo**: se lanzan y entran en el próximo compás.
- **Quick chain**: varias secciones tocadas en un mismo hold quedan en loop.
- **SONG REC**: graba la canción mientras la tocás.
- **STORE / RECALL**: guarda la sección que suena o vuelve a como estaba guardada.

**Escenas**: cada fila de la canción puede tener una escena. Cuando la fila entra, pone los mutes de los tracks, los
mutes de grupo de DRUM-X, las macros M1–M4 y los punch-in MIDI que estaban apretados (OCT+, 1/2 TEMPO, STUTTER…).
La escena sigue vigente en las filas siguientes que no tengan una propia, y los efectos se apagan al parar la canción.
- **Guardar una escena**: en la página SONG, KNOB 4 (SCENE) a la derecha guarda el estado de ese momento en la fila
  elegida; a la izquierda la borra.
- **Con SONG REC**: cada fila grabada guarda su escena sola, con el estado de cuando empezó. Tocás la canción con
  mutes, macros y efectos, y queda grabada así.

### Envolventes con loop
ENV DEST > **LOOP**: con la nota sostenida, la envolvente vuelve al ataque al llegar al sustain; es un LFO con forma de
ADSR para amplitud, filtro, pitch y forma.

### Modulación secuenciada (LFO > MSEQ)
Cada track tiene un secuenciador de modulación, como un secuenciador de CV: 16 niveles, **LEN** (1–16 pasos), **DIV**
(el ritmo de los pasos) y **SLEW** (de saltos a glides; KNOB 4 en la página MSEQ). Corre con el transporte y en la matriz MOD es la fuente
**STEP**: cualquier destino (filtro, pitch, MORPH, envíos, CLIP…) sigue la secuencia. En la página MSEQ, KNOB 1 elige
el paso y KNOB 2 su nivel; los niveles también se automatizan.

### Efectos
- **DIST** por track con **TYPE** (SOFT, HARD, FOLD, CRUSH, RECT) y **TONE** (más oscuro o más brillante): página FX > DIST.
- **Delay**: además de COLR (agudos), **HPF** recorta los graves de las repeticiones (más dub, más fino).
- **WIDTH**: abre el estéreo. El eco derecho del delay llega un poco después y el chorus separa L y R. En 0 todo
  suena como antes.
- **Reverb**: **PRE** (pre-delay de hasta 100 ms), y **MOD** y **RATE** modulan la red de resonancia. En ROOM se mueve
  el largo de los combs hasta ~6 ms: la cola se desafina y se ensancha, como un chorus. En SPRING, un vaivén mucho
  más profundo. En REVERB 2 también **FILT** (a la izquierda más oscura, a la derecha sin graves) y **WIDE** (abre la
  reverb en estéreo).
- **GRAIN delay** (DLY 2): **PITCH** pasa cada repetición por dos granos, ±12 semitonos (repeticiones que suben tipo
  shimmer o que bajan), y **SPRY** los dispersa al azar (hasta ~190 ms). Con los dos en 0 es el delay de siempre. Usa
  la misma memoria del delay: no ocupa RAM extra.

### Master con carácter (FX > MASTER)
- **CLIP**: satura la mezcla antes del limiter.
- **PNCH**: bus de batería como el Drum Buss de Ableton: el ataque de cada golpe sube hasta +11 dB empujando la
  saturación de la voz y la cola baja hasta −14 dB.
- **DUCK**: el bombo baja a los demás tracks hasta −30 dB, los mantiene abajo ~25 ms y vuelven con una curva de
  sidechain (bombeo), con su release.

## Lo que trae

- **4 tracks** con 8 voces compartidas.
- **8 engines**, todos de síntesis, sin samples: ANALOG, FM6 (Dexed, importa .syx), PHASE, LOFI, VOICE, WHEEL, NOISE
  y DRUM (DRUM-X).
  - **ANALOG** suma las ondas de TRIO: **SYNC** (sync duro, DTN barre la relación), **RING** (modulación en anillo,
    campanas y metales) y **SAW3** (tres sierras desafinadas). Los sonidos de TRIO guardados cargan como ANALOG.
  - **LOFI** tiene **BYTE**: bytebeat, 32 fórmulas de 8 bits elegidas con ALGO (DUTY) y una variable VAR (CRSH) que
    las deforma; el tiempo sigue la nota tocada. Ningún otro firmware lo tiene.
  - **PHASE** suma **FB**: la salida realimenta la fase, de un borde más duro a growl y ruido.
- **Modulación**: 4 slots por track. Fuentes: LFO, ENV, VEL, KEY, RAND, controladores MIDI, las 4 macros y STEP (el
  secuenciador de modulación).
- **Efectos**: distorsión de 5 tipos y SLICER por track; envíos a chorus, delay (con HPF) y reverb (ROOM o SPRING, con
  pre-delay, modulación, filtro y width); grain delay; WIDTH estéreo; master con CLIP, PNCH, DUCK y limiter.
- **Arpegiador** con 16 modos y 16 escalas con modo de teclas blancas; glide; POLY, MONO, LEGATO y UNISON.
- **Conexiones**:
  - USB: MIDI class-compliant y audio (el master llega a la computadora).
  - MIDI por TRS.
  - Clock interno, USB o TRS.
- **Guardado**: 32 presets de usuario con nombre y autoguardado al apagar.
- **Editor web** de todos los parámetros: patches FM6, grilla, mezcla y backup completo (las 8 canciones con sus
  secciones, filas y escenas).

## Estado

| Objetivo | Estado |
| --- | --- |
| Song mode estilo SLOOP: 8 canciones × 4 secciones, con backup completo desde el editor | ✅ |
| DRUM-X: motor, MORPH, WARP, mutes por grupo y por sonido, INIT / RECALL | ✅ |
| Master: CLIP, PNCH, DUCK | ✅ |
| Punch-in FX de audio y MIDI, con su lane automatizable | ✅ |
| Macros M1–M4 con su capa y su mapa | ✅ |
| SHIFT (OFS / PIT), ARP TRNS, SEQ + REC | ✅ |
| Paleta térmica JIANT | ✅ |
| Escenas por fila de la cadena (mutes, macros, punch-in) | ✅ |
| Secuenciador de modulación (STEP), DIST con tipos y tono, delay HPF, WIDTH, reverb MOD y PRE | ✅ |
| Interfaz orgánica con las ilustraciones ([docs/TONIC-UI.md](docs/TONIC-UI.md)) | ⏸ después de sonido y performance |

<details>
<summary><b>Qué cambió respecto de Felucca 1.1.5.1</b></summary>

- **DRUM** es DRUM-X; los kits de Felucca se retiraron.
- **CHORD** se retiró; sus dos parámetros son ahora SHIFT (OFS / PIT).
- **PHYS** se retiró, lo que liberó 50 KB de RAM. Un sonido PHYS viejo suena como el primer preset de ANALOG.
- **Sin samples**: se fueron los samples de usuario (USR1–3, para hacer lugar a las canciones) y los engines de samples
  SAMPLE, SLICE y GRAIN con sus samples de fábrica: 140 KB de flash y 32 KB de RAM para DRUM-X, los efectos y los
  punch-in. Un sonido de esos engines carga como ANALOG; el viejo PERC de SAMPLE, como DRUM-X.
- **8 canciones**: la canción 1 son los 4 proyectos de siempre, así que lo guardado con Felucca aparece ahí.
- **Capa FX**:
  - se fueron REVERSE, TAPE STOP, FREEZE y el armonizador;
  - entraron los punch-in MIDI.
- **Sin undo**: en SAVE sostenido, OCT+ guarda la sección que suena y OCT− la recupera.
- **Sin lock de capas**: una capa está abierta mientras se mantiene su botón.
- **EDIT sostenido**: hace INIT / RECALL en lugar del selector de voces.
- **Capa REC**: CLEAR y CLICK.
- **Paleta**: JIANT, térmica, viene de fábrica.
- **Nombres USB**: siguen siendo "Felucca" para que el editor web conecte.

</details>

## Controles

- Tocar un botón de página abre su página; otra vez, la siguiente. HOME vuelve a la pantalla principal.
- **Mantener** un botón abre su capa rápida: las teclas y las perillas cambian de función mientras está
  apretado.

| Mantener | Teclas | Perillas |
| --- | --- | --- |
| **FX** | F3 G3 A3 REPEAT 1/8, 1/16, 1/32; B3 LPF; C4 HPF. Punch-in MIDI: D4 OCT−, E4 OCT+, F4 1/2 TEMPO, G4 DEC−, A4 DEC+, B4 C5 STUTTER 1/16 · 1/32, D5 ATK+ (ataque de todo arriba), E5 ARP, F5 RANDOM (notas y pasos). Todo entra en la próxima 1/16 del transporte. Teclas negras 1–4: mute de T1–T4; A#4 elige a qué tracks afectan los MIDI (todos, sintes, batería). **Automatizar**: con REC armado y tocando, lo que mantengas queda grabado en la lane de la sección (64 pasos de 1/16); G5 la borra donde pasa, o entera con el transporte parado | FILTER, CRUSH, THROW, DEPTH |
| **ARP TRNS** | Con el modo de ARP en TRNS, las teclas (y el MIDI que entra) transponen la secuencia del track según su intervalo desde C4, sin tocar notas; la transposición queda al soltar | — |
| **SEQ > SHIFT** | OFS corre la secuencia del track de −32 a +32 pasos (dentro de LEN; grabar en vivo escribe donde se escucha) y PIT la transpone ±24 semitonos (no en kits). Los dos se automatizan y están también en la capa SCL (KNOB 3 / 4) | OFS, PIT |
| **REC + otro botón** | Mantener REC y apretar FX, EDIT, ENV, LFO, SCL, ARP o GLO: borra la automatización de esa parte del track elegido (movimientos de perillas y locks por paso; los valores guardados quedan). FX: también la lane de punch-in. GLO: niveles y paneo de los 4 tracks. Al revés (FX sostenido y REC) arma la grabación, como siempre | — |
| **SEQ + REC** | Mantener SEQ y apretar REC: CLEAR ALL SEQUENCES? (OCT+ confirma): borra los pasos y la automatización de los 4 tracks y la lane de punch-in | — |
| **LFO** | — (capa **MACRO**: abajo, a dónde va cada macro) | M1, M2, M3, M4 |
| **GLO** | Teclas negras 1–4 mute de T1–T4 (fijo); 5–8 mute de grupo de DRUM: KICK, SNARE, HAT, PERC; F3–B3 solo mientras se mantiene; C4 desmutea todo; F4 tap tempo | Nivel de T1–T4 |
| **SCL** | Cualquier tecla elige la raíz | ROOT, SCL, OFS, PIT |
| **EDIT** | F3 **INIT**: el sonido del track vuelve al de fábrica (en DRUM, también el kit DRUM-X). G3 **RECALL**: vuelve al sonido guardado en la sección (en DRUM, con su kit). Los dos piden confirmación y no tocan los pasos. En un track DRUM, las teclas negras 1–8 mutean cada sonido de DRUM-X. El engine y los sonidos se eligen en PRESETS | Las 4 primeras perillas de EDIT del engine |
| **SEQ** | En las páginas SEQ: SEQ TOOLS | LEN, DIV, SWING, GATE |
| **REC** | F3 CLEAR del track, G3 CLICK | CLICK |
| **HOME** | Menú | — |
| **SAVE** | Capa de canción (en la página SONG, KNOB 4 = SCENE): F3–B3 tocan A–D en el próximo compás (varias en un mismo hold: quick chain), C4–F4 guardan en A–D, D5 LOOP / SONG, E5 SONG REC, G5 página SONG; OCT+ guarda la sección que suena, OCT− la recupera | KNOB 1: canción 1–8 (entra al soltar SAVE, parado) |

Otros: **PLAY** arranca y para; **REC** arma el track elegido; **SELECT** cambia el tempo; **ALGORITHM**
elige el track en todas las páginas; **PRESETS** cambia el sonido; **OCT− / OCT+** la octava (en páginas
de acción y diálogos: volver / confirmar); GLO + PLAY reinicia desde el principio.

## Compilar y probar

Ver [BUILDING.md](BUILDING.md). En resumen:

```
./build.sh                 # firmware: build/felucca.fwsc
web/emu/build.sh           # emulador en el navegador: build/emu
tests/run_tests.sh         # tests de host, editor web y emulador
```

Todo cambio se prueba primero en el emulador. La versión publicada
([juanjiant-bit.github.io/JIANT](https://juanjiant-bit.github.io/JIANT/)) la arma
[.github/workflows/emulator.yml](.github/workflows/emulator.yml) en cada push a `main` (en el repositorio:
Settings → Pages → Source: **GitHub Actions**, una sola vez).

## Instalar (bajo tu riesgo)

1. Hacé un **backup del flash** antes de la primera instalación. **JIANT no tiene samples de usuario:** si venís de
   Felucca, los samples USR1–3 se pierden al instalar (ese flash ahora guarda las canciones). Guardá tus WAV.
2. Instalá desde el **[instalador web](https://juanjiant-bit.github.io/JIANT/webapp/installer/)** (Chrome o Edge, el FM-1 por
   USB directo a la computadora), con el firmware que tenga (el oficial V15, Felucca o una versión anterior de JIANT). Se
   arma solo con cada cambio que entra a `main`. Sin internet: `python3 tools/fm1_install.py build/felucca.fwsc`.
3. Para volver al firmware oficial, usá el actualizador de M-VAVE o **Return to official V15** del
   instalador.

Si el FM-1 queda en negro después de una actualización interrumpida, revisá si la computadora lo ve como
**WL80UBOOT** (o un dispositivo USB 4C4A:8057): es el modo de arranque del chip y se puede recuperar
volviendo a correr el instalador con otro cable. Si no alcanza,
[FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) lee y escribe el flash con una
Seeed XIAO RP2040.

## Documentos

| Documento | Qué tiene |
| --- | --- |
| [FELUCCA-TONIC-VISION.md](FELUCCA-TONIC-VISION.md) | La visión y la lista completa de deseos |
| [FELUCCA-TONIC-SPEC.md](FELUCCA-TONIC-SPEC.md) | Las fases y las reglas de trabajo |
| [docs/TONIC-AUDIT.md](docs/TONIC-AUDIT.md) | Auditoría: memoria, engines, efectos, song mode de SLOOP |
| [docs/TONIC-SONG-PLAN.md](docs/TONIC-SONG-PLAN.md) | Plan del song mode |
| [docs/TONIC-DRUMX.md](docs/TONIC-DRUMX.md) | DRUM-X: la voz, el kit por sección y las fases |
| [docs/TONIC-UI.md](docs/TONIC-UI.md) | La interfaz JIANT FM: mapa de pantallas, formato, medidas de flash |
| [BUILDING.md](BUILDING.md) | Toolchain, build, emulador y tests |
| [web/EDITOR_PROTOCOL.md](web/EDITOR_PROTOCOL.md) | Protocolo SysEx del editor |
| [LICENSING.md](LICENSING.md) | Licencias de cada parte |

## Créditos

- **[Felucca](https://github.com/hugelton/Felucca)** de [Hügelton Instruments](https://hugelton.com)
  (Leo Kuroshita, [@kurogedelic](https://github.com/kurogedelic)): la base completa de este firmware;
  las formas de onda de PHASE (port de [CrispyZebra](https://github.com/hugelton/CrispyZebra), GPL-3.0);
  las voces y kits de DRUM; la fuente de íconos
  [Fukiai](https://github.com/hugelton/Fukiai) ([MIT](LICENSES/MIT-Fukiai.txt)). Y quienes contribuyeron
  a Felucca: keremimo, ChanceTheMaker, andreahaku, spinkham, zednaked, jasonpersinger.
- **[SLOOP](https://github.com/isod89/sloop-fm1)** de isod89 (GPL-3.0): el sistema de secciones,
  quick chain y SONG REC en el que se basa el song mode.
- Fuentes: [Inter Tight](https://github.com/rsms/inter-tight) ([SIL OFL 1.1](LICENSES/OFL-InterTight.txt));
  en el emulador, [DotGothic16](https://github.com/fontworks-fonts/DotGothic16) ([SIL OFL 1.1](LICENSES/OFL-DotGothic16.txt)).
- VOICE: según [klattsch](https://github.com/tgies/klattsch) de Tony Gies (MIT); datos de formantes de
  Klatt (1980) y Hillenbrand et al. (1995).
- FM6: msfa de [Dexed](https://github.com/asb2m10/dexed), Google Inc. y Pascal Gauthier
  ([Apache-2.0](LICENSES/Apache-2.0-msfa.txt)).
- Emulador: según [X0X](https://github.com/charlesvestal/fm1-x0x) de charlesvestal (GPL-3.0).
- Formato de paquete: [JieLi AC79 SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK)
  ([Apache-2.0](LICENSES/Apache-2.0.txt); tres de sus archivos van en cada paquete, ninguno en este árbol).

## Desarrollo asistido por IA

JIANT se desarrolla con ayuda de agentes de IA para el código, los tests y la documentación. El diseño
del instrumento y las decisiones las toma su autor.

## Licencia

Software libre: [GPL-3.0-only](LICENSE), igual que Felucca. Las fuentes y el DSP portado conservan sus
licencias ([LICENSES/](LICENSES/)); detalles en [LICENSING.md](LICENSING.md).

"Felucca" y "Hügelton Instruments" son nombres de Hügelton Instruments; JIANT no está afiliado a ellos.
M-VAVE y FM-1 son marcas de sus dueños; JIANT no está afiliado ni avalado por ellos.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments — Felucca.
Las modificaciones de JIANT se distribuyen bajo la misma licencia.
