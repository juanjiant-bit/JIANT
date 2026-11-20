# FM-1 Rescue para Android — LÉEME

[English below](#english)

Repara un **M-VAVE FM-1 en hard brick** (pantalla negra, no aparece por USB) con un **RP2040** y **tu móvil
Android**. No hace falta ordenador, ni Python, ni copiar archivos: la app graba el RP2040 por el cable OTG y
después te va diciendo qué hacer mientras repara el FM-1.

> **Estado:** versión 0.1, **sin probar todavía en hardware real**. El núcleo (lectura de la V15, la imagen que
> se graba en el RP2040 y el protocolo con el bootrom) está probado en el ordenador contra un RP2040 simulado
> y el resultado es idéntico byte a byte al de la página web (`rescue/fm1-rescue.html`). Úsala con la batería
> del FM-1 cargada y bajo tu responsabilidad.

## Qué necesitas

| | |
|---|---|
| Móvil Android 7 o más nuevo | con USB OTG (casi todos los actuales) |
| Adaptador **OTG** USB-C (o micro-USB) a USB-A, o un cable USB-C a USB-C | para enchufar el RP2040 al móvil |
| **Waveshare RP2040-Zero** o **Seeed XIAO RP2040** | la placa de la guía de GoodPerson42 |
| Cable USB-C con 4 hilos sueltos | negro, rojo, blanco, verde |
| Internet **la primera vez** | la app descarga sola la V15 oficial (700 KB); luego funciona sin conexión |

## Instalar

Descarga `fm1-rescue.apk`: pestaña **Actions** del repositorio → ejecución **android** → **Artifacts** →
`fm1-rescue` (dentro del zip está `app-debug.apk`). Ábrelo en el móvil y permite "instalar apps de origen
desconocido" si Android lo pide.

## Usar

1. **Abre la app con internet.** Descarga sola la V15 oficial y la comprueba por su hash (sale "✓ V15 oficial
   lista"). Solo la primera vez: la guarda. Si prefieres usar tu propio `FM-1.fwsc` de la V15, pulsa "O elegir
   mi propio FM-1.fwsc" (cualquier otro archivo se rechaza; la V16 no sirve aquí).
2. **Elige tu placa:** RP2040-Zero o XIAO RP2040.
3. **Cableado**, con el FM-1 **apagado** (el rojo no se conecta):

   | Hilo | RP2040-Zero | XIAO RP2040 |
   |---|---|---|
   | verde (D+) | `0` | `D6` |
   | blanco (D−) | `1` | `D7` |
   | negro (GND) | `GND` | `GND` |
   | rojo (5 V) | — | — |

   Enchufa el USB-C de esos hilos al FM-1.
4. **Mantén pulsado BOOT** en el RP2040 y conéctalo al móvil con el adaptador OTG. Acepta el aviso de acceso USB
   (marca "usar siempre" para que no vuelva a salir).
5. La app **graba el RP2040 sola** (unos 15 segundos, con barra de progreso) y lo reinicia.
6. Cuando la app diga **"Enciende el FM-1 ahora"**, enciéndelo. Verás "Reparando el FM-1…" con el progreso
   (1–3 minutos). **No desconectes nada.**
7. **"¡Listo!"** → apaga el FM-1, quita los cables y vuelve a encenderlo: arranca con la V15. Después instala el
   firmware que quieras (JIANT, Felucca, V16…) de la forma normal.

Si el RP2040 ya tiene el firmware de rescate (o el FM-1 Transporter), no hace falta pulsar BOOT: la app lo
pasa a modo de grabación ella misma. Para reparar otro FM-1 basta con desconectar y volver a conectar el RP2040.

## Si algo sale mal

| La app dice | Qué hacer |
|---|---|
| "Permite el acceso USB" | Acepta el aviso de Android. |
| "No se pudo grabar el RP2040" | Desconéctalo y vuelve a conectarlo con BOOT pulsado. Prueba otro adaptador OTG. |
| "Apaga el FM-1" | El FM-1 estaba encendido con otro firmware. Apágalo, reconecta el RP2040, enciéndelo. |
| Error: "Se perdió la conexión…" | Revisa los cables; prueba a intercambiar verde y blanco. Batería cargada. |
| Error: "Esto no parece un FM-1" | No se escribió nada. Revisa que sea un FM-1. |
| Error: "Una escritura no se completó…" | Apaga el FM-1, reconecta el RP2040 y repite: retoma donde quedó. |

El LED del RP2040 dice lo mismo sin mirar el móvil: amarillo parpadeando = esperando que enciendas el FM-1,
azul = trabajando, verde fijo = listo, rojo parpadeando rápido = error.

## Qué hace exactamente

- **En el RP2040** (por PICOBOOT, el protocolo del bootrom que usa `picotool`): borra, escribe y **relee** cada
  sector del firmware de rescate (`rescue/firmware`, basado en el FM-1 Transporter) más la V15 y el loader, y lo
  reinicia.
- **En el FM-1** (lo hace el RP2040): lo pone en modo de arranque de la ROM, comprueba que es un FM-1 (chip
  `0x980F`, flash `0x856014`/`0xB36014`) y escribe **solo los sectores de `0x4000..0x92FFF` que difieren** de la
  V15, cada uno verificado, con una relectura final completa.
- **Nunca** toca la cabecera de arranque (`0x0000..0x3FFF`) ni tus datos (`0x93000..`: ajustes, sonidos,
  canciones). A diferencia del método con Python, **no** guarda una copia completa del FM-1 antes.
- El móvil solo alimenta el RP2040 y lee su progreso. Lo único que se descarga es la V15: el `V15-FM-1.fwsc` de
  [Quixotic7/FM1Backups](https://github.com/Quixotic7/FM1Backups) en un commit fijado, idéntico al archivo de
  M-VAVE (sha256 `db1642b2…`), y la app solo lo acepta si el hash de su región de firmware es el de la V15.
  La V15 no va dentro del APK porque es software de M-VAVE; el loader de JieLi sí (jl-uboot-tool, MIT,
  licencia en `assets/wl82loader-LICENSE.txt`).

### ¿Y la V16?

No se puede escribir con este método. La V16 ocupa hasta `0xA8FFF` (más allá de la zona `0x4000..0x92FFF`) y trae
su propio bootloader en la cabecera, que este método nunca toca para no arriesgar un brick peor. Se repara con
la V15 y después se actualiza a la V16 (o a JIANT) con el instalador normal.

## Para desarrolladores

| | |
|---|---|
| `app/src/main/java/fm/jiant/rescue/RescueImage.java` | `.fwsc` → región V15, payload, sectores a grabar (igual que `rescue/builder.js`) |
| `Picoboot.java` | protocolo PICOBOOT del bootrom del RP2040 |
| `RescueStatus.java` | la respuesta de `rescue` del firmware |
| `MainActivity.java` | la pantalla, USB OTG y permisos |
| `test/run.sh` | prueba en el ordenador con un RP2040 simulado (necesita la V15, la V16 y el loader) |

Compilar: `gradle -p android/rescue assembleDebug` (JDK 17, Gradle 8.9); el firmware para cada placa sale de
`rescue/base/` (`rescue/build.sh`). Licencia MIT; créditos en `rescue/README.md`.

## English

FM-1 Rescue for Android unbricks an FM-1 with an RP2040 and the phone: on first launch it downloads the
official V15 (pinned, hash-checked; or choose your own), JieLi's loader is bundled; pick the board, wire green → GP0, white → GP1, black → GND (FM-1 off, red unconnected), hold BOOT and
plug the RP2040 into the phone with an OTG adapter. The app programs the board over PICOBOOT, reboots it, and
then shows the rescue's progress read from the board: "switch the FM-1 on now", "repairing…", "done". Only
the differing sectors of `0x4000..0x92FFF` are written, each verified; the boot head and user data are never
touched. Version 0.1, **not yet tested on hardware**.
