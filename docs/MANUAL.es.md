# JIANT FM — Manual de usuario

**JIANT FM V10.7.22 para el M-VAVE FM-1**

> *English version: [MANUAL.md](MANUAL.md).*

Este manual es para músicos que acaban de instalar JIANT FM y quieren saber cómo tocarlo, cómo armar temas con él
y, sobre todo, **cómo guardar su trabajo sin perder nada**. Sigue el firmware tal como está en esa versión: cada
botón, perilla y mensaje aparece nombrado exactamente como lo muestra el equipo.

Si vas a leer un solo capítulo, leé [12. Guardar y cargar](#12-guardar-y-cargar). Si venís de la V10.6c (la última
versión pública) o de antes: [Qué hay de nuevo en esta versión](#qué-hay-de-nuevo-en-esta-versión).

## Contenido

- [Qué hay de nuevo en esta versión](#qué-hay-de-nuevo-en-esta-versión)
0. [Inicio rápido](#0-inicio-rápido)
1. [El panel](#1-el-panel)
2. [La pantalla](#2-la-pantalla)
3. [Páginas y capas](#3-páginas-y-capas)
4. [Motores](#4-motores)
5. [DRUM-X](#5-drum-x)
6. [El secuenciador](#6-el-secuenciador)
7. [ARRANGE: canciones, secciones y escenas](#7-arrange-canciones-secciones-y-escenas)
8. [Dados y UNDO](#8-dados-y-undo)
9. [Efectos y master](#9-efectos-y-master)
10. [Modulación y macros](#10-modulación-y-macros)
11. [Tocar en vivo](#11-tocar-en-vivo)
12. [Guardar y cargar](#12-guardar-y-cargar)
13. [Sistema: TRACKS, LOAD y el MENU](#13-sistema-tracks-load-y-el-menu)
14. [MIDI y USB](#14-midi-y-usb)
15. [Herramientas web: emulador, instalador, editor, backup](#15-herramientas-web-emulador-instalador-editor-backup)
16. [Problemas y preguntas frecuentes](#16-problemas-y-preguntas-frecuentes)

---

## Qué hay de nuevo en esta versión

Todo lo que cambió desde la V10.6c, la última versión pública, en una lista; cada punto lleva a su lugar en el manual.

**Antes de actualizar**
- **6 canciones, no 8** (V10.6p). Las canciones 7 y 8 ya no se abren: esa memoria queda para un banco de samples de
  batería. Pasalas a una entre 1 y 6, o hacé antes un backup completo (**Save backup** del editor, o el backup del
  instalador); el backup guarda las ocho, y al restaurarlo te dice qué canciones quedaron solo en el archivo.
- **Proyectos muy viejos retirados** (V10.6s): los proyectos guardados por JIANT antes de la 1.0.3 (FUN1 a FUN6) se
  rechazan sin tocarlos, y un sonido DIGITAL carga como el init de FM6. Si todavía los necesitás, abrilos en una
  versión anterior (V10.6c) y guardalos de nuevo ahí.
- **Proyectos más viejos retirados** (después de V10.7.13, lugar para FORM MY LOOP de GENE): los proyectos y backups
  guardados en los formatos FUN7 a FUNB (Felucca 1.0 / 1.1 y JIANT hasta 0.5) ya no se leen; al encender, un proyecto
  así muestra **OLD PROJECT: NOT READ** y su slot queda vacío (la flash lo conserva hasta que guardes encima). El editor
  web no los convierte: abrilos en V10.7.13 o anterior y guardalos de nuevo ahí (así quedan en el formato de hoy).
  También se fueron: las conversiones de presets de usuario viejos (SAMPLE PERC, TRIO, los chips de LOFI, valores de
  0.3 / 0.4: un preset así carga como estaba guardado, y si es de un motor retirado suena como ANALOG), la pista de un
  motor retirado en un proyecto ahora recibe el primer preset de ANALOG (no su sonido más parecido), y la criatura propia
  de ADD (ADD dibuja el ser, como los otros motores antes de 0.9.6).
- **FM-1 nuevos** (V10.6z): los FM-1 vendidos desde fines de 2026 (con el logo en el frente) traen otro chip de
  flash. Las versiones anteriores de JIANT andaban en esos equipos solo desde la RAM (sin guardar, sin GENE, sin
  actualizarse desde el equipo); esta versión guarda normalmente. Si el tuyo no conservaba tus canciones, actualizá.

**Canciones**
- SAVE abre en el tema: **TIMELINE**, **PARTS** y **CLIPS** (V10.6o, V10.6s), después SECTIONS. Ver
  [TIMELINE, PARTS y CLIPS](#timeline-parts-y-clips-save--song).
- **Un tema conserva los sonidos que cambiás mientras suena** (V10.6z8); EDIT > **RECALL** trae de vuelta el sonido de
  la sección en una pista.
- **Guardar un tema** se hace en SAVE > PROJECT > **PROJECTS**: KNOB 1 elige el proyecto (no lo abre), SAVE le pone
  nombre y en la pantalla NAME el KNOB 3 elige el tema entero (SONG) o solo el LOOP (la página SAVE SONG AS ya no está).

**Batería y generador**
- **DRUM-X**: una pasada de calidad (V10.6q), un cencerro CMOS clásico, el mapa de teclas **EXTENDED** por defecto con
  redobles de verdad y cada sonido cromático (V10.6r), ACOUST ya no está (V10.6o), y volvieron la pegada de los bombos y
  los hats brillantes (V10.6z9). Ver [Las teclas: EXTENDED](#las-teclas-extended-v106r).
- **GENE** (el generador de HOME, GEN hasta la V10.6m) toca los géneros que eligió el autor, entre ellos CUMBIA, EBM,
  BAILE FUNK, PHONK, HIP HOP (trap, drill, pluggnb, boom bap o lofi), JAZZ en cualquier tempo y TECHNO ROCK (rock o
  synth 80s). El rol DRUM de SCL > SCENE puede tomar un kit generado para el género (**SCENE DRUM GEN**).
- La **paleta térmica** en todos lados: cada nota con su calor, el piano roll coloreado por nota (V10.6w).
- **Siete paletas, todas propias de JIANT** (MENU > DISPLAY > COLOR): **JIANT** (como estaba, la que viene por
  defecto), **SELVA** (verdes), **CONTRAST** (accesible: negro y blanco puros, grises más claros, sin tinte en el
  panel), **BRASA** (cálida), **ABISAL** (fría), **HUESO** (blanco hueso y un solo acento naranja) y **NEON** (magenta y
  cian). La rampa térmica (los seres, las notas, las barras) está en las siete; las cartas de las perillas toman cuatro
  colores propios de cada paleta (JIANT: el calor de cada valor, como antes). Reemplazan a
  GREY, GREEN, AMBER, ICE, VIOLET, ROSE, PAPER, HI-CON, NIGHT y MONO; si tenías guardada una de esas, quedás en la más
  parecida (GREY, MONO, NIGHT → HUESO; GREEN → SELVA; AMBER, ROSE → BRASA; ICE → ABISAL; VIOLET → JIANT; PAPER,
  HI-CON → CONTRAST).
- **Dos pieles**: MENU > DISPLAY > SKIN es **AIR** o **COLORS**. BOTTOM y HUD ya no están (si tenías guardada alguna,
  cargás con COLORS).
- **Sin la fila STYLE**: el aspecto relleno (FLAT) es el único estilo; LINE ya no está (si lo tenías guardado, cargás
  con FLAT) y el menú tiene una fila menos.
- **Letras chicas más grandes**: los nombres, las pestañas, las palabras del encabezado, la miga y los rótulos de los
  paneles tienen mayúsculas de 9 px (eran letras de píxel de 7 px). Las ocho pestañas de SEQ y de FX se ven de a cinco;
  el menú lista ocho filas por vez; se fueron algunos datos que repetían una carta (QUANT y ALL TR a la derecha de la
  rueda de SCL, los números adentro de los anillos de BEING).
- **Valores más grandes**: el número de una carta va en cifras de 14 px (eran de 11) cuando entra con su unidad, y gira
  como antes; los nombres y las palabras siguen en la letra de 11 px. El medio del encabezado dice el motor y el sonido
  sin "T1 ·" (el número de la pista está en su cajita); se fueron la línea "FX · T1" de FX y los nombres de la página y
  del sonido adentro del panel de EDIT (los dicen el encabezado y la miga).
- **MENU > LARGE** en las páginas de SEQ, SAVE y FX: las cartas altas que ya tenían HOME y EDIT (el piano roll, la
  grilla de batería y las listas conservan su panel, con las cartas normales).

**Secuenciador**
- El componente **RAMP** reemplaza a JUMP (V10.6v): el lock de parámetro de un paso se desliza hasta su valor en 1–32
  pasos o 1–16 compases.
- **REC respeta cómo tocás** (V10.6z4): el largo de las notas, el legato como SLIDE, la velocidad tal cual. **GATE
  100 %** en las pistas nuevas.

**Tocar**
- **Velocidad y aftertouch en las teclas** (V10.6z7, a partir de *Velo* de Charles Vestal): F3..C5 sensibles, cada
  tecla se calibra mientras tocás; MENU > CONTROL > **VELOCITY**, **VEL MIN**, **AFTERTOUCH**, **VEL LEARN**. Ver
  [Velocidad y aftertouch](#velocidad-y-aftertouch-de-las-teclas-v106z7).
- **Teclas de expresión** (V10.6z4): una nota mantenida + OCT− un vibrato, + OCT+ un bend (MENU > AUDIO > **OCT BEND**).
- **SWARM** (el instrumento de Bluetooth de EXT): una melodía según qué tan cerca estás, el morph FAR / NEAR, la
  caminata por la escala y **CHAOS** (donde estaba la perilla SELF).
- La calibración FAR / NEAR ahora toma la mediana de 2,5 s (**HOLD STILL**) (V10.6z8).
- **VOICE** ya no satura con las vocales abiertas; WORDS tiene **CONS**, el largo de las consonantes (V10.6y).

**Conexiones**
- **NRPN** para todo parámetro automatizable, y las perillas salen como NRPN con MIDI OUT ALL (V10.6z3): [docs/NRPN.md](NRPN.md).
- El protocolo del editor es un contrato versionado (1.2): [web/EDITOR_PROTOCOL.md](../web/EDITOR_PROTOCOL.md).
- El segundo núcleo arranca solo con la alimentación de fábrica (después de un SAFE START: un núcleo) (V10.6z2).

---

## 0. Inicio rápido

### Instalación

1. Abrí el **instalador web** en Chrome o Edge en una computadora, o en Chrome en Android con un cable USB OTG
   (iPhone y iPad no tienen Web MIDI): <https://juanjiant-bit.github.io/JIANT/webapp/installer/>
2. Conectá el FM-1 **directamente** con un cable USB de datos.
3. **¿Venís de otro firmware custom (Felucca, Skiff, baudgirl o cualquier otro)?** Volvé primero al oficial con el
   botón **Volver al oficial** del instalador (que además guarda el backup de ese firmware). Escribir JIANT
   directamente encima de otro firmware custom ya dejó algún FM-1 sin poder arrancar.
4. Apretá **Instalar**. Primero revisa el firmware del FM-1 y solo escribe sobre el **V16 oficial**, el **V15 oficial**
   o JIANT (si encuentra otro, te manda a volver al oficial primero). No desenchufes el cable mientras escribe.

**Volver al oficial** acepta el `FM-1.fwsc` de M-VAVE del **V16** (718.196 bytes, el que ofrece hoy) o del **V15**
(699.956 bytes), descomprimido de la página de descargas de M-VAVE y verificado byte por byte. Primero guarda un
backup completo de JIANT; para volver, apretá Instalar y restaurá el backup con el editor.

JIANT no tiene samples de usuario: si venís de Felucca, sus samples de usuario (USR1–3) se pierden al instalar.
Guardá tus archivos WAV.

### Tu primer sonido (1 minuto)

1. Encendé el equipo. La firma se dibuja sola y después llegás a **HOME**: seis criaturas, una por pista. (V10.6e) Las
   pistas arrancan como una banda afinada: **T1 DRUM-X** (ACOUSTIC), **T2 ANALOG** (PLUCK), **T3 PHASE** (INIT SAW), **T4 FM6** (FM BASS), **T5 VOICE** (CHANT) y **T6 ANIMAL** (WHISTLE), cada una con el sonido de su
   preset, seco.
2. Girá **ALGORITHM** para elegir una pista (T1–T6). Con el valor por defecto, **TRACKS 4**, se detiene en T4: T5 y T6 aparecen solo con TRACKS 6 (ver
   [13](#13-sistema-tracks-load-y-el-menu)).
3. Girá **PRESETS** para recorrer sonidos. Tocá las teclas.
4. Girá **KNOB 1–4**: en HOME son los cuatro controles más útiles del motor de la pista.
5. ¿No te gusta? Mantené **EDIT** y girá **SELECT**: el sonido se tira a los dados y sale una variación nueva, que escuchás al
   instante. **OCT+ OK** la deja, **OCT− CANCEL** devuelve el sonido.

### Tu primer tema (5 minutos)

1. Mantené **HOME** y girá **SELECT** (o en HOME tocá **HOME** dos veces: ECO → BEING → **GEN**). Se abre el **GENERATOR** en
   **EXPLORE** (ningún género, todas las reglas rotas): apretá **OCT+** así, o antes elegí RANDOM o uno de los géneros
   con KNOB 1 o PRESETS. Aparece una idea de tema completa: motores, sonidos, partes, escala, tempo y efectos. Apretá
   **PLAY**.
2. ¿No te convence? Apretá **OCT+** otra vez: semilla nueva, pieza nueva. ¿Demasiado cargada? Girá **SELECT** un paso a la
   derecha para GEN 2 y bajá **DRUMS** o **ARP**, después OCT+: la misma pieza con eso cambiado. **OCT−** vuelve a ECO. Para volver a lo que había antes: mantené **GLO** y apretá **G4** (UNDO).
3. ¿Te gusta? **Pará** (PLAY), apretá **SAVE** (abre TIMELINE, el tema) y girá **SELECT** tres pasos a la derecha
   (pasando PARTS y CLIPS): **SECTIONS**, las cuatro secciones del proyecto. Girá **KNOB 4** a la derecha para que su tarjeta diga
   **OCT+** y apretá **OCT+**. Escribí un nombre con las teclas (o dejá el que te ofrece) y apretá **OCT+** otra vez. La
   pantalla dice **SAVED**.
4. Hacé una variación (cambiá algunos pasos, muteá una pista) y guardala en la sección **B**: en SECTIONS girá
   **KNOB 1** hasta **B** y después KNOB 4 y OCT+ como antes.
5. Girá **SELECT** dos pasos a la derecha (pasando PROJECTS): **ARRANGE** (las filas del tema). Apretá **OCT+** para agregar una fila,
   elegí su sección con **KNOB 2** y su largo con **KNOB 3** (compases). Agregá una fila para A y otra para B. Apretá
   **PLAY**: el tema reproduce las filas en orden.

Ese es todo el ciclo: **hacer música → guardarla en una sección → encadenar secciones en ARRANGE**.

---

## 1. El panel

Los nombres de abajo son los que usa el firmware (la pantalla HARDWARE CALIBRATION los pide con estos nombres).

### Encoders y perillas

| Control | Qué hace |
| --- | --- |
| **SELECT** (encoder) | Solo (0.9.2): **recorre las páginas de la sección**, una página por paso, en una sola cadena, frenando en la primera y en la última (sin niveles, sin zoom; ver [Cómo funciona la navegación](#cómo-funciona-la-navegación-seis-reglas-092)). En el MENU: sus filas, las cuatro pestañas en una cadena. Con un botón mantenido: **HOME** + SELECT **GEN**, **EDIT / SEQ / LFO / FX** + SELECT los **dados** de esa parte, **GLO** + SELECT el **tempo**, **ENV / SCL / ARP / SAVE** + SELECT recorren la cadena de esa sección desde donde estés. |
| **ALGORITHM** (encoder) | Elige la pista desde cualquier página: T1–T6 (T1–T4 con TRACKS 4). La pista es siempre de ALGORITHM (0.9.2: PRESETS ya no la elige en ENV, LFO, CHORD, ARP). En el MENU: la pestaña siguiente / anterior. Con una capa mantenida: su vista, MAP ↔ KNOBS ↔ STATE. |
| **PRESETS** (encoder) | **Siempre presets** (0.9.2): los sonidos de la pista seleccionada (los presets de fábrica de cada motor y después tus presets de usuario) en todas las páginas; en las páginas **SEQ** los **patrones** (abre el cargador PHRASES con el patrón siguiente elegido: OCT+ lo carga, OCT− vuelve a la página SEQ); en **GEN** también los sonidos (V10.7.22; su GENRE es la KNOB 1 de GEN 1). En el MENU y con un botón mantenido no hace nada, salvo con **EDIT mantenido** (V10.7): salta al primer sonido del motor siguiente / anterior, con el nombre del motor en pantalla. Nunca cambia de página, de pista, de módulo ni de fila. |
| **KNOB 1–4** | Las cuatro tarjetas (columnas) de la pantalla. Una página que elige algo lo elige con **KNOB 1** (el paso, el compás, el slot, la etapa, el operador, el sonido del kit, la fila). Con una capa mantenida, las perillas propias de la capa. Girar rápido recorre todo el rango, despacio ajusta fino (MENU › CONTROL › KNOB ACCEL lo desactiva). En el MENU: KNOB n el ajuste de la tarjeta n. |
| **MASTER** | El volumen de salida. |

### Botones

| Botón | Toque | Mantener |
| --- | --- | --- |
| **HOME** | HOME (0.9.2, como el botón de cualquier sección): desde otra sección, la última página de HOME; en HOME la primera página del grupo siguiente, en ronda: **ECO → BEING → GENE → ECO** (GENE: el generador, GEN hasta la V10.6m). | Unos 0,7 s: abre el **MENU**. |
| **ENV** | La sección ENV: desde otro lado, la última página mostrada (al encender, ENV); adentro, el grupo siguiente: **TRACKS → ENV → STAGE**, en ronda. | (sin capa) |
| **LFO** | La sección LFO: desde otro lado, la última página mostrada (al encender, LFO); adentro **MOD → LFO → MSEQ**, en ronda. | Capa **MACRO**: KNOB 1–4 son M1–M4. |
| **FX** | La sección FX: desde otro lado, la última página mostrada; adentro, el módulo siguiente: **TRACK → BUSES → DELAY → REVERB → CHORUS → SLICER → PUNCH → MASTER** (MASTER · TAPE · EQ), en ronda. | Capa de **FX punch-in**. |
| **SCL** | La sección SCL (0.9.2: propia, separada de ARP): **KEY → CHORD → SCENE** (V10.24). | Capa **SCL**: las teclas fijan la tónica. |
| **EDIT** | La sección EDIT: desde otro lado, la última página mostrada (una memoria para los sintes y una para FM6 y otra para DRUM); adentro el grupo siguiente: **SOUND → TONE → FOCUS** (FM6: SOUND → TONE → OPERATOR; DRUM: KIT → SOUND → LEVELS → TONE). Siempre EDIT (0.9.2: borrar un paso es SEQ + G5, renombrar SAVE + C5). Un paso mantenido + EDIT sigue borrando los locks de ese paso. | Capa **EDIT**: INIT, RECALL, mutes de los sonidos de DRUM-X, SELECT los dados de sonido. Mantenido + KNOB 1–4: **MOMENTARY** (la página de abajo, vuelve al soltar). |
| **GLO** | La sección GLO: desde otro lado, la última página mostrada; adentro **LIVE → MIX → COMPS → GLOBAL → SYSTEM**, en ronda (al prender MIXER; LIVE, las secuencias en vivo, primero; V10.37: COMPS después de MIX). | Capa **GLO**: mutes, solos, niveles, tap tempo, UNDO. |
| **SAVE** | La sección SAVE: desde otro lado, la última página mostrada, y USER o SECTIONS llegan con su **SAVE ya elegido** (SAVE, OCT+, OCT+ como antes); adentro **TOOLS → SONG → PROJECT → SOUND**, en ronda (V10.2: primero el tema; al prender TIMELINE; V10.7.22: TOOLS, con CLEAR PROJECT, es la primera página, un paso de SELECT a la izquierda de TIMELINE). | Capa **SAVE**: tocar/guardar secciones en vivo; C5 NAME. |
| **ARP** | La sección ARP (ARP · ARP 2, un grupo): desde otro lado, la última página mostrada; adentro, vuelve a ARP. Un acorde mantenido + ARP al grabar por pasos lo sigue arpegiando. | (sin capa) |
| **SEQ** | La sección SEQ: desde otro lado, la última página mostrada (al encender STEP, el piano roll); adentro el grupo siguiente (V10.6): **STEP → LENGTH → EUCLID → PLAYHEADS → STRETCH → COMP → AUTO → GENERATE**, en ronda. | Capa **SEQ TOOLS** (en todas las páginas SEQ); G5 CLR STEP. |
| **PLAY** | Arranca / para. Mantenido (~0,7 s): **PANIC**. | — |
| **REC** | Arma / desarma la grabación en la pista seleccionada. | Capa **REC**: CLEAR, CLICK. (V10.7) Mantenido más que HOLD y soltado sin tocar nada más: **UNDO** (otra vez: rehacer). |
| **OCT− / OCT+** | Octava del teclado (−3 a +3; los dos juntos: vuelve a 0; en una pista DRUM el kit queda en sus teclas: ratchets arriba, SHORT / GHOST / FLAM abajo, ver [5](#5-drum-x)). En páginas de acción y en diálogos: **OCT−** = atrás / NO, **OCT+** = hacerlo / YES. | — |

### Las teclas

27 teclas de **F3 a G5** (16 blancas, 11 negras). Tocan la pista seleccionada. Con una capa mantenida se convierten en
atajos de esa capa; en la grilla DRUM son pasos y carriles; en la pantalla NAME escriben.

### Combinaciones útiles

| Combinación | Resultado |
| --- | --- |
| **GLO** mantenido + **PLAY** | Reinicia desde el principio sin parar (clock interno). |
| **PLAY** mantenido (~0,7 s) | **PANIC: ALL NOTES OFF**. Para el transporte y corta todo el sonido de una: las voces de todas las pistas, los acordes en LATCH, los arpegios, las repeticiones del punch-in y las notas que tiene MIDI IN (una tecla o nota MIDI que soltás después no hace nada; la siguiente suena como siempre). MIDI OUT manda el note-off de cada nota que mandó y All Notes Off (CC123) en los canales 1–6 de las pistas (no con MIDI OUT en OFF). Usalo si te queda una nota colgada, por ejemplo de un controlador que perdió su note-off; el CC120 / CC123 de MIDI IN sigue haciendo lo mismo en su canal. |
| **GLO** mantenido + **SELECT** | Tempo, desde cualquier página. |
| **GLO** mantenido + **G4** | **UNDO** del último cambio grande (otra vez: rehacer). |
| **REC** mantenido solo, soltado | (V10.7) También **UNDO**: mantenido más que HOLD (~0,5 s) sin tocar tecla, perilla, botón ni OCT; otra vez: rehacer. Si usás otra cosa mientras lo mantenés, es la capa REC; un toque sigue armando la grabación. |
| **SEQ** mantenido + **REC** | **CLEAR ALL SEQUENCES?** — los pasos y la automatización de las seis pistas, y el carril de punch-in. |
| **REC** mantenido + **FX / EDIT / ENV / LFO / SCL / ARP / GLO** | Borra la automatización de esa parte en la pista seleccionada (GLO: la automatización de nivel y paneo de todas las pistas; FX: también el carril de punch-in). Sin diálogo. |
| **SAVE** mantenido + **OCT+** / **OCT−** | Guarda la música en la sección que está sonando / recupera esa sección tal como quedó guardada. |
| **HOME** mantenido + **SELECT** | **GEN**, el generador de temas (las últimas páginas de HOME; ver [8](#8-dados-y-undo)). |
| **EDIT / SEQ / LFO / FX** mantenido + **SELECT** | Los dados: el sonido / la secuencia / el LFO / los efectos (ver [8](#8-dados-y-undo)). |
| **ENV / SCL / ARP / SAVE** mantenido + **SELECT** | Entra a esa sección y recorre su cadena, como SELECT adentro. (0.9.2: botón mantenido + **PRESETS** ya no recorre páginas.) |
| **EDIT** mantenido + **PRESETS** | (V10.7) El primer sonido del **motor** siguiente / anterior (su nombre en pantalla), en vez de recorrer los presets de a uno. No mientras hay un ajuste momentáneo en curso (EDIT mantenido + una perilla girada). |
| **SEQ** mantenido + **G5** | **CLR STEP**: borra el paso del cursor y sus locks (NOTE con una nota en foco: esa nota; AUTO LIST: borra el registro). |
| **SAVE** mantenido + **C5** | **NAME**: renombra el slot en SAVE > USER, la sección o el proyecto en SECTIONS / PROJECTS. |
| Un paso mantenido (SEQ > STEP) + **KNOB 1–4** | Un parameter lock en ese paso. |
| Teclas de paso mantenidas (SEQ, el roll / la grilla) + **SELECT** / una tecla de paso + **REC** | **COPIAR** esos pasos / **PEGARLOS** desde ese paso (ver Copiar y pegar). |
| **OCT− + OCT+** mantenidos al encender | HARDWARE CALIBRATION. |
| **OCT− + OCT+** mantenidos 5 s con el equipo funcionando | Modo de actualización (UBOOT), con una cuenta regresiva desde los 2 s; soltá para cancelar. |
| Una nota mantenida + **OCT−** / **OCT+** (V10.6z4) | **Teclas de expresión**: OCT− mantenido hace entrar un vibrato como al subir una rueda de modulación (~200 ms), OCT+ mantenido dobla la nota hacia arriba con una curva suave fija (~300 ms) hasta el rango de MENU > AUDIO > **OCT BEND** (2 semitonos por defecto); al soltar vuelven solas. Sin ninguna nota mantenida, OCT− / OCT+ cambian la octava como siempre (OCT BEND OFF: siempre). Se graban como automatización al grabar (REC + EDIT la borra); con MIDI OUT ALL salen como CC1 y pitch bend (poné el rango de bend del receptor igual al de OCT BEND). Las dos mantenidas sobre una nota nunca inician UBOOT. (V10.7.22) También son las fuentes **OCT+** y **OCT-** de la matriz MOD: una pista con un slot de MOD que rutea OCT+ (SRC, DEST y AMT puestos) barre ese DEST con la misma curva en vez de doblar la nota (y no manda pitch bend); una que rutea OCT- recibe el mismo vibrato de 5,5 Hz, entrando de a poco, en ese DEST en vez de en la afinación (el CC1 sale igual). |


### Velocidad y aftertouch de las teclas (V10.6z7)

Las teclas F3..C5 del FM-1 apoyan sobre una pastilla de carbón cuya resistencia baja cuanto más apretás (el método es
de Charles Vestal, *Velo*, github.com/charlesvestal/fm1-velocity, GPL-3.0). JIANT la lee en cada toque:

- **Velocidad**: qué tan fuerte llega la tecla, en los primeros milisegundos después de tocar. Es continua (1..127:
  cada toque entre suave y fuerte da un valor intermedio), y después pasa por la curva de MENU > CONTROL >
  **VELOCITY** y el piso de **VEL MIN**. En una pista de sintetizador una nota espera su golpe como mucho 2,2 ms (V10.7.10: un límite fijo, aunque se pierda una lectura de la tecla), por debajo de lo que se nota; en una pista **DRUM** (la elegida, o la mitad de KEY SPLIT que va a una) no espera nada: suena enseguida con el golpe leído hasta ahí, o el promedio reciente de la tecla. Llega a todo lo que toca una tecla: el sonido (los motores y el VEL de la matriz), los
  acordes y el strum, el arpegiador, la grabación (solo 127 se graba como acento) y el MIDI OUT. VELOCITY **OFF**:
  todas las teclas 100, como antes.
- **La fila de arriba (C#5..G5)** no tiene sensor (el FM-1 no puede leer la presión de esas siete teclas), así que toca con el
  **promedio de tus golpes recientes** (100 hasta que toques las otras). No tiene aftertouch (un acorde con teclas
  sensadas conserva el de ellas).
- **Calibración**: cada tecla es distinta, y cada una aprende tus toques suave, normal y fuerte mientras tocás (VEL
  LEARN ON, por defecto: se mueve despacio, un toque raro no la desarma). Empieza de la calibración de fábrica (medida
  en el FM-1 del dueño) y se guarda en la configuración, medio minuto después de que dejás de tocar. **VEL LEARN
  RESET** vuelve a la de fábrica. Para verla: la página <https://juanjiant-bit.github.io/JIANT/webapp/vel/> (*En vivo*, Chrome, por el cable USB del editor) muestra la velocidad de cada nota y
  la calibración de cada tecla.
- **Aftertouch**: con una tecla abajo, apretala más: su presión (0..127) es la fuente **AT** de la matriz (EDIT > MOD:
  cualquier destino: filtro, profundidad del vibrato, nivel ...). Los sonidos nuevos (INIT SOUND, un proyecto nuevo)
  la mandan al filtro (ranura 4, AT -> CUT +20) si esa ranura está libre; los proyectos y presets guardados mantienen su
  propia matriz. Un dedo apoyado no hace nada (una zona muerta), el valor se desliza (sin escalones) y al soltar la
  tecla vuelve a 0. MENU > CONTROL > **AFTERTOUCH** elige la sensibilidad (LOW, MID, HIGH) o lo apaga (OFF). Funciona
  también con OCT− / OCT+ mantenidos; se graba como automatización junto con las teclas de expresión y sale como
  aftertouch de canal con MIDI OUT ALL.
- **El VEL de un sonido** (EDIT > VOICE 2, KNOB 3; 0..100 %, todos los motores menos DRUM): cuánta velocidad le llega
  a *este* sonido. **100 %** (todo proyecto, preset y canción de antes) toca cada nota tan fuerte como se tocó; menos
  acerca la velocidad de cada nota a 100 (50 %: un toque de 60 suena 80, uno de 120 suena 110); **0 %** toca todas las
  notas a 100, como VELOCITY OFF, pero solo en esta pista. Vale para todas las fuentes: las teclas, los pasos del
  secuenciador, el arpegiador, las repeticiones del punch-in y el MIDI de entrada; (V10.7.22) Es solo cuánta velocidad le llega al
  *sonido* (su nivel y la respuesta propia del motor): la fuente **VEL** de la matriz sigue siempre la velocidad tal como se tocó (las teclas, la de cada paso, el arpegiador, el MIDI de entrada, un teclado externo), sea cual sea VEL, 0 % incluido — VEL 0 % con VEL → CUT: todas las notas igual de fuertes y el filtro siguiendo cuán fuerte tocás. El **ACENTO** de un paso sigue sonando 127, y HUMAN y el DECAY de un RATCHET siguen variando las notas. La grabación guarda la velocidad que tocaste: si después subís VEL,
  vuelve la dinámica. Los presets de **bajo** de fábrica cargan en **0 %** (un bajo queda mejor parejo), el bajo de GENE
  en **75 %** (sus notas fantasma compuestas siguen más suaves); los demás presets en 100 %. Se guarda con el sonido (proyectos,
  presets de usuario, canciones); NRPN 80, y el grupo VOICE del editor web.

---

## 2. La pantalla

### El aspecto: COLORS

Las lentes tienen una sola piel, **COLORS** (MENU > DISPLAY > SKIN y AIR, su otra opción, ya no están desde la V10.7.23:
si tenías guardada AIR, o las viejas BOTTOM y HUD, cargás con COLORS): la estructura LENTE con los **colores de la 0.6**.
Las **cartas de las perillas no tienen caja**: cada una va sobre el fondo, con su forma dibujada como una **línea fina
del color de su valor**, así que la línea cambia con el valor (y con la paleta); la perilla que girás lleva la línea de
2 px. El nombre de la carta va en gris (MID), el valor y su barra en el **color de cada perilla** (JIANT: el color de
cada parámetro **se mueve con su cantidad**, como calor, violeta si es poco, amarillo y crema si es mucho; uno centrado
como PAN o DETUNE frío en el medio y caliente en los dos extremos; las otras seis paletas: cuatro colores propios, uno
por perilla); la pestaña del grupo rellena de violeta; el número de la pista en una cajita naranja, un ▶ violeta y el
BPM grande en violeta en el encabezado; los seres con el calor de la 0.6 (púas violetas, anillos rojo y naranja, núcleos
crema). El dibujo de abajo de las cartas va sobre el fondo (sin caja de panel). Las cartas de NOTICE y de NAME van
dibujadas igual, con su línea.

### El MENU (mantené HOME)

**SELECT** recorre todas las filas (0.9.2), las cuatro pestañas en una sola cadena (DISPLAY → CONTROL → AUDIO →
SYSTEM; frena en la primera y en la última), **ALGORITHM** salta a la pestaña siguiente / anterior (a su primera fila),
**cualquier perilla** u **OCT+ / OCT−** cambia el valor; PRESETS acá no hace nada. HOME lo cierra (en ese momento se
guarda la configuración). La pantalla muestra las filas de la pestaña, las cuatro de la fila elegida como tarjetas
(ROWS 1–4 / ROWS 5–6; DISPLAY: ROWS 5–7; CONTROL también ROWS 9–12, las cuatro de las teclas) con el número de su perilla; las pestañas arriba, el pie **MENU · DISPLAY ·
SCOPE  2/34**. El encabezado dice MENU · DISPLAY.

| Pestaña | Fila | Valores |
| --- | --- | --- |
| DISPLAY | COLOR | **JIANT** (por defecto), SELVA, CONTRAST, BRASA, ABISAL, HUESO, NEON — siete paletas de la misma familia, todas oscuras, todas con la rampa térmica, cada una con sus cuatro colores de perilla |
| DISPLAY | SCOPE | **OFF** (de fábrica), ON — (0.9.7) las páginas EDIT de un motor: OFF la criatura en todo el panel, centrada; ON la criatura a la izquierda, la carta SCOPE y las fichas como antes. Los ajustes guardados antes de 0.9.7 cargan OFF |
| DISPLAY | LARGE | OFF, ON (etiquetas y valores grandes) |
| DISPLAY | ANIM | ON, OFF (los valores saltan directo; la firma de encendido aparece entera) |
| DISPLAY | LEDS | OFF, DIM LO, **DIM HI** (por defecto), INV — (0.7.3) los LEDs del brillo de reposo se turnan cuadro a cuadro, así el brillo ya no zumba por el parlante cuando la música para |
| DISPLAY | SCREEN OFF | **NEVER** (por defecto), 5 MIN, 15 MIN, 30 MIN, 60 MIN |
| CONTROL | HOLD | 0.3 s, **0.4 s** (por defecto), 0.5 s, 0.6 s — cuánto tarda un botón mantenido en abrir su capa |
| CONTROL | KNOB ACCEL | **ON** (por defecto), OFF |
| CONTROL | FX LATCH | **OFF** (por defecto desde 0.6.64), ON |
| CONTROL | BPM LOCK | OFF, ON — DICE ALL conserva el tempo (el tempo es GLO + SELECT, F4 TAP de GLO o el BPM de GLOBAL) |
| CONTROL | SCALE LEDS | OFF, ON — las teclas de la escala se iluminan, la tónica más brillante |
| CONTROL | MIDI OUT | **KEYS**, CLOCK, ALL, OFF — qué sale por MIDI USB (y Bluetooth; ver [14](#14-midi-y-usb)) |
| CONTROL | MIDI MAP | **FACTORY** (el mapa de CC estándar), CUSTOM ADD (el mapa y tus CC aprendidos), CUSTOM (solo tus CC aprendidos) — ver [MIDI LEARN](#midi-learn-0912) |
| CONTROL | LEARN | **OFF**, SEL TRACK, FIXED TRK, CLEAR ALL — arma el MIDI LEARN al cerrar el MENU; no se guarda |
| CONTROL | VELOCITY | OFF, SOFT, **NORMAL** (por defecto), HARD — las teclas suenan tan fuerte como las tocás (F3..C5; la fila de arriba C#5..G5 no tiene sensor y sigue tu dinámica reciente). La curva: SOFT más fuerte con menos, HARD pide más. OFF: todas las teclas 100, exactamente como antes. Ver [Velocidad y aftertouch de las teclas](#velocidad-y-aftertouch-de-las-teclas-v106z7) |
| CONTROL | VEL MIN | 1, 10, **20** (por defecto), 30, 40, 50, 64, 80 — la velocidad del toque más suave (el rango se escala, sin saltos) |
| CONTROL | AFTERTOUCH | OFF, LOW, **MID** (por defecto), HIGH — apretar más una tecla ya abajo: la fuente **AT** de la matriz (los sonidos nuevos la mandan al filtro, CUT +20), se graba y sale como aftertouch de canal con MIDI OUT ALL |
| CONTROL | VEL LEARN | **ON** (por defecto), OFF, RESET — cada tecla aprende tus toques suave, normal y fuerte mientras tocás (queda en la configuración); RESET vuelve a la calibración de fábrica y sigue aprendiendo |
| AUDIO | SPEAKER EQ | FLAT, LOWCUT, BASS+ (un EQ en el master para el parlante chico; también llega a las salidas y al USB) |
| AUDIO | USB LEVEL | MASTER (el USB sigue a la perilla MASTER), FIXED (USB a nivel completo), −6 / −12 / −18 / −24 dB (USB fijo y atenuado: para un celular o una computadora cuya entrada satura a nivel completo) |
| AUDIO | CLICK | OFF, REC (mientras una pista está armada y sonando), ON |
| AUDIO | CLICK LEVEL | LOW, MID, HIGH |
| AUDIO | COUNT-IN | OFF, 1 BAR, 2 BARS |
| AUDIO | TRACKS | **4** (por defecto), 6 |
| AUDIO | LOAD | ECO, LITE, **FULL** (por defecto) — lo mismo que GLO › SYSTEM KNOB 2 |
| AUDIO | OCT BEND | OFF, 1, **2** (por defecto), 3, 4, 5, 7, 12 ST — (V10.6z4) las teclas de expresión: OCT+ mantenido sobre una nota dobla hasta acá, OCT− un vibrato; OFF: OCT es la octava aunque haya una nota mantenida (ver Combinaciones útiles) |
| SYSTEM | USB | AUDIO (audio USB estéreo y MIDI, por defecto; hasta la V10.6d era CONSOLE, cuando se fue la consola serie), STEREO (solo audio + MIDI, clase 0; era OFF: ayuda a que macOS 13–15 vea la entrada de audio USB), STEMS (0.7.1: entrada de audio de 8 canales, ver "Stems" más abajo), IN+OUT (0.7.5: audio estéreo en los dos sentidos, ver "Entrada de audio USB" más abajo), STEMS+IN (0.8.0: los stems con la salida de audio también, ver "Stems con una voz" más abajo) |
| SYSTEM | RESTORE LAST | **OFF** (por defecto: sin autoguardado, el encendido arranca en cero), ON (la última sesión vuelve al encender) |
| SYSTEM | CPU CORES | 1 CORE, **2 CORES** (por defecto) — desde el próximo arranque; (V10.6z2) el segundo núcleo arranca solo con la alimentación de fábrica (no después de un SAFE START) |
| SYSTEM | CPU CLOCK | **360 MHZ** (por defecto), 320 MHZ — desde el próximo arranque |
| SYSTEM | BLUETOOTH | **OFF**, ON (MIDI por Bluetooth: ADV, después CONN), AIR (solo escucha: PROX y SWARM) — OCT+ en la fila abre la página PROX (FAR / NEAR); ver [Bluetooth](#midi-por-bluetooth-y-prox-v1013v1020) |
| SYSTEM | CALIBRATION | OCT+ abre HARDWARE CALIBRATION (enseñarle cada botón y perilla) |
| SYSTEM | INIT SYSTEM | OCT+ pregunta (**CLEAR PROJECT?**), y después todo vuelve a cero para una canción nueva (lo mismo que SAVE > TOOLS > CLEAR PROJECT; UNDO devuelve la música) |
| SYSTEM | ABOUT | OCT+ abre: versión, código QR, AUDIO LOAD (NOW, PEAK, LATE, SHED desde el encendido) y créditos (se desplaza con SELECT, OCT− vuelve) |

Si escuchás clicks, abrí ABOUT y mirá **LATE** (bloques de audio que llegaron tarde) y **SHED** (voces que la
protección desvaneció): una foto de esa pantalla ayuda cuando reportás un problema.

---

## 14. MIDI y USB

- **USB** es MIDI y audio class-compliant: la salida master le llega a la computadora como una entrada de audio USB.
- **MIDI IN** por USB y por el jack **TRS**. **ROUTE** decide qué pista toca cada canal (ver
  [13](#13-sistema-tracks-load-y-el-menu)). Las notas entrantes en una pista armada se graban igual que las teclas.
- **MIDI OUT** (por USB; MENU > CONTROL > **MIDI OUT**, 0.6.72), cada pista por su canal (T1 = canal 1 … T6 =
  canal 6):
  - **KEYS** (por defecto): las teclas mandan sus notas, como siempre.
  - **CLOCK**: y además el clock (24 por negra) con **START / STOP**, mientras el tempo es el del FM-1 (CLOCK INT): una
    caja de ritmos o un DAW lo siguen. El clock corre también parado, así el que sigue conoce el tempo antes del PLAY.
  - **ALL**: y además todas las notas que toca el FM-1: el secuenciador, el arpegiador, PLAYHEADS y las repeticiones de
    los punch-in, con el OCT− / OCT+ y el RANDOM de los punch-in aplicados; cada nota tiene su note-off, y STOP, un
    pánico o un cambio del ajuste apagan lo que quede sonando. Grabá la música del FM-1 en un DAW como MIDI, o tocá
    otros sintes con él.
  - **OFF**: nada.
- **PROGRAM CHANGE** (0.6.72): el programa *n* en el canal que toca una pista carga en esa pista el sonido número
  *n* + 1 de la lista de PRESETS (los presets de cada motor en su orden, después los presets de usuario); la pista
  seleccionada no cambia. (0.9.9) Los motores en el orden de la 0.9.8, ANALOG, FM6, PHASE, MATH (el número de LOFI, V10.6h), VOICE, WHEEL, NOISE,
  DRUM, ANIMAL, muestre lo que muestre la página PRESETS (que ahora empieza por DRUM-X): los números de programa de un
  equipo MIDI siguen cargando los sonidos de antes. Los programas llegan a los primeros 128 sonidos de la lista: desde 0.8.3 eso termina dentro
  de los presets de ANIMAL (va último), así que los presets de usuario que vienen después se eligen en el equipo o
  desde el editor.
- **Secciones y proyectos** (0.6.80): el **canal 16** es el de la canción: su PROGRAM CHANGE nunca carga un sonido
  (con cualquier ROUTE). Los programas **0–3** saltan a la sección **A–D** del proyecto (sonando: en el compás
  siguiente, como las teclas de la capa de la canción; parado: en el momento; si está vacía, lo avisa); los programas
  **16–21** cargan el proyecto **1–6** con su sección A (solo parado, como SAVE > SECTIONS). Un pedal MIDI o un DAW
  pueden llevar un set entero.
- **No es posible**: MIDI OUT por el jack **TRS** (el jack del FM-1 es solo de entrada: no le llega ninguna línea de
  salida). El MIDI por Bluetooth funciona desde la V10.12: ver [MIDI por Bluetooth y PROX](#midi-por-bluetooth-y-prox-v1013v1020).
- **Clock**: GLO > GLOBAL > CLOCK **USB** o **TRS** sigue el clock y los Start / Continue / Stop de esa entrada. Con un
  clock externo, TAP y el reinicio con GLO + PLAY quedan desactivados.
- **Controladores** (en la pista que toca el canal): CC5 glide, CC7 nivel, CC10 paneo, CC71 resonancia y CC74 cutoff
  (los propios del motor, donde los tiene), CC72 release, CC73 attack, CC75 decay, CC91 envío de reverb, CC93 envío
  de chorus, CC94 envío de delay; **CC16–23** los ocho valores propios del motor (las perillas de EDIT 1 / EDIT 2,
  E1–E8, sea cual sea el motor: CC16 es el KNOB 1 de EDIT 1), **CC102 / CC103** cutoff / resonancia del filtro post,
  **CC104** drive, **CC105** sustain (0.6.96). CC1 (rueda de modulación), CC11 (expresión) y el aftertouch de canal son fuentes de
  la matriz MOD (MODW, EXPR, AT). Funcionan el pitch bend, el sustain (CC64), All Sound Off (CC120) y All Notes Off
  (CC123). Un CC se graba como automatización igual que un movimiento de perilla.
- **NRPN** (V10.6z3): todos los parámetros que la automatización puede grabar (el sonido, la mezcla, los efectos, E1–E8
  de cualquier motor) y los globales (tempo, swing, los buses, el SLICER, el DJ FILTER ..; no el clock, el ruteo, las
  cargas ni el cambio de motor) tienen un NRPN, con hasta 14 bits de resolución. **CC99 0** + **CC98** el número del
  parámetro en el canal que toca la pista (el canal 16 también con ROUTE CH1-6: la pista seleccionada); **CC99 1** +
  CC98 el número del global, en el canal 16 (o cualquier canal que el FM-1 escuche); después **CC6** (y **CC38** para
  la parte fina) el valor, sobre todo el rango del parámetro. Los números y rangos: [docs/NRPN.md](NRPN.md) (hecho a
  partir del propio firmware). Los CC de arriba quedan como estaban. Con **MIDI OUT ALL**, girar una perilla del panel
  manda su NRPN (el de una pista por su canal, el de un global por el canal 16): un DAW graba los movimientos y se los
  devuelve al FM-1.
- **Para apps** (V10.6z3): el protocolo del editor (SysEx, web/EDITOR_PROTOCOL.md) es un contrato con versión (1.0)
  que solo crece; una app puede seguir además el transporte, los punch-in y cada MUTATE en el momento, sin ningún
  ajuste de MIDI OUT.
- Nombre USB MIDI y audio: **"FM-1"** (desde la V10, 30/10/2026, para que lo encuentren las apps hechas para el FM-1;
  de 0.9.12 a V9 "JIANT", de 0.7.0 a 0.9.11 "JIANT FM"; "FM-1 Update" mientras se instala un paquete). En un DAW el
  puerto MIDI es "FM-1": Ableton Live y los demás programas de Windows muestran "FM-1" (un segundo, "2- FM-1"), macOS
  el dispositivo "FM-1" y su puerto "FM-1", Linux "FM-1 MIDI 1". Hasta 0.9.11 Windows
  podía seguir mostrando "Felucca" (se quedaba con el primer nombre que vio); desde 0.9.12 lo vuelve a leer. Si macOS
  sigue mostrando un nombre viejo, abrí Configuración de Audio MIDI > Estudio MIDI con el FM-1 desenchufado, borrá su
  entrada vieja y volvé a enchufarlo. El editor web y el instalador aceptan también un equipo que todavía se llame
  "JIANT FM" o "Felucca".

### SCENE: una tecla toca toda la banda (V10.24)

SCL › **SCENE** (SCL, y SCL de nuevo hasta SCENE, o SELECT). Cada pista tiene un **rol**; con **SCENE ON** cada tecla
que tocás llega a la vez a todas las pistas con rol, cada una a su manera, todo desde esa nota y la tonalidad (SCL):

| Rol | Qué toca la tecla en esa pista |
| --- | --- |
| **KEEP** | nada: la pista sigue con su propia secuencia (un kit es siempre KEEP) |
| **CHORD** | el acorde de la tecla en la escala (el tipo de CHORD de la pista; OFF: una tríada); la voz pasa a POLY |
| **ARP** | ese acorde en el arpegiador de la pista (se prende, UP, si estaba apagado) |
| **BASS** | la tecla una octava abajo |
| **LEAD** | la tecla una octava arriba |
| **DRUM** | (un kit) la tecla escribe un ritmo propio (V10.6j; KNOB 4 STYLE: FOUR, BREAK, HALF, SPARSE, o **GEN**, V10.6r: el kit y el ritmo que GENE escribe para el último género, cada tecla BASE, DENSE, BREAK o FILL) |

- **KNOB 1** SCENE OFF / ON, **KNOB 2** la pista (TRACK), **KNOB 3** su ROLE, **KNOB 4** el tipo de CHORD de esa pista;
  en BASS y LEAD su **PATTERN** sobre el acorde de la tecla (V10.6j; BASS: ROOT PUMP OCTAVE R-5 WALK ROLL BOUNCE ACID;
  LEAD: HOOK ARC RISE STEPS SPARSE BOUNCE CONVRG RANDOM; V10.6m: **GEN**, una línea al estilo del último género de
  GENE), en DRUM su STYLE.
- Al encender SCENE está en OFF y todas las pistas en KEEP (V10.26). **OCT+** en esta página **tira el dado** de los roles:
  cada rol va a una sola pista, así nunca hay dos pistas haciendo lo mismo ni dos tocando acordes (cuida la polifonía);
  el kit queda en KEEP. **OCT−** pone todas en KEEP. Si lo prendés sin roles, SCENE tira el dado solo. Darle a una pista
  (KNOB 3) un rol que ya tiene otra pasa esa otra a KEEP. T5 y T6 necesitan **6 TRACKS** (MENU).
- Con **REC** armado y sonando: cada pista con rol graba lo que tocó en sus propios pasos (los acordes en un paso, el
  arpegio sus notas), así una toma escribe toda la escena; las pistas en KEEP no se tocan.
- La página muestra las seis pistas, sus roles y las notas que hizo la última tecla en cada una.
- SCENE y los roles son de la sesión: al encender arrancan en OFF.

### MIDI por Bluetooth y PROX (V10.13–V10.20)

- **Conectar**: MENU › SYSTEM › **BLUETOOTH** en ON (la primera vez, con la música parada). La fila dice **ADV** (esperando)
  y **CONN** cuando una app se conecta. Conectate desde una app de MIDI Bluetooth del celular o la compu (aparece como
  **JIANT FM-1**); dejá la app en primer plano, los celulares cortan el Bluetooth de las apps en segundo plano.
- **MIDI de entrada**: lo que manda la app toca el FM-1 igual que por USB-MIDI (notas, CC, program change, bend,
  clock con CLOCK USB +BT, MIDI LEARN y MAP). **MIDI de salida**: lo que deja salir MENU › CONTROL › MIDI OUT (KEYS / CLOCK / ALL)
  también va a la app. Sin SysEx (el editor y las actualizaciones siguen por USB).
- **PROX**: una fuente de modulación según **qué tan cerca está el celular conectado** (su señal Bluetooth,
  filtrada). Sigue al celular, no a la mano. En la fila BLUETOOTH (CONN) apretá **OCT+**: la página PROX. Celular lejos (a un
  metro, más o menos): **OCT−** (FAR). Celular apoyado en el FM-1, arriba a la izquierda: **OCT+** (NEAR). Después, en
  la página MOD de una pista, elegí **SRC PROX** (por ejemplo DEST CUT). Vuelve a 0 cuando el celular se desconecta.
  La página también muestra la conexión (L1, paquetes leídos / recibidos) y las palabras crudas de la señal.
- **AIR** (V10.35, experimental): el tercer valor de la fila BLUETOOTH. La radio deja de anunciarse (sin MIDI) y solo
  **escucha** a todos los dispositivos Bluetooth de alrededor: celulares, relojes, auriculares, televisores. PROX
  sigue entonces con qué fuerza se los oye, y un cuerpo o una mano cerca de la antena (atrás, arriba a la izquierda)
  los tapa: no hace falta tener un celular en la mano. En la fila BLUETOOTH (AIR) apretá **OCT+** y calibrá FAR (lejos) y
  NEAR (la mano encima) como arriba. Necesita algunos dispositivos Bluetooth cerca; sin ninguno queda en 0.

### USB HOST: un teclado enchufado directo (V10.41, experimental)

MENU › SYSTEM › USB › **HOST** y cerrá el MENU: el puerto USB del FM-1 pasa a ser el host de un teclado o controlador
USB-MIDI class compliant (sin computadora en el medio), solo o detrás de un hub. Lo que toca le llega al FM-1 igual que
por USB-MIDI: notas, CC, program change, bend, el clock y start / stop (CLOCK USB +BT), MIDI LEARN y MAP.
- **Alimentación**: el FM-1 no alimenta el puerto. Usá un teclado con su propia fuente, o poné un **hub USB
  alimentado** entre los dos.
- Mensajes: USB HOST: PLUG A MIDI KEYBOARD, USB MIDI KEYBOARD READY, NOT A USB MIDI KEYBOARD (un dispositivo sin entrada
  MIDI para el FM-1), LOW SPEED: NOT SUPPORTED, USB KEYBOARD LOST (se reconecta solo).
- Con HOST activo no hay USB hacia la computadora: ni editor, ni stems, ni MIDI a la compu. Cualquier otro valor de USB
  lo apaga. HOST nunca se guarda: el FM-1 siempre prende como dispositivo USB, así el instalador y el editor siempre
  funcionan.
- Solo entrada: no se le manda nada al teclado. Su código vive en la zona Bluetooth de la flash (una instalación rota
  dice USB HOST: INSTALL AGAIN). De Melodee (Kerem Kilic; la enumeración USB host de recursivefieldinstruments).

### MIDI LEARN (0.9.12)

Cualquier perilla del sonido de una pista a cualquier CC de tu controlador, en el equipo (del MIDI LEARN de Felucca 1.5):

1. **MENU › CONTROL › LEARN**: **SEL TRACK** (el CC sigue a la pista que esté elegida cuando llega) o **FIXED TRK** (el
   CC queda en la pista donde lo aprendiste). Mantené HOME, ALGORITHM hasta la pestaña CONTROL, SELECT hasta LEARN, y
   después HOME para cerrar el menú: empieza el aprendizaje.
2. **Girá una perilla** en cualquier página que edite un parámetro de una pista (HOME, EDIT, ENV, LFO, FX, el MIXER,
   los niveles de GLO): la pantalla dice **LEARN T2 CUT: SEND A CC** (o su CC si ya tiene uno).
3. **Mové la perilla del controlador** (mandá el CC): **CC21 > T2 CUT**. Desde ahí ese CC mueve el CUT de T2 como la
   perilla (0–127 sobre su rango, grabado como automatización igual que una perilla), en **cualquier canal** y por
   **USB o el jack TRS**.
4. Girá la próxima perilla, mandá el próximo CC … **LEARN OFF** cuando termines (con LEARN prendido, las perillas
   siguen eligiendo).

- Un CC mueve un parámetro y un parámetro tiene un CC: aprenderlo de nuevo reemplaza el anterior. Como máximo **diez**
  (**LEARN FULL**: liberá uno aprendiendo su CC en otra perilla, o todos con LEARN **CLEAR ALL**).
- **Nunca se aprenden** (**CC7 RESERVED**, y siguen haciendo lo suyo): CC0 / 32 (bank select), CC1 (rueda de
  modulación), CC6 / 38 / 96–101 (data entry, RPN), CC11 (expresión), CC64 (sustain), CC120–127 (modos de canal) y
  todos los CC del mapa estándar de arriba (CC5 7 10 16–23 71–75 91 93 94 102–105). Así un CC aprendido y el mapa nunca
  se pisan.
- **Se guarda con la configuración** (no en un proyecto; un backup de la configuración lo lleva); **MENU › CONTROL ›
  LEARN CLEAR ALL** (al cerrar el MENU) los olvida todos. ROUTE sigue decidiendo qué canales se escuchan (CH1-6 ignora los canales 7–16).
- **MENU › CONTROL › MIDI MAP** decide qué hacen los CC: **FACTORY** solo el mapa estándar, **CUSTOM ADD** el mapa y
  tus CC aprendidos (aprender con FACTORY pasa a este), **CUSTOM** solo tus CC aprendidos. Los CC aprendidos andan
  por USB, el jack TRS y Bluetooth.

### Stems (0.7.1)

Dos maneras de sacar cada pista por separado, para procesarlas de a una en un DAW:

- **Por USB, en vivo: MENU > SYSTEM > USB > STEMS.** El equipo se vuelve a enumerar como una entrada de audio USB de 8
  canales a 44.1 kHz, 16 bits (en el DAW, la entrada del equipo con 8 canales): los canales 1–6 son las seis pistas, cada
  una en mono con su LEVEL, DIST, ruteo al SLICER y mutes incluidos y sin su PAN (lo que suma a la mezcla, L + R a la
  mitad); los canales 7–8 son el retorno de FX en estéreo (los buses DELAY, REVERB y CHORUS, y el del SLICER cuando está
  encendido). Todo antes del master: sin leveler, limiter, CLIP ni perilla MASTER, así la suma de los ocho es la mezcla
  tal como la oyen los buses. USB LEVEL a −6 dB o menos deja margen. La consola serie queda apagada en este modo; el
  USB-MIDI, el editor y el instalador siguen funcionando. El parlante y los auriculares tocan el master como siempre.

  **0.9.8: stems limpios.** Hasta la 0.9.7 el endpoint de 8 canales decía 720 bytes (45 frames) mientras el FM-1 manda
  46 frames cada vez que su reloj se adelanta (seguido: anda apenas rápido): el host descartaba cada uno de esos paquetes
  y se frenaba, el anillo del FM-1 se llenaba y se perdían bloques enteros de 32 frames, unas cien veces por segundo, en
  todos los canales a la vez: clics y un sonido entrecortado, "separado por IA", en cada stem (los modos de mezcla estéreo
  no tenían el problema). Ahora son 736 bytes; además el anillo guarda un milisegundo más en STEMS y STEMS+IN, y un frame
  repetido tras un faltante se recupera repartido en un paquete en vez de saltearse. **Actualizá el FM-1 y la app (JIANT
  Stems 1.1) y volvé a grabar**: una toma hecha con la 0.9.7 no se puede arreglar después. Una pista que pasa por el
  **SLICER** tiene ahora en su stem su propio sonido tal como el SLICER lo dejó pasar (sus repeticiones en el stem de la
  primera de esas pistas, o con el FX ON del SLICER en el retorno de FX), así los ocho siguen sumando la mezcla (hasta la
  0.9.7 esa pista estaba seca en su stem y otra vez, recortada, en el retorno de FX).
  Qué es un stem: exactamente lo que la pista suma a la mezcla en esa toma, así que se anula contra la pista renderizada
  sola en la misma toma. Lo que hace que una pista dependa de las otras está en él, como en la mezcla: el **DUCK** (el
  bombeo del bombo sobre las otras pistas) y las **voces compartidas** (8 entre todas las pistas; con **LOAD ECO**: 6, 4
  por pista, y con el presupuesto de carga ajustado se recortan las colas que ya están bajo −50 dB): una nota a la que otra
  pista le sacó la voz termina antes (un fundido de 6 ms). En la mezcla eso se disimula; en solo, un tema denso puede sonar
  como con gate. Para stems, grabá en **LOAD FULL** si la CPU lo permite y mantené la polifonía del tema dentro del
  presupuesto.
- **Desde un backup, fuera de línea: "Stems" en la página del emulador.** Hacé un backup del FM-1 ("Back up" en el
  editor o el instalador), abrí la página del emulador, desplegá "Stems", cargá el archivo, elegí una canción (sus filas
  se tocan una vez; una canción en LOOP, una vuelta) o la sección en vivo con una cantidad de compases, y apretá Render.
  Cada pista se renderiza sola con su propia parte de DELAY, REVERB y CHORUS (sin los envíos de las otras), más la mezcla
  completa MIX (el master tal como lo toca el FM-1), todas del mismo largo, estéreo 16 bits 44.1 kHz, en un ZIP: T1.wav ..
  T6.wav y MIX.wav (una pista en silencio no se incluye). Más rápido que el tiempo real, sin límites del USB y sin tocar
  el equipo.

### Entrada de audio USB (0.7.5, versión de prueba)

**MENU > SYSTEM > USB > IN+OUT** hace que el FM-1 sea, además de entrada, una salida de audio USB de la computadora o
el celular: el equipo se vuelve a enumerar (como con STEMS, la consola serie se apaga) y el host ve "JIANT FM" como
parlante, estéreo, 16 bits, 44.1 kHz. Lo que el host le mande entra por la **última pista** (T6, o T4 con TRACKS 4) y
se mezcla como el sonido de esa pista:

- su **LEVEL** fija el nivel de la entrada, su **PAN** funciona como balance, sus envíos de **CHORUS, DELAY y REVERB**
  llevan la entrada a los efectos (desde la suma de izquierda y derecha), el **DUCK** la atenúa, su **MUTE** y la capa
  de mutes la silencian;
- el motor propio de la pista sigue sonando (con el patrón vacío queda solo la entrada); su DIST y su filtro no se
  aplican a la entrada;
- lo que se graba por USB es el master estéreo, como con STEREO, con la entrada incluida.

Elegí el FM-1 como salida en la configuración de sonido del host (o en el DAW o la app), poné 44.1 kHz y reproducí.
El FM-1 sigue el reloj del host (lee el audio un poco más rápido o más lento, con ~3.6 ms de retardo); si el host deja
de mandar, la entrada se apaga suave y vuelve cuando retoma. Algunos sistemas ponen como salida por defecto al
dispositivo USB recién conectado: por eso IN+OUT es una opción aparte y no viene activada. Versión de prueba: contanos
cómo anda con tu computadora o tu celular.

### Stems con una voz (0.8.0, versión de prueba)

**MENU > SYSTEM > USB > STEMS+IN** es STEMS e IN+OUT juntos, para la app del celular: el micrófono del celular va al
FM-1, entra en la **última pista** como la entrada de IN+OUT (su LEVEL, PAN, MUTE y sus envíos de CHORUS, DELAY y
REVERB: tu voz por los efectos de esa pista) y vuelven **10 canales**: T1–T6, el retorno de efectos L R, después **la
voz como la mezcló la pista** y **la voz como llegó**. La voz tiene un canal propio (no está en el stem de la pista),
así la app la corre hacia atrás según la latencia de ida y vuelta y la voz cae a tiempo con lo que escuchaste; el
segundo canal de voz es la referencia contra la que la app mide esa latencia. La parte de la voz en el retorno de
efectos no se puede correr (está mezclada con la de todas las pistas): su reverb y sus delays quedan atrasados esa
latencia (~20–30 ms: en una reverb suena como pre-delay). La app lee y escribe el FM-1 con su propio driver USB
(Android pasa a las apps solo algunos canales de un dispositivo USB). Versión de prueba: por ahora la app es JIANT
Probe (su prueba 7).

---

## 15. Herramientas web: emulador, instalador, editor, backup

Las tres funcionan en el navegador y se actualizan solas con cada cambio del firmware.

### Emulador — <https://juanjiant-bit.github.io/JIANT/emu/>

El mismo código que el FM-1, con sonido, en el navegador. Tocalo con el mouse o con el teclado de la computadora: las
filas **Z–'** y **Q–O** son las teclas, **1** a **=** los botones de FX a REC, **[ ]** OCT− / OCT+, **↑ ↓** SELECT,
**← →** ALGORITHM; **Shift-clic** mantiene un botón. Ideal para aprender las capas sin el equipo.

### Instalador — <https://juanjiant-bit.github.io/JIANT/webapp/installer/>

Chrome o Edge en una computadora, o Chrome en Android (USB OTG), con el FM-1 conectado directo.

- **Instalar**: instala el JIANT actual (primero revisa el FM-1: solo sobre el V16 oficial, el V15 oficial o JIANT).
  **Hacé backup antes** si actualizás desde la V10.6o o anterior: esta versión guarda 6 canciones (antes 8), y los
  proyectos muy viejos (antes de la 1.0.3) ya no se abren. Las versiones anteriores siguen en la lista.
- **Volver al oficial**: dale el **FM-1.fwsc** original sin modificar de M-VAVE del **V16** o del **V15** (se verifica
  localmente en el navegador). Primero guarda un backup completo (música actual, música guardada, bancos de sonido y
  configuración). Usalo antes de instalar JIANT si tu FM-1 tiene **otro firmware custom** (Felucca, Skiff,
  baudgirl…), y siempre que quieras volver al firmware de fábrica. El firmware oficial no puede abrir música de JIANT;
  para restaurar el backup más adelante, reinstalá JIANT y usá el editor.

### Vista de velocidad — <https://juanjiant-bit.github.io/JIANT/webapp/vel/>

(V10.6z5, V10.6z7) La velocidad de cada nota y la calibración de cada tecla, en vivo, por USB (Chrome o Edge;
protocolo 1.1 o posterior). Ver [Velocidad y aftertouch](#velocidad-y-aftertouch-de-las-teclas-v106z7) y la prueba
paso a paso en [VELOCITY_DIAG.es.md](VELOCITY_DIAG.es.md).

### Editor — <https://juanjiant-bit.github.io/JIANT/webapp/editor/>

Todos los parámetros desde la computadora, por USB. Pestañas: Sound, 6-OP FM, Sequencer, Tracks, Library, Projects,
Settings.

- **6-OP FM**: editá patches FM6, importá .syx y **Send to the device's voice bank (B1–B32)** (enviá al banco de voces
  del equipo).
- **Projects**: los slots de proyecto y el **Complete backup**: **Save backup** y **Restore backup**.
- **Sound**, en una pista VOICE (0.8.5): el recuadro **Lyric**: LANG, LYRIC y el texto de la letra; **Send** la
  escribe en la pista (convierte acentos y ñ).

### El backup completo

**Save backup** descarga un único archivo con: la música actual, **los 6 proyectos con sus 4 secciones cada uno**, los
arreglos y las escenas, los 32 presets de usuario (con sus patches FM6) y la configuración. **Restore backup**
reemplaza todo eso en el equipo (mantené el USB conectado). El **banco de voces FM6 (B1–B32) no está incluido**:
guardá tus archivos .syx.

Hacé un backup antes de actualizar, antes de volver al firmware de fábrica, y cada vez que terminás algo que te
encanta.

---

## 16. Problemas y preguntas frecuentes

### No me deja instalar: qué hacer

Instalador: <https://juanjiant-bit.github.io/JIANT/webapp/installer/>

**Antes que nada:** si algo falla, el instalador muestra un recuadro **«Qué hacer»** con los pasos para ese error. Si
con eso no alcanza, apretá **COPIAR INFORME** y pegá el texto en tu mensaje: dice qué navegador usás, qué puertos MIDI
vio, qué firmware tiene tu FM-1 y en qué paso se cortó.

**«Este navegador no tiene WebMIDI» / no pasa nada al apretar Instalar.**
Usá **Chrome o Edge en una computadora** (Windows, Mac, Linux o Chromebook). En **iPhone y iPad no se puede** con
ningún navegador (Apple no permite WebMIDI). Safari no sirve; Firefox no es recomendable. En Android, Chrome con un
cable OTG puede andar, pero la computadora es más segura.

**«No se permitió el acceso a MIDI» / «MIDI está bloqueado».**
Hacé clic en el ícono a la izquierda de la dirección → *Configuración del sitio* → *Dispositivos MIDI* → **Permitir**.
Recargá la página. En Brave, desactivá los Shields para el sitio.

**«No se encontró el FM-1».**
- El FM-1 tiene que estar **prendido**.
- Usá un **cable USB de datos**: muchos cables solo cargan. Probá con otro.
- Conectalo **directo a la computadora**, sin hub ni adaptador.
- Si sigue igual, apretá **«¿Está en modo boot (WL80UBOOT)?»** en el instalador.

**«Otro programa está usando su puerto MIDI».**
En Windows solo un programa a la vez puede usar el FM-1. **Cerrá la app de M-VAVE, tu DAW** (Ableton, FL Studio,
Reaper…), MIDI-OX y **las otras pestañas** con el editor de JIANT. Desconectá y volvé a conectar el FM-1, recargá la
página y apretá Instalar.

**«Este FM-1 tiene un firmware oficial más viejo» (V14, V13…).**
Abrí **«Volver al oficial»**, elegí el archivo **FM-1.fwsc** del V16 (o del V15) y apretá su botón (no hace falta backup).
Cuando diga *Listo*, apretá **Instalar**.

**«Este FM-1 tiene Felucca / Skiff / otro firmware».**
No se puede instalar JIANT directo encima de otro firmware custom (puede dejar el FM-1 sin arrancar). Usá
**«Volver al oficial»**: primero guarda un archivo de backup (Felucca: con los samples; Skiff: su propio formato,
que solo restaura el editor de Skiff) y después escribe el oficial. Si ese firmware no puede hacer backup, la página te
pregunta antes de seguir sin él. Después apretá **Instalar**.

**¿De dónde saco el FM-1.fwsc?**
De la página de descargas de **M-VAVE** (<https://www.m-vave.com/download>): buscá el FM-1, bajá el **firmware V16**
(el que ofrece hoy), **descomprimí el ZIP** y elegí el archivo **FM-1.fwsc** (718.196 bytes; el del V15, si lo
guardaste, pesa 699.956 bytes). No lo podemos subir nosotros: es de M-VAVE.
Si elegís el ZIP, otra versión o un archivo de JIANT, el instalador te dice cuál elegiste y qué falta.

**Se cortó a mitad de camino («No se encontró el FM-1 en modo de actualización», «Se detuvo», «Se perdió la conexión»).**
**No apagues el FM-1.** Volvé a conectar el cable (si podés en otro puerto) y apretá **el mismo botón** otra vez:
sigue donde estaba. En Windows, la primera vez puede tardar un minuto en instalar el driver del modo de actualización:
esperá y apretá de nuevo.

**La pantalla quedó negra.**
Conectalo y apretá Instalar de nuevo. Si la computadora lo ve como **WL80UBOOT**, está en el modo boot del chip:
probá otro cable directo a la computadora; si no vuelve, se recupera con
[FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter).

**«Page not found» / la página no existe.**
Usá exactamente <https://juanjiant-bit.github.io/JIANT/webapp/installer/> (sin nada pegado al final). Los enlaces
viejos (`/JIANT/installer/`, `/JIANT/webapp/`…) ahora te redirigen solos.

**¿Qué versión elijo?**
La que dice **(la última)** es la versión estable aprobada. Las de abajo son anteriores (para volver atrás). El
instalador *preview* (enlace al pie) tiene la próxima versión sin aprobar: solo para probar.

### El firmware

**MENU › SYSTEM › BLUETOOTH dice MISSING (antes NO SEG), o "BLUETOOTH PART MISSING: REINSTALL: JIANT INSTALLER".** La
parte Bluetooth de JIANT (que también tiene GEN, el guardado de canciones, USB HOST y más) no se instaló: reinstalá
con el instalador web (arriba). Otras herramientas de flasheo escriben solo el firmware principal.

**Al prender, la pantalla dice BLUETOOTH PART MISSING, "REINSTALL WITH THE JIANT WEB INSTALLER:
juanjiant-bit.github.io/JIANT", ANY KEY: CONTINUE (desde la V10.7.22).** Es el mismo caso, avisado una vez cada vez que
prendés. Tocá cualquier tecla (no hace nada más) y tocá como siempre: anda todo menos lo que vive en esa parte (GEN
muestra el mismo aviso en vez de sus tarjetas; guardar una canción, Bluetooth y USB HOST dicen BLUETOOTH PART MISSING).
Para arreglarlo, abrí el instalador, conectá y apretá **Instalar** (la misma versión sirve): escribe las dos partes. El
instalador revisa la parte Bluetooth después de reiniciar el FM-1 y, si falta, instala una vez más solo; si sigue
faltando, lo dice: ahí apretá **COPIAR INFORME** y mandalo. Si dice que el FM-1 está en modo de actualización con otro
programa de actualización (el oficial: se cortó una vuelta al oficial), terminá eso primero en **Volver al oficial** con
**Retomar una actualización interrumpida** marcado, y después apretá **Instalar**.

**Mis canciones no quedan guardadas al apagar (un FM-1 nuevo).** Los FM-1 vendidos desde fines de 2026 (con el logo en
el frente) traen otro chip de flash (JEDEC 0xB36014). JIANT antes de la V10.6z no lo conocía y andaba solo desde la RAM:
no guardaba nada, faltaba GENE y el equipo no podía actualizarse solo. Instalá la versión actual (el instalador escribe
desde la computadora, así que funciona en esos equipos): desde ahí guarda normalmente.

**No están las canciones 7 y 8.** Desde la V10.6p JIANT guarda 6 canciones (proyectos; la memoria de la 7 y la 8 queda
para un banco de samples de batería). Un FM-1 que quedó en la canción 7 u 8 abre en la 6, con un aviso único si tenían
música. Un backup hecho antes de actualizar todavía las tiene: pasá lo que necesites a 1–6 con una versión anterior, o
guardá el archivo.

**Dice SAFE START.** Un arranque que se colgó se reinició, y este arranque se salteó el ajuste de alimentación (V10.46).
Así toca con un núcleo (V10.6z2: el segundo arranca solo con la alimentación de fábrica); el próximo encendido normal
vuelve a dos.

**T5 o T6 no suenan.** TRACKS está en 4 (el valor por defecto): T5 y T6 están dormidas. GLO > SYSTEM > TRACKS **6** (o
MENU > AUDIO > TRACKS). La pantalla te avisa cuando tocás en una pista dormida.

**Después de borrar una secuencia, seguían sonando notas en todas las pistas.** Esto se corrigió en la v0.6.28: ahora
un borrado termina las repeticiones del carril de punch-in (STUTTER, ARP), una pista DRUM sin pasos no recibe fill de
ARP, y se cancela un salto de sección o una cadena rápida que estaba esperando el compás. Si algo sigue sonando
después de un borrado, puede ser el **carril de punch-in**: mantené **REC y apretá FX** para borrarlo (o
**SEQ + REC** para borrar todo).

**Dos núcleos (0.6.71) y MENU › SYSTEM › CPU CORES (0.9.12).** El segundo núcleo calcula algunas pistas de sinte; MENU ›
SYSTEM › ABOUT dice **CORES 2**, o **CORES 1** y por qué. Desde la 0.9.12, **MENU › SYSTEM › CPU CORES** elige **2 CORES** (por
defecto) o **1 CORE**; se guarda con los ajustes y vale desde el próximo arranque. Si el FM-1 se cuelga o crashea con
los dos núcleos trabajando, o el segundo núcleo lee mal, deja de responder o falla, **apaga los 2 núcleos para
siempre** y lo avisa (**2 CORES OFF AFTER A CRASH** / **CORE 2 FAILED: 1 CORE**, y luego MENU › SYSTEM › CPU CORES); ABOUT
dice entonces **CORES 1 (AFTER A CRASH)**. Volvé a poner 2 CORES solo si querés probarlo. Mantener **REC** al prender
da un núcleo solo en esa sesión.
Con los dos núcleos andando las pistas comparten **12 voces** en vez de 8 (cada pista sigue con 8 como máximo); con un
núcleo (1 CORE, después de un crash, REC al prender) son 8, y si el segundo núcleo se detiene el presupuesto vuelve a 8
desde la próxima nota.

**Se cuelga: pantalla roja "JIANT CRASH" (o "FELUCCA CRASH" antes de la 0.7.0) con números.** Sacale una foto y
decinos la versión (MENU › SYSTEM › ABOUT). Cómo leerla:

- Desde la 0.9.12: la versión, **CORE 0 OF 2** (el núcleo que falló, de cuántos andaban), **CODE xx-nn** (la causa y
  el vector, abajo), y después **VEC, PC, EMU, DBG, RETS**.
- Antes de la 0.9.12 salían esos mismos cinco números sin nombre, en ese orden de arriba abajo: VEC (1 = la excepción
  de la CPU), PC (dónde falló), EMU y DBG (los bits de la causa), RETS (quién llamó a la función que falló).
- El código: **IL** instrucción ilegal, **MA** acceso desalineado, **DZ** división por cero, **ST** desborde de pila,
  **PC** un salto fuera del código, **WR** una escritura en una zona protegida, **MM** la unidad de memoria, **BU** un
  acceso a memoria que no existe (un puntero malo), **WD** un cuelgue (lo reinició el watchdog: sin pantalla roja),
  **??** ninguna de esas.
- El FM-1 se reinicia solo a los 4 s. El último crash queda guardado en los ajustes, aunque lo apagues: **MENU ›
  SYSTEM › ABOUT › LAST CRASH** lo muestra en una línea (`IL01 2C 0.9.12 0201ABCD 0201ABCD`: código y vector, núcleos,
  versión, PC, RETS). El backup de ajustes del editor también lo lleva.
- Para desarrolladores: `tools/crash_decode.py VEC PC EMU DBG RETS` dice la función, con el ELF de la versión que
  falló.

**Qué probar si se cuelga de vez en cuando (aunque no hagas nada).** En este orden, unas horas cada cosa:
1. **MENU › SYSTEM › CPU CORES 1 CORE** (la 0.9.12 lo hace sola después del primer crash con dos núcleos; antes de la
   0.9.12, mantené REC en cada encendido).
2. **MENU › SYSTEM › CPU CLOCK 320 MHZ**: el chip al reloj que JieLi garantiza en vez de los 360 MHz que pone el
   arranque del FM-1 (se aplica en el próximo encendido; ABOUT dice **CPU 320 MHZ**). Es ~11 % menos de CPU: una
   canción pesada puede soltar voces antes (LOAD LITE ayuda). Si el FM-1 no puede (su reloj está configurado de otra
   forma), ABOUT sigue diciendo **CPU 360 MHZ**.
3. Si sigue colgándose: la foto, la versión y LAST CRASH, por favor.

**Clicks, cortes, el medidor de carga lleno.** Dejá TRACKS en 4, probá **LOAD LITE** o **ECO**, usá menos voces (MONO,
menos pistas FM6), reverbs más livianas (CLOUD y SHIMMER son las más pesadas). Revisá MENU > SYSTEM > ABOUT >
AUDIO LOAD.

**SELECT no cambia el tempo.** Desde 0.6.67 SELECT solo recorre las páginas; el tempo es **GLO mantenido + SELECT**
(o F4 TAP de GLO, o la perilla BPM de GLO > GLOBAL).

**Las perillas controlan M1–M4 en lugar de la página.** Las macros están enganchadas (el encabezado dice [LFO] LATCH).
Apretá LFO.

**Un tema no arranca: "PATTERN A EMPTY" / "ADD A SONG ROW".** Una fila apunta a una sección vacía, o no hay filas.
Guardá música en esa sección, o cambiá la fila.

**Dice SONG PLAYS: STOP IT TO EDIT.** Está sonando un tema (modo SONG, SAVE + D5, o PLAY de ARRANGE / TIMELINE) o una
cadena rápida; paralo para editar pasos, usar GEN, EUCLID, herramientas o UNDO (o apagá el modo SONG con SAVE + D5
para que PLAY toque el loop).

**Las teclas parecen invertidas / un botón hace otra cosa.** Corré HARDWARE CALIBRATION: mantené OCT− y OCT+ mientras
encendés (o MENU > SYSTEM > CALIBRATION) y enseñale cada botón y perilla. Una tecla del piano cancela.

**El nuevo modo USB no aparece en el teléfono o la computadora.** Desde la 0.9.10 el FM-1 sale del bus medio segundo
cuando cambia MENU › SYSTEM › USB (volvía a los ~25 ms y algunos teléfonos y hubs no notaban que se había ido, así que
seguían con el modo anterior); aparece más o menos un segundo después de cerrar el menú. Con firmware anterior: cambiá
de nuevo o desenchufá.

**SAVED (NO FLASH).** No se detectó el chip de flash; lo guardado queda solo en RAM. Apagá y prendé el equipo. Si
persiste, hacé un backup de lo que puedas con el editor y reinstalá.

**El FM-1 queda en negro después de una actualización interrumpida.** Fijate si la computadora lo ve como
**WL80UBOOT** (un dispositivo USB 4C4A:8057): ese es el modo de arranque del chip y el instalador lo puede volver a
escribir (probá con otro cable). Para entrar a propósito al modo de actualización desde un equipo funcionando, mantené
**OCT− + OCT+** durante 5 segundos. Si eso no alcanza,
[FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) lee y escribe la flash con una Seeed XIAO RP2040.

**Quiero volver al firmware de fábrica.** Usá la sección **Volver al oficial** del instalador con el V16 o el V15 de
M-VAVE (hace un backup antes).

**Vengo de baudgirl / Felucca / Skiff. ¿Puedo instalar directamente?** No: primero volvé al oficial con el instalador y
después instalá JIANT. Pasar directamente desde otro firmware custom ya brickeó algún FM-1.

**¿Adónde fueron mis samples de Felucca?** JIANT no tiene samples de usuario; esa flash ahora guarda los temas.

---

Si mi trabajo resuena con vos, considerá apoyarme en Ko-fi (https://ko-fi.com/juanjiant) — en Argentina: Cafecito (https://cafecito.app/juanjiant).
