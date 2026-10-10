# JIANT FM

**Bio-synthetic operating system for the M-VAVE FM-1** — síntesis, secuencia, mutación, performance.

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)
[![Emulador](https://img.shields.io/badge/probalo-en%20el%20navegador-ff7a00.svg)](https://juanjiant-bit.github.io/JIANT/)

![JIANT FM: la firma, las pantallas y lo nuevo de la 0.5](docs/jiant-hero.png)

JIANT FM es un firmware alternativo para el **M-VAVE FM-1**, pensado para tocar en vivo y armar canciones enteras
sin computadora. Es un fork de [Felucca](https://github.com/hugelton/Felucca) 1.1.5.1 de Hügelton Instruments
(GPL-3.0) y toma de [SLOOP](https://github.com/isod89/sloop-fm1) el sistema de canciones.

**▶ [Probalo en el navegador](https://juanjiant-bit.github.io/JIANT/)** · **[Instalador web](https://juanjiant-bit.github.io/JIANT/webapp/installer/)** · **[Editor web](https://juanjiant-bit.github.io/JIANT/webapp/editor/)**: el emulador corre el mismo código que el
FM-1, con sonido, y se maneja con el teclado de la compu o el mouse. El emulador, el instalador y el editor se
actualizan solos con cada cambio que entra a `main`.

> **Estado: en desarrollo (v0.5).** Compila y pasa todos los tests (los de Felucca y los propios), pero
> **todavía no se probó a fondo en un FM-1 real**. Antes de instalarlo, hacé un backup del flash.

## La idea

Un sintetizador de bolsillo no debería sentirse como un menú. JIANT parte de una pregunta simple: ¿qué pasa si el
aparato se comporta menos como una herramienta y más como un organismo?

Por eso todo en JIANT está vivo y es visible. La pantalla funciona como una **cámara térmica**: el color no es
decoración, es intensidad. Lo frío es cian y violeta, lo caliente rojo, naranja, amarillo y blanco. Un parámetro
alto quema; una voz que suena brilla; el silencio se enfría. Cada motor es un **ser**, un protozoo dibujado en líneas
cuya forma sale de lo que estás tocando: abrís el filtro y le crecen espinas, subís la resonancia y la membrana
vibra, desafinás los osciladores y sus núcleos se separan. HOME es un **ecosistema**: cuatro criaturas, una por
track, que respiran con su propio audio. No hay que leer números para saber qué está pasando; se ve.

Al mismo tiempo, lo que es técnico se muestra técnico. El LFO corre a su velocidad, la envolvente marca dónde está la
voz, la distorsión dibuja su curva de transferencia, el delay sus ecos, la reverb su respuesta. Lo orgánico invita a
jugar; lo técnico deja entender qué hiciste.

La segunda idea es que **el azar es una invitación, no un accidente**. Cada sesión arranca con macros sorteadas
hacia lo que tus sonidos tienen; SELECT tira el dado del sonido en cualquier página; los dados de macros solo caen
sobre lo que está sonando. El sistema te empuja a mover cosas y siempre te deja cerca de algo que funciona: los
dados parten de presets de fábrica y varían alrededor, nunca del vacío.

La tercera es que **todo se toca y todo se graba**. Los efectos se tocan con las teclas y quedan en una lane
automatizable; las perillas graban su movimiento; las canciones se arman por compases y escenas, y se graban
mientras las tocás. Sin computadora, sin samples: todo se sintetiza en tiempo real en un chip pensado para otra
cosa.

Y la última es una disciplina: **liviano**. Cada gráfico es un puñado de líneas, cada efecto cabe en el
presupuesto de CPU del audio, la firma que aparece al encender son 428 bytes. Un instrumento que se siente grande
dentro de un aparato chico.

## El sistema

### Pantalla viva

![HOME, ANALOG, DRUM-X, LFO, DIST y GLOBAL](docs/jiant-screens.png)

*HOME (el ecosistema), el ser de ANALOG, la colonia de DRUM-X, el LFO corriendo, la curva de DIST y el compás de
GLOBAL, tal como los dibuja el firmware.*

- **Paleta térmica**: valores, curvas, pasos, golpes y notas se colorean por intensidad (cian → azul → violeta →
  rojo → naranja → amarillo → blanco). La selección es violeta y lo activo naranja. Letra
  [Chakra Petch](https://github.com/m4rc1e/Chakra-Petch): técnica, legible, de tecnología primitiva.
- **HOME, el ecosistema**: un ser por track (la especie según su engine, la forma según sus cuatro perillas). Cada uno
  se calienta, se hincha y se mueve con su propio audio; el track elegido se ve más grande; los muteados quedan
  apagados; en silencio todos quedan quietos.
- **Páginas de engine**: el ser del sonido en isotermas (cuatro contornos del mismo cuerpo, el borde frío, el núcleo
  caliente). Las perillas se leen por lo que son: **CUT** abre espinas (más armónicos, más espinoso), **RES** hace
  vibrar la membrana, **DTN/SPRD** separa dos núcleos, **NOIS/RAND/CRSH** deshilacha el borde, **WAVE** cambia los
  lóbulos.
- **DRUM-X, la colonia**: un ser por sonido del kit, cada uno con su forma (kick redondo, snare espinoso, hats con
  flecos, clap en racimo, cowbell en estrella) que se hincha y se calienta cuando golpea.
- **Gráficos técnicos vivos**: LFO (la onda corre a su rate, con cabezal y valor actual), ENV (un punto recorre la
  curva con la voz que suena), DIST (curva de transferencia según TYPE y respuesta de TONE), DLY y DLY 2 (los ecos, L
  y R), REVERB y REVERB 2 (la respuesta al impulso, estéreo), CHORUS (las tres líneas moduladas), MASTER (curva del
  clipper, forma del PUNCH, cuánto baja el DUCK), ENV/LFO DEST (columnas bipolares bajo cada perilla, encendidas por lo
  que mandan), VOICE (una casilla por voz y la curva del glide), GLOBAL (el compás en barras térmicas con el swing, el
  clock y la afinación).
- **Medidor de carga**: cinco rayitas al lado de la batería con la carga de audio del momento.
- **Encendido**: la firma "Jiant FM1" se escribe trazo a trazo en colores térmicos (MENU > ANIM OFF: aparece
  entera).

### Motores
Ocho engines, todos de síntesis, sin samples: **ANALOG, FM6, PHASE, LOFI, VOICE, WHEEL, NOISE y DRUM (DRUM-X)**.
- **ANALOG**: dos osciladores con **INT** (intervalo de ±24 semitonos: quintas, octavas) y DTN; además de SAW, SQR, TRI,
  SIN y PWM tiene **SYNC**, **RING** y **SAW3** (tres sierras). Su filtro es por voz.
- **FM6**: el motor de Dexed (importa .syx), con sus algoritmos graficados.
- **PHASE**: phase distortion con **FB** (la salida realimenta la fase: de borde duro a growl).
- **LOFI**: chip de 1, 4 y 8 bits con **BYTE** (bytebeat: 32 fórmulas elegidas con ALGO y deformadas con VAR). En BYTE,
  **BEND** pliega el tiempo, **RES** da resonancia al filtro y **LOOP** repite un tramo corto de la fórmula: el ruido se
  vuelve un tono afinado a la nota.
- **VOICE** (formantes, según klattsch), **WHEEL** (órgano de drawbars) y **NOISE** (ruido coloreado y metálico).
- **FILTER en todos los motores** (EDIT > FILTER): **TYPE** LP, HP, BP o **COMB** (un comb afinado a la nota que estás
  tocando; CUT lo mueve ±32 semitonos y RES es cuánto resuena), **CUT** y **RES**. En ANALOG es su filtro por voz, con
  DRV; en LOFI también es por voz (su CUT y RES son los de esta página); en los demás es un filtro sobre el track, que sigue al LFO y a la envolvente (LFO/ENV DEST FLT), y su DRV es el
  DIST del track.

### DRUM-X: batería sintetizada con morph
Un kit de 8 sonidos (BD SD CP CH OH TM RS CB) generado en tiempo real, al estilo Microtonic.
- Cada sonido tiene dos lados, **A** y **B**; **MORPH** (KNOB 1) se mueve entre los dos.
- **FOLD** pasa el oscilador de cada sonido por un wavefolder después de su envolvente: el golpe arranca brillante y
  la cola vuelve a la onda limpia. **FM** cambia el timbre con FM armónica en 8 bandas; además TUNE, TONE, DECAY,
  NOISE y DRV.
- **EDIT > SOUND 3** (por sonido): **PMOD** elige cómo se mueve el pitch (DECAY, LONG como el 808, NOISE al azar, SINE),
  **DRV** satura el oscilador para kicks densos, y **WAVE** cambia la onda.
- **Mutes por grupo** (KICK, SNARE, HAT, PERC) con GLO sostenido y **por sonido** con EDIT sostenido.

### Dados: el sistema te invita a moverlo todo
- **SELECT** cambia el tempo en HOME y GLOBAL (y con GLO sostenido). En cualquier otra página **tira el dado del
  sonido** del track: un sinte carga uno de sus presets de fábrica al azar y mueve cada parámetro del engine hasta un
  cuarto de su rango; DRUM-X sortea los lados A y B de cada sonido alrededor del kit de fábrica.
- **Macros M1–M4** (LFO sostenido, y quedan fijas al soltar hasta volver a apretar LFO): cada sesión arranca con dos
  rutas al azar por track hacia lo que su sonido tiene. Con las macros abiertas, **SELECT** tira rutas nuevas, solo
  sobre los tracks que están sonando; F3 también, G3 las borra. Se asignan a mano en MOD (SRC M1…M4) y se guardan con
  el proyecto.

### Punch-in FX: los efectos se tocan y se graban
Con FX sostenido las teclas son efectos que actúan mientras se mantienen; al soltar todo vuelve exacto.
- **De audio**: REPEAT 1/8 · 1/16 · 1/32, LPF y HPF.
- **Sobre las notas, tipo OP-Z**: OCT− y OCT+, 1/2 TEMPO, DECAY corto (también baja el sustain: plucks) y largo,
  STUTTER 1/8 · 1/16, ATK+ (también en la batería), ARP momentáneo (en la batería, un fill distinto cada vez) y RANDOM.
- **Cuantizados**: entran en la próxima semicorchea del transporte; REPEAT y SLICER siguen la grilla desde el primer
  momento.
- **Automatizables**: con REC armado quedan en una lane de 4 compases por sección. A#4 elige si afectan a todo, a los
  sintes o a la batería.

### Secuencias
- 64 pasos por track, piano roll, grilla de batería, parameter locks, chance, ratchets, slide y automatización de
  perillas; grabación en vivo con overdub, metrónomo y count-in.
- **SHIFT**: corre la secuencia en pasos (OFS) y en altura (PIT), automatizable.
- **Una escala para todo**: ROOT y SCALE valen para todos los tracks melódicos; **STRN** mueve todas las secuencias
  por los grados de la escala. **ARP TRNS**: las teclas transponen la secuencia.
- **SEQ + REC** borra todas las secuencias; **REC + FX / EDIT / ENV / LFO…** borra solo la automatización de esa parte.

### Canciones dentro del aparato
**8 canciones**, cada una con **4 secciones (A–D)** y una cadena de filas por compases, al estilo SLOOP: secciones en
vivo que entran en el próximo compás, quick chain, SONG REC y STORE / RECALL. Cada fila puede tener una **escena**
(mutes, mutes de grupo de DRUM-X, macros y punch-in MIDI) que se guarda con KNOB 4 en la página SONG o sola al grabar
con SONG REC.

### Modulación
- **Matriz MOD**: 4 slots por track. Fuentes: LFO, ENV, VEL, KEY, RAND, controladores MIDI, las 4 macros y STEP.
- **MSEQ**: un secuenciador de modulación por track (16 niveles, LEN, DIV, SLEW), como un secuenciador de CV.
- **ENV LOOP**: con la nota sostenida la envolvente vuelve al ataque al llegar al sustain: un LFO con forma de ADSR.

### Efectos y master
- **DIST** por track con TYPE (SOFT, HARD, FOLD, CRUSH, RECT) y TONE; **SLICER** por track.
- **Delay**: TIME sincronizado o libre (5 ms a 1,48 s), un solo TONE, **grain delay** con PITCH y SPRAY (también en
  tiempos largos), WIDTH estéreo.
- **Reverb** de cinco modelos: ROOM, SPRING, **SHIMR** (shimmer, sube una octava en cada vuelta), **RESO** (cuatro
  cuerdas afinadas a la escala del tema) y **CLOUD** (nube granular que se congela); con pre-delay, modulación, filtro
  y width; **chorus**.
- **Master**: **CLIP** (saturación con el nivel compensado: suma carácter, no volumen), **PNCH** (transientes de la
  batería), **DUCK** (el kick baja lo demás) y un **nivelador siempre activo** (compresión 2:1 con make-up automático,
  de −9 a +6 dB) antes del limitador y el soft clip: patches quietos y CLIP fuerte suenan a un volumen coherente.

### Perillas con aceleración
Girar rápido barre el rango entero; girar lento es ajuste fino. Se apaga en MENU > KNOB ACCEL.

### Conexiones y guardado
- USB: MIDI class-compliant y audio (el master llega a la computadora); MIDI por TRS; clock interno, USB o TRS.
- 32 presets de usuario con nombre y autoguardado al apagar.
- **Editor web** de todos los parámetros: patches FM6, grilla, mezcla y backup completo (las 8 canciones con sus
  secciones, filas y escenas).

<details>
<summary><b>Qué cambió respecto de Felucca 1.1.5.1</b></summary>

- **Interfaz nueva**: paleta térmica, letra Chakra Petch, seres, ecosistema en HOME, gráficos técnicos vivos y firma
  al encender.
- **DRUM** es DRUM-X; los kits de Felucca se retiraron. **CHORD** se retiró (sus parámetros son SHIFT). **PHYS** y
  **TRIO** se retiraron (TRIO vive dentro de ANALOG; un sonido PHYS suena como el primer preset de ANALOG).
- **Sin samples**: se fueron los samples de usuario y los engines SAMPLE, SLICE y GRAIN, para hacer lugar a DRUM-X, los
  efectos, los punch-in y las canciones. Un sonido de esos engines carga como ANALOG.
- **8 canciones**: la canción 1 son los 4 proyectos de siempre, así que lo guardado con Felucca aparece ahí.
- **Capa FX**: se fueron REVERSE, TAPE STOP, FREEZE y el armonizador; entraron los punch-in MIDI.
- **SELECT** es tempo solo en HOME y GLOBAL; en el resto, el dado del sonido.
- **EDIT sostenido**: INIT / RECALL en lugar del selector de voces. **Sin undo** (SAVE sostenido: OCT+ guarda la
  sección, OCT− la recupera). **Sin lock de capas.**
- **Nombres USB**: siguen siendo "Felucca" para que el editor web conecte.

</details>

## Controles

- Tocar un botón de página abre su página; otra vez, la siguiente. HOME vuelve a la pantalla principal.
- **Mantener** un botón abre su capa rápida: las teclas y las perillas cambian de función mientras está
  apretado.

| Mantener | Teclas | Perillas |
| --- | --- | --- |
| **FX** | F3 G3 A3 REPEAT 1/8, 1/16, 1/32; B3 LPF; C4 HPF. Punch-in MIDI: D4 OCT−, E4 OCT+, F4 1/2 TEMPO, G4 DEC−, A4 DEC+, B4 C5 STUTTER 1/8 · 1/16, D5 ATK+ (ataque de todo arriba, batería incluida), E5 ARP (batería: un fill nuevo en cada toque), F5 RANDOM (notas y pasos). Todo entra en la próxima 1/16 del transporte. Teclas negras 1–4: mute de T1–T4; A#4 elige a qué tracks afectan los MIDI (todos, sintes, batería). **Automatizar**: con REC armado y tocando, lo que mantengas queda grabado en la lane de la sección (64 pasos de 1/16); G5 la borra donde pasa, o entera con el transporte parado | FILTER, CRUSH, THROW, DEPTH |
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

Otros: **PLAY** arranca y para; **REC** arma el track elegido; **SELECT** cambia el tempo en HOME y GLOBAL y en el resto de las páginas tira el dado del sonido; **ALGORITHM**
elige el track en todas las páginas; **PRESETS** cambia el sonido; **OCT− / OCT+** la octava (en páginas
de acción y diálogos: volver / confirmar); GLO + PLAY reinicia desde el principio.

## Manual de usuario

Una guía práctica de lo nuevo, en el orden en que lo vas a ir encontrando. La tabla de [Controles](#controles) tiene
el detalle de cada capa.

### 1. Moverse
- Cada botón de página (EDIT, ENV, LFO, FX, SEQ, SCL, ARP, GLO, SAVE) abre su primera página; apretarlo otra vez pasa a
  la siguiente. El título arriba dice dónde estás. **HOME** vuelve al ecosistema.
- **ALGORITHM** elige el track (T1–T4) desde cualquier página; las páginas de track muestran el track elegido.
- Las cuatro perillas manejan las cuatro columnas de la pantalla. Girar rápido barre todo el rango, lento ajusta
  fino.
- **Mantener** un botón abre su capa (FX, GLO, SCL, LFO, EDIT, SAVE…): al soltar volvés a donde estabas.

### 2. Leer la pantalla
- **HOME**: cada ser es un track. Su especie dice el engine, su forma sale de sus cuatro perillas, se hincha y se
  calienta con su propio audio. El más grande es el track elegido; uno apagado está muteado.
- **Colores**: frío (cian, azul) es poco, caliente (naranja, amarillo, blanco) es mucho. Violeta es lo seleccionado,
  naranja lo que está activo.
- **Medidor de carga** (arriba a la derecha, al lado de la batería): cinco rayitas que muestran cuánto del tiempo de
  audio está usando el sonido en este momento. La RAM del firmware se reserva entera al arrancar y no cambia, así que
  lo que vale la pena mirar es esto: con las cinco encendidas (rojo, amarillo) estás cerca del límite y conviene
  bajar voces, reverb CLOUD o efectos pesados.
- **Encendido**: la firma se dibuja en unos dos segundos. Si querés arrancar directo: MENU > **ANIM OFF**.

### 3. Hacer un sonido
1. Elegí el track con ALGORITHM y el engine/preset con **PRESETS**.
2. **EDIT** abre las páginas del engine. Cada engine tiene una página **FILTER**: KNOB 1 TYPE (LP, HP, BP, COMB),
   KNOB 2 CUT, KNOB 3 RES y KNOB 4 DRV (en ANALOG la saturación del filtro; en los demás el DIST del track). COMB con
   mucha RES y CUT cerca del centro vuelve metálico cualquier sonido, afinado a la nota.
3. **ENV** y **LFO** tienen sus páginas DEST: cada columna manda a un destino (FLT mueve también el filtro de la
   página FILTER). **ENV LOOP** convierte la envolvente en un LFO mientras la nota está sostenida.
4. **¿Sin ideas?** **SELECT** (fuera de HOME y GLOBAL) tira el dado: carga un preset de fábrica al azar del engine y
   mueve sus parámetros. Tiralo varias veces hasta que algo te guste y seguí desde ahí.
5. **EDIT sostenido**: F3 INIT vuelve el sonido al de fábrica, G3 RECALL al que guardaste en la sección.

**LOFI BYTE** (LOFI con WAVE en BYTE): ALGO elige una de 32 fórmulas de bytebeat, VAR la deforma, BEND pliega el
tiempo (glitches), RES da resonancia al filtro y LOOP repite un tramo cortito: con LOOP alto el ruido se vuelve una
nota afinada. Para el corte usá la página FILTER.

### 4. Batería DRUM-X
- Poné un track en DRUM. **KNOB 1 MORPH** va del lado A al lado B de todo el kit: es la perilla para tocar en vivo.
- **FOLD** pliega la onda de cada golpe: el ataque brilla y la cola vuelve limpia. **FM** ensucia el timbre.
- **EDIT > SOUND 1–3** edita cada sonido (BD, SD, CP, CH, OH, TM, RS, CB; KNOB 1 elige cuál). SOUND 3 tiene **PMOD**
  (cómo cae el pitch: DECAY, LONG tipo 808, NOISE, SINE), DRV y WAVE.
- Mutes: **GLO sostenido** + teclas negras 5–8 (KICK, SNARE, HAT, PERC); **EDIT sostenido** + teclas negras 1–8 (cada
  sonido).
- SELECT en un track DRUM sortea un kit nuevo alrededor del de fábrica.

### 5. Macros (M1–M4)
- **Mantené LFO**: las perillas pasan a ser M1–M4. Soltá LFO y quedan **fijas** (latch) hasta que vuelvas a apretar
  LFO.
- Cada sesión arranca con rutas al azar. Con las macros abiertas, **SELECT** tira rutas nuevas solo sobre los tracks
  que están sonando; **F3** también, **G3** las borra.
- Para elegirlas a mano: LFO > **MOD**, SRC M1…M4 hacia cualquier destino. Se guardan con el proyecto.

### 6. Efectos
- **FX sostenido** convierte el teclado en efectos que actúan mientras mantenés la tecla y entran a tempo: REPEAT,
  LPF/HPF, octavas, stutter, decay corto/largo, ARP, RANDOM… (detalle en [Controles](#controles)). Con REC armado y
  tocando, quedan grabados en la lane de la sección; G5 la borra.
- Páginas **FX** (globales): DIST y SLICER por track, DLY / DLY 2, REVERB / REVERB 2, CHORUS y MASTER.

**REVERB > TYPE** (KNOB 1) elige el modelo. Las mismas perillas cambian de sentido según el tipo:

| TYPE | Qué es | SIZE | DAMP | MOD | RATE | WIDE |
| --- | --- | --- | --- | --- | --- | --- |
| **ROOM** | Sala clásica | Largo de la cola | Opaca los agudos | Modula la cola (chorus) | Velocidad de la modulación | Estéreo |
| **SPRING** | Resorte de amplificador | Largo del resorte | Opaca | Cuánto se bambolea | Velocidad del bamboleo | Estéreo |
| **SHIMR** (shimmer) | Sala que sube una octava en cada vuelta: cola angelical | Largo de la cola | Opaca | **Cuánto shimmer** (0 = ROOM) | — | — |
| **RESO** | Cuatro cuerdas que resuenan **afinadas a la escala** (I, III, V y VII de ROOT/SCALE) | Cuánto sostienen | Oscurece las cuerdas | Más de la mitad: abre el acorde una octava | — | — |
| **CLOUD** | Nube granular: granos del audio pasado, desparramados | Largo de los granos; **al máximo congela** la nube | Opaca | Cuántos granos cambian de altura (×2, ×½, quinta) | Densidad de granos | Estéreo |

PRE (pre-delay) y FILT (tono de entrada) valen para todos. Ideas: RESO con la batería para que el kit cante en la
tonalidad del tema; CLOUD con SIZE al máximo para congelar un acorde y seguir tocando encima; SHIMR con un pad lento.

**MASTER**: CLIP satura sin subir el volumen (el nivelador lo compensa), PNCH marca los ataques de la batería, DUCK
hace que el kick baje lo demás. El nivelador está siempre activo: no hace falta cuidar el volumen entre patches.

### 7. Secuencias y modulación
- **SEQ**: pasos, piano roll, locks por paso, chance, ratchets. **REC** arma la grabación en vivo; mover una perilla
  mientras suena graba la automatización.
- **SEQ > SHIFT**: OFS corre la secuencia en el tiempo, PIT la transpone. **SCL**: ROOT y SCALE valen para todos los
  tracks (y para la reverb RESO).
- **LFO > MSEQ**: un secuenciador de modulación de 16 pasos. KNOB 1 elige el paso, KNOB 2 su nivel, LEN y SLEW su
  largo y suavidad; se manda a un destino desde MOD (SRC STEP).
- Borrar: **REC + botón** borra la automatización de esa parte; **SEQ + REC** borra todas las secuencias.

### 8. Canciones y escenas
- **SAVE sostenido**: F3–B3 lanzan las secciones A–D en el próximo compás (varias seguidas arman una cadena), C4–F4
  guardan la sección actual en A–D, D5 alterna LOOP / SONG, E5 graba la canción en vivo (SONG REC).
- La página **SONG** arma la cadena de filas por compases; KNOB 4 guarda una **escena** (mutes, macros, punch-in) en
  la fila.
- 8 canciones: KNOB 1 con SAVE sostenido elige cuál (entra al soltar, con el transporte parado).

### 9. Guardar
- **Presets de sonido**: SAVE > **USER**: KNOB 1 elige el slot (32), KNOB 4 SAVE guarda el sonido del track, KNOB 2
  LOAD lo carga, KNOB 3 ERASE lo borra. EDIT le pone nombre.
- **Proyecto**: SAVE > PROJECT. Todo se autoguarda al apagar.
- **Backup**: el editor web (USB) descarga y restaura todo: canciones, secciones, escenas y presets.

### Presets personales hechos a medida
Si querés presets propios dentro del firmware: armalos en el aparato, guardalos en USER, bajá el backup con el editor
web y compartilo (o describí la idea: nombre, engine, para qué lo usás). Los mejores pueden entrar como presets de
fábrica en la próxima versión.

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
- Fuentes: [Chakra Petch](https://github.com/m4rc1e/Chakra-Petch) ([SIL OFL 1.1](LICENSES/OFL-ChakraPetch.txt)), la
  letra de la interfaz; hasta la 0.4, [Inter Tight](https://github.com/rsms/inter-tight) ([SIL OFL 1.1](LICENSES/OFL-InterTight.txt));
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
