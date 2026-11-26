# JIANT FM y MIDI: una guía práctica

[English](MIDI.md) · Firmware **V10.8.48** · Todo lo que está acá se comprobó contra el código de V10.8.48
(`firmware/src/midi_control.c`, `midi_learn.c`, `midi_uart.c`, `midi_clock.c`, `usb.c`, `usb_host.c`, `seq.c`,
`menu_items.c`, `params.c`). Lo que no se pudo comprobar en el código dice **sin verificar**.

La referencia completa sigue en el manual, [capítulo 14, MIDI y USB](MANUAL.es.md#14-midi-y-usb). Esta página es
para hacer cosas:

1. [Tocar JIANT desde un teclado MIDI externo](#3-tocar-jiant-desde-un-teclado)
2. [Modular JIANT con CC](#4-modular-jiant-con-cc)
3. [Sincronizar y controlar JIANT desde otros equipos, o controlarlos desde JIANT](#5-clock-y-sincronía)
4. [Una receta completa: Novation Circuit Tracks, una caja h2midi y cables TRS-A](#7-receta-circuit-tracks-h2midi-y-cables-trs-a)

---

## 1. Por dónde entra (y sale) el MIDI

| Conexión | MIDI in | MIDI out | Clock in (GLO › GLOBAL › CLOCK) | SysEx (editor, actualizaciones) |
| --- | --- | --- | --- | --- |
| **USB, el FM-1 como dispositivo** (a una compu, un teléfono, una caja USB host) | sí | sí (MENU › CONTROL › MIDI OUT) | **USB +BT** | sí |
| **USB HOST** (MENU › SYSTEM › USB › HOST: un teclado enchufado directo al FM-1) | sí | **no** | **USB +BT** | no |
| **Jack TRS** (3,5 mm) | sí | **no**: el jack es solo entrada | **TRS** | no (se ignora) |
| **MIDI por Bluetooth** (MENU › SYSTEM › BLUETOOTH ON) | sí | sí (lo que deja salir MIDI OUT) | **USB +BT** | no |

- **Todas las entradas están siempre activas y se mezclan.** No hay un ajuste de "fuente de MIDI IN": notas y CC
  de USB, USB HOST, el jack TRS y Bluetooth tocan el FM-1 a la vez, por el mismo ruteo. (El firmware todavía tiene
  un global viejo `MIDI` USB / TRS, pero nada lo lee y ninguna página lo muestra.) Lo único que se elige por entrada
  es **de cuál se sigue el clock** (sección 5).
- **USB como dispositivo.** El FM-1 es class compliant y se llama **"FM-1"** (MIDI USB y audio USB en un solo
  dispositivo). Enchufalo a una compu o a cualquier cosa que haga de host de MIDI USB class compliant. Es también la
  única forma de usar el editor web y el instalador.
- **USB HOST** (V10.41, experimental): MENU › SYSTEM › **USB** › **HOST**, y cerrá el MENU. El FM-1 pasa a ser el host
  de un teclado o controlador USB-MIDI class compliant, solo o detrás de un hub.
  - **Alimentación**: el FM-1 **no** le da corriente al puerto. El teclado necesita su propia fuente o batería, o
    poné un **hub USB con alimentación** en el medio.
  - No anda con dispositivos USB low speed (la pantalla dice LOW SPEED: NOT SUPPORTED).
  - Solo entrada: no se le manda nada al teclado. Con HOST prendido no hay conexión USB con una compu (ni editor, ni
    MIDI a la compu). HOST nunca se guarda: el FM-1 siempre arranca como dispositivo USB.
  - Más: [USB HOST en el manual](MANUAL.es.md#usb-host-un-teclado-o-una-interfaz-de-audio-enchufados-directo-v1041-experimental).
- **MIDI por Bluetooth**: MENU › SYSTEM › **BLUETOOTH ON**; aparece como **JIANT FM-1** (la fila muestra ADV y
  después CONN). Notas, CC, program change, bend y clock andan igual que por USB. Más:
  [MIDI por Bluetooth y PROX](MANUAL.es.md#midi-por-bluetooth-y-prox-v1013v1020).

## 2. El jack TRS: ¿tipo A o tipo B?

El jack de 3,5 mm del FM-1 recibe MIDI a 31250 baudios por un solo pin de entrada (`hal/fm1_uart.h`: PH8 → UART1
RX). **El código no puede decir si el jack está cableado tipo A o tipo B**: eso lo define el hardware del FM-1 (qué
anillo del plug llega al optoacoplador), no el firmware. O sea: **sin verificar**.

Cómo averiguarlo con lo que tenés:

1. En el FM-1 poné **GLO › SYSTEM › ROUTE SEL** (todos los canales tocan la pista elegida), así el canal no puede ser
   el problema.
2. Conectá el MIDI OUT del que manda con un cable **TRS tipo A** (o el adaptador tipo A que trajo ese equipo) y tocá
   notas.
3. Si suena la pista elegida: el jack del FM-1 anda con tipo A. Si no pasa nada, probá un cable **tipo B** o un
   **conversor tipo A ↔ B**. Invertir punta y anillo normalmente solo da silencio, no rompe nada, pero fijate en el
   manual del otro equipo para estar seguro.
4. La documentación de M-VAVE para el FM-1, o el adaptador que vino en la caja, también pueden decir de qué tipo es.

En V10.8.48 el FM-1 no tiene una luz de actividad MIDI para el jack TRS (la página de estado GLO › SYSTEM 2 se fue en
V10.8.38), así que la prueba es simplemente: ¿suena una pista?

## 3. Tocar JIANT desde un teclado

### Qué canal toca qué pista: ROUTE

**GLO › SYSTEM, KNOB 3: ROUTE**

| ROUTE | Qué pasa |
| --- | --- |
| **CH1-6** (por defecto) | El canal 1 toca la pista 1 … el canal 6 la pista 6. **Los canales 7–16 se ignoran del todo** (notas, CC, bend, aftertouch, pánico), así quedan libres para otros instrumentos. Dos excepciones en el canal 16: su PROGRAM CHANGE (secciones y proyectos) y su NRPN (ver 4.3). |
| **SEL** | **Todos** los canales tocan la **pista elegida** (la elige ALGORITHM). Lo más fácil para un solo teclado. |

- Las pistas 5 y 6 solo suenan con **MENU › AUDIO › TRACKS 6** (con 4 TRACKS, T5 y T6 no reciben notas).
- Pasar ROUTE de SEL a CH1-6 suelta lo que tenían agarrado los canales 7–16, así no queda ninguna nota colgada.
- **KEY SPLIT** (GLO › KEY SPLIT y KEY SPLIT 2) también divide **un canal MIDI** en dos pistas: **MIDI CH** (1–16) y
  **MIDI CUT** (la nota del corte, 60 por defecto) en KEY SPLIT 2, LOW TRK / HIGH TRK y LOW OCT / HIGH OCT como para
  las teclas. El canal del split se escucha diga lo que diga ROUTE.

### Qué puede mandar un teclado

| Mensaje | Qué hace JIANT |
| --- | --- |
| **Note on / off** | Toca la pista a la que rutea el canal (arriba). La velocidad se usa tal como llega (el VEL del sonido decide cuánto pesa). Un note-on con velocidad 0 es un note-off. En una pista armada, con el transporte andando, las notas que entran **se graban** como las teclas del panel. En una pista DRUM las notas eligen los sonidos del kit (ver [DRUM-X](MANUAL.es.md#5-drum-x)). |
| **Pitch bend** | Dobla la pista. Rango **±2 semitonos** por defecto; el RPN 0 (CC101 0, CC100 0, después CC6 semitonos 0–24 y CC38 cents 0–99) lo cambia por canal. Las pistas DRUM ignoran el bend. |
| **CC64 sustain** | Sostiene las notas (≥ 64 apretado). Las pistas DRUM lo ignoran. |
| **CC1 rueda de modulación** | La fuente **MODW** de la matriz MOD. No hace nada hasta que un slot de MOD la usa (sección 4.4). |
| **CC11 expresión** | La fuente **EXPR** de la matriz MOD. Hasta que un controlador manda CC11 vale al máximo (127). |
| **Aftertouch de canal** | La fuente **AT** de la matriz MOD (compartida con la presión de las propias teclas del FM-1: gana la última). Los sonidos nuevos mandan AT al filtro. |
| **Aftertouch polifónico** | **Se ignora.** |
| **Program change** | Carga el sonido número *n* + 1 de la lista PRESETS en la pista que toca ese canal. En el **canal 16**: los programas 0–3 saltan a la sección A–D, los programas 16–21 cargan el proyecto 1–6 (solo parado). Ver [capítulo 14](MANUAL.es.md#14-midi-y-usb). El bank select (CC0 / CC32) se ignora. |
| **CC120 / CC123** | All Sound Off (inmediato) / All Notes Off (respeta el pedal). |
| **CC121** | Reset All Controllers: bend, rueda, aftertouch, expresión y pedal vuelven a reposo; el rango del bend se mantiene. |

Las filas **MENU › CONTROL › VELOCITY / VEL MIN / AFTERTOUCH** le dan forma a las teclas del propio FM-1, no al MIDI
que entra.

### Armados rápidos

- **Un teclado, tocar la pista que esté elegida**: ROUTE **SEL**. Elegí la pista con ALGORITHM.
- **Un teclado, bajo en la mano izquierda y acordes en la derecha**: KEY SPLIT en ON, LOW TRK / HIGH TRK, en KEY
  SPLIT 2 **MIDI CH** = el canal de tu teclado y **MIDI CUT** = la nota del corte.
- **Un secuenciador tocando varias pistas**: ROUTE **CH1-6**, las pistas del secuenciador en los canales 1–6 (TRACKS 6
  para T5 / T6). Sus otros canales (7–16) no se escuchan, así que una caja de ritmos en el canal 10 puede compartir el
  cable.

## 4. Modular JIANT con CC

Hay cuatro capas. Andan igual por USB, USB HOST, TRS y Bluetooth, en la pista que toca el canal (ROUTE, como las
notas). Un CC mueve su parámetro exactamente como girar la perilla: **0–127 sobre todo el rango del parámetro** (64 es
el centro de uno bipolar), y **AUTOMATION lo graba** como un movimiento de perilla (la pista elegida, armada y
andando).

### 4.1 El mapa fijo de CC (siempre activo con MIDI MAP FACTORY o CUSTOM ADD)

Fuente: `firmware/src/midi_control.c`, `MIDI_CC_MAP` y `MIDI_CC_ENG` (líneas 239–262 en V10.8.48).

| CC | Parámetro |
| --- | --- |
| 5 | GLIDE |
| 7 | LEVEL |
| 10 | PAN |
| 16–23 | **E1–E8**: los ocho valores propios del motor, las perillas de EDIT 1 / EDIT 2 sea cual sea el motor (CC16 = KNOB 1 de EDIT 1) |
| 71 | la **resonancia** propia del motor (ver abajo) |
| 72 | REL (release de amplitud) |
| 73 | ATK (ataque de amplitud) |
| 74 | el **brillo / corte** propio del motor (ver abajo) |
| 75 | DEC (decay de amplitud) |
| 91 | REV (envío a reverb) |
| 93 | CHORUS (envío a chorus) |
| 94 | DLY (envío a delay) |
| 102 | el corte del filtro final (FCUT) |
| 103 | la resonancia del filtro final (FRES) |
| 104 | DIST (drive) |
| 105 | SUS (nivel de sustain de amplitud; no el pedal, que es el CC64) |

**CC74 y CC71 por motor** (`MIDI_CC_ENG`): van a uno de los slots E1–E8 del motor, o a nada.

| Motor | CC74 | CC71 |
| --- | --- | --- |
| ANALOG | E5 (CUT) | E6 (RES) |
| PHASE | E3 | — |
| MATH | el CUT de la página FILTER (como CC102) | el RES de la página FILTER (como CC103) |
| VOICE | — | E7 |
| WHEEL | — | — |
| STRING | E3 | E2 |
| DRUM | E3 (TONE) | — |
| WEST | E1 (FOLD) | E7 (RES) |
| FM6 | E3 | — |
| ADD | E2 | E6 |
| ANIMAL | E2 | E3 |
| EXT | — | — |

"—": ese motor ignora el CC. Los números de slot salen del código; los nombres entre paréntesis son los que muestra
hoy la página EDIT de ese motor ahí. Para los demás, fijate en qué perilla está el slot en EDIT 1 / EDIT 2 del motor
(E1–E4 = KNOB 1–4 de EDIT 1, E5–E8 = los de EDIT 2).

### 4.2 MIDI MAP y MIDI LEARN: tus propios CC

**MENU › CONTROL › MIDI MAP**: **FACTORY** (solo el mapa de arriba), **CUSTOM ADD** (el mapa y tus CC aprendidos; un
CC aprendido le gana al significado que el mapa le da al mismo número), **CUSTOM** (solo tus CC aprendidos: los CC
del mapa no hacen nada si no los aprendiste).

Para aprender (cualquier perilla del sonido de una pista a cualquier CC):

1. **MENU › CONTROL › LEARN**: **SEL TRACK** (el CC sigue a la pista que esté elegida cuando llega) o **FIXED TRK**
   (queda en la pista donde lo aprendiste). Cerrá el MENU (HOME): la pantalla dice LEARN: TURN A KNOB.
2. Girá la perilla en el FM-1: **LEARN T2 CUT: SEND A CC**.
3. Mové la perilla del controlador: **CC21 > T2 CUT**. Después la perilla siguiente, el CC siguiente …
4. **LEARN OFF** cuando termines. **LEARN CLEAR ALL** (al cerrar el MENU) los olvida todos.

- **Diez** CC aprendidos como máximo (**LEARN FULL**). Un CC mueve un parámetro, un parámetro tiene un CC.
- **Nunca se aprenden** (la pantalla dice CCn RESERVED; siguen con lo suyo): CC0, CC1, CC6, CC11, CC32, CC38, CC64,
  CC96–101, CC120–127 (`ml_free_cc` en `midi_control.c`).
- Aprender con MIDI MAP en FACTORY lo pasa a CUSTOM ADD.
- Un CC aprendido responde en cualquier canal que ROUTE escuche (con CH1-6, no en 7–16). Se guarda con los ajustes,
  no en un proyecto.

> La sección MIDI LEARN del manual dice que los CC del mapa fijo (5, 7, 10, 16–23 …) nunca se aprenden. En el código
> de V10.8.48 **sí** se pueden aprender: solo se rechaza la lista de arriba, y en CUSTOM ADD un CC aprendido le gana
> al mapa.

### 4.3 NRPN: todos los parámetros, 14 bits

Todo parámetro que la automatización puede grabar, y la mayoría de los globales, tiene un número NRPN:

- **Parámetro de pista**: CC99 **0**, CC98 el número del parámetro, en el canal que toca la pista (con ROUTE CH1-6,
  el canal 16 también llega a la pista elegida), después **CC6** (los 7 bits altos del valor; solo, es un valor de 7
  bits) y **CC38** (los 7 bits bajos).
- **Global** (tempo, swing, los buses, el SLICER, el DJ FILTER …; no el clock, el ruteo, las cargas ni el cambio de
  motor): CC99 **1**, CC98 el número del global, en el canal 16 o en cualquier canal que el FM-1 escuche.

Los números y rangos, generados desde el firmware: **[docs/NRPN.md](NRPN.md)**.

### 4.4 Fuentes de MOD: MODW, AT, EXPR

La rueda de modulación, el aftertouch y el pedal de expresión no mueven un parámetro fijo: son **fuentes** de la
matriz MOD, así que vos decidís qué hacen, por pista.

**LFO › MOD**: KNOB 1 el SLOT (1–4), KNOB 2 **SRC** = **MODW** (CC1), **AT** (aftertouch de canal) o **EXPR** (CC11),
KNOB 3 **DEST** (CUT, PITCH, AMP, E1–E8, un envío …), KNOB 4 **AMT**. Ejemplo: SRC MODW, DEST CUT, AMT 40: la rueda
abre el filtro. Ver [Modulación y macros](MANUAL.es.md#10-modulación-y-macros).

## 5. Clock y sincronía

### JIANT sigue a otra máquina

**GLO › GLOBAL, KNOB 3: CLOCK**

| CLOCK | El tempo viene de |
| --- | --- |
| **INT** (por defecto) | el propio FM-1 (BPM) |
| **USB +BT** | el clock que llega por USB, USB HOST o Bluetooth |
| **TRS** | el clock que llega por el jack TRS |

- El FM-1 sigue **24 pulsos por negra**, **Start** (desde el paso 1), **Continue** (desde donde paró) y **Stop**. Solo
  se escuchan el clock y el transporte de la entrada elegida; las otras entradas igual tocan notas.
- El tempo se mide sobre hasta dos negras y se sostiene como un BPM entero (40–240), que se ve como el BPM.
- Si el clock se corta medio segundo mientras toca, el FM-1 para (un cable que se sale no deja nada colgado).
- Con clock externo, TAP y GLO + PLAY (volver a empezar) no hacen nada. Cambiar CLOCK para el transporte.
- **El Song Position Pointer no se lee**: Continue sigue desde donde paró el FM-1, no desde donde está el maestro.

### JIANT manda

**MENU › CONTROL › MIDI OUT CLOCK** (o ALL), con **CLOCK INT**: el FM-1 manda el clock (24 por negra), **Start** cuando
arranca y **Stop** cuando para. El clock corre también parado, así el que sigue ya conoce el tempo antes del PLAY.

- Sale por **USB** (cuando el FM-1 es un dispositivo conectado a un host) y por **Bluetooth**. **Por el jack TRS no
  sale nada** (es solo entrada), y en modo USB HOST tampoco.
- Mientras el FM-1 sigue un clock externo (CLOCK USB +BT o TRS) **no** manda clock: nunca reenvía un clock.

## 6. MIDI OUT

**MENU › CONTROL › MIDI OUT**

| Valor | Qué sale |
| --- | --- |
| **KEYS** (por defecto) | las notas que tocás en las teclas del FM-1 |
| **CLOCK** | las teclas, y el clock con Start / Stop (con CLOCK INT) |
| **ALL** | lo anterior, y cada nota que toca el FM-1 (secuenciador, arpegiador, PLAYHEADS, repeticiones de los punch-in con su OCT / RANDOM), la presión de las teclas como aftertouch de canal, las teclas de expresión como CC1 y pitch bend, y cada perilla que girás en el panel como **NRPN** (la de una pista en su canal, la de un global en el canal 16) |
| **OFF** | nada |

- **Los canales son fijos**: la pista 1 manda por el canal 1 … la pista 6 por el canal 6. No se pueden cambiar.
- Cada nota mandada tiene su note-off; STOP, un pánico o un cambio del ajuste apagan lo que quede sonando.
- Con ALL, una pista con el ARP prendido manda las notas del arpegio (no las teclas crudas).
- El MIDI que entra **no** se vuelve a mandar (no hay MIDI thru, ver sección 8).

## 7. Receta: Circuit Tracks, h2midi y cables TRS-A

> **Todavía sin probar en equipos reales.** El FM-1 es un dispositivo USB estándar (clase MIDI + audio) y su USB HOST acepta teclados MIDI estándar; si una caja USB-host como la h2midi alimenta y acepta al FM-1, y si un Circuit Tracks enumera en el USB HOST del FM-1, no se probó. Si lo probás, contanos qué pasó (Discord o un issue de GitHub).

Lo que garantiza el código del lado del FM-1 está marcado como tal. **Los detalles del Circuit Tracks y del h2midi no
salen del código de JIANT**: salen de la documentación habitual de esos productos y hay que comprobarlos en sus
propios manuales (acá, **sin verificar**).

### A. Manda el Circuit Tracks, JIANT lo acompaña

**Cables (elegí un camino):**

- **Camino 1, solo TRS (el más simple).** El Circuit Tracks tiene su propio MIDI OUT TRS (tipo A, según Novation; sin
  verificar acá). **MIDI OUT** del Circuit → cable TRS-A → **jack TRS del FM-1**. Para esta dirección no hace falta el
  h2midi. Si el FM-1 no responde, mirá la [sección 2](#2-el-jack-trs-tipo-a-o-tipo-b) (el tipo del jack del FM-1 está
  sin verificar).
- **Camino 2, por el h2midi.** **USB** del Circuit → puerto USB host del h2midi; MIDI OUT del h2midi → cable → **jack
  TRS del FM-1**. Los mismos ajustes del FM-1 que en el camino 1. Si los jacks MIDI del h2midi son TRS (y de qué tipo)
  o DIN de 5 pines depende del modelo: fijate en su manual.
- **Camino 3, sin ninguna caja (experimental).** **USB** del Circuit → **USB del FM-1** con el FM-1 en **USB HOST**. El
  Circuit tiene que alimentarse solo (batería o su fuente): el FM-1 no da corriente. No probado con un Circuit Tracks.

**En el Circuit Tracks** (en su Setup view; los botones exactos, en su manual):

- Prendé el **envío de clock MIDI (Tx)**, y la salida de notas de las pistas que querés que JIANT escuche.
- Sus canales MIDI por defecto son, según Novation (sin verificar acá): Synth 1 = canal 1, Synth 2 = canal 2, MIDI 1
  = canal 3, MIDI 2 = canal 4, batería = canal 10. Ponelos en las pistas de JIANT que querés tocar.

**En el FM-1:**

1. **GLO › SYSTEM › ROUTE CH1-6**: el canal *n* toca la pista *n*. La batería del Circuit en el canal 10 queda
   ignorada. (O **SEL**, si una sola pista del Circuit tiene que tocar la pista elegida de JIANT.)
2. **MENU › AUDIO › TRACKS 6** si usás los canales 5 o 6.
3. **GLO › GLOBAL › CLOCK TRS** para los caminos 1 y 2, **CLOCK USB +BT** para el camino 3.
4. Opcional: un slot de MOD con SRC MODW / AT, o MIDI LEARN, si las macros del Circuit mandan CC.

**Qué esperar:** apretás PLAY en el Circuit y el FM-1 arranca con él (Start), a su tempo; STOP para los dos. El BPM
del FM-1 muestra el tempo del Circuit. Las notas del Circuit en los canales 1–6 tocan las pistas 1–6 de JIANT, y se
graban en una pista de JIANT armada. Apretar PLAY en el FM-1 no arranca el Circuit.

### B. Manda JIANT, el Circuit Tracks lo sigue

El FM-1 solo puede mandar MIDI por **USB** (o Bluetooth), nunca por TRS. Entonces el h2midi es lo que convierte el
USB del FM-1 en un cable MIDI:

- **Cables:** **USB del FM-1** → **puerto USB host** del h2midi; **MIDI OUT** del h2midi → cable (TRS-A, o el adaptador
  DIN del Circuit, según los jacks del h2midi) → **MIDI IN del Circuit**. El FM-1 tiene que aparecerle al h2midi como
  un dispositivo MIDI USB class compliant (lo es: "FM-1"); si el h2midi lo alimenta y lo acepta está **sin
  verificar**.
- **En el FM-1:** **CLOCK INT**, MENU › CONTROL › **MIDI OUT CLOCK** (solo clock y transporte) o **ALL** (además cada
  nota que toca JIANT, la pista *n* por el canal *n*, así la T1 de JIANT puede tocar el Synth 1 del Circuit, la T2 su
  Synth 2 …). No uses el modo USB HOST acá: en HOST el FM-1 no manda nada.
- **En el Circuit:** prendé la **recepción de clock MIDI (Rx)** y la entrada de notas de las pistas que JIANT tiene
  que tocar (fijate en su manual).
- **Qué esperar:** PLAY en el FM-1 arranca el Circuit (Start), al BPM de JIANT; STOP lo para. El clock sigue corriendo
  parado, así el Circuit ya muestra el tempo.

### Las dos direcciones a la vez

Se puede: TRS OUT del Circuit → jack TRS del FM-1 (notas a JIANT), y USB del FM-1 → h2midi → MIDI IN del Circuit
(notas y clock al Circuit). Dejá **un solo** maestro: si manda el FM-1 (CLOCK INT, MIDI OUT CLOCK / ALL), apagá el
envío de clock del Circuit; si manda el Circuit (CLOCK TRS en el FM-1), poné MIDI OUT en KEYS u OFF en el FM-1. JIANT
no repite lo que recibe, así que por el FM-1 mismo no se arma un loop MIDI; igual puede armarse uno si el Circuit
repite su entrada en su salida (revisá sus ajustes de thru / eco).

## 8. Lo que no puede hacer (V10.8.48)

Todo comprobado en el código salvo donde se indica.

- **No hay MIDI OUT por el jack TRS**: el jack es solo entrada (el firmware configura solo una línea de recepción).
- **No hay MIDI THRU ni reenvío**: las notas y CC que entran tocan JIANT y no se mandan a ningún lado; un clock externo
  no se reenvía (el clock sale solo con CLOCK INT).
- **No hay SysEx por TRS ni por Bluetooth**: el parser de TRS descarta el SysEx, y Bluetooth lleva solo mensajes de
  canal y el clock. El editor, los backups y las actualizaciones son SysEx solo por **USB**.
- **No hay MPE**: un canal toca una pista entera (ROUTE), y una pista tiene un solo bend; los canales por nota
  repartirían las notas en pistas distintas (CH1-6) o compartirían un bend (SEL).
- **No hay aftertouch polifónico** (se ignora); solo aftertouch de canal.
- **No hay Song Position Pointer ni MIDI Time Code**: solo clock, Start, Continue y Stop.
- **No hay bank select**: CC0 / CC32 se ignoran; el program change llega a los primeros 128 sonidos de la lista
  PRESETS.
- **Los canales de MIDI OUT son fijos** (pista *n* = canal *n*), y los canales de recepción son los de ROUTE (CH1-6 o
  SEL), más el canal único de KEY SPLIT.
- **No hay MIDI out en modo USB HOST**, ni conexión con la compu mientras HOST está prendido, ni corriente en el
  puerto, ni dispositivos low speed.
- **Diez CC aprendidos como máximo.**
- **No hay selector de entrada**: todas las entradas se escuchan siempre (solo se elige la fuente del clock).
- **Tempo externo de 40 a 240 BPM.**

## 9. Sin verificar (para comprobar en el aparato)

- El tipo del jack TRS del FM-1 (A o B): lo define el hardware, no se ve en el firmware.
- Circuit Tracks: su tipo de TRS, sus canales por defecto, y los ajustes de clock y notas de su Setup view.
- h2midi: si sus jacks son TRS (de qué tipo) o DIN, si su puerto host alimenta y acepta al FM-1 (un dispositivo MIDI
  + audio USB).
- El Circuit Tracks directo al USB HOST del FM-1 (camino 3): no probado.
- Los nombres de los slots E de CC74 / CC71 en PHASE, VOICE, STRING, FM6, ADD y ANIMAL (los números de slot están
  verificados).
